import matplotlib.pyplot as plt
import argparse
import os

parser = argparse.ArgumentParser(description="creates graphs of zipfian distributions")
parser.add_argument("txt_files", nargs='+', help="files containing distributions")
args = parser.parse_args()

if not os.path.exists("images"):
    os.mkdir("images")

for file in args.txt_files:
	with open(file, "r") as inFile:
		line = inFile.readline()
		while(line[0] == '#'):
			line = inFile.readline()

		ids = []
		repetitions = []
		while(line != ""):
			data = line.split(":")
			i = int(data[0].rstrip()) + 1
			reps = int(data[1].rstrip())

			ids.append(i)
			repetitions.append(reps)
			
			line = inFile.readline()

		plt.plot(ids, repetitions, label=file.split("_")[2][:-5])

plt.legend()
plt.xlabel("Ids")
plt.ylabel("Counts")
plt.yscale("log")
plt.xscale("log")
plt.savefig("Distributions.png");
plt.show()
plt.clf()
