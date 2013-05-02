#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <ApiFpProject.h>
#include <dectUtils.h>
#include <stdint.h>
#include <dectNvsCtl.h>

#include "dectd.h"


#define MAX_LINE 16384

/* Globals */
struct bufferevent *dect;
struct event_base *base;
struct info *dect_info;


struct info {
	const char *name;
	struct packet *pkt;
};


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


static send_client(struct bufferevent *bev, uint8_t status) {
	
	packet_t *p;

	if ((p = (packet_t *)malloc(sizeof(packet_t))) == NULL)
		perror("failure");

	p->type = RESPONSE;

	printf("send_client\n");
	bufferevent_write(bev, p, sizeof(packet_t));

	free(p);
	printf("send_client 1\n");
}


int register_handsets_start(void)
{
  
  unsigned char *tempPtr = NULL;  
  printf("register_handsets_start\n");

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
  *(tempPtr+4) = 1; //disable registration

  write_frame(tempPtr);

  return 0;
}


void dectDrvWrite(void *data, int size)
{   
    int i;
    unsigned char* cdata = (unsigned char*)data;
    printf("[WDECT][%04d] - ",size);

    for (i=0 ; i<size ; i++) {
        printf("%02x ",cdata[i]);
    }
    printf("\n");

   if (-1 == bufferevent_write(dect, data, size))
   {
      perror("write to API failed");
      return;
   }

   return;
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



int write_frame(unsigned char *fr)
{
  unsigned short fr_size = 256*fr[0] + fr[1];
      
  dectDrvWrite((void *)(fr+2), fr_size);

  return 0;
}


void reg_timer(void) {

	printf("reg timer expired\n");
}


void handle_dect_packet(unsigned char *buf) {

  RosPrimitiveType primitive;

  /* /\* Dump the packet. *\/ */
  /* printf("[RDECT][%04d] - ", buf->size); */
  /* for (i=0 ; i<n ; i++) */
  /* 	  printf("%02x ", buf->data[i]); */
  /* printf("\n"); */
  
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




static registration(struct bufferevent *bev, packet_t *p) {

	struct timeval tv = {5,0};
	struct event *timeout;
	

	printf("enable registration\n");
	register_handsets_start();
	timeout = event_new(base, -1, EV_TIMEOUT, (void *)register_handsets_stop, NULL);
	event_add(timeout, &tv);
	//send_client(bev, OK);
}


static get_status(struct bufferevent *bev) {

	
}


void handle_client_packet(struct bufferevent *bev, packet_t *p) {

	switch (p->type) {

	case GET_STATUS:
		printf("GET_STATUS\n");
		get_status(bev);
		break;

	case REGISTRATION:
		printf("REGISTRATION\n");
		registration(bev, p);
		break;

	case PING_HSET:
		printf("PING_HSET\n");
		break;

	case DELETE_HSET:
		printf("DELETE_HSET\n");
		break;

	default:
		printf("unknown packet\n");
		break;

	}	  

}



void packet_read(struct bufferevent *bev, void *ctx) {
	
	uint8_t *buf;
	int n, i, read;
	struct info *info = ctx;
	struct evbuffer *input = bufferevent_get_input(bev);
	
	printf("packet read, %s\n", info->name);

	/* Do we have a packet header? */
	if (!info->pkt->size && (evbuffer_get_length(input) >= sizeof(packet_header_t))) {
		
		n = evbuffer_remove(input, info->pkt, sizeof(packet_header_t));

		if (info->pkt->size > sizeof(packet_t)) {
			printf("need to allocate more space for packet\n");
			if((info->pkt = realloc(info->pkt, info->pkt->size)) == NULL)
				exit_failure("realloc");

		}
		
	}


	/* Is there an entire packet in the buffer? */
	if (evbuffer_get_length(input) >= (info->pkt->size - sizeof(packet_header_t))) {

		n = evbuffer_remove(input, info->pkt->data, info->pkt->size - sizeof(packet_header_t));

		switch (info->pkt->type) {
		case DECT_PACKET:
			handle_dect_packet(info->pkt->data);
			break;
		default:
			handle_client_packet(bev, info->pkt);
			break;
		}

		
		/* Reset packet struct */
		free(info->pkt);
		info->pkt = calloc(sizeof(packet_t), 1);

	}
}



void errorcb(struct bufferevent *bev, short error, void *ctx) {
	
	if (error & BEV_EVENT_EOF) {
		printf("client connection closed\n");
	} else if (error & BEV_EVENT_ERROR) {
	} else if (error & BEV_EVENT_TIMEOUT) {
	}

	bufferevent_free(bev);
}



void do_accept(evutil_socket_t listener, short event, void *arg) {

	struct event_base *base = arg;
	struct sockaddr_storage ss;
	socklen_t slen = sizeof(ss);
	int fd = accept(listener, (struct sockaddr*)&ss, &slen);


	/* Setup client bufferevent */
	struct info *client_info = calloc(sizeof(struct info), 1);
	client_info->name = "client";
	client_info->pkt = calloc(sizeof(struct packet), 1);


	printf("accepted client connection\n");
	/* Setup dect bufferevent */


	if (fd < 0) {
		perror("accept");
	} else if (fd > FD_SETSIZE) {
		close(fd);
	} else {
		struct bufferevent *bev;
		evutil_make_socket_nonblocking(fd);
		bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
		bufferevent_setcb(bev, packet_read, NULL, errorcb, client_info);
		bufferevent_setwatermark(bev, EV_READ, 0, MAX_LINE);
		bufferevent_enable(bev, EV_READ | EV_WRITE);
	}
}


void dect_event(struct bufferevent *bev, short events, void *ptr) {

	if (events & BEV_EVENT_CONNECTED) {
		printf("connected to dectproxy\n");
	} else if (events & BEV_EVENT_ERROR) {
		printf("error\n");
	}
}


struct bufferevent *create_connection(int address, int port) {

	struct bufferevent *be;
	struct sockaddr_in proxy;
	
	

	/* Open connection to dectproxy. */
	memset(&proxy, 0, sizeof(proxy));
	proxy.sin_family = AF_INET;
	proxy.sin_addr.s_addr = htons(address);
	proxy.sin_port = htons(port);

	/* Setup dect bufferevent */
	dect_info = calloc(sizeof(struct info), 1);
	dect_info->name = "dect";
	dect_info->pkt = calloc(sizeof(struct packet), 1);

	be = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
	bufferevent_setcb(be, packet_read, NULL, dect_event, dect_info);

	if (bufferevent_socket_connect(be, (struct sockaddr *)&proxy, sizeof(proxy)) < 0) {
		bufferevent_free(be);
		return NULL;
	}

	bufferevent_enable(be, EV_READ | EV_WRITE);

	return be;
}


void run(void) {

	evutil_socket_t listener;
	struct sockaddr_in sin;
	struct event *listener_event;


	if ((base = event_base_new()) == NULL)
		exit_failure("event_base_new");


	/* Connect to dectproxy */
	if ((dect = create_connection(INADDR_ANY, 7777)) == NULL)
		exit_failure("create_connection");


	/* Open listening socket. */
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = 0;
	sin.sin_port = htons(40713);

	listener = socket(AF_INET, SOCK_STREAM, 0);
	evutil_make_socket_nonblocking(listener);

	if (bind(listener, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
		perror("bind");
		return;
	}

	if (listen(listener, 16) < 0) {
		perror("listen");
		return;
	}

	listener_event = event_new(base, listener, EV_READ | EV_PERSIST, do_accept, (void *)base);
	event_add(listener_event, NULL);

	event_base_dispatch(base);
}


int main(void) {

	setvbuf(stdout, NULL, _IONBF, 0);

	run();
	return 0;
}



	
