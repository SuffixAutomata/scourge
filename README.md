Note. 

W23: 0 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 2 3 15 19 18 47 66 141 170 222 302 279 2088 6352 11607 18325 22271 31622 1/30769 4/35058 75/45656 713/62948 1356/66727 4349/85584 13184/90538 14126/70748 14117/72703 21682/79002 35880/88260 33395/60335 17498/25686 5250/7104 1345/1600 202/236    38/41      5/5
W21: 0 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 2 3 15 18 17 33 45  91  96 133 187 170  465  647   767   874   830  1086    1053     938     1044      1295       1210       1414        1532        1351        1551        1982        3017        3535        3470    3/3013   50/2615 67/2917 260/4128 462/5113 762/6011 880/5504 988/4254 743/2833 893/2718 783/2167 791/1573 394/716 159/268 52/90 20/32 10/12 1/1
W22: 0 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 2 3 15 19 18 43 57 120 139 186 249 231 1245 2812  4113  5279 741/5499 791/5798 751/4666 1478/3995 833/2977 1453/3084 902/1524 
TODO 

 - [x] Only dump every 10 minutes
   - [ ] Later: Split dump into folder (maybe one file for several nodes, or one new file per dump)
     - [ ] EVEN LATER: Option to merge a folder
 - [ ] Delete useless nodes of search tree
 - [ ] Distributify
   - [ ] One node per workunit dumps, tag with `TAG_REMOTE_QUEUED`
   - [ ] Merging saves with `TAG_INDETERMINATE_NODEID` into `TAG_REMOTE_QUEUED` nodes
   - [ ] Explore multiple nodes deep into a subtree; use `TAG_INDETERMINATE_NODEID` on returned nodes
   - [ ] Contributor label for each big tree node
 - [ ] (Far future) Node-wise bisection index adjustment/floating
 
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