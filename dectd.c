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
int s, c;
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

static send_client(uint8_t status) {
	
	packet *p;

	if ((p = (packet *)malloc(sizeof(packet))) == NULL)
		exit_failure("malloc");

	p->type = RESPONSE;
	p->arg = status;
	printf("send_client\n");
	if (send(c, p, sizeof(packet), 0) == -1)
		exit_failure("send");
	
	free(p);

}



int register_handsets_start(void)
{
  
  unsigned char *tempPtr = NULL;  
  printf("register_handsets_start\n");

  tempPtr = (unsigned char*) malloc(4);
  if (tempPtr == NULL)
    {
      printf("No more memory!!!");
      return;
    }


  *(tempPtr+0) = 0; // Length MSB
  *(tempPtr+1) = 3; // Length LSB
  *(tempPtr+2) = (unsigned char) ((API_FP_MM_SET_REGISTRATION_MODE_REQ&0xff00)>>8); // Primitive MSB
  *(tempPtr+3) = (unsigned char) (API_FP_MM_SET_REGISTRATION_MODE_REQ&0x00ff);      // Primitive LSB
  *(tempPtr+4) = 1; //disable registration

  write_frame(tempPtr);

  return 0;
}



void dectDrvWrite(void *data, int size)
{   
    int i;
    unsigned char* cdata = (unsigned char*)data;
    printf("\n[WDECT][%04d] - ",size);

    for (i=0 ; i<size ; i++) {
        printf("%02x ",cdata[i]);
    }
    printf("\n");

   if (-1 == write(s, data, size))
   {
      perror("write to API failed");
      return;
   }

   return;
}



int write_frame(unsigned char *fr)
{
  unsigned short fr_size = 256*fr[0] + fr[1] + 1;
      
  dectDrvWrite((void *)(fr+2), fr_size);

  return 0;
}


int register_handsets_stop(void)
{

  unsigned char *tempPtr = NULL;
  
  printf("register_handsets_stop\n");

  tempPtr = (unsigned char*) malloc(5);
  if (tempPtr == NULL)
    {
      printf("No more memory!!!");
      return;
    }

  *(tempPtr+0) = 0; // Length MSB
  *(tempPtr+1) = 3; // Length LSB
  *(tempPtr+2) = (unsigned char) ((API_FP_MM_SET_REGISTRATION_MODE_REQ&0xff00)>>8); // Primitive MSB
  *(tempPtr+3) = (unsigned char) (API_FP_MM_SET_REGISTRATION_MODE_REQ&0x00ff);      // Primitive LSB
  *(tempPtr+4) = 0; //disable registration

  write_frame(tempPtr);

  return 0;
}



static registration(packet *p) {

	switch (p->arg) {
		
	case ENABLED:
		printf("enable registration\n");
		register_handsets_start();
		send_client(OK);
		break;

	case DISABLED:
		printf("disable registration\n");
		register_handsets_stop();
		send_client(OK);
		break;

	default:
		printf("unknown arg\n");
		send_client(ERROR);
		break;


	}
}


void handle_client_packet(packet *p) {

	switch (p->type) {

	case GET_STATUS:
		printf("GET_STATUS, \t%d\n", p->arg);
		break;

	case REGISTRATION:
		printf("REGISTRATION, \t%d\n", p->arg);
		registration(p);
		break;

	case PING_HSET:
		printf("PING_HSET, \t%d\n", p->arg);
		break;

	case DELETE_HSET:
		printf("DELETE_HSET, \t%d\n", p->arg);
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
			if ((c = accept(l, (struct sockaddr *) &remote_addr, &remote_addr_size)) == -1)
				exit_failure("accept");
			else {
				printf("accepted connection: %d\n", c);
				/* Add new connection to rfds */
				FD_SET(c, &master);
				fdmax = MAX(fdmax, c);
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

		/* Check the client connection */
		if (FD_ISSET(c, &rfds)) {
			ret = recv(c, buf, sizeof(buf), 0);
			if (ret == -1) {
				perror("recv");
			} else if (ret == 0) {
				/* Client closed connection */
				printf("client closed connection\n");
				if (close(c) == -1)
					exit_failure("close");
				FD_CLR(c, &master);
			} else {
				handle_client_packet(buf);
				/* printf("client: %d\n", ret); */
				/* if (write(d, buf, ret) == -1) */
				/* 	perror("write dect"); */
				/* 	} */
				/* } */
			}
		}
	}
	return 0;
}


