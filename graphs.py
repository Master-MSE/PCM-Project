import matplotlib.pyplot as plt
import json

x_labels = ["2", "4", "8", "16", "32", "64", "128", "256"]
y_label = "Execution time (s)"

if __name__ == "__main__":
    with open('export.json', 'r') as file:
        data = json.load(file)
    
    x = []
    heights = []
    for i, result in enumerate(data["results"]):
        x.append(x_labels[i])
        heights.append(result["mean"])

    plt.title("Execution time by thread count")
    plt.xlabel("Thread count")
    plt.ylabel(y_label)
    plt.bar(x, heights)
    plt.show()