CFLAGS=-std=c11 -Ofast -march=native -g
SDLFLAGS=-ISDL-1.2.15/include -D_GNU_SOURCE=1 -D_REENTRANT -LSDL-1.2.15/build/.libs -Wl,-rpath,SDL-1.2.15/build/.libs -lSDL -lpthread
SRC_DIR=${PWD}
SDL_DIR=${SRC_DIR}/SDL-1.2.15


all: SDL npx

.PHONY: all SDL clean test distclean

SDL:
	cd $(SDL_DIR); if not test -f Makefile; then sh -c ./configure; fi; $(MAKE) all

npx: nanopond-2.0.c
	gcc --verbose 									\
		-Wall									\
		${CFLAGS} nanopond-2.0.c -o npx				\
		${SDLFLAGS}

clean:
	rm -f ./npx

distclean:
	rm -f ./npx
	$(MAKE) -C $(SDL_DIR) distclean

test:  npx
	/usr/bin/time ./npx


