#include "context_switch.h"


/*  IPC storage:
 *	All inter process communication structures come with an
 *	ObjectNode, all of which are stored in ipc_table;
 *
 *	ipc_table functions as a hash table, though the hash
 *	function is simple (h(x) = x % ipc_table length)
 *
 *	This is appropriate, because although the hash is not
 *	random, which objects get removed over time is unknown
 *	and subject to user whims, and thus can be considered 
 *	pseudo-random.
 *	
 *	Hash table is dynamically resized based on load factor.
 *	Actual resizing parameters defined below, and can be
 *	easily changed, as the current definitions are solely
 * 	based on the authors' best guesses.
 *	
 *	After an ipc object has been reclaimed, all processes
 *	waiting on that object are put back in the ready queue
 *	If they try to interact with the object further,
 *	they will receive the return code ERROR.
 */

#define PIPE 0
#define LOCK 1
#define CVAR 2
#define SEMA 3
#define IPC_NUM 4 

#define SMALLER 0
#define LARGER 1
//when the load factor is less than 0.5
#define MIN_AVG_TABLE_DEPTH 0.5
//cut table in half, new load factor = 1 
#define SMALLER_RATIO 0.5
//when load factor is greater than 2.5
#define MAX_AVG_TABLE_DEPTH 2.5
//double table size, new load factor = 1.25
#define LARGER_RATIO 2
#define ORIGINAL_LENGTH 2

#define PIPE_MAX_LENGTH 10

#define BUSY 1
#define IDLE 0

typedef struct obj_node {
	int id;
	struct obj_node *next;
} ObjectNode;

typedef struct Pipe_Structure {
	Queue read_queue;
	Queue write_queue;
	char buf[PIPE_MAX_LENGTH];
	int start;
	int end;
	ObjectNode object_node;
} Pipe;

typedef struct Lock_Structure {
	int state;
	int holder_pid;
	Queue acquire_queue;
	ObjectNode object_node;
} Lock;

typedef struct Condition_Variable {
	Queue wait_queue;
	ObjectNode object_node;
} Cvar;

typedef struct Sema_Structure {
	int value;
	Queue down_queue;
	ObjectNode object_node;
} Semaphore;

/*** Inter Process Communication tables */
//Pipes, locks, and cvars each have an id
//that id should be an index into an array of pointers
//need function to manage array sizes

extern int HASH_LEN; //Current length of ipc_table
extern ObjectNode **ipc_table;
extern int ipc_count[];

extern ObjectNode *Find(int); //finds a pipe, lock or cvar just from its id
extern void GetNewID(int, ObjectNode *);//sets up a new pipe, lock, or cvar id
extern ObjectNode *RemoveID(int);


