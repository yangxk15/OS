/*
 *  }:->
 */

#include <hardware.h>
#include <yalnix.h>

#include <stdlib.h>
#include <assert.h>

//Builds a pipe, forks a child and gives that child garbage
void Piper(void)
{
	TtyPrintf(0, "Piper piping!\n");
	int i = 0;
	int num_children = 20;
	int len = 40;
	char ch;
	int child_no;
	int pipe_array[num_children];
	char *buf = malloc(len);
	while(1)
	{
		if (i % 10 == 0 && i / 10 < 20) {
			child_no = i / 10;
			TtyPrintf(0, "New child and pipe!\n");
			PipeInit(&pipe_array[child_no]);
			int pid = Fork();
			while (0 == pid) {
				PipeRead(pipe_array[child_no],(void *) buf, len);
				TtyPrintf(0, "Piper child #%d: ", child_no);
				TtyPrintf(0, buf);
			}
		}
		int j;
		for (j = 0; j < len - 2; j++) {
			ch = 'a' + rand() % 26;
			buf[j] = ch;
		}	
		buf[len - 2] = '\n';
		buf[len - 1] = 0;
		for (j = 0; j <= child_no; j++) {
			TtyPrintf(0, "Piper writing to child #%d\n", j);
			PipeWrite(pipe_array[j],(void *) buf, len);
		}
		i++;
		//Delay(5);
	}
}

void ForceIPCResize(void)
{
	int pid;
	int child_num = 0;
	int total_children = 20;
	int children_left = total_children;
	int Pipes[total_children];
	char *buf = malloc(sizeof(char));
	*buf = 'a';
	int len = 1;
	TtyPrintf(2, "ForceIPCResize\n");
	while(1)
	{
		if (child_num < total_children) {
			PipeInit(&Pipes[child_num]);	
			pid = Fork();
			if (0 == pid) {
				TtyPrintf(2, "Pipe for child #%d\n", child_num);
				PipeRead(Pipes[child_num], (void *) buf, len);
				TtyPrintf(2, "Child #%d has read from pipe, now to die!\n", child_num);
				Exit(1);
			}
			TtyPrintf(2, "Parent:Pipes[child_num] = %d\n", Pipes[child_num]);
			child_num++;
		} else if (children_left > 0) {
			TtyPrintf(2, "ForceResize, now to start killing children\n");
			PipeWrite(Pipes[children_left - 1], (void *) buf, len);
			int status;
			Wait(&status);
			Reclaim(Pipes[children_left -1]);
			children_left--;
			TtyPrintf(2, "ForceResize only %d children left\n", children_left);
		} else {
			TtyPrintf(2, "ForceResize starts again\n");
			child_num = 0;
			children_left = total_children;
		}
		Delay(1);
	}
}


void Semaphorist(void)
{
 	TtyPrintf(2, "Semaphorist semaphoring!\n");
	int num_children = 20;
	int child_no, SemaID;
	int i = 0;
	int flags = 0;
	SemInit(&SemaID, flags);
	while(1)
	{
		if (i % 10 == 0 && i / 10 < num_children) {
			child_no = i / 10;
			TtyPrintf(2, "New child\n");
			int pid = Fork();
			while (0 == pid) {
				SemDown(SemaID);
				TtyPrintf(2, "Child #%d picked up a flag!\n", child_no);
				SemUp(SemaID);
				TtyPrintf(2, "Child #%d put down a flag!\n", child_no);
				Delay(rand() % 10);
			}
		}
		if (rand() % 2) {
			TtyPrintf(2, "Semaphorist giving out a flag\n");
			SemUp(SemaID);
			flags++;
		}
		if (rand() %2 && flags > 0)  {
			TtyPrintf(2, "Semaphorist taking away a flag\n");
			SemDown(SemaID);
			flags--;
		}
		i++;
		Delay(5);
	}
}


// Waits for a cvar signal, prints a message
void Bouncer(int cvar, int mutex)
{
    TtyPrintf(1, "Bouncer here\n");
    while (1)
    {
	TtyPrintf(1, "Waiting for ping...");
	Acquire(mutex);
	CvarWait(cvar, mutex);
	Release(mutex);
	TtyPrintf(1, "boing!\n");
    }
    TtyPrintf(1, "*** SHOULDN'T BE HERE!!!\n");
    Exit(-1);
}



// Keeps breaking in
void ThatAnnoyingGuy(void)
{
    while (1)
    {
	TtyPrintf(1, "Hey!  Hey!  You remember that time when...\n");
	Delay(1);
	TtyPrintf(1, "Hi!  I'm that annoying guy that all the other processes hate!\n");
	Delay(1);
	TtyPrintf(1, "Hehehehehehehehehehe!\n");
	Delay(1);
	TtyPrintf(1, "Why did the chicken cross the road?  Ha ha...\n");
	Delay(1);
	TtyPrintf(1, "42!!!\n");
	Delay(1);
	TtyPrintf(1, "Get it?  42?  Get it?  Huh?  Hey, you're not listening...\n");
	Delay(1);
	TtyPrintf(1, "You know what would be a really scary Halloween costume?\n");
	Delay(1);
	TtyPrintf(1, "A null pointer!  Ha ha ha.\n");
	Delay(1);
	TtyPrintf(1, "My while loop is over, so I'm goint to repeat all my lame jokes now.\n");
	Delay(1);
    }
    TtyPrintf(1, "*** I SHOULDN'T BE HERE!!!\n");
    Exit(-1);
}



void MallocLeak(void)
{
    int npg;
    void *ptr;

    while (1)
    {
	npg = rand() % 21;
	TtyPrintf(2, "MallocLeak: malloc'ing %d pages\n", npg);
	ptr = malloc(PAGESIZE*npg);
	if (NULL == ptr) {
		TtyPrintf(2, "MallocLeak: malloc error!\n");
	}
	Delay(3);	
    }
    TtyPrintf(1, "*** I SHOULDN'T BE HERE!!!\n");
    Exit(-1);
}

void MallocMan(void)
{
    int npg;
    void *ptr;

    while (1)
    {
	npg = rand() % 21;
	TtyPrintf(2, "MallocMan: malloc'ing %d pages\n", npg);
	ptr = malloc(PAGESIZE*npg);
	if (NULL == ptr) {
		TtyPrintf(2, "MallocMan: malloc error!\n");
	}
	Delay(3);	
	TtyPrintf(2, "MallocMan: freeing the stuff I just malloc'ed\n");
	free(ptr);
    }
    TtyPrintf(1, "*** I SHOULDN'T BE HERE!!!\n");
    Exit(-1);
}

void SonarGuy(int cvar, int mutex)
{
    TtyPrintf(0, "SonarGuy here (my job is to signal Bouncer)\n");
    while (1)
    {
	TtyPrintf(0, "Signaling Bouncer...");
	Delay(2);
	Acquire(mutex);
	CvarSignal(cvar);
	Release(mutex);
	TtyPrintf(0, "ping!\n");
    }
    TtyPrintf(1, "*** I SHOULDN'T BE HERE!!!\n");
    Exit(-1);
}



void GarbageMan(void)
{
    int i, j, sentLen, wordLen;
    char punc[] = ".!?;:";
    char ch;

    TtyPrintf(3, "Hi, I'm the garbage man!\n");
    Delay(6);
    TtyPrintf(3, "All I do is spew garbage!  Watch!\n");
    while (1)
    {
	sentLen = rand() % 8;
	Delay(rand() % 7);
	for (i = 0; i < sentLen; i++)
	{
	    wordLen = rand() % 13;
	    for (j = 0; j < wordLen; j++)
	    {
		ch = 'a' + rand() % 26;
		TtyWrite(3, &ch, 1);
	    }
	    if (i < (sentLen - 1)) {
		ch = ' ';
		TtyWrite(3, &ch, 1);
	    }
	}
	TtyWrite(3, &punc[rand() % 5], 1);
	ch = ' ';
	TtyWrite(3, &ch, 1);
    }
    TtyPrintf(1, "*** I SHOULDN'T BE HERE!!!\n");
    Exit(-1);
}
		



int main(void)
{
    int pid;
    int r, cvar, mutex;

    char buf[100];

    r = CvarInit(&cvar);
    assert(r != ERROR);
    r = LockInit(&mutex);
    assert(r != ERROR);

    TtyPrintf(0, "Enter 'go' to go:\n");
    do {
	TtyRead(0, buf, 100);
    } while (buf[0] != 'g' || buf[1] != 'o');

    pid = Fork();
    if (pid == 0)
	Bouncer(cvar, mutex);
    
    pid = Fork();
    if (pid == 0)
	ThatAnnoyingGuy();

    pid = Fork();
    if (pid == 0)
	SonarGuy(cvar, mutex);
  
    pid = Fork();
    if (pid == 0)
	MallocLeak();

    pid = Fork();
    if (pid == 0)
	MallocMan();

    pid = Fork();
    if (pid == 0)
	GarbageMan();

    pid = Fork();
    if (pid == 0)
	Piper();

    pid = Fork();
    if (pid == 0)
	Semaphorist();

    pid = Fork();
    if (pid == 0)
	ForceIPCResize();


    while(1)
    {
	Delay(100);
	TtyPrintf(0, "Parent woke up; going back to sleep\n");
	//Reclaim(mutex);
    }

    TtyPrintf(0, "*** I SHOULDN'T BE HERE!!!\n");
    return -1;
}
