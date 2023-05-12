import csv
import numpy as np
import matplotlib.pyplot as plt
TABLE_SIZE = 11
SAMPLE_COUNT = 5
def plot_runtime(filename, labels, divide = False, axis_y = "total runtime [ms]"):


    file = open(filename)
    np_data = np.genfromtxt(filename, delimiter=",")
    print(np_data)
    xs = np_data[0:TABLE_SIZE, 0]
    ys_cores = np.mean(np_data[:, 1:], axis=1)
    print(ys_cores)
    for index, label in enumerate(labels):
        ys = ys_cores[index * TABLE_SIZE: (index+1) * TABLE_SIZE]
        plt.plot(xs, np.divide(xs,ys) * 1000 if divide else ys, label = str(label))
    plt.xlabel("dataset size [milions of hits]")
    plt.ylabel(axis_y)
    plt.legend()
    plt.show()


plot_runtime("output/benchmark_results/BENCHMARK_CORE_COUNT", ["20 split", "16 split", "14 split", "12 split", "10 split", "8 split", "6 split", "4 split", "3 split", "2split"])

plot_runtime("output/benchmark_results/BENCHMARK_CORE_COUNT", 
             ["20 split", "16 split", "14 split", "12 split", "10 split", "8 split", "6 split", "4 split", "3 split", "2 split"],
             True, axis_y= "clustering speed [MHit/s]")
plot_runtime("output/benchmark_results/BENCHMARK_WRITER", [ "8 split parallel write", "4 split parallel write", "4 split serial write"], True, axis_y= "clustering speed [MHit/s]")
plot_runtime("output/benchmark_results/BENCHMARK FREQUENCY", ["16Mhit/s", "8MHit/s", "4MHit/s", "2MHit/s"], True, axis_y= "clustering speed [MHit/s]")
plot_runtime("output/benchmark_results/BENCHMARK_SPLIT_NODE", ["reader split", "calibrator split", "sorter split", "no split"], True, axis_y= "clustering speed [MHit/s]")

