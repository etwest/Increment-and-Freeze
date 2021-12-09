import matplotlib.pyplot as plt
import argparse
import os

parser = argparse.ArgumentParser(description="creates graphs from Paging Simulator output")
parser.add_argument("txt_files", nargs='+', help="files to create graphs from")
args = parser.parse_args()

if not os.path.exists("images"):
    os.mkdir("images")

for file in args.txt_files:
	with open(file, "r") as inFile:
		line = inFile.readline()
		while(line[0] == '#'):
			line = inFile.readline()

		memory_sizes = []
		faults = []
		while(line != ""):
			data = line.split(":")
			memsize = int(data[0].rstrip())
			nfaults = int(data[1].rstrip())

			memory_sizes.append(memsize)
			faults.append(nfaults)
			
			line = inFile.readline()

		plt.plot(memory_sizes, faults)
		#plt.tight_layout()
		plt.show()
		plt.clf()
