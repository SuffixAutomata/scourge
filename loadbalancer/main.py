host = "."

import requests
import blacksheep

app = blacksheep.Application()

@blacksheep.get("/getconfig")
async def getconfig():
  return blacksheep.text("...")

@blacksheep.post("/getwork")
async def getwork(text: blacksheep.FromText):
  pass

@blacksheep.post("/returnwork")
async def returnwork(text: blacksheep.FromText):
  pass