#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <getopt.h>

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



/* static handle_response(packet_t *p) { */

/* 	switch (p->arg) { */
		
/* 	case OK: */
/* 		printf("OK\n"); */
/* 		break; */

/* 	case ERROR: */
/* 		printf("ERROR\n"); */
/* 		break; */
/* 	} */
		

/* } */


static set_registration(uint8_t s, uint8_t mode) {

	client_packet *p;
	char nl = '\n';
	int sent;

	if ((p = (client_packet *)malloc(sizeof(client_packet))) == NULL)
		exit_failure("malloc");

	p->size = sizeof(client_packet);
	p->type = REGISTRATION;
	
	printf("p->size: %d\n", p->size);
	
	if ((sent = send(s, p, p->size, 0)) == -1)
		exit_failure("send");

	printf("sent: %d\n", sent);
	/* if (recv(s, p, sizeof(packet), 0) == -1) */
	/* 	exit_failure("recv"); */

	/* handle_response(p); */

	free(p);
}



int establish_connection(void) {

	int s;

	struct sockaddr_in remote_addr;
	socklen_t remote_addr_size;

	/* Connect to dectd */
	memset(&remote_addr, 0, sizeof(remote_addr));
	remote_addr.sin_family = AF_INET;
	remote_addr.sin_addr.s_addr = INADDR_ANY;
	remote_addr.sin_port = 40713;
	
	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		exit_failure("socket");

	if (connect(s, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr)) < 0)
		exit_failure("error connecting to dectproxy");
	
	return s;
}

#define ACTIVATE_REG 1 << 0
#define DELETE_HSET  1 << 1
#define PING_HSET    1 << 2
#define GET_STATUS   1 << 3


int main(int argc, char *argv[]) {

	int s, c, flags;

	s = establish_connection();

	/* Parse command line options */
	while ((c = getopt (argc, argv, "rdps")) != -1) {
		switch (c) {
		case 'r':
			flags |= ACTIVATE_REG;
			break;
		case 'd':
			flags |= DELETE_HSET;
			break;
		case 'p':
			flags |= PING_HSET;
			break;
		case 's':
			flags |= GET_STATUS;
			break;
	
		}
	}

	if (flags & ACTIVATE_REG) {
		printf("activate registration\n");
		set_registration(s, ENABLED);
	}

	/* if (flags & DELETE_HSET) { */
	/* 	printf("delete hset\n"); */
	/* } */

	/* if (flags & PING_HSET) { */
	/* 	printf("ping hset\n"); */
	/* } */

	/* if (flags & GET_STATUS) { */
	/* 	printf("get status\n"); */
	/* } */
	
	return 0;
}
