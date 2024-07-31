CC = g++
CFLAGS = -O3 -lpthread -Wall -Wextra -pedantic
# LDFLAGS = -O3 -lpthread -Wall -Wextra -pedantic -std=c++23 -lssl -lcrypto
LDFLAGS = -O3 -lpthread -Wall -Wextra -pedantic -std=c++23

all: terezi vriska

mongoose.o: mongoose.c
	# $(CC) -c $< $(CFLAGS) -DMG_TLS=MG_TLS_OPENSSL
	$(CC) -c $< $(CFLAGS)

terezi: terezi.cpp mongoose.o
	$(CC) -o $@ $^ $(LDFLAGS)

vriska: vriska.cpp cadical/build/libcadical.a mongoose.o
	$(CC) -o $@ $^ $(LDFLAGS) -static

vriska_core: vriska_core.cpp cadical/build/libcadical.a satlogic.h
	$(CC) -o $@ vriska_core.cpp cadical/build/libcadical.a $(LDFLAGS) -static

clean:
	rm -f terezi vriska mongoose.o
