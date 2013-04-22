#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>

#include "dectd.h"


void exit_failure(const char *format, ...)
{
#define BUF_SIZE 500
	char err[BUF_SIZE], msg[BUF_SIZE];
	va_list ap;
	
	strncpy(err, strerror(errno), BUF_SIZE);

	va_start(ap, format);
	vsprintf(msg, format, ap);
	va_end(ap);
	
	fprintf(stderr, "%s: %s\n", msg, err);
	exit(EXIT_FAILURE);
}



int main(void)
{
	int d, fdmax, res, l, n, ret, i, len, pkt_len, s;
	fd_set rfds, master;
	unsigned char buf[MAX_MAIL_SIZE];
	struct sockaddr_in my_addr, remote_addr;
	socklen_t remote_addr_size;
	uint8_t hdr[2];
	int client[MAX_LISTENERS];
	client_packet *p;


	/* Connect to dectd */
	memset(&remote_addr, 0, sizeof(remote_addr));
	remote_addr.sin_family = AF_INET;
	remote_addr.sin_addr.s_addr = INADDR_ANY;
	remote_addr.sin_port = 7778;
	
	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		exit_failure("socket");

	if (connect(s, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr)) < 0)
	    exit_failure("error connecting to dectproxy");

	if ((p = (client_packet *)malloc(sizeof(client_packet))) == NULL)
		exit_failure("malloc");


	p->type = PING_HSET;
	p->size = 1;
	
	if (send(s, p, sizeof(client_packet), 0) == -1)
		exit_failure("send");

	p->type = GET_STATUS;
	p->size = 2;
	
	if (send(s, p, sizeof(client_packet), 0) == -1)
		exit_failure("send");

	p->type = REGISTRATION;
	p->size = 3;
	
	if (send(s, p, sizeof(client_packet), 0) == -1)
		exit_failure("send");

	p->type = DELETE_HSET;
	p->size = 4;	

	if (send(s, p, sizeof(client_packet), 0) == -1)
		exit_failure("send");


	
}
