#pragma once

#include <torch/torch.h>
#include "config/headers/config.h"

// HIVENET - ResNet-style CNN for Hive
//
// Architecture:
//   Input [B, 24, 26, 26]
//   → Conv3x3(24→256) → BN → ReLU
//   → ResidualBlock × 19
//   → Policy Head: Conv1x1(256→2) → BN → ReLU → Flatten → FC(2*26*26 → 5488)
//   → Value Head:  Conv1x1(256→1) → BN → ReLU → Flatten → FC(676→256) → ReLU → FC(256→1) → Tanh
//
// Total: ~15-25M parameters

namespace Hive::Learning {

    // Residual Block: Conv3x3 → BN → ReLU → Conv3x3 → BN → (+skip) → ReLU
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

    // HiveNet: full network with policy and value heads
    struct HiveNetImpl : torch::nn::Module {
        // Initial convolution
        torch::nn::Conv2d inputConv{nullptr};
        torch::nn::BatchNorm2d inputBn{nullptr};

        // Residual tower
        std::vector<ResidualBlock> resBlocks;

        // Policy head
        torch::nn::Conv2d policyConv{nullptr};
        torch::nn::BatchNorm2d policyBn{nullptr};
        torch::nn::Linear policyFc{nullptr};

        // Value head
        torch::nn::Conv2d valueConv{nullptr};
        torch::nn::BatchNorm2d valueBn{nullptr};
        torch::nn::Linear valueFc1{nullptr};
        torch::nn::Linear valueFc2{nullptr};

        HiveNetImpl() {
            // Input convolution: 24 → 256 channels
            inputConv = register_module("input_conv",
                torch::nn::Conv2d(torch::nn::Conv2dOptions(NUM_CHANNELS, NUM_FILTERS, 3).padding(1)));
            inputBn = register_module("input_bn", torch::nn::BatchNorm2d(NUM_FILTERS));

            // Residual tower
            for (int i = 0; i < NUM_RESIDUAL_BLOCKS; ++i) {
                auto block = ResidualBlock(NUM_FILTERS);
                resBlocks.push_back(register_module("res_block_" + std::to_string(i), block));
            }

            // Policy head
            policyConv = register_module("policy_conv",
                torch::nn::Conv2d(torch::nn::Conv2dOptions(NUM_FILTERS, POLICY_CHANNELS, 1)));
            policyBn = register_module("policy_bn", torch::nn::BatchNorm2d(POLICY_CHANNELS));
            policyFc = register_module("policy_fc",
                torch::nn::Linear(POLICY_CHANNELS * GRID_SIZE * GRID_SIZE, ACTION_SPACE));

            // Value head
            valueConv = register_module("value_conv",
                torch::nn::Conv2d(torch::nn::Conv2dOptions(NUM_FILTERS, VALUE_CHANNELS, 1)));
            valueBn = register_module("value_bn", torch::nn::BatchNorm2d(VALUE_CHANNELS));
            valueFc1 = register_module("value_fc1",
                torch::nn::Linear(VALUE_CHANNELS * GRID_SIZE * GRID_SIZE, VALUE_HIDDEN));
            valueFc2 = register_module("value_fc2",
                torch::nn::Linear(VALUE_HIDDEN, 1));
        }

        // Forward pass: returns {policy_logits [B, ACTION_SPACE], value [B, 1]}
        std::pair<torch::Tensor, torch::Tensor> forward(torch::Tensor x) {
            // Input convolution
            x = torch::relu(inputBn(inputConv(x)));

            // Residual tower
            for (auto& block : resBlocks) {
                x = block->forward(x);
            }

            // Policy head
            auto p = torch::relu(policyBn(policyConv(x)));
            p = p.view({p.size(0), -1}); // flatten
            p = policyFc(p);             // logits

            // Value head
            auto v = torch::relu(valueBn(valueConv(x)));
            v = v.view({v.size(0), -1}); // flatten
            v = torch::relu(valueFc1(v));
            v = torch::tanh(valueFc2(v));

            return {p, v};
        }

        // Forward with legal move masking: applies mask, returns softmax policy + value
        std::pair<torch::Tensor, torch::Tensor> forward_masked(torch::Tensor x, torch::Tensor mask) {
            auto [logits, value] = forward(x);

            // Apply mask: set illegal actions to -infinity
            auto masked_logits = logits + (1.0f - mask) * (-1e9f);

            // Softmax over legal actions
            auto policy = torch::softmax(masked_logits, /*dim=*/1);

            return {policy, value};
        }
    };
    TORCH_MODULE(HiveNet);

} // namespace Hive::Learning
