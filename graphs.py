import matplotlib.pyplot as plt
import json

x_labels = ["2", "4", "8", "16", "32", "64", "128", "256"]
y_label = "Execution time (s)"

def plot_speedup(times, labels=None):
    """
    Generates a speedup graph given a list of execution times.

    Parameters:
        times (list of float): List of execution times.
        labels (list of str, optional): Labels for the x-axis, corresponding to the times.
                                       Defaults to integers (1, 2, 3, ...).
    """
    if not times or len(times) < 2:
        raise ValueError("The 'times' list must have at least two elements.")

    # Calculate speedups
    baseline = times[0]
    speedups = [baseline / t for t in times]

    # Generate default labels if none provided
    if labels is None:
        labels = [str(i + 1) for i in range(len(times))]

    if len(labels) != len(times):
        raise ValueError("The 'labels' list must have the same length as the 'times' list.")

    # Plot the graph
    plt.figure(figsize=(8, 6))
    plt.plot(labels, speedups, marker='o', linestyle='-', color='b')
    plt.title('Speedup Graph')
    plt.xlabel('Number of Threads / Configurations')
    plt.ylabel('Speedup (Baseline = 1)')
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.xticks(rotation=45)
    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    with open('export.json', 'r') as file:
        data = json.load(file)
    
    x = []
    heights = []
    for i, result in enumerate(data["results"]):
        x.append(x_labels[i])
        heights.append(result["mean"])

    plot_speedup(heights, x)

    # plt.title("Execution time by thread count")
    # plt.xlabel("Thread count")
    # plt.ylabel(y_label)
    # plt.bar(x, heights)
    # plt.show()