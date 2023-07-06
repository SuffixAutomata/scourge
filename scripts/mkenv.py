import os
open("status.json","w").write("""{"time":0.0, "times": {}, "presence": {}}""")
os.mkdir("dumps")
os.mkdir("submits")
os.mkdir("tasks")
os.mkdir("to-aggregate")
os.mkdir("to-delete")