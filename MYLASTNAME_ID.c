#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <pthread.h>

#define SEM_MUTEX   "mutex" 		//	SEMAPHORE NAMES FOR THREAD AND PROCESS SYNCHRONIZATION
#define SEM_NEMPTY  "nempty"
#define SEM_NSTORED "nstored"

char *ptr1, *ptr2;			//	POINTER TO SHARED MEM ( CREATED BY MMAP SYSTEM CALL )
struct {
	sem_t *mutex, *nempty, *nstored;	//	SEMAPHORE
} shared;


char* SRCFILENAME = "test.txt";			//	GLOBAL VARIABLES FOR PARENT AND CHILD PROCESS
char* OUTFILENAME = "test.out";
int CHUNK_SIZE = 10;
int BUFFER_SIZE = 10;
int NBUFF = 10;					//	NUMBER OF CHUNKS THAT CAN BE ACCOMODATED IN BUFFER


						
void *produce(void *), *consume(void *);		//	FUNCTION POINTERS TO PRODUCE AND CONSUME DATA FROM / TO SHARED MEMORY

void zeroBytes(int size);				//	FUNCTION TO SET BYTES IN CHUNK AS ZERO
int maxChunkSize(int start, int curr);			//	CHECKING MAXIMUM AVAILABLE CLUSTER SIZE IN BUFFER AFTER EACH OPERATION	
char* readChunk();					//	READ DATA FROM EACH CHUNK
int getSizeOfBytes(char* buff);				//	UTILITY FUNCTION TO GET SIZE OF BYTES READ FROM SHARED MEMORY
int writeChunkToOutputFile(int fd, char* buff);		//	FUNCTION TO WRITE OUTPUT TO OUTPUT FILE
char* getCharPointerFromArray(char buff[], int n);	//	UTILITY FUNCTION TO CONVERT CHAR[] TO CHAR*


int main(int argc, char** argv)
{
	pthread_t producer, consumer;
	int fd;
	pid_t pid;
	void* address;
	int fd1, fd2;
	
	int flags = O_EXCL | O_CREAT;

	if (argc != 5)
	{
		printf(" ERROR\n RUN PROGRAM AS FOLLOWS\n ./MYLASTNAME_ID.exe SRC_FILE CHUNK_SIZE OUT_FILE BUFFER_SIZE\n");
		exit(-2);
	}

	SRCFILENAME = argv[1];
	CHUNK_SIZE = atoi(argv[2]);
	OUTFILENAME = argv[3];
	BUFFER_SIZE = atoi(argv[4]);

	sem_unlink(SEM_MUTEX);		//	UNLINK SEMAPHORE AND SHARED MEMORY AS A MEASURE TO CLOSE PREVIOUSLY OPENED FILE WITH SAME NAME
	sem_unlink(SEM_NEMPTY);
	sem_unlink(SEM_NSTORED);
	shm_unlink(SRCFILENAME);

	NBUFF = BUFFER_SIZE / CHUNK_SIZE;

	shared.mutex = sem_open(SEM_MUTEX, flags, S_IRWXU, 1);		//	MUTEX
	shared.nempty = sem_open(SEM_NEMPTY, flags, S_IRWXU, NBUFF);	//	COUNTING SEMAPHORE
	shared.nstored = sem_open(SEM_NSTORED, flags, S_IRWXU, 0);	//	COUNTING SEMAPHORE


	fd1 = shm_open(SRCFILENAME, flags | O_RDWR, S_IRWXU);		//	OPENING SHARED MEMORY
	ftruncate(fd1, BUFFER_SIZE);
	
	address = (void *)malloc(BUFFER_SIZE);		//	ADDRESS THAT IS USED FOR MAPPING IN MMAP FUNCTION
	
	pid = fork();		//	FORK() CREATING CHILD AND PARENT

	if (pid < 0)
	{
		exit(-1);
	}
	if (pid == 0)
	{
		ptr2 = mmap(address, BUFFER_SIZE, PROT_READ, MAP_SHARED, fd1, 0);		//	CONSUMER CODE
		pthread_create(&consumer, NULL, consume, NULL);
		pthread_join(consumer, NULL);
		fd2 = shm_open(SRCFILENAME, flags | O_RDONLY, S_IRWXU);
		exit(0);
	}
	
	ptr1 = mmap(address, BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd1, 0);		//	PRODUCER CODE
	pthread_create(&producer, NULL, produce, NULL);
	pthread_join(producer, NULL);


	sem_unlink(SEM_MUTEX);		// 	UNLINKING ALL THE FILES
	sem_unlink(SEM_NEMPTY);
	sem_unlink(SEM_NSTORED);
	shm_unlink(SRCFILENAME);
	return 0;
}


