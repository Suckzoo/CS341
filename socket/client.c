#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <resolv.h>
#include <unistd.h>
#include "packet.h"
#include "rio.h"

int protocol;
char host[16];
int port;
int argument_handling(int argc, char** argv)
{
	if(argc != 7)
	{
		return -1;
	}
	if(!strcmp(argv[1], "-p"))
	{
		port = atoi(argv[2]);
	} else if(!strcmp(argv[3], "-p"))
	{
		port = atoi(argv[4]);
	} else if(!strcmp(argv[5], "-p"))
	{
		port = atoi(argv[6]);
	} else {
		return -1;
	}
	if(!strcmp(argv[1], "-m"))
	{
		protocol = atoi(argv[2]);
	} else if(!strcmp(argv[3], "-m"))
	{
		protocol = atoi(argv[4]);
	} else if(!strcmp(argv[5], "-m"))
	{
		protocol = atoi(argv[6]);
	} else {
		return -1;
	}
	if(!strcmp(argv[1], "-h"))
	{
		strcpy(host, argv[2]);
	} else if(!strcmp(argv[3], "-h"))
	{
		strcpy(host, argv[4]);
	} else if(!strcmp(argv[5], "-h"))
	{
		strcpy(host, argv[6]);
	} else {
		return -1;
	}
	return 0;
}
int initialize(int* sockfd, struct sockaddr_in* dest, char* host, uint16_t port)
{	
	if((*sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("Socket");
		exit(errno);
	}
	bzero(dest, sizeof(*dest));
	dest->sin_family = AF_INET;
	dest->sin_port = htons(port);
	dest->sin_addr.s_addr = inet_addr(host);
	if(connect(*sockfd, (struct sockaddr*)dest, sizeof(*dest)) != 0)
	{
		perror("Connect");
		exit(errno);
	}
}
int read_from_fd(int fd, char* buf, int max_len)
{
	char wd;
	int i;
	int m = max_len>10000?10000:max_len;
	for(i=0;i<m;i++)
	{
		if(read(fd, &wd, 1) == 0)
		{
			break;
		}
		buf[i] = wd;
	}
	return i;
}
//int Write(int fd, char* buf, int max_len)
//{
//	int a;
//	if(a = write(fd, buf, max_len)<0) {
//		perror("Write");
//		exit(errno);
//	}
//	return a;
//}

uint16_t checksum(struct packet p)
{
	uint32_t sum;
	sum = 0;
	sum += ((uint16_t)p.op);
	sum += ((uint16_t)p.proto)<<8;
	sum += p.transid & 0xFFFF;
	sum += (p.transid>>16) & 0xFFFF;
	while(sum>>16)
		sum = (sum & 0xFFFF) + (sum >> 16);
	return ((uint16_t)(~sum));
}
int is_valid_hello(struct packet p)
{
	if(p.op!=0)
	{
		return -1;
	}
	if(p.proto!=0)
	{
		return -1;
	}
	if(p.sum!=checksum(p))
	{
		return -1;
	}
	return 0;
}
int hello(int sockfd)
{
	struct packet p;
	char buf[10];
	int recv_buf_len = 0;
	while(recv_buf_len<8)
	{
		recv_buf_len += read_from_fd(sockfd, buf, 8);
	}
	p = *((struct packet*)buf);
	p.sum = p.sum;
	p.transid = p.transid;
	if(is_valid_hello(p))
	{
		close(sockfd);
		printf("invalid packet received\n");
		exit(0);
	}
	p.op = 1;
	p.proto = protocol;
	p.sum = checksum(p);
	p.transid = (p.transid);
	p.sum = (p.sum);
	rio_writen(sockfd, (char*)&p, 8);
	return 0;
}
void protocol1(int sockfd)
{
	char buf[3000];
	char message_buf[3000];
	int nbuff = 0, nmessage = 0;
	char wb;
	int i;
	while(1)
	{
		if((nbuff = read(STDIN_FILENO, buf, 2000)) <= 0)
		{
			message_buf[nmessage++] = '\\';
			message_buf[nmessage++] = '0';
			rio_writen(sockfd, message_buf, nmessage);
			memset(buf, 0, sizeof(buf));
			memset(message_buf, 0, sizeof(message_buf));
			nbuff = 0;
			nmessage = 0;
			while(1)
			{
				if((nbuff = read(sockfd, buf, sizeof(buf))) <= 0)
				{
					break;
				}
				for(i=0;i<nbuff;i++)
				{
					
					if(buf[i]=='\\' && buf[i+1]=='0')
					{
						break;
					}
					if(buf[i]=='\\') i++;
					if(i==nbuff) Write(STDOUT_FILENO, "\\", 1);
					else Write(STDOUT_FILENO, buf+i, 1);
				}
				if(i<nbuff) break;
			}
			break;
		}
		for(i=0;i<nbuff;i++) {
			if(buf[i]=='\\')
			{
				message_buf[nmessage++] = '\\';
				message_buf[nmessage++] = '\\';
			}
			else
			{
				message_buf[nmessage++] = buf[i];
			}
		}
		message_buf[nmessage++] = '\\';
		message_buf[nmessage++] = '0';
		rio_writen(sockfd, message_buf, nmessage);
		memset(buf, 0, sizeof(buf));
		memset(message_buf, 0, sizeof(message_buf));
		nbuff = 0;
		nmessage = 0;
		while(1)
		{
			if((nbuff = read(sockfd, buf, sizeof(buf))) <= 0)
			{
				break;
			}
			for(i=0;i<nbuff;i++)
			{
				
				if(buf[i]=='\\' && buf[i+1]=='0')
				{
					break;
				}
				if(buf[i]=='\\') i++;
				if(i==nbuff) Write(STDOUT_FILENO, "\\", 1);
				else Write(STDOUT_FILENO, buf+i, 1);
			}
			if(i<nbuff) break;
		}
	}
}
void protocol2(int sockfd)
{
	char* buff;
	buff = (char*)malloc(2048);
	int nbuff=0, nmessage=0;
	while((nbuff = read(STDIN_FILENO, buff, 2048)) > 0)
	{
		uint32_t n_nbuff = htonl(nbuff);
		write(sockfd, &n_nbuff, sizeof(uint32_t));
		write(sockfd, buff, nbuff);
		memset(buff, 0, sizeof(buff));
		read(sockfd, &n_nbuff, sizeof(int));
		nbuff = ntohl(n_nbuff);
		rio_readn(sockfd, buff, nbuff);
		rio_writen(STDOUT_FILENO, buff, nbuff);
		//int i;
		//char a[60];
		//for(i=0;i<nmessage;i+=20)
		//{
		//	nbuff = read(sockfd, a, 20);
		//	Write(STDOUT_FILENO, a, nbuff);
		//}	
	}
	free(buff);
}
int main(int argc, char** argv)
{
	if(argument_handling(argc, argv) != 0)
	{
		perror("Argument");
		return 0;
	}
	int sockfd;
	struct sockaddr_in dest;
	initialize(&sockfd, &dest, host, (uint16_t)port);
	hello(sockfd);
	if(protocol==1) protocol1(sockfd);
	if(protocol==2) protocol2(sockfd);
	close(sockfd);
	return 0;
}
