build:
	mpicc tema4.c -o tema4
run:
	mpiexec -np 12 ./tema4 fisier_topologie.txt fisier_mesaje.txt
clean:
	rm -Rf *.o *~ tema4 
