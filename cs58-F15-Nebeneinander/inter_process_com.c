#include "inter_process_com.h"

//length of hash table, could be resized later
int HASH_LEN = IPC_NUM * ORIGINAL_LENGTH;

ObjectNode **ipc_table = NULL;
//the following two counts may seem redundant
//but ipc_count is for active and dead
int ipc_count[IPC_NUM] = {0}; 
int total_active_ipc = 0;	

//hash function
int h(x) {
	return x % HASH_LEN;
}


ObjectNode *Find(int id) {
	ObjectNode *index =  ipc_table[h(id)];
	while (NULL != index) {
		if (index->id == id) {
			return index;
		}
		index = index->next;
	}
	return index;
}

void GetNewID(int type, ObjectNode *object_node) {
	char *indent = "GetNewID:";
	TracePrintf(0, "%s Start!\n", indent);

	if (++total_active_ipc > 
	    MAX_AVG_TABLE_DEPTH * HASH_LEN) {
		ResizeIPCTable(LARGER);
	} 

	object_node->id = IPC_NUM * ipc_count[type] + type;
	ipc_count[type]++;

	object_node->next = ipc_table[h(object_node->id)];
	ipc_table[h(object_node->id)] = object_node;

	TracePrintf(0, "%s Finish!\n", indent);
}

ObjectNode *RemoveID(int id) {
	int hash_value = h(id);
	ObjectNode *toReturn;
	if (NULL == ipc_table[hash_value] || ipc_table[hash_value]->id == id) {
		toReturn = ipc_table[hash_value];
		if (NULL != toReturn) {
			ipc_table[hash_value] = ipc_table[hash_value]->next;
		}
	} else {
		ObjectNode *prev = ipc_table[hash_value];
		ObjectNode *curr = prev->next;
		while (NULL != curr) {
			if (curr->id == id) {
				prev->next = curr->next;
				break;
			}
			prev = prev->next;
			curr = curr->next;
		}
		toReturn = curr;
	}
	//resize if load factor is too small
	if (--total_active_ipc < 
			MIN_AVG_TABLE_DEPTH * HASH_LEN) {
		ResizeIPCTable(SMALLER);
	}
	
	return toReturn;
}

int ResizeIPCTable(int SmallerOrLarger){
	char *indent = "ResizeIPCTable:";
	TracePrintf(0, "%s Start!\n", indent);
	int oldTableSize = HASH_LEN;
	if (SMALLER == SmallerOrLarger) {
		if (HASH_LEN <= IPC_NUM * ORIGINAL_LENGTH) {
		// don't make smaller than ORIGINAL_LENGTH per ipc type
			TracePrintf(3, "%s Don't resize, table small enough\n",
					indent);
			TracePrintf(0, "%s Finish\n", indent);
			return SUCCESS;
		}
		TracePrintf(3, "%s Make smaller\n", indent);
		HASH_LEN = SMALLER_RATIO * HASH_LEN;
	} else {
		TracePrintf(3, "%s Make larger\n", indent);
		HASH_LEN = LARGER_RATIO * HASH_LEN;	
	}
	ObjectNode **oldTable = ipc_table;
	ipc_table = (ObjectNode **) malloc(sizeof(ObjectNode *) * HASH_LEN);
	if (NULL == ipc_table) {
		ipc_table = oldTable;
		HASH_LEN = oldTableSize;
		TracePrintf(0, "%s malloc error, no resize\n", indent);
		return FAILURE;
	}
	memset(ipc_table, 0, sizeof(ObjectNode *) * HASH_LEN);
	u_int i;
	int hash_value;
	ObjectNode *curObject;
	ObjectNode *nextObject;
	for (i = 0; i < oldTableSize; i++) {
		curObject = oldTable[i];
		while (NULL != curObject) {
			hash_value = h(curObject->id);
			nextObject = curObject->next;
			curObject->next = ipc_table[hash_value];
			ipc_table[hash_value] = curObject;
			curObject = nextObject;
		}
	}
	free(oldTable);
	TracePrintf(0, "%s Finish!\n", indent);
}
