#2015-09-24
#Xiankai Yang

To run this program, make sure that album.c and Makefile are in the same directory, which also contains a folder called "photos" that contains the all the photos user is going to peruse.

First, type "make" to compile the source code, then album.o and album will be generated.

Then, type "./album photos/*.jpg" to run the program. Choose whether or not rotate every image displayed in the thumbnail version, then type in a caption for every image. Thumbnails and Medium size versions will be generated in the same directory. A album index html called "index.html" will also be generated in the same directory. A sample html result can be visited by "www.cs.dartmouth.edu/~xyang/project1/", which is the result I ran the program on tahoe.

Also, type "make clean" to clean all the generated files: album, album.o, index.html, and all the other thumbnails and medium size photos. Clean them before testing the program again.

A pdf with the life cycle of processes (one cycle) is attached. The code is based on the process gragh in the pdf.
