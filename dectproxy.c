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

#define MAX_MAIL_SIZE 4098
#define MAX_LISTENERS 10

#define MAX(a,b) ((a) > (b) ? (a) : (b))


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
				printf("accepted connection\n");
				/* Add new connection to rfds */
				FD_SET(n, &master);
				fdmax = MAX(fdmax, n);
			}
		}

		
		/* Check /dev/dect for data */
		if (FD_ISSET(d, &rfds)) {
			ret = read(d, buf, sizeof(buf));
			printf("dect %d\n", ret);
			if (ret == -1) {
				exit_failure("read");
			} else {
				for (i = 0; i <= fdmax; i++) {
					/* If data is read from /dev/dect, send it to all clients */
					if (i != l && i != d) {
						hdr[0] = (uint8_t)((ret & 0xff00) >> 8); // MSB
						hdr[1] = (uint8_t)(ret & 0x00ff);        // LSB
						if (send(i, hdr, 2, 0) == -1)
							perror("send");

						/* TODO, should sendall here */
						if (send(i, buf, ret, 0) == -1)
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
					//exit_failure("recv");
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
						exit_failure("write");
				}
			}
		}
	}
	return 0;
}


