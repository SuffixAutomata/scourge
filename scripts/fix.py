import sys
import os
import json
import time
import pathlib

cwd = str(pathlib.Path().resolve())
sep = 20
stat = json.load(open("status.json"))
queued = set()
cnt = 0
sys.stdin = open("dump.txt")
input()
n = int(input())
for i in range(n):
  x = input()
  if x.endswith("q"):
    queued.add(i)
s = sorted(queued)[:240]
id = {}
for i in s:
  if stat["presence"][str(i)] not in id:
    id[stat["presence"][str(i)]] = []
  id[stat["presence"][str(i)]] += [i]

fn = f"submits/job-fix-{int(time.time())}.submit"
f = open(fn, "w")
ostdout = sys.stdout
sys.stdout = f

print(f"""executable = /home/dandan/.local/bin/vriska
request_cpus = 1
request_memory = 1GB
request_disk = 128MB
on_exit_hold = (ExitBySignal == True) || (ExitCode != 0)
periodic_release = NumJobStarts < 10
error = err
output = out
log = log
""")
print()

for idd in id:
  print(f"transfer_input_files = {cwd}/tasks/tasks{idd}.txt")
  wus = id[idd]
  for idx in range(0,len(wus),sep):
    sub = wus[idx:idx+sep]
    a, b = max(sub), min(sub)
    for i in sub:
      stat["times"][i] = int(time.time())
      stat["presence"][i] = idd
    print(f"arguments = tasks{idd}.txt tmp{i}.out {b} {a}")
    print(f'transfer_output_files = tmp{i}.out')
    print(f'transfer_output_remaps = "tmp{i}.out={cwd}/to-aggregate/{idd}-{a}.txt"')
    print("queue 1")
    cnt += 1

f.close()
sys.stdout = ostdout
print(f"QU3U3D {cnt} JOBS TO {fn}")
json.dump(stat, open(f"status.json", "w"))