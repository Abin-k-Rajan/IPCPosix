#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>

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


int countDigits(int num)
{
	int count = 0;
	while (num)
	{
		count += 1;
		num /= 10;
	}
	return count;
}



char* numberToChar(int num)
{
	int size = countDigits(num);
	char* buff = (char *)malloc(size);
	for(int i = 0; i < size; i++)
	{
		*(buff + size - i - 1) = (char)(num % 10 + 48);
		num /= 10;
	}
	return buff;
}

int getBufferSize(char* buff)
{
	int size = 0;
	while (*(buff + size) != 0)
	{
		size++;
	}
	return size;
}


void outputToTerminal(int who, char* buff, int index)
{
	char* parent = "PARENT: ";
	char* child = "CHILD: ";

	char* _in = "IN = ";
	char* _item = "ITEM = ";
	char* number = numberToChar(index);

	int digitsCount = countDigits(index);
	int bufferSize = getBufferSize(buff);


	int buff1Size = getBufferSize((who == 0 ? parent : child)) + getBufferSize(_in) + digitsCount + 1;
	int buff2Size = getBufferSize((who == 0 ? parent : child)) + getBufferSize(_item) + bufferSize + 1;

	int totalBuffSize = buff1Size + buff2Size;

	char* buff1 = (char *)malloc(totalBuffSize);

	buff1 = strcat(buff1, (who == 0 ? parent : child));
	buff1 = strcat(buff1, _in);
	buff1 = strcat(buff1, number);
	buff1 = strcat(buff1, "\n");

	buff1 = strcat(buff1, (who == 0 ? parent : child));
	buff1 = strcat(buff1, _item);
	buff1 = strcat(buff1, buff);
	buff1 = strcat(buff1, "\n");
	write(STDOUT_FILENO, buff1, totalBuffSize);
}


void produce()
{
	in = ptr1 + BUFFER_SIZE;
	out = ptr1 + BUFFER_SIZE + 4;
	done = ptr1 + BUFFER_SIZE + 8;
	*in = 0;
	*out = 0;
	*done = 0;
	int iters = 1;

	char* buff = (char*)malloc(CHUNK_SIZE);

	int fd = open(SRCFILENAME, O_RDONLY);
	int read_bytes = read(fd, buff, CHUNK_SIZE);

	while (read_bytes > 0)
	{
		*(buff + read_bytes) = 0x00;
		for (int i = 0; i < read_bytes; i++)
		{
			while ((*in + 1) % BUFFER_SIZE == *out)
			{

			}
			*(ptr1 + *in) = (char)(*buff++);
			*in = (*in + 1) % BUFFER_SIZE;
		}
		buff -= read_bytes;
		outputToTerminal(0, buff, iters++);
		read_bytes = read(fd, buff, CHUNK_SIZE);
	}
	*done = 1;
	close(fd);
}

void consume()
{
	in = ptr2 + BUFFER_SIZE;
	out = ptr2 + BUFFER_SIZE + 4;
	done = ptr2 + BUFFER_SIZE + 8;

	int iters = 1;

	int fd = open(OUTFILENAME, O_CREAT | O_WRONLY);

	char* buff = (char*) malloc(CHUNK_SIZE);
	int count = 0;
	
	for (;;)
	{
		while (*in == *out)
		{
			if (*done == 1)
			{	
				buff -= count;
				outputToTerminal(1, buff, iters);
				int res = write(fd, buff, count);
				if (res < 0)
				{
					exit(-2);
				}
				close(fd);
				return;	
			}
		}
		*buff++ = *(ptr2 + *out);
		count++;
		if (count == CHUNK_SIZE)
		{
			buff -= count;
			outputToTerminal(1, buff, iters++);
			
			int res = write(fd, buff, count);
			if (res < 0)
			{
				exit(-2);
			}
			count = 0;
			buff = (char *)malloc(CHUNK_SIZE);
		}
		*out = (*out + 1) % BUFFER_SIZE;
	}

}
