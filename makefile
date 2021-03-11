# primary make rule
all: peer ds
	
# peer make rule
peer: peer.o
	gcc -Wall peer.o -o peer
	
# discovery server make rule
ds: ds.o
	gcc -Wall ds.o -o ds
	
# file cleanup
clean:
	rm *o client server