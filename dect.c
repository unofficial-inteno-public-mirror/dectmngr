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


#define F_ACTIVATE_REG 1 << 0
#define F_DELETE_HSET  1 << 1
#define F_PING_HSET    1 << 2
#define F_GET_STATUS   1 << 3



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



static handle_response(packet_t *p) {

	switch (p->type) {
		
	case RESPONSE:
		printf("OK\n");
		break;

	case ERROR:
		printf("ERROR\n");
		break;
	default:
		printf("unknown packet\n");
		break;
	}
}


static void status_packet(struct status_packet *p) {

	int i, j;

	for (i = 0; i < MAX_NR_HSETS; i++) {
		printf("hset: %d", i + 1);

		if (p->handset[i].registered == TRUE) {
			printf("\tregistered\t");

			for (j = 0; j < 5; j++)
				printf("%2.2x ", p->handset[i].ipui[j]);

			if (p->handset[i].present == TRUE)
				printf("\tpresent");
			else 
				printf("\tnot present");

		} else {
			printf("\tnot registered");
		}
		printf("\n");

	}
	printf("\n");
	
	if (p->reg_mode == ENABLED)
		printf("reg_state: ENABLED\n");

	if (p->reg_mode == DISABLED)
		printf("reg_state: DISABLED\n");

	

}

static send_packet(uint8_t s, uint8_t type, uint8_t arg) {

	client_packet *p;
	char nl = '\n';
	int sent;

	if ((p = (client_packet *)malloc(sizeof(client_packet))) == NULL)
		exit_failure("malloc");

	p->size = sizeof(client_packet);
	p->type = type;
	p->data = arg;
	
	if ((sent = send(s, p, p->size, 0)) == -1)
		exit_failure("send");

	/* if (recv(s, p, sizeof(packet), 0) == -1) */
	/* 	exit_failure("recv"); */

	/* handle_response(p); */

	free(p);
}



static void *get_reply(uint8_t s) {
	
	uint8_t *buf;
	int r;

	if ((buf = (client_packet *)malloc(sizeof(struct status_packet))) == NULL)
		exit_failure("malloc");

		       
	if (r = recv(s, buf, sizeof(struct status_packet), 0) == -1)
		exit_failure("recv");
	
	return buf;
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



int main(int argc, char *argv[]) {

	int s, c, handset = 0;
	int flags = 0;

	s = establish_connection();

	/* Parse command line options */
	while ((c = getopt (argc, argv, "rd:p:s")) != -1) {
		switch (c) {
		case 'r':
			flags |= F_ACTIVATE_REG;
			break;
		case 'd':
			flags |= F_DELETE_HSET;
			handset = atoi(optarg);
			break;
		case 'p':
			flags |= F_PING_HSET;
			handset = atoi(optarg);
			break;
		case 's':
			flags |= F_GET_STATUS;
			break;
	
		}
	}

	if (flags & F_ACTIVATE_REG) {
		printf("activate registration\n");
		send_packet(s, REGISTRATION, 0);
	}

	if (flags & F_DELETE_HSET) {
		printf("delete hset %d\n", handset);
		send_packet(s, DELETE_HSET, handset);
	}

	if (flags & F_PING_HSET) {
		printf("ping hset %d\n", handset);
		send_packet(s, PING_HSET, handset);
	}

	if (flags & F_GET_STATUS) {
		printf("get status\n");
		send_packet(s, GET_STATUS, 0);
		struct status_packet *p = get_reply(s);
		status_packet(p);
	}
	
	return 0;
}
