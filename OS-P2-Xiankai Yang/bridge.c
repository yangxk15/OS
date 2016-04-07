//Simulate the Ledyard Bridge Construction Project

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#define MAX_CARS 3
#define MAX_TIME 2
#define CARS 30 
#define PASS 3
#define TO_NORWICH 1
#define TO_HANOVER 0 
#define Direction(x) ((x) ? TO_NORWICH : TO_HANOVER)
#define DString(x) ((x) ? "NORWICH" : "HANOVER")

pthread_mutex_t lock =  PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cvar[2] = {PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER}; 

int current_cars;
int waiting_cars[2];
int wait_timestamp;
int current_direction;

char *indent = "===============";

typedef struct {
	int i;
	int direction;
} Car;

void *OneVehicle(void *arg) {

	Car *tmpCar = (Car *) arg;
	int i = tmpCar -> i;
	int direction = tmpCar -> direction;
	int rc, t;
	 
	//ArriveBridge(direction);
	
	rc = pthread_mutex_lock(&lock);
	if (rc) {
		printf("Car %d: Arrive failed!\n", i);
		exit(-1);
	}
	
	//printf("Car %d get the lock! Check conditions...\n", i);

	waiting_cars[direction]++;
	if (current_direction >= 0) {
		while ((current_cars != 0 && current_direction != direction) || current_cars == MAX_CARS) {
			//printf("Car %d is waiting for driving towards %s.\n", i, DString(direction));
			pthread_cond_wait(&cvar[direction], &lock);
		}
	}
	waiting_cars[direction]--;
	
	// now the car is on the bridge!
	if (current_direction != direction) {
		current_direction = direction;
		wait_timestamp = clock();
		printf("%s Now the bridge's current direction is  %s %s Timestamp : %d %s \n", indent, DString(direction), indent, wait_timestamp, indent);
	}

	current_cars++;

	pthread_mutex_unlock(&lock);
	if (rc) {
		printf("Car %d: release lock failed!\n", i);
		exit(-1);
	}

	//printf("Car %d is on the bridge now!\n", i);

	//OnBridge(direction);

	for (t = 1; t <= PASS; t++) {
		sleep(1);

		rc = pthread_mutex_lock(&lock);
		if (rc) {
			printf("Car %d: on bridge failed!", i);
			exit(-1);
		}

		printf("Car %d has been on the bridge for %d minutes.\tThe bridge has %d %s.\tCars waiting to NORWICH : %d\tCars waiting to HANOVER : %d\n", i, t, current_cars, current_cars <= 1 ? "car" : "cars", waiting_cars[TO_NORWICH], waiting_cars[TO_HANOVER]);
		
		rc = pthread_mutex_unlock(&lock);
		if (rc) {
			printf("Car %d: release lock failed!\n", i);
			exit(-1);
		}

	}

	//ExitBridge(direction);
	
	rc = pthread_mutex_lock(&lock);
	if (rc) {
		printf("Car %d: Get off failed!", i);
		exit(-1);
	}

	//printf("Car %d is getting off the bridge!\n", i);

	current_cars--;
	int waiting_time = (clock() - wait_timestamp) / 1000;
	if (waiting_cars[1 - direction] == 0 || waiting_time <= MAX_TIME) {
		//printf("%s %s %s waiting time is %d\n", indent, indent, indent, waiting_time);
		pthread_cond_signal(&cvar[direction]);
	}
	//else
	//	printf("Stop! You have been here too long!\n");
	if (current_cars == 0)
		pthread_cond_broadcast(&cvar[1 - direction]);	
	
	rc = pthread_mutex_unlock(&lock);
	if (rc) {
		printf("Car %d release lock failed!\n", i);
		exit(-1);
	}

	// now the car is off
	
	//printf("Car %d has got off the bridge.\n", i);

	return NULL;
}



int main(int argc, char *argv[]) {

	pthread_t threads[CARS];
	Car Cars[CARS];

	int i, rc;

	current_direction = -1;
	current_cars = 0;
	waiting_cars[TO_NORWICH] = 0;
	waiting_cars[TO_HANOVER] = 0;

	for (i = 0; i < CARS; i++) {

		Cars[i].i = i;
		Cars[i].direction = Direction(i % 2);

		rc = pthread_create(&threads[i], NULL, OneVehicle, (void *) &Cars[i]);

		if (rc) {
			printf("Create new thread failed!\n");
			exit(-1);
		}

	}
	
	for (i = 0; i < CARS; i++) {
		
		rc = pthread_join(threads[i], NULL);

	}
		
	return 0;
}
