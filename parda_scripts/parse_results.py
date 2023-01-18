
import sys

if len(sys.argv) < 2:
  raise ValueError("Incorrect number of Arguments!")

with open("tmp_latency.csv", "w") as latency_out:
  with open("tmp_memory.csv", "w") as memory_out:
    for input_file in sys.argv[1:]:
      with open(input_file, "r") as in_file:
        in_file.seek(0, 2) # seek end of file
        eof = in_file.tell()
        in_file.seek(max(eof - 32000, 0), 0) # seek to 32KB before end

        lines = []
        line = in_file.readline()
        while line != "":
          lines.append(line)
          line = in_file.readline()

        idx = len(lines) - 1
        memory = 0
        while lines[idx][:3] == "Max":
          memory += int(lines[idx].split('=')[1])
          idx = idx - 1

        latency = float(lines[idx - 2].split('s')[-1])
        latency_out.write(", " + str(round(latency, 2)))
        memory_out.write(", " + str(memory))

    latency_out.write("\n")
    memory_out.write("\n")
