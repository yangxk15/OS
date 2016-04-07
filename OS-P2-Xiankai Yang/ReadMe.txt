#Xiankai Yang
#10-02-2015

Three files are attached: bridge.c(source file), Makefile(make), ReadMe.

Using the command “make” to compile source file and generate executive file: bridge.

Using the command “./bridge” to run the program.

This program uses one mutex lock and two condition variables.

Parameters explanation:

MAX_CARS: The capacity of the bridge. There can be at most MAX_CARS cars on the bridge at the same time. (Default value 3)

MAX_TIME: (Graduate Student Requirement) In order to prevent the starvation situation, we set the maximum time of waiting as MAX_TIME. When there exists at least one car waiting because of the opposite current direction, we use clock() to make a timestamp to calculate the waiting time, and if the waiting time is greater than MAX_TIME with at least one car waiting, every current car on the bridge will no longer send signals to the cars on their side. As all the cars on the bridge get off the bridge, cars waiting for the other direction will get the signal and start to go. (Default value 2) (MAX_TIME can be set lower to observe frequent direction switching)

CARS: The number of cars being tested. They will be divided into two list representing cars from two directions. It is also the number of the threads that are about to be created. (Default value 30)

PASS: The time on the bridge. Set for simulation and observation. (Default value 3)

You can erase the comment “//“ to use “printf” to observe more details in the program.

Reference: Lecture Notes(including sample code), Linux man page.