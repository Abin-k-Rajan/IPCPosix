#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <pthread.h>


#define NBUFF 10
#define SEM_MUTEX   "mutex" 
#define SEM_NEMPTY  "nempty"
#define SEM_NSTORED "nstored"

int fd1;
int fd2;
int nitems;
char *ptr1, *ptr2;
struct {
	int buff[NBUFF];
	sem_t *mutex, *nempty, *nstored;
} shared;


char* filename = "test.txt";
int CHUNK_SIZE = 10;
int BUFFER_SIZE = 10;

void *produce(void *), *consume(void *);

int main(int argc, char** argv)
{
	pthread_t producer, consumer;
	int fd;
	pid_t pid;
	int shared_memory_size = 0;
	
	int flags = O_EXCL | O_CREAT;

	nitems = atoi(argv[1]);
	CHUNK_SIZE = atoi(argv[2]);

	sem_unlink(SEM_MUTEX);
	sem_unlink(SEM_NEMPTY);
	sem_unlink(SEM_NSTORED);
	shm_unlink(filename);

	shared_memory_size = nitems * CHUNK_SIZE;

	shared.mutex = sem_open(SEM_MUTEX, flags, S_IRWXU, 1);
	shared.nempty = sem_open(SEM_NEMPTY, flags, S_IRWXU, NBUFF);
	shared.nstored = sem_open(SEM_NSTORED, flags, S_IRWXU, 0);


	fd1 = shm_open(filename, flags | O_RDWR, S_IRWXU);
	ftruncate(fd1, shared_memory_size);
	
	pid = fork();

	if (pid < 0)
	{
		exit(-1);
	}
	if (pid == 0)
	{
		void* address = (void *)malloc(sizeof(int));
		ptr2 = mmap(NULL, shared_memory_size, PROT_READ, MAP_SHARED, fd1, 0);
		pthread_create(&consumer, NULL, consume, NULL);
		pthread_join(consumer, NULL);
		fd2 = shm_open(filename, flags | O_RDONLY, S_IRWXU);
		exit(0);
	}
	
	ptr1 = mmap(NULL, shared_memory_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd1, 0);
	pthread_create(&producer, NULL, produce, NULL);
	pthread_join(producer, NULL);
	
	struct stat stat;
	fstat(fd1, &stat);
	printf("%d\n", stat.st_size);

	sem_unlink(SEM_MUTEX);
	sem_unlink(SEM_NEMPTY);
	sem_unlink(SEM_NSTORED);
	//ptr = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	return 0;
}


void zeroBytes()
{
	for (int i = 0; i < CHUNK_SIZE; i++)
	{
		*(ptr1 + i) = 0x00;
	}
}


void* produce(void* arg)
{
	char* ref = ptr1;
	int produced_count = 0;
	int i;
	int fd = open(filename, O_RDWR);
	char buff[CHUNK_SIZE];
	int read_bytes = read(fd, buff, CHUNK_SIZE);
	while (read_bytes > 0)
	{
		sem_wait(shared.nempty);
		sem_wait(shared.mutex);
		shared.buff[i % NBUFF] = i;
		zeroBytes();
		for (int j = 0; j < read_bytes; j++)	
			*ptr1++ = buff[j];
		read_bytes = read(fd, buff, CHUNK_SIZE);
		if (ptr1 == ref + NBUFF * CHUNK_SIZE)
		{
			ptr1 = ref;
		}
		produced_count++;
		printf("PARENT: IN = %d\n", produced_count);
		sem_post(shared.mutex);
		sem_post(shared.nstored);
	}
}


void printChunk()
{
	int count = 0;
	while (*ptr2 != 0)
	{
		if (count == CHUNK_SIZE)
			return;
		printf("%c", *ptr2++);
		count++;
	}
}



void* consume(void* arg)
{
	int i;
	char* ref = ptr2;
	for (;;)
	{
		sem_wait(shared.nstored);
		sem_wait(shared.mutex);
		//if (shared.buff[i % NBUFF] != i)
		printChunk();
		printf(" %d \n", ptr2);
		if (ptr2 == ref + NBUFF * CHUNK_SIZE)
		{
			ptr2 = ref;
		}
		sem_post(shared.mutex);
		sem_post(shared.nempty);
	}
}
