#pragma once

#include <torch/torch.h>
#include "alphaZeroEngine/config/headers/config.h"

// HIVENET - ResNet-style CNN for Hive with spatial policy head
//
// Inputs:
//   planes  [B, NUM_CHANNELS, GRID_SIZE, GRID_SIZE]  - spatial features
//   scalars [B, NUM_SCALAR_FEATURES]                 - global features
//
// Body:
//   Conv3x3(NUM_CHANNELS → NUM_FILTERS) → BN → ReLU
//   → ResidualBlock × NUM_RESIDUAL_BLOCKS
//
// Policy head (spatial + pass):
//   Conv1x1(NUM_FILTERS → POLICY_PLANES) on trunk          → [B, 28, H, W]
//   GlobalAvgPool(trunk) → Linear(NUM_FILTERS → 1)         → [B, 1]    (pass logit)
//   concat(flatten(spatial), pass) → policy logits         → [B, ACTION_SPACE]
//
// Value head:
//   Conv1x1(NUM_FILTERS → VALUE_CHANNELS) → BN → ReLU → flatten
//   concat(flat, scalars) → Linear → ReLU → Linear → tanh   → [B, 1]
//
// forward_masked applies the legal-move mask before softmax.

namespace Hive::Learning {

    struct ResidualBlockImpl : torch::nn::Module {
        torch::nn::Conv2d conv1{nullptr};
        torch::nn::BatchNorm2d bn1{nullptr};
        torch::nn::Conv2d conv2{nullptr};
        torch::nn::BatchNorm2d bn2{nullptr};

        ResidualBlockImpl(int channels) {
            conv1 = register_module("conv1",
                torch::nn::Conv2d(torch::nn::Conv2dOptions(channels, channels, 3).padding(1)));
            bn1 = register_module("bn1", torch::nn::BatchNorm2d(channels));
            conv2 = register_module("conv2",
                torch::nn::Conv2d(torch::nn::Conv2dOptions(channels, channels, 3).padding(1)));
            bn2 = register_module("bn2", torch::nn::BatchNorm2d(channels));
        }

        torch::Tensor forward(torch::Tensor x) {
            auto residual = x;
            x = torch::relu(bn1(conv1(x)));
            x = bn2(conv2(x));
            x = torch::relu(x + residual);
            return x;
        }
    };
    TORCH_MODULE(ResidualBlock);

    struct HiveNetImpl : torch::nn::Module {
        torch::nn::Conv2d inputConv{nullptr};
        torch::nn::BatchNorm2d inputBn{nullptr};

        std::vector<ResidualBlock> resBlocks;

        // Policy head: spatial conv + pass logit head
        torch::nn::Conv2d policyConv{nullptr};
        torch::nn::Linear passFc{nullptr};

        // Value head: conv to scalar plane + concat with scalar features
        torch::nn::Conv2d valueConv{nullptr};
        torch::nn::BatchNorm2d valueBn{nullptr};
        torch::nn::Linear valueFc1{nullptr};
        torch::nn::Linear valueFc2{nullptr};

        HiveNetImpl() {
            inputConv = register_module("input_conv",
                torch::nn::Conv2d(torch::nn::Conv2dOptions(NUM_CHANNELS, NUM_FILTERS, 3).padding(1)));
            inputBn = register_module("input_bn", torch::nn::BatchNorm2d(NUM_FILTERS));

            for (int i = 0; i < NUM_RESIDUAL_BLOCKS; ++i) {
                auto block = ResidualBlock(NUM_FILTERS);
                resBlocks.push_back(register_module("res_block_" + std::to_string(i), block));
            }

            // Spatial policy: 1x1 conv produces POLICY_PLANES planes,
            // flattened into (POLICY_PLANES * GRID_SIZE * GRID_SIZE) logits.
            policyConv = register_module("policy_conv",
                torch::nn::Conv2d(torch::nn::Conv2dOptions(NUM_FILTERS, POLICY_PLANES, 1)));

            // Pass logit: average-pool the trunk to a NUM_FILTERS-vector,
            // then a tiny FC to a single scalar logit appended to the policy.
            passFc = register_module("pass_fc",
                torch::nn::Linear(NUM_FILTERS, 1));

            // Value head: same conv-then-flatten structure as before, with
            // scalar features concatenated before the first FC.
            valueConv = register_module("value_conv",
                torch::nn::Conv2d(torch::nn::Conv2dOptions(NUM_FILTERS, VALUE_CHANNELS, 1)));
            valueBn = register_module("value_bn", torch::nn::BatchNorm2d(VALUE_CHANNELS));
            valueFc1 = register_module("value_fc1",
                torch::nn::Linear(VALUE_CHANNELS * GRID_SIZE * GRID_SIZE + NUM_SCALAR_FEATURES,
                                  VALUE_HIDDEN));
            valueFc2 = register_module("value_fc2",
                torch::nn::Linear(VALUE_HIDDEN, 1));
        }

        // Forward: returns {policy_logits [B, ACTION_SPACE], value [B, 1]}
        std::pair<torch::Tensor, torch::Tensor> forward(
                torch::Tensor planes, torch::Tensor scalars) {
            auto x = torch::relu(inputBn(inputConv(planes)));
            for (auto& block : resBlocks) {
                x = block->forward(x);
            }
            // x: [B, NUM_FILTERS, GRID_SIZE, GRID_SIZE]

            // Spatial policy logits
            auto spatial = policyConv(x);                            // [B, POLICY_PLANES, H, W]
            spatial = spatial.view({spatial.size(0), -1});            // [B, POLICY_PLANES * H * W]

            // Pass logit from globally-pooled trunk features
            auto pooled = torch::adaptive_avg_pool2d(x, {1, 1})
                              .view({x.size(0), -1});                 // [B, NUM_FILTERS]
            auto passLogit = passFc(pooled);                          // [B, 1]

            auto policyLogits = torch::cat({spatial, passLogit}, /*dim=*/1); // [B, ACTION_SPACE]

            // Value head with scalar features concatenated
            auto v = torch::relu(valueBn(valueConv(x)));              // [B, VC, H, W]
            v = v.view({v.size(0), -1});                              // [B, VC*H*W]
            v = torch::cat({v, scalars}, /*dim=*/1);                  // [B, VC*H*W + S]
            v = torch::relu(valueFc1(v));
            v = torch::tanh(valueFc2(v));

            return {policyLogits, v};
        }

        // Forward with legal-move masking: returns softmax-normalized policy + value
        std::pair<torch::Tensor, torch::Tensor> forward_masked(
                torch::Tensor planes,
                torch::Tensor scalars,
                torch::Tensor mask) {
            auto [logits, value] = forward(planes, scalars);
            auto masked = logits + (1.0f - mask) * (-1e9f);
            auto policy = torch::softmax(masked, /*dim=*/1);
            return {policy, value};
        }
    };
    TORCH_MODULE(HiveNet);

} // namespace Hive::Learning
