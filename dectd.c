#define DECT_MAX_HANDSET 6

#include <dectctl.h>
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
#include <ApiFpProject.h>
#include <dectUtils.h>
#include <stdint.h>
#include <dectNvsCtl.h>


#include "dectd.h"



/* globals */
int s;
struct sigaction act;
dect_state state;





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


static update_state(void) {

	if (state.reg_state == DISABLED) {
		state.reg_state = ENABLED;
		printf("ENABLED\n");
	} else {
		state.reg_state = DISABLED;
		printf("DISABLED\n");
	}
}


void handle_dect_packet(unsigned char *buf) {

  RosPrimitiveType primitive;
  
  primitive = ((recDataType *) buf)->PrimitiveIdentifier;
  
  switch (primitive) {
	
  case API_FP_CC_SETUP_IND:
    printf("API_FP_CC_SETUP_IND\n");
    break;

  case API_FP_CC_REJECT_IND:
    printf("API_FP_CC_REJECT_IND\n");
    break;

  case API_FP_CC_RELEASE_IND:
    printf("API_FP_CC_RELEASE_IND\n");
    break;

  case API_FP_CC_CONNECT_CFM:
    printf("API_FP_CC_CONNECT_CFM\n");
    break;

  case API_FP_CC_INFO_IND:
    printf("API_FP_CC_INFO_IND\n");
    break;

  case API_FP_LINUX_NVS_UPDATE_IND:
    printf("API_FP_LINUX_NVS_UPDATE_IND\n");
    break;

  case API_FP_MM_HANDSET_PRESENT_IND:
    printf("API_FP_MM_HANDSET_PRESENT_IND\n");
    break;

  case API_FP_MM_SET_REGISTRATION_MODE_CFM:
    printf("API_FP_MM_SET_REGISTRATION_MODE_CFM\n");
    update_state();
    break;

  case API_FP_MM_GET_HANDSET_IPUI_CFM:
    printf("API_FP_MM_GET_HANDSET_IPUI_CFM\n");
    break;

  case API_FP_MM_GET_REGISTRATION_COUNT_CFM:
    printf("API_FP_MM_GET_REGISTRATION_COUNT_CFM\n");
    break;

  case API_FP_MM_REGISTRATION_COMPLETE_IND:
    printf("API_FP_MM_REGISTRATION_COMPLETE_IND\n");
    break;

  case API_FP_LINUX_INIT_CFM:
    printf("API_FP_LINUX_INIT_CFM\n");
    break;

  case API_FP_GET_EEPROM_CFM:
    printf("API_FP_GET_EEPROM_CFM\n");
    break;

  default:
    printf("Unknown packet\n");
    break;
  }
  

}



void handle_client_packet(client_packet *p) {

	switch (p->type) {

	case GET_STATUS:
		printf("GET_STATUS, %d\n", p->size);
		break;

	case REGISTRATION:
		printf("REGISTRATION, %d\n", p->size);
		break;

	case PING_HSET:
		printf("PING_HSET, %d\n", p->size);
		break;

	case DELETE_HSET:
		printf("DELETE_HSET, %d\n", p->size);
		break;

	default:
		printf("unknown packet\n");
		break;

	}	  

}



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


	/* Connect to dectproxy */
	memset(&remote_addr, 0, sizeof(remote_addr));
	remote_addr.sin_family = AF_INET;
	remote_addr.sin_addr.s_addr = INADDR_ANY;
	remote_addr.sin_port = 7777;
	
	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		exit_failure("socket");

	if (connect(s, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr)) < 0)
	    exit_failure("error connecting to dectproxy");
	
	fdmax = s;
	FD_ZERO(&master);
	FD_SET(s, &master);
	
	/* Listen on port 7778 */
	memset(&my_addr, 0, sizeof(my_addr));
	my_addr.sin_family = AF_INET;
	my_addr.sin_addr.s_addr = INADDR_ANY;
	my_addr.sin_port = htons(7778);


	if ((l = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		exit_failure("listening socket");
	
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
			remote_addr_size = sizeof(remote_addr);
			if ((n = accept(l, (struct sockaddr *) &remote_addr, &remote_addr_size)) == -1)
				exit_failure("accept");
			else {
				printf("accepted connection: %d\n", n);
				/* Add new connection to rfds */
				FD_SET(n, &master);
				fdmax = MAX(fdmax, n);
			}
		}

		
		/* Check for data from dectproxy */
		if (FD_ISSET(s, &rfds)) {
			
			len = read(s, buf, 2);
			
			if (len == 2) {
				pkt_len = buf[0] << 8; // MSB
				pkt_len |= buf[1];     // LSB

				printf("packet len: %d\n", pkt_len);
				len = read(s, buf, pkt_len);
			} else {
				printf("wrong header length: %d\n", len);
			}

			if (len > 0) {

				/* Debug printout */
				printf("\n[RDECT][%04d] - ", len);
				for (i = 0; i < len; i++)
					printf("%02x ", buf[i]);
				printf("\n");

				handle_dect_packet(buf);
			}
		}

		/* Check the client connections */
		for (i = 0; i <= fdmax; i++) {
			if (i != l && i != s && FD_ISSET(i, &rfds)) {
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
					handle_client_packet(buf);
					/* printf("client: %d\n", ret); */
					/* if (write(d, buf, ret) == -1) */
					/* 	perror("write dect"); */
				}
			}
		}
	}
	return 0;
}


