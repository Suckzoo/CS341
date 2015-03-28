#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <resolv.h>
#include "packet.h"
#include "rio.h"


#define MAXLINE 1024 
#define SOCK_SETSIZE 1021

int port;
int protocol[SOCK_SETSIZE];
char last_char[SOCK_SETSIZE];
int init_commit[SOCK_SETSIZE];
fd_set readfds, allfds;

int argument_handling(int argc, char** argv)
{
	if(argc != 3)
	{
		return -1;
	}
	if(!strcmp(argv[1], "-p"))
	{
		port = atoi(argv[2]);
	} else {
		return -1;
	}
	return 0;
}
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
	if(p.op!=1)
	{
		return -1;
	}
	if(p.proto!=1 && p.proto!=2)
	{
		return -1;
	}
	if(p.sum!=checksum(p))
	{
		return -1;
	}
	return 0;
}
uint32_t random_trans_id_gen()
{
	uint32_t a = ((rand()%2)<<15) + rand();
	uint32_t b = ((rand()%2)<<15) + rand();
	return (a<<16) + b;
}
int sayhello(int sockfd)
{
	struct packet p;
	p.op = 0;
	p.proto = 0;
	p.transid = random_trans_id_gen();
	p.sum = checksum(p);
	rio_writen(sockfd, (char*)&p, 8);
	return 0;
}
int gethello(int sockfd)
{
	struct packet p;
	char buf[10];
	rio_readn(sockfd, buf, 8);
	p = *((struct packet*)buf);
	if(is_valid_hello(p))
	{
		close(sockfd);
		FD_CLR(sockfd, &readfds);
		//printf("[Server]invalid packet received\n");
		return -1;
	}
	protocol[sockfd] = p.proto;
	return 0;
}
void redundancy2(int sockfd, char *buf, int size)
{
	int i;
	int ns=0;
	char* nbuf = (char*)malloc(size);
	for(i=0;i<size;i++)
	{
		if(last_char[sockfd]!=buf[i] || !init_commit[sockfd])
		{
			init_commit[sockfd] = 1;
			nbuf[ns++] = buf[i];
			last_char[sockfd] = buf[i];
		}
	}
	int n_ns = htonl(ns);
	rio_writen(sockfd, (char*)&n_ns, 4); 
	rio_writen(sockfd, nbuf, ns);
	free(nbuf);
}
void protocol1(int sockfd)
{
	char buf[2500];
	char nbuf[2500];
	int ns=0;
	int size;
	int i;
	memset(nbuf, 0, sizeof(nbuf));
	//while(1)
	//{
		memset(buf, 0, sizeof(buf));
		size = read(sockfd, buf, 2048);
		if(size<=0)
		{
			close(sockfd);
			FD_CLR(sockfd, &readfds);
			init_commit[sockfd] = 0;
			protocol[sockfd] = 0;
			last_char[sockfd] = -1;
			return;
			//break;
		}
		for(i=0;i<size;i++)
		{
			if(init_commit[sockfd] == 0)
			{
				if(buf[i]=='\\')
				{
					init_commit[sockfd] = 2;
				}
				else
				{
					last_char[sockfd] = buf[i];
					init_commit[sockfd] = 1;
					nbuf[ns++] = buf[i];
				}
			}
			else if(init_commit[sockfd] == 1)
			{
				if(buf[i]=='\\')
				{
					init_commit[sockfd] = 2;
				}
				else if(buf[i]!=last_char[sockfd])
				{
					last_char[sockfd] = buf[i];
					nbuf[ns++] = buf[i];
				}
			}
			else if(init_commit[sockfd] == 2)
			{
				if(buf[i]=='\\')
				{
					init_commit[sockfd] = 1;
					if(last_char[sockfd]!='\\') {
						last_char[sockfd] = '\\';
						nbuf[ns++] = '\\';
						nbuf[ns++] = '\\';
					}
				}
				else if(buf[i]=='0')
				{
					init_commit[sockfd] = 1;
				}
				else
				{
					close(sockfd);
					FD_CLR(sockfd, &readfds);
					init_commit[sockfd] = 0;
					protocol[sockfd] = 0;
					last_char[sockfd] = -1;
					return;
				}
			}
		}
		nbuf[ns++] = '\\';
		nbuf[ns++] = '0';
		rio_writen(sockfd, nbuf, ns);
		memset(nbuf, 0, sizeof(nbuf));
		ns = 0;
	//}
}
void protocol2(int sockfd)
{
	uint32_t size;
	if(rio_readn(sockfd, (char*)&size, 4)<=0)
	{
		close(sockfd);
		FD_CLR(sockfd, &readfds);
		init_commit[sockfd] = 0;
		protocol[sockfd] = 0;
		last_char[sockfd] = -1;
		return;
	}
	size = ntohl(size);
	char* buf;
	buf = (char*)malloc(size);
	rio_readn(sockfd, buf, size);
	redundancy2(sockfd, buf, size);
	free(buf);
}
void process(int sockfd)
{
	if(protocol[sockfd]==0) gethello(sockfd);
	else if(protocol[sockfd]==1) protocol1(sockfd);
	else if(protocol[sockfd]==2) protocol2(sockfd);
}
int main(int argc, char **argv)
{
	int listen_fd, client_fd;
	socklen_t addrlen;
	int fd_num;
	int maxfd = 0;
	int sockfd;
	int i= 0;
	char buf[MAXLINE];
	srand(time(NULL));
	memset(last_char, -1, sizeof(last_char));

	argument_handling(argc, argv);

	struct sockaddr_in server_addr, client_addr;

	if((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket error");
		return 1;
	}   
	memset((void *)&server_addr, 0x00, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(port);
	
	if(bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
	{
		perror("bind error");
		return 1;
	}   
	if(listen(listen_fd, 5) == -1)
	{
		perror("listen error");
		return 1;
	}   
	
	FD_ZERO(&readfds);
	FD_SET(listen_fd, &readfds);

	maxfd = listen_fd;
	while(1)
	{
		allfds = readfds;
		//printf("Select Wait %d\n", maxfd);
		fd_num = select(maxfd + 1 , &allfds, (fd_set *)0,
					  (fd_set *)0, NULL);

		if (FD_ISSET(listen_fd, &allfds))
		{
			addrlen = sizeof(client_addr);
			client_fd = accept(listen_fd,
					(struct sockaddr *)&client_addr, &addrlen);

			FD_SET(client_fd,&readfds);

			if (client_fd > maxfd)
				maxfd = client_fd;
			//printf("Accept OK\n");
			sayhello(client_fd);
			continue;
		}

		for (i = 0; i <= maxfd; i++)
		{
			sockfd = i;
			if (FD_ISSET(sockfd, &allfds))
			{
				process(sockfd);
			}
		}
	}
}

