import sys
import subprocess
import time

_, hostURL, contributorID, timeout, threads = sys.argv

timeout = int(timeout)
t0 = time.time()
while time.time() - t0 < timeout - 10:
  xto = int(timeout - (time.time() - t0) - 5)
  subprocess.run(["vriska", hostURL, contributorID, xto, threads])