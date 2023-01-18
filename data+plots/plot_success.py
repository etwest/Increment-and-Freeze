import matplotlib.pyplot as plt
import argparse
import os

parser = argparse.ArgumentParser(description="Creates graphs from success functions")
parser.add_argument("txt_files", nargs='+', help="Files containing a success function")
args = parser.parse_args()

if not os.path.exists("images"):
  os.mkdir("images")

for file in args.txt_files:
  with open(file, "r") as inFile:
    line = inFile.readline()
    while(line[0] == '#'):
      line = inFile.readline()

    cache_sizes = []
    hits = []
    hit_rates = []
    while(line != ""):
      data = line.split()

      if data[0].rstrip() == "Misses":
        break

      cache_size = int(data[0].rstrip())
      nhits = int(data[1].rstrip())
      hit_rate = float(data[2].split('%')[0])

      cache_sizes.append(cache_size)
      hits.append(nhits)
      hit_rates.append(hit_rate)
      
      line = inFile.readline()

    plt.plot(cache_sizes, hit_rates, label=file.rsplit('.', 1)[0].replace("/", "_"))

  plt.xlabel("Cache Size")
  plt.ylabel("Cache Hit-rate(%)")
  plt.legend()
  plt.savefig("images/hit-rate_curves.png")
