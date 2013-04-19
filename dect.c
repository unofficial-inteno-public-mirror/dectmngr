#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "dectd.h"

int main(void)
{
	int d, fdmax, res, l, n, ret, i, len, pkt_len;
	fd_set rfds, master;
	unsigned char buf[MAX_MAIL_SIZE];
	struct sockaddr_in my_addr, remote_addr;
	socklen_t remote_addr_size;
	uint8_t hdr[2];
	int client[MAX_LISTENERS];

	
	/* Setup signal handler. When writing data to a
	   client that closed the connection we get a
	   SIGPIPE. We need to catch it to avoid beeing killed */
	memset(&act, 0, sizeof(act));
	act.sa_sigaction = sighandler;
	act.sa_flags = SA_SIGINFO;
	sigaction(SIGPIPE, &act, NULL);


	/* Connect to dectd */
	memset(&remote_addr, 0, sizeof(remote_addr));
	remote_addr.sin_family = AF_INET;
	remote_addr.sin_addr.s_addr = INADDR_ANY;
	remote_addr.sin_port = 7778;
	
	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		exit_failure("socket");

	if (connect(s, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr)) < 0)
	    exit_failure("error connecting to dectproxy");
}
