local g = golly()

local x, y, w, h = table.unpack(g.getselrect())
if h ~= 1 and h ~= 2 then
  g.exit("ofind needs one or two rows to be specified...")
end

local period = tonumber(g.getstring("Enter the period of the osc to find."))

if h == 1 then
  local firstrows = {}
  for gen = 1,period do
    local firstrow = {}
    for i = x, x+w-1 do
      -- g.note(g.getcell(i,y))
      table.insert(firstrow, g.getcell(i, y)==1 and "o" or ".")
    end
    table.insert(firstrows, table.concat(firstrow))
    g.run(1)
  end
  g.setclipstr(table.concat(firstrows, "\n") .. "\n")
elseif h == 2 then
  local firstrows, secondrows = {}, {}
  for gen = 1,period do
    local firstrow, secondrow = {}, {}
    for i = x, x+w-1 do
      table.insert(firstrow, g.getcell(i, y)==1 and "o" or ".")
      table.insert(secondrow, g.getcell(i, y+1)==1 and "o" or ".")
    end
    table.insert(firstrows, table.concat(firstrow))
    table.insert(secondrows, table.concat(secondrow))
    g.run(1)
  end
  g.setclipstr(table.concat(firstrows, '\n') .. '\n' .. table.concat(secondrows, '\n') .. "\n")
end
