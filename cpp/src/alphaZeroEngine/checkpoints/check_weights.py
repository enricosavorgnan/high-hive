import torch

# Load the file
pt_data = torch.load("pretrained_best.pt", map_location=torch.device('cpu'), weights_only=False)

# View the keys inside the file (e.g., weights, biases, epochs)
print("--- Model Parameters ---")
for name, param in pt_data.named_parameters():
    print(f"Name: {name} | Shape: {list(param.shape)}")
    tensor_values = param.detach()

    # write on file
    with open("weights.txt", "a") as f:
        f.write(f"Name: {name} | Shape: {list(param.shape)}\n")
        f.write(f"Values:\n{tensor_values}\n\n")
