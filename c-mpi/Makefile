# Add gol-par-bonus1 and gol-par-bonus2 when available
all: gol-seq gol-par

gol-seq: gol-seq.c
	gcc -Wall -O3 -o gol-seq gol-seq.c -lm

# assumption is that the MPI module has been preloaded in the environment
gol-par: gol-par.c
	mpicc -Wall -O3 -o gol-par gol-par.c -lm

gol-par-bonus1: gol-par-bonus1.c
	mpicc -Wall -O3 -o gol-par-bonus1 gol-par-bonus1.c -lm

gol-par-bonus2: gol-par-bonus2.c
	mpicc -Wall -O3 -o gol-par-bonus2 gol-par-bonus2.c -lm

clean:
	rm -f *.o gol-seq gol-par gol-par-bonus1 gol-par-bonus2
