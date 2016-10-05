#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h>
#include <wait.h>
#include <sys/types.h>
#include <math.h>

#define READ 0 
#define WRITE 1 
#define MAX 1024
#define BUFSIZE 2048

struct record {
  int RWflag;		//flag whether record is ready for read or write
  char msg[100];	//message to test piping
};


pid_t forkCheck(void);
void pipeCheck (int fd[]);
int calcPathId (int path[], int len);
void startMaster (int readLeft, int readRight);
void startMerger (int readLeft, int readRight, int writeParent);
void startSorter (int pathId, int writeParent);

int fdLeft[2], fdRight[2], fdParent[2];

int main(int argc, char* argv[]) 
{ 
  pid_t pid1, pid2; 
  //int fdLeft[2], fdRight[2], fdParent[2];
  int fileNum = 4, fileMax = 4;
  int pathLen = (int) log2((double) fileNum);
  int pathTracker[pathLen];
  
  
  while(1){   
    //sleep(1);
    //create pipe for Left child
    pipeCheck(fdLeft);
    //create left child
    pid1 = forkCheck();
    
    //if parent, need to create right child; else skip
    if(pid1 > 0) { //parent w/ 1
      //create pipe for right child
      pipeCheck(fdRight);
      //create right child
      pid2 = forkCheck();
      if (pid2 > 0) { //same parent w/ 2 
	if (fileNum >= fileMax)
	  startMaster(fdLeft[READ], fdRight[READ]);
	else 
	  startMerger(fdLeft[READ], fdRight[READ], fdParent[WRITE]);	
      } else { //Right child 
	pathTracker[(int) log2((double) fileNum) - 1] = 1;
	fdParent[WRITE] = fdRight[WRITE];
      }
    } else { //Left child 
      pathTracker[(int) log2((double) fileNum) - 1] = 0;
      fdParent[WRITE] = fdLeft[WRITE];
    }
    //applies to both children
    fileNum /= 2;
    if(fileNum < 2)
      //printf("%d\n", fdParent[WRITE]
      startSorter(calcPathId (pathTracker, pathLen), fdParent[WRITE]);
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

void startMaster (int readLeft, int readRight) {
  //printf("Master Ready\n");
  char testStr[100] = "faq";
  
  while (1) {
    read(readLeft, testStr, 100);
    printf("-----Master gets: %s\n", testStr);
    
    read(readRight, testStr, 100);
    printf("-----Master gets: %s\n", testStr);
    //sleep(1);
  }
  pause();
}

void startMerger (int readLeft, int readRight, int writeParent) {
  //printf ("Merger Ready\n");
  char testStr[100] = "faq";
  
  while(1) {
    read(readLeft, testStr, 100);
    printf("---Merger gets: %s\n", testStr);
    
    read(readRight, testStr, 100);
    printf("---Merger gets: %s\n", testStr);
    
    write(fdParent[WRITE], testStr, 100);
    //sleep(1);
  }
  pause();
}

void startSorter (int pathId, int writeParent) {
  //printf("Sorter %d Ready\n",pathId);
  char testStr[100];
  while (pathId < 10) {
    sprintf(testStr, "%d", pathId);
    //printf("Sending: %s\n", testStr);
    write(writeParent, testStr, 100);
    pathId++;
    //sleep(1);
  }
  pause();
}








