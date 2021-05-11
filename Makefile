diskgraph: diskgraph.c
	cc -D_POSIX_C_SOURCE=199309L -std=c99 -Wall -g -o diskgraph diskgraph.c -lm

clean:
	rm -f ./diskgraph

