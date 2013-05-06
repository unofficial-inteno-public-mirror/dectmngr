#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include "dectd.h"


#define MAX_MAIL_SIZE 4098
#define MAX_LISTENERS 10

#define MAX(a,b) ((a) > (b) ? (a) : (b))


struct sigaction act;

void sighandler(int signum, siginfo_t *info, void *ptr)
{
	printf("Recieved signal %d\n", signum);
}

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
	int d, fdmax, res, l, n, ret, i;
	fd_set rfds, master;
	unsigned char buf[MAX_MAIL_SIZE];
	struct sockaddr_in my_addr, peer_addr;
	socklen_t peer_addr_size;
	uint8_t hdr[2];
	int client[MAX_LISTENERS];

	/* Setup signal handler. When writing data to a
	   client that closed the connection we get a
	   SIGPIPE. We need to catch it to avoid beeing killed */
	memset(&act, 0, sizeof(act));
	act.sa_sigaction = sighandler;
	act.sa_flags = SA_SIGINFO;
	sigaction(SIGPIPE, &act, NULL);

	d = open("/dev/dect", O_RDWR);
	if (d == -1)
		exit_failure("open");

	fdmax = d;
	FD_ZERO(&master);
	FD_SET(d, &master);

	memset(&my_addr, 0, sizeof(my_addr));
	my_addr.sin_family = AF_INET;
	my_addr.sin_addr.s_addr = INADDR_ANY;
	my_addr.sin_port = htons(7777);


	if ((l = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		exit_failure("socket");
	
	if (bind(l, (struct sockaddr *) &my_addr, sizeof(struct sockaddr)) == -1)
		exit_failure("bind");
	
	if (listen(l, MAX_LISTENERS) == -1)
		exit_failure("listen");

	FD_SET(l, &master);
	fdmax = MAX(fdmax, l);
	
	while (1) {

		memcpy(&rfds, &master, sizeof(fd_set));
		
		if (select(fdmax + 1, &rfds, NULL, NULL, NULL) == -1)
			exit_failure("select");


		/* Accept new connectons */
		if (FD_ISSET(l, &rfds)) {
			peer_addr_size = sizeof(peer_addr);
			if ((n = accept(l, (struct sockaddr *) &peer_addr, &peer_addr_size)) == -1)
				exit_failure("accept");
			else {
				printf("accepted connection: %d\n", n);
				/* Add new connection to rfds */
				FD_SET(n, &master);
				fdmax = MAX(fdmax, n);
			}
		}

		
		/* Check /dev/dect for data */
		if (FD_ISSET(d, &rfds)) {
			struct dect_packet p;
			p.type = DECT_PACKET;
			ret = read(d, p.data, MAX_MAIL_SIZE);
			printf("dect %d\n", ret);
			if (ret == -1) {
				exit_failure("read");
			} else {
				p.size = ret + sizeof(struct packet_header);
				for (i = 0; i <= fdmax; i++) {
					/* If data is read from /dev/dect, send it to all clients */
					if (i != l && i != d) {
						if (send(i, &p, p.size, 0) == -1)
							perror("send");
					}
				}
			}
		}

		/* Check the client connections */
		for (i = 0; i <= fdmax; i++) {
			if (i != l && i != d && FD_ISSET(i, &rfds)) {
				ret = recv(i, buf, sizeof(buf), 0);
				if (ret == -1) {
					perror("recv");
				} else if (ret == 0) {
					/* Client closed connection */
					printf("client closed connection\n");
					if (close(i) == -1)
						exit_failure("close");
					FD_CLR(i, &master);
				} else {
					/* If data is read from client, send it to /dev/dect */
					printf("client: %d\n", ret);
					if (write(d, buf, ret) == -1)
						perror("write dect");
				}
			}
		}
	}
	return 0;
}


