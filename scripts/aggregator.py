import sys
import os
import json
f = open("pending.txt", "a")
stat = json.load(open("status.json"))
ok = []
for i in os.listdir("to-aggregate"):
  fn = "to-aggregate/"+i
  s = [l.strip() for l in open(fn)]
  if len(s) and s[-1].endswith("ns"):
    ok += [i]
queued = set()
sys.stdin = open("dump.txt")
input()
n = int(input())
for i in range(n):
  x = input()
  if x.endswith("q"):
    queued.add(i)

for i in ok:
  fn = "to-aggregate/"+i
  s = [l.strip() for l in open(fn)]
  assert s[-1].endswith("ns")
  for x in s[:-1]:
    if int(x.split()[0]) not in queued:
      continue
    if x.split()[0] not in stat["times"]:
      print("R3COV3R3D",x)
      # continue
    else:
      del stat["times"][x.split()[0]]
    print(x, file=f)
  os.system(f"mv {os.path.abspath(fn)} to-delete")
  stat["time"] += (int(s[-1][:-2]))/1e9
f.close()
json.dump(stat, open("status.json", "w"))