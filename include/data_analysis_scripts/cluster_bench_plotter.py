import sys
import csv
import numpy as np
import matplotlib.pyplot as plt
import matplotlib
from enum import Enum

# After passing a data file with means and standard deviations of execution time,
# create a plot which visualizes the performance


class PlotType(Enum):
    XY_PLOT = 1,
    BAR_PLOT = 2


def jitter(array, jitter_size, factor=0.06):
    return array + factor * jitter_size


def plot_bar_transposed(test, test_name):
    plt.figure(figsize=(14, 5))
    plt.title(test_name)
    ys = np.array(test["x_values"])
    plt.xlabel(test["ylabel"])
    particle_index = 0
    for (particle_name) in test["data"]:
        particle_index += 1
        particle_data = test["data"][particle_name]
        means = np.array([mean for [mean, std] in particle_data])
        stds = np.array([std for [mean, std] in particle_data])
        plt.errorbar(np.array(means), jitter(np.arange(
            len(ys)), particle_index), xerr=stds, marker="o", capsize=3, linestyle="None")
    plt.yticks(range(len(ys)), ys, wrap=True)
    plt.legend([key for key in test["data"]])
    plt.gca().set_xlim(left=0)
    plt.show()


def plot_xy(test, test_name):
    plt.figure(figsize=(14, 5))
    plt.title(test_name)
    xs = np.array(test["x_values"])
    plt.xlabel(test["xlabel"])
    plt.ylabel(test["ylabel"])
    for (particle_name) in test["data"]:
        particle_data = test["data"][particle_name]
        means = np.array([mean for [mean, std] in particle_data])
        stds = np.array([std for [mean, std] in particle_data])
        plt.plot(xs, np.array(means))
    for (particle_name) in test["data"]:
        particle_data = test["data"][particle_name]
        means = np.array([mean for [mean, std] in particle_data])
        stds = np.array([std for [mean, std] in particle_data])
        plt.fill_between(xs, y1=means - stds, y2=means + stds, alpha=0.3)

    plt.legend([key for key in test["data"]])
    plt.gca().set_ylim(bottom=0)
    plt.show()


def plot_parsed_data(parsed_data):
    for test_name in parsed_data:
        test = parsed_data[test_name]
        if (test["plottable_type"] == PlotType.XY_PLOT):
            plot_xy(test, test_name)
        elif (test["plottable_type"] == PlotType.BAR_PLOT):
            plot_bar_transposed(test, test_name)


def parse_benchmark_files(filename):
    TEST_SEPARATOR = "%"
    MODEL_SEPARATOR = "#"
    KEY_VALUE_SEPARATOR = ":"
    DATA_SEPARATOR = " "
    AXIS_LABEL_SEPARATOR = "|"
    PLOTTABLE = "XY_PLOTTABLE"
    parsed_data = {}
    test_labels = []
    model_labels = []
    with open(filename, "r") as filestream:
        lines = filestream.readlines()
        current_test = None
        plottable = False
        for line in lines:
            if (line.startswith(TEST_SEPARATOR)):

                current_test = line[1:]
                plottable = PLOTTABLE in current_test
                axis_name_tokens = line.split(AXIS_LABEL_SEPARATOR)
                current_test = current_test.split(AXIS_LABEL_SEPARATOR)[0]
                parsed_data[current_test] = {}
                if plottable:
                    parsed_data[current_test]["xlabel"] = axis_name_tokens[2]
                    parsed_data[current_test]["plottable_type"] = PlotType.XY_PLOT
                else:
                    parsed_data[current_test]["plottable_type"] = PlotType.BAR_PLOT
                parsed_data[current_test]["models"] = []
                parsed_data[current_test]["ylabel"] = axis_name_tokens[1]
                parsed_data[current_test]["data"] = {}
                parsed_data[current_test]["x_values"] = []

            elif line.startswith(MODEL_SEPARATOR):
                current_model = line[1:]
                if plottable:
                    parsed_data[current_test]["x_values"].append(
                        float(current_model.split(KEY_VALUE_SEPARATOR)[-1]))
                else:
                    parsed_data[current_test]["x_values"].append(current_model)

            else:
                tokens = line.split(KEY_VALUE_SEPARATOR)
                particle = tokens[0]
                data = tokens[1]
                if not particle in parsed_data[current_test]["data"]:
                    parsed_data[current_test]["data"][particle] = []
                values = [float(value) for value in data.split(DATA_SEPARATOR)]
                parsed_data[current_test]["data"][particle].append(values)
    plot_parsed_data(parsed_data)


property_header = ["diameter", "timespan"]
filenames = [
    "output/cluster_properties/cluster_properties_pion0.txt",
    "output/cluster_properties/cluster_properties_pion30.txt",
    "output/cluster_properties/cluster_properties_neutron60.txt",
    "output/cluster_properties/cluster_properties_neutron0.txt",
    "output/cluster_properties/cluster_properties_lead90.txt",
    "output/cluster_properties/cluster_properties_lead0.txt",
]


def plot_hist(data):
    plt.hist(data[:, 0], bins=256, alpha=0.65)
    plt.axvline(data[:, 0].mean(), color='k', linestyle='dashed')
    plt.text(data[:, 0].mean() + 5, plt.ylim()[1] *
             0.2, f"Mean: {round(data[:,0].mean(), 2)}")
    plt.title("Histogram of the cluster " + property_header[0])
    plt.xlabel("diameter[pixels]")
    plt.ylabel("number of clusters")
    plt.yscale('log')
    plt.show()

    plt.hist(data[:, 1], bins=256, alpha=0.65)
    plt.axvline(data[:, 1].mean(), color='k', linestyle='dashed')
    plt.yscale('log')
    plt.title("Histogram of the cluster " + property_header[1])
    plt.xlabel("timespan[ns]")
    plt.ylabel("number of clusters")
    plt.text(data[:, 1].mean() + 15, plt.ylim()[1] *
             0.2, f"Mean: {round(data[:,1].mean(), 2)}")
    plt.show()


def plot_cluster_attributes():
    matplotlib.rcParams.update(**{"font.size": 16})
    data = np.empty(shape=[0, 3])
    for file in filenames:
        new_data = np.genfromtxt(file, delimiter=',')
        # plot(new_data)
        data = np.vstack((data, new_data))
        data = data[data[:, 0] != 1]
    plot_hist(data)


if __name__ == "__main__":
    # plot_cluster_attributes()
    matplotlib.rcParams.update(**{"font.size": 16})
    if (len(sys.argv) != 2):
        # print("Incorrect number of arguments passed (expected 1)")

        filename = "output/benchmark_results_laptop/validation_result.txt"
        parse_benchmark_files(filename)

        filename = "output/benchmark_results_laptop/FINAL_PLOTS.txt"
        parse_benchmark_files(filename)
        filename = "output/benchmark_results_server/FINAL_PLOTS.txt"
        parse_benchmark_files(filename)

    else:
        filename = sys.argv[1]

        parse_benchmark_files(filename)
print("done")
