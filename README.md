# scourge

![](scripts/test.gif)

For standard operation, you only need to compile `terezi`:

```
cd cadical
./configure && make
cd ..
g++ -o terezi terezi.cpp cadical/build/libcadical.a -O3 -std=c++23
```

Then, `./terezi search` will lead you through the rest.

## Running (simple version)

On the first prompt, enter the number of threads to use, the number of nodes to search to before stopping, and the lookahead (~49 for p7, but make larger if there's too many nodes and smaller if the search is too slow) separated by a space. 

On the second prompt, enter the period, width, symmetry, and stator width. The stator width is **deducted** from the total width **on both sides**.

On the third prompt, follow the given instructions, or adjust as desired.

After that, enter the 2*p rows, in ofind format, to begin the search with.

For the symmetry parameter, 0 = none, 1 = vertical.

Here's an example input, for discovering 754P7. (Not tested.)

```
4 -1 49
7 24 1 2
6
........................
.........o....o.........
........oo....oo........
........oo....oo........
.......oooooooooo.......
........oooooooo........
........................
..........oooo..........
.........oo..oo.........
........o.o..o.o........
.......oo.oooo.oo.......
........................
.......oooooooooo.......
.......o........o.......
```