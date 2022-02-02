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
		hits = []
		while(line != ""):
			data = line.split(":")
			memsize = int(data[0].rstrip())
			nhits = int(data[1].rstrip())

			memory_sizes.append(memsize)
			hits.append(nhits)
			
			line = inFile.readline()

		plt.plot(memory_sizes, hits)
		plt.xlabel("cache size")
		plt.ylabel("cache hits")
		#plt.tight_layout()
		plt.savefig("IMAGE.png");
		plt.show()
		plt.clf()
