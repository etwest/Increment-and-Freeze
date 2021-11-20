#! /usr/local/bin/python3

import numpy
import sys
import matplotlib.pyplot as plt

def get_zipf(a, size):
	return numpy.random.zipf(a, size)

if __name__ == '__main__':
	if len(sys.argv) != 3:
		print("Invalid ARGS: Must be ziph.py [alpha] [num_samples]")
		exit()
	alpha = float(sys.argv[1])
	size  = int(sys.argv[2])

	if alpha < 1:
		print("Invalid ARGS: alpha must be greater than 1")
		exit();
	if size < 1:
		print("Invalid ARGS: size must be greater than 1")
		exit();

	outFile = open("ziph_data", "w")

	samples = get_zipf(alpha, size)
	for sample in samples:
		outFile.write(str(sample) + "\n")

	# plt.hist(samples[samples<1000], 1000, density=True)
	# plt.show()

	