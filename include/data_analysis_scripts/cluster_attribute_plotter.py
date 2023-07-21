import matplotlib.pyplot as plt
import numpy as np

#an auxiliary scipt to analyze the file with computed halo 
class histo2d_plotter:
    def plot(file_name):
        data = np.genfromtxt(file_name, dtype=float, delimiter=',')
        histo = plt.hist2d(data[:, 0], data[:, 1], norm="log", bins=(
            500, np.int(np.max(data[:, 1])) + 1))
        plt.colorbar(histo[3])
        plt.xlim(50, 600)
        plt.xlabel("energy")
        plt.ylabel("halo radius")
        plt.show()


if __name__ == '__main__':
    histo2d_plotter.plot("/home/tomas/MFF/DT/clusterer/output/halos.txt")
