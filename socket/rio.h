#ifndef __IO_H__
#define __IO_H__
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#define READ_ERROR_TOLERANCE 3
#define WRITE_ERROR_TOLERANCE 3
int Read(int fd, char* buf, int max_len)
{
	int a;
	a = read(fd, buf, max_len);
	return a;
}
int Write(int fd, char* buf, int max_len)
{
	int a;
	a = write(fd, buf, max_len);
	return a;
}
int rio_readn(int fd, char* buf, int max_len)
{
	int sum = 0;
	int a;
	int res_length = max_len;
	int t = 0;
	while(1)
	{
		a = Read(fd, buf+sum, res_length);
		if(a>0) 
		{
			res_length -= a;
			sum += a;
		} else
		{
			t++;
			if(t==READ_ERROR_TOLERANCE) return a;
		}
		if(!res_length) break;
	}
	return sum;
}
int rio_writen(int fd, char* buf, int max_len)
{
	int sum = 0;
	int a;
	int res_length = max_len;
	int t = 0;
	while(1)
	{
		a = Write(fd, buf+sum, res_length);
		if(a>0) {
			res_length -= a;
			sum += a;
		} else
		{
			t++;
			if(t==WRITE_ERROR_TOLERANCE) return a;
		}
		if(!res_length) break;
	}
	return sum;
}
#endif
