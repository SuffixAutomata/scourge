Note. 

W21: 0 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 2 3 15 18 17 33 45  91  96 133 187 170  465  647   767   874   830  1086  1053   938  1044  1295  1210  1414  1532  1351   1551  1982  3017  3535  3470  3013  2618  2964  4305  5653  7217  7650  7062  6009   7707   9851  11423  12235  10950   9796   8144   8624   9593  11028  11610  10744   9419   7683   8710  10168  10391   9066   7358   5746   4391   4248   4206   3760   3333   2971   2368  1822  2102  2457  2373  2147  2245  1750  1285  1073  1009   860   664   551   454   335   276   266   225  177  128   82   59   45   50   47   34   26   22    5    5    5    4    2    1    1
W22: 0 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 2 3 15 19 18 43 57 120 139 186 249 231 1245 2812  4113  5279  5499  6821  6393  6412  7482  9439  9171 11191 12233 11077  14270 19524 31735 38083 37005 32586 28250 32771 48934 65872 84037 93066 89986 82208 112726 152434 188504 209716 199786 185308 161443 188363 239094 290603 315755 313256 274410 230939 280346 341359 384161 368472 326811 280227 217701 189193 186599 172108 147806 127255 100286 75307 67605 73342 74208 71383 68367 52778 39710 33523 34012 29355 24275 20039 16123 12199 10225 11135 10126 8565 6793 5281 4136 3401 3546 3497 3124 2598 2048 1643 1370 1621 1861 1675 1642 1289 968 853 872 775 594 495 428 298 206 227 180 128 107 90 73 48 43 32 20 12 8 4 3 3 1 1
W23: 0 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 2 3 15 19 18 47 66 141 170 222 302 279 2088 6352 11607 18325 22271 31622 30769 35059 45661 63060 67665 88774 99650 92376 120764 113/175451 776/304595 6441/382499 10758/376749 21466/328081 43288/270715 61344/266389 64770/304593 85929/319089 107746/287947 92787/188167 53146/87653 21570/29527 6760/8692 1717/2068 299/354 52/53
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