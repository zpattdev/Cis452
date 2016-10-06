#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h>
#include <wait.h>
#include <sys/types.h>
#include <math.h>
#include <time.h>

#define READ 0 
#define WRITE 1 
#define MAX 1024
#define BUFSIZE 2048
#define FALSE 0
#define TRUE 1
#define TESTFILE 8

struct record {
  int testVal;		//value to test compare
  char msg[BUFSIZE-5];	//message to test piping
};

pid_t forkCheck(void);
void pipeCheck (int fd[]);
int calcPathId (int path[], int len);
void wakeupHandler();
void noticeHandler();
void startMaster (pid_t leftChild, pid_t rightChild);
void startMerger (pid_t leftChild, pid_t rightChild, int level);
void startSorter (int pathId);

int readyChildren = 0, readyParent = TRUE;
int fdLeft[2], fdRight[2], fdParent[2];

int main(int argc, char* argv[]) 
{ 
  pid_t pid1, pid2; 
  //int fdLeft[2], fdRight[2], fdParent[2];
  int fileNum = TESTFILE, fileMax = TESTFILE;
  int pathLen = (int) log2((double) fileNum);
  int pathTracker[pathLen];
  
  while(1){   
    //sleep(1);
    //create pipe for Left child
    pipeCheck(fdLeft);
    //create left child
    pid1 = forkCheck();
    
    //if parent, need to create right child; else skip
    if(pid1 > 0) { //parent w/ 1 child
      //create pipe for right child
      pipeCheck(fdRight);
      //create right child
      pid2 = forkCheck();
      if (pid2 > 0) { //same parent w/ 2 
	signal(SIGUSR1, wakeupHandler);
	signal(SIGUSR2, noticeHandler);
	if (fileNum >= fileMax)
	  startMaster(pid1, pid2);
	else 
	  startMerger(pid1, pid2, fileNum);	
      } else { //Right child 
	pathTracker[(int) log2((double) fileNum) - 1] = 1;
	fdParent[WRITE] = fdRight[WRITE];
	signal(SIGUSR1, wakeupHandler);
      }
    } else { //Left child 
      pathTracker[(int) log2((double) fileNum) - 1] = 0;
      fdParent[WRITE] = fdLeft[WRITE];
      signal(SIGUSR1, wakeupHandler);
    }
    //applies to both children
    signal(SIGUSR1, wakeupHandler);
    fileNum /= 2;
    if(fileNum < 2)
      //printf("%d\n", fdParent[WRITE]
      startSorter(calcPathId (pathTracker, pathLen));
  }
  return 0;
}

pid_t forkCheck (void) {
  int pid;
  if ((pid = fork()) < 0){
    perror("fork failed");
    exit(1);
  }
  return pid;
}

void pipeCheck (int fd[]){
  if ((pipe (fd)) < 0) { 
    perror ("plumbing problem"); 
    exit(1); 
  }
}

int calcPathId (int path[], int len) {
  int id = 0, m = 1, i;
  for(i = 0; i < len; i ++) {
    id += path[i] * m;
    m *= 2;
  }
  return id + 1;
}

void wakeupHandler(int sigNum) {
  readyParent = TRUE;
}

void noticeHandler(int sigNum) {
  readyChildren++;
  
}

void startMaster (pid_t leftChild, pid_t rightChild) {
  //printf("Master Ready\n");
  //char testStr[BUFSIZE] = "faq";
  struct record leftRec, rightRec;
  int leftEmpty = TRUE, rightEmpty = TRUE, first = TRUE;
  
  while (1) {
    //sleep(1);
    //if previous read was not Right child
    //printf("Master has %d children ready\n", readyChildren);
    if (readyChildren >= 2 && readyParent) {
      if (leftEmpty && !first){
	printf("Master reads left\n");
	read(fdLeft[READ], (void *) &leftRec, BUFSIZE);
	printf("---Master gets: %s\n", leftRec.msg);
      }
      //if previous read was not Left child
      if (rightEmpty && !first) {
	printf("Master reads right\n");
	read(fdRight[READ], (void *) &rightRec, BUFSIZE); 
	printf("---Master gets: %s\n", rightRec.msg);
      }
      readyChildren = 1;
      if(leftEmpty) {
	kill(leftChild,SIGUSR1);
	printf("signal to left Merger\n");
      }
      if(rightEmpty) {
	kill(rightChild,SIGUSR1);
	printf("signal to right Merger\n");
      }
    }
    first = FALSE;
    pause();
  }
}

void startMerger (pid_t leftChild, pid_t rightChild, int level) {
  //printf ("Merger Ready\n");
  //char testStrL[BUFSIZE] = "faqL", testStrR[BUFSIZE] = "faqR";
  struct record leftRec, rightRec, lowerRec;
  int leftEmpty = TRUE, rightEmpty = TRUE, first = TRUE;
  
  while(1) {
    if (readyChildren >= 2 && readyParent) {
      if (leftEmpty && !first){
	//printf("start left read\n");
	read(fdLeft[READ], (void *) &leftRec, BUFSIZE);
	leftEmpty = FALSE;
	printf("---Merger %d gets: %s\n", level, leftRec.msg);
      }
      if (rightEmpty && !first) {
	//printf("start right read\n");
	read(fdRight[READ], (void *) &rightRec, BUFSIZE); 
	printf("---Merger %d gets: %s\n", level, rightRec.msg);
	rightEmpty = FALSE;
      }
      
      if (leftRec.testVal < rightRec.testVal) {
	lowerRec = leftRec;
	leftEmpty = TRUE;
      } else {
	lowerRec = rightRec;
	rightEmpty = TRUE;
      }

      printf("Merger sends %s\n", lowerRec.msg);
      write(fdParent[WRITE], (void *) &lowerRec, BUFSIZE);
      kill(getppid(),SIGUSR2);
      readyParent = FALSE;

      
      readyChildren = 1;
      if(leftEmpty) kill(leftChild,SIGUSR1);
      if(rightEmpty) kill(rightChild,SIGUSR1);
    }
    first = FALSE;
    pause();
    //sleep(1);
  }
}

void startSorter (int pathId) {
  //printf("Sorter %d Ready\n",pathId);
  int count = 0;
  struct record writeRec;
    srand(pathId);
  //char testStr[BUFSIZE];
  
  while (count < 5) {
    int r = rand() % 10;
    writeRec.testVal = r;
    sprintf(writeRec.msg, "Sorter %d sends %d", pathId, r);
    //printf("Sending: %s\n", testStr);
    write(fdParent[WRITE], (void *) &writeRec, BUFSIZE);
    kill(getppid(),SIGUSR2);
    //printf("Sorter %d waiting\n", pathId);
    pause();
    //printf("Sorter %d received signal\n", pathId);
    
    count++;
    //sleep(1);
  }
  printf("limit reached\n");
  pause();
}








