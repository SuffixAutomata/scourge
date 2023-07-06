import os
os.system("python3 ../scripts/aggregator.py")
s = os.listdir("tasks")
os.system("echo 49 | terezi distrib")
s2 = os.listdir("tasks")
assert f"tasks{len(s)}.txt" not in s and f"tasks{len(s)}.txt" in s2
os.system(f"python3 ../scripts/newwork.py {len(s)}")
