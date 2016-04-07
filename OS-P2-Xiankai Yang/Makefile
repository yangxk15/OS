.SUFFIXES: .c

SRCS = bridge.c
OUTPUT = bridge 

CC = gcc
CFLAGS = -m32 
LIBS = -lpthread 

demo: 
	$(CC) $(CFLAGS) -o $(OUTPUT) $(SRCS) $(LIBS)

clean:
	rm -f $(OUTPUT)

depend:
	makedepend -I/usr/local/include/g++ -- $(CFLAGS) -- $(SRCS) 

# DO NOT DELETE
