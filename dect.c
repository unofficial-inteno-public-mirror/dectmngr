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



static handle_response(packet *p) {

	switch (p->arg) {
		
	case OK:
		printf("OK\n");
		break;

	case ERROR:
		printf("ERROR\n");
		break;
	}
		

}


static set_registration(uint8_t s, uint8_t mode) {

	packet *p;

	if ((p = (packet *)malloc(sizeof(packet))) == NULL)
		exit_failure("malloc");

	p->type = REGISTRATION;
	p->arg = mode;
	
	if (send(s, p, sizeof(packet), 0) == -1)
		exit_failure("send");

	if (recv(s, p, sizeof(packet), 0) == -1)
		exit_failure("recv");

	handle_response(p);

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
	remote_addr.sin_port = 7778;
	
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
		set_registration(s, ENABLED);

		set_registration(s, DISABLED);
	}

	if (flags & DELETE_HSET) {
		printf("delete hset\n");
	}

	if (flags & PING_HSET) {
		printf("ping hset\n");
	}

	if (flags & GET_STATUS) {
		printf("get status\n");
	}
	
	return 0;
}
