Note.

(SAT) depth reached 27; 6879869 (4492726 queued) implied nodes; tree profile 0/1 0/1 0/1 0/2 0/8 0/32 0/112 0/112 0/280 0/252 0/322 0/540 611/930 355/355

(new logic) depth reached 28; 375779067 (282588130 queued) implied nodes; tree profile 0/1 0/1 0/1 0/2 0/8 0/32 0/122 0/112 0/336 0/336 0/504 0/554 0/1059 656/1142 1076/1076


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