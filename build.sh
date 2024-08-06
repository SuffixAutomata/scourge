#!/bin/bash
cd "$(dirname "${BASH_SOURCE[0]}")"
g++ -o terezi_core terezi_core.cpp cadical/build/libcadical.a -O3 -lpthread -Wall -Wextra -pedantic -std=c++23 -static
# g++ -o vriska vriska.cpp cadical/build/libcadical.a -O3 -Wall -Wextra -pedantic -std=c++23 -static-libstdc++ -static-libgcc
# g++ -o vriska2 vriska.cpp cadical/build/libcadical.a -O3 -Wall -Wextra -pedantic -std=c++23 -static

g++ -o vriska vriska.cpp cadical/build/libcadical.a -O3 -Wall -Wextra -pedantic -std=c++23 -static