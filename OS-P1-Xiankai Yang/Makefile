.SUFFIXES: .c

SRCS = album.c
OBJS = $(SRCS:.c=.o)
OUTPUT = album 

CC = gcc
CFLAGS = 
LIBS = 

compile: $(OBJS)
	$(CC) $(CFLAGS) -o $(OUTPUT) $(OBJS) $(LIBS)

clean:
	rm -f $(OBJS) $(OUTPUT) index.html *.jpg

depend:
	makedepend -I/usr/local/include/g++ -- $(CFLAGS) -- $(SRCS) 

# DO NOT DELETE