void* produce(void* arg)
{
	char* ref = ptr1;
	int produced_count = 0;
	int i;
	int fd = open(SRCFILENAME, O_RDONLY);
	char buff[CHUNK_SIZE];
	int read_bytes = read(fd, buff, CHUNK_SIZE);
	while (read_bytes > 0)
	{
		sem_wait(shared.nempty);
		sem_wait(shared.mutex);

		int cluster_size = maxChunkSize((int)ref, (int)ptr1);
		zeroBytes(CHUNK_SIZE);

		for (int j = 0; j < read_bytes; j++)	
			*ptr1++ = buff[j];

		if (cluster_size < CHUNK_SIZE)
		{
			ptr1 = ref;
			cluster_size = CHUNK_SIZE;
		}
		produced_count++;
		
		char* str = getCharPointerFromArray(buff, read_bytes);
		printf("PARENT: IN = %d\n", produced_count);
		printf("PARENT: ITEM = %s\n", str);
		
		read_bytes = read(fd, buff, cluster_size);
		sem_post(shared.mutex);
		sem_post(shared.nstored);
	}
}


void* consume(void* arg)
{
	int i;
	int count = 0;
	char* ref = ptr2;
	int fd = open(OUTFILENAME, O_WRONLY | O_CREAT, S_IRWXU);
	for (;;)
	{
		sem_wait(shared.nstored);
		sem_wait(shared.mutex);
		int cluster_size = maxChunkSize((int) ref, (int) ptr2);
		char* buff = readChunk();
		writeChunkToOutputFile(fd, buff);		

		if (cluster_size < CHUNK_SIZE)
		{
			ptr2 = ref;
		}

		count++;
		printf("CHILD: OUT = %d\n", count);
		printf("CHILD: ITEM = %s\n", buff);
		sem_post(shared.mutex);
		sem_post(shared.nempty);
	}
}



char* getCharPointerFromArray(char buff[], int n)
{
	char* buffer = (char*) malloc(n);
	for (int i = 0; i < n; i++)
	{
		*buffer++ = buff[i];
	}
	buffer -= n;
	return buffer;
}


void zeroBytes(int size)
{
	for (int i = 0; i < size; i++)
	{
		*(ptr1 + i) = 0x00;
	}
}


int maxChunkSize(int start, int curr)
{
	int diff = (start + BUFFER_SIZE) - curr;
	return (diff < CHUNK_SIZE ? diff : CHUNK_SIZE);
}



char* readChunk()
{
	char* buff = (char *)malloc(CHUNK_SIZE);
	int count = 0;
	while (*ptr2 != 0)
	{
		if (count == CHUNK_SIZE)
			break;
		*buff++ = *ptr2++;
		count++;
	}
	buff -= count;
	return buff;
}

int getSizeOfBytes(char* buff)
{
	int count = 0;
	while (*(buff + count) != 0)
	{
		count++;
	}
	return count;
}


int writeChunkToOutputFile(int fd, char* buff)
{
	int res = write(fd, buff, getSizeOfBytes(buff));
	if (res < 0)
		return -1;
	return 0;
}
