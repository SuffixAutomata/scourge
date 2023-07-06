import os
import subprocess
import sys
import json
import time
import pathlib

cwd = str(pathlib.Path().resolve())
stat = json.load(open("status.json"))


ostdout = sys.stdout
idd = int(sys.argv[1])
maxq = 10000
sep = 20

wus = [int(i.split()[0]) for i in open(f"{cwd}/tasks/tasks{idd}.txt")][1:]
wus = [i for i in wus if str(i) not in stat["times"]]

while len(wus):
  fn = f"submits/job-{int(time.time())}.submit"
  f = open(fn, "w")
  sys.stdout = f
  print(f"""executable = /home/dandan/.local/bin/vriska
  request_cpus = 1
  request_memory = 1GB
  request_disk = 1GB
  on_exit_hold = (ExitBySignal == True) || (ExitCode != 0)
  periodic_release = NumJobStarts < 10
  transfer_input_files = {cwd}/tasks/tasks{idd}.txt
  error = err
  output = out
  log = log
  """)
  print()
  cnt = 0
  for idx in range(0,len(wus),sep):
    sub = wus[idx:idx+sep]
    a, b = max(sub), min(sub)
    for i in sub:
      stat["times"][str(i)] = int(time.time())
      stat["presence"][str(i)] = idd
    print(f"arguments = tasks{idd}.txt tmp{i}.out {b} {a}")
    print(f'transfer_output_files = tmp{i}.out')
    print(f'transfer_output_remaps = "tmp{i}.out={cwd}/to-aggregate/{idd}-{a}.txt"')
    print("queue 1")
    cnt += 1
    if cnt == maxq:
      break
  f.close()
  sys.stdout = ostdout
  print(f"QU3U3D {cnt} JOBS TO {fn}")
  # os.system(f"condor_submit {fn}")
  wus = [i for i in wus if str(i) not in stat["times"]]
  time.sleep(1)

json.dump(stat, open(f"status.json", "w"))
