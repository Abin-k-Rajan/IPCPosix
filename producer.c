#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

char *ptr1, *ptr2;
int* in, *out, *done;

char* SRCFILENAME = "test.txt";			//	GLOBAL VARIABLES FOR PARENT AND CHILD PROCESS
char* OUTFILENAME = "test.out";
int CHUNK_SIZE = 10;
int BUFFER_SIZE = 10;
int NBUFF = 10;					//	NUMBER OF CHUNKS THAT CAN BE ACCOMODATED IN BUFFER


void produce();
void consume();



int main(int argc, char** argv)
{
	int fd;
	pid_t pid;
	SRCFILENAME = argv[1];
	CHUNK_SIZE = atoi(argv[2]);
	OUTFILENAME = argv[3];
	BUFFER_SIZE = atoi(argv[4]);
	int flags = O_EXCL | O_CREAT;
	void* address;

	shm_unlink(SRCFILENAME);

	fd = shm_open(SRCFILENAME, flags | O_RDWR, S_IRWXU);		//	OPENING SHARED MEMORY
	
	ftruncate(fd, BUFFER_SIZE + 12);		//	EXTRA 2 BYTES FOR VARS IN AND OUT (SYNCHRONIZATION)

	address = (void *)malloc(BUFFER_SIZE + 12);		//	ADDRESS THAT IS USED FOR MAPPING IN MMAP FUNCTION
	
	pid = fork();		//	FORK() CREATING CHILD AND PARENT


	if (pid < 0)
	{
		exit(-1);
	}
	if (pid == 0)
	{
		ptr2 = mmap(address, BUFFER_SIZE + 12, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);		//	CONSUMER CODE
		consume();
		exit(0);
	}
	
	ptr1 = mmap(address, BUFFER_SIZE + 12, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);		//	PRODUCER CODE
	produce();	
	shm_unlink(SRCFILENAME);
	return 0;
}

void print(int num)
{

	printf("%d\n", num);
}


void produce()
{
	in = ptr1 + BUFFER_SIZE;
	out = ptr1 + BUFFER_SIZE + 4;
	done = ptr1 + BUFFER_SIZE + 8;
	*in = 0;
	*out = 0;
	*done = 0;

	for (int i = 0; i < 26; i++)
	{
		while ((*in + 1) % BUFFER_SIZE == *out)
		{

		}
		*(ptr1 + *in) = (char)(97 + i);
		*in = (*in + 1) % BUFFER_SIZE;
	}
	*done = 1;
}

void consume()
{
	in = ptr2 + BUFFER_SIZE;
	out = ptr2 + BUFFER_SIZE + 4;
	done = ptr2 + BUFFER_SIZE + 8;

	char* buff = (char*) malloc(CHUNK_SIZE);
	int count = 0;
	
	for (;;)
	{
		while (*in == *out)
		{
			if (*done == 1)
			{	
				buff -= count;
				printf("%s\n", buff);
				return;	
			}
		}
		*buff++ = *(ptr2 + *out);
		count++;
		*out = (*out + 1) % BUFFER_SIZE;
	}

	printf("%s\n", buff);	
}
