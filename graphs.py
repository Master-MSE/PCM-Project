import matplotlib.pyplot as plt
import json


if __name__ == "__main__":
    with open('export.json', 'r') as file:
        data = json.load(file)
    
    x = []
    heights = []
    for result in data["results"]:
        x.append(result["command"])
        heights.append(result["mean"])

    plt.title("Execution time by thread count")
    plt.xlabel("Thread count")
    plt.ylabel("Execution time (ms)")
    plt.bar(x, heights)
    plt.show()