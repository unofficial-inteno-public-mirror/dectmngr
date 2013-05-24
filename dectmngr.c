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
#include <stdint.h>

#include <ApiFpProject.h>
#include <dectUtils.h>
#include <dectNvsCtl.h>
#include <json/json.h>

#include "dect.h"


/* Globals */
struct bufferevent *dect;
struct event_base *base;
struct info *dect_info;
struct status_packet status;


void handle_dect_packet(unsigned char *buf);
void packet_read(struct bufferevent *bev, void *ctx);
void handle_client_packet(struct bufferevent *bev, client_packet *p);
void ApiFreeInfoElement(ApiInfoElementType **IeBlockPtr);
void ApiBuildInfoElement(ApiInfoElementType **IeBlockPtr,
                         rsuint16 *IeBlockLengthPtr,
                         ApiIeType Ie,
                         rsuint8 IeLength,
                         rsuint8 *IeData);


static void exit_failure(const char *format, ...)
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
	bufferevent_write(bev, p, sizeof(packet_t));
	free(p);
}


static void _write_dect(void *data, int size) {

    int i;
    unsigned char* cdata = (unsigned char*)data;
    printf("[WDECT][%04d] - ",size);

    for (i=0 ; i<size ; i++) {
        printf("%02x ",cdata[i]);
    }
    printf("\n");

    if (-1 == bufferevent_write(dect, data, size)) {
	    perror("write to API failed");
	    return;
    }

   return;
}


static int write_dect2(int header) {
	
	unsigned char o_buf[2];

	*(o_buf + 0) = (unsigned char) ((header&0xff00)>>8); // Primitive MSB
	*(o_buf + 1) = (unsigned char) (header&0x00ff);      // Primitive LSB

	_write_dect(o_buf, 2);
}


static int write_dect3(int header, int arg) {
	
	unsigned char o_buf[3];

	*(o_buf + 0) = (unsigned char) ((header&0xff00)>>8); // Primitive MSB
	*(o_buf + 1) = (unsigned char) (header&0x00ff);      // Primitive LSB
	*(o_buf + 2) = arg; //disable registration

	_write_dect(o_buf, 3);
}


static int write_dect5(int header, int arg) {
	
	unsigned char o_buf[5];
	
	*(o_buf + 0) = (unsigned char) ((header&0xff00)>>8); // Primitive MSB
	*(o_buf + 1) = (unsigned char) (header&0x00ff);      // Primitive LSB
	*(o_buf + 2) = arg; //disable registration
        *(o_buf + 3) = 0;
	*(o_buf + 4) = 0;

	_write_dect(o_buf, 3);
}


static void list_handsets(void) {
  
	printf("list_handsets\n");
	write_dect2(API_FP_MM_GET_REGISTRATION_COUNT_REQ);
}


void ApiFreeInfoElement(ApiInfoElementType **IeBlockPtr) {

	free((void*)*IeBlockPtr);
	*IeBlockPtr = NULL;
}


void ApiBuildInfoElement(ApiInfoElementType **IeBlockPtr,
                         rsuint16 *IeBlockLengthPtr,
                         ApiIeType Ie,
                         rsuint8 IeLength,
                         rsuint8 *IeData) {

	rsuint16 newLength = *IeBlockLengthPtr + RSOFFSETOF(ApiInfoElementType, IeData) + IeLength;

	/* Ie is in little endian inside the infoElement list while all arguments to function are in bigEndian */
	rsuint16 targetIe = Ie;

	/* Allocate / reallocate a heap block to store (append) the info elemte in. */
	ApiInfoElementType *p = malloc(newLength);
	if (p == NULL) {
		// We failed to get e new block.
		// We free the old and return with *IeBlockPtr == NULL.
		ApiFreeInfoElement(IeBlockPtr);
		*IeBlockLengthPtr = 0;
	} else {
		// Update *IeBlockPointer with the address of the new block
		//     *IeBlockPtr = p;
		if( *IeBlockPtr != NULL ) {

			/* Copy over existing block data */
			memcpy( (rsuint8*)p, (rsuint8*)*IeBlockPtr, *IeBlockLengthPtr);
      
			/* Free existing block memory */
			ApiFreeInfoElement(IeBlockPtr);
		}
    
		/* Assign newly allocated block to old pointer */
		*IeBlockPtr = p;

		// Append the new info element to the allocated block
		p = (ApiInfoElementType*)(((rsuint8*)p) + *IeBlockLengthPtr); // p now points to the first byte of the new info element
    
		p->Ie = targetIe;

		p->IeLength = IeLength;
		memcpy (p->IeData, IeData, IeLength);
		// Update *IeBlockLengthPtr with the new block length
		*IeBlockLengthPtr = newLength;
	}
}


static void ping_handset_start(int handset) {

	ApiCallingNameType * pCallingNameIe    = NULL;
	ApiInfoElementType * pingIeBlockPtr    = NULL;
	ApiFpCcSetupReqType * pingMailPtr      = NULL;
	unsigned short pingIeBlockLength       = 0;
	char callingName[]                     = "HANDSET LOCATOR";
	int callIdx;

	/************************************************
	 * create API_IE_CALLING_PARTY_NAME infoElement *
	 ************************************************/

	pCallingNameIe = malloc( (sizeof(ApiCallingNameType) - 1) + (strlen(callingName)+1) );

	if(pCallingNameIe != NULL ) {
			pCallingNameIe->UsedAlphabet     = AUA_DECT;
			pCallingNameIe->PresentationInd  = API_PRESENTATION_HANSET_LOCATOR;
			pCallingNameIe->ScreeningInd     = API_NETWORK_PROVIDED;
			pCallingNameIe->NameLength       = strlen(callingName);
			memcpy( &(pCallingNameIe->Name[0]), callingName, (strlen(callingName)+1) );

			/* Add to infoElement block */
			ApiBuildInfoElement( &pingIeBlockPtr,
					     &pingIeBlockLength,
					     API_IE_CALLING_PARTY_NAME,
					     ((sizeof(ApiCallingNameType) - 1) + (strlen(callingName)+1) ),
					     (unsigned char*)pCallingNameIe);

			/* free infoElement */
			free(pCallingNameIe);

			if(pingIeBlockPtr == NULL) {
				printf("dectCallMgrSetupPingingCall: ApiBuildInfoElement FAILED for API_IE_CALLING_PARTY_NAME!!\n");
				return;
			}
	} else {
		printf("dectCallMgrSetupPingingCall: malloc FAILED for API_IE_CALLING_PARTY_NAME!!\n");
		return;
	}

	/*****************************************************
	 * create API_FP_CC_SETUP_REQ mail *
	 *****************************************************/
	if(pingIeBlockLength > 0) {
		/* Allocate memory for mail */
		pingMailPtr = (ApiFpCcSetupReqType *) malloc( (sizeof(ApiFpCcSetupReqType)-1) + pingIeBlockLength );
		if (pingMailPtr != NULL) {

			/* Fillout mail contents */
			((ApiFpCcSetupReqType *) pingMailPtr)->Primitive    = API_FP_CC_SETUP_REQ;
			((ApiFpCcSetupReqType *) pingMailPtr)->CallReference.HandsetId = handset;
			((ApiFpCcSetupReqType *) pingMailPtr)->BasicService = API_BASIC_SPEECH;
			((ApiFpCcSetupReqType *) pingMailPtr)->CallClass    = API_CC_NORMAL;
			((ApiFpCcSetupReqType *) pingMailPtr)->SourceId     = 0; /* 0 is the base station id */
			((ApiFpCcSetupReqType *) pingMailPtr)->Signal       = API_CC_SIGNAL_ALERT_ON_PATTERN_2;

			/* Copy over infoElements */
			memcpy( &(((ApiFpCcSetupReqType *) pingMailPtr)->InfoElement[0]), pingIeBlockPtr, pingIeBlockLength );
			ApiFreeInfoElement( &pingIeBlockPtr );

			((ApiFpCcSetupReqType *) pingMailPtr)->InfoElementLength = pingIeBlockLength;
		} else {
			printf("dectCallMgrSetupPingingCall: No more memory available for API_FP_CC_SETUP_REQ!!!\n");
			return;
		}
	} else {
		printf("dectCallMgrSetupPingingCall: zero pingIeBlockLength!!!\n");
		ApiFreeInfoElement( &pingIeBlockPtr );
		return;
	}

	/* Send the mail */
	printf("OUTPUT: API_FP_CC_SETUP_REQ (ping)\n");
	_write_dect( (unsigned char *)pingMailPtr, (sizeof(ApiFpCcSetupReqType)-1) + pingIeBlockLength );
}


static void get_handset_ipui(int handset) {  

	write_dect3(API_FP_MM_GET_HANDSET_IPUI_REQ, handset);
}


static void registration_count_cfm(unsigned char *mail) {

	int i, handset;
	
	printf("INPUT: API_FP_MM_GET_REGISTRATION_COUNT_CFM\n");
	if (((ApiFpMmGetRegistrationCountCfmType*) mail)->Status == RSS_SUCCESS) {

		printf("Max Number of Handset allowed: %d\n", ((ApiFpMmGetRegistrationCountCfmType*) mail)->MaxNoHandsets);
		 
		for (i = 0 ; i < (((ApiFpMmGetRegistrationCountCfmType*) mail)->HandsetIdLength ) ; i++ ) {
			 
			handset = ((ApiFpMmGetRegistrationCountCfmType*) mail)->HandsetId[i];
			status.handset[handset - 1].registered = TRUE;
		}
		 
		/* Get the ipui of the first handset. For some damn
		   reason we can't to all of them at once. */
		handset = ((ApiFpMmGetRegistrationCountCfmType*) mail)->HandsetId[0];
		if (handset > 0)
			get_handset_ipui(handset);
	}
}


static void ping_handset_stop(struct event *ev, short error, void *arg) {

	int *handset = arg;

	status.handset[(*handset) - 1].pinging = FALSE;

	printf("API_FP_CC_RELEASE_REQ\n");
	write_dect5(API_FP_CC_RELEASE_REQ, *handset);
}


static void register_handsets_start(void) {

	write_dect3(API_FP_MM_SET_REGISTRATION_MODE_REQ, 1);
}


static void register_handsets_stop(void) {

	status.reg_mode = DISABLED;
	write_dect3(API_FP_MM_SET_REGISTRATION_MODE_REQ, 0);
}


static void delete_hset(int handset) {  

	printf("delete handset: %d\n", handset);
	write_dect3(API_FP_MM_DELETE_REGISTRATION_REQ, handset);
}


static void present_ind(unsigned char *mail) {  

	int handset;
	
	handset = ((ApiFpMmHandsetPresentIndType*) mail)->HandsetId;
	printf("INPUT: API_FP_MM_HANDSET_PRESENT_IND from handset (%d)\n", handset);

	status.handset[handset - 1].present = TRUE;
}


static void delete_registration_cfm(unsigned char *mail) {  

	int handset;

	handset = ((ApiFpMmDeleteRegistrationCfmType*) mail)->HandsetId;
	
	printf("deleted handset: %d\n", handset);
	status.handset[handset - 1].registered = FALSE;
	status.handset[handset - 1].present = FALSE;
}


static void connect_ind(unsigned char *buf) {

	ApiHandsetIdType handset;
	struct brcm_pvt *p;
	struct brcm_subchannel *sub;
	unsigned char o_buf[5];

	handset = ((ApiFpCcConnectCfmType*) buf)->CallReference.HandsetId;
	
	if (status.handset[(handset) - 1].pinging == TRUE)
		status.handset[(handset) - 1].pinging = FALSE;
}


static void handset_ipui_cfm(unsigned char *mail) {  

	int handset, i;

	printf("INPUT: API_FP_MM_GET_HANDSET_IPUI_CFM\n");

	handset = ((ApiFpMmGetHandsetIpuiCfmType *) mail)->HandsetId;

	for (i = 0; i < 5; i++)
		status.handset[handset - 1].ipui[i] = ((ApiFpMmGetHandsetIpuiCfmType *) mail)->IPUI[i];
	 
	/* Check if the next handset is also registeried */
	if (status.handset[handset].registered == TRUE)
		get_handset_ipui(handset + 1);
}


static void registration(struct bufferevent *bev, client_packet *p) {

	struct timeval tv = {60,0};
	struct event *timeout;
	
	printf("enable registration\n");
	register_handsets_start();
	timeout = event_new(base, -1, EV_TIMEOUT, (void *)register_handsets_stop, NULL);
	event_add(timeout, &tv);
}


static void ping_handset(int handset) {

	struct timeval tv = {20,0};
	struct event *timeout;
	
	printf("ping_handset\n");
	status.handset[handset - 1].pinging = TRUE;

	int *hst = malloc(sizeof(int));
	*hst = handset;

	ping_handset_start(handset);
	timeout = event_new(base, -1, EV_TIMEOUT, (void *)ping_handset_stop, hst);
	event_add(timeout, &tv);
}


static void get_status(struct bufferevent *bev) {
	
	bufferevent_write(bev, &status, sizeof(status));
}


static void errorcb(struct bufferevent *bev, short error, void *ctx) {
	
	if (error & BEV_EVENT_EOF) {
		printf("client connection closed\n");
	} else if (error & BEV_EVENT_ERROR) {
	} else if (error & BEV_EVENT_TIMEOUT) {
	}

	bufferevent_free(bev);
}


static void do_accept(evutil_socket_t listener, short event, void *arg) {

	struct sockaddr_storage ss;
	struct bufferevent *bev;
	struct event_base *base = arg;
	socklen_t slen = sizeof(ss);
	int fd = accept(listener, (struct sockaddr*)&ss, &slen);

	if (fd < 0) {
		perror("accept");
	} else if (fd > FD_SETSIZE) {
		close(fd);
	} else {
		printf("accepted client connection\n");

		/* Setup client bufferevent */
		struct info *client_info = calloc(sizeof(struct info), 1);
		client_info->name = "client";
		client_info->pkt = calloc(sizeof(struct packet), 1);

		evutil_make_socket_nonblocking(fd);
		bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
		bufferevent_setcb(bev, packet_read, NULL, errorcb, client_info);
		bufferevent_setwatermark(bev, EV_READ, 0, MAX_LINE);
		bufferevent_enable(bev, EV_READ | EV_WRITE);
	}
}


static void dect_event(struct bufferevent *bev, short events, void *ptr) {

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


static void init_status(void) {

	memset(&status, 0, sizeof(status));
	status.size = sizeof(status);
	status.type = RESPONSE;
	status.reg_mode = DISABLED;
}


void packet_read(struct bufferevent *bev, void *ctx) {
	
	uint8_t *buf;
	int n, i, read;
	struct info *info = ctx;
	struct evbuffer *input = bufferevent_get_input(bev);
	
	while (1) {
		/* Do we have a packet header? */
		if (!info->pkt->size && (evbuffer_get_length(input) >= sizeof(packet_header_t))) {
		
			n = evbuffer_remove(input, info->pkt, sizeof(packet_header_t));
			if (info->pkt->size > sizeof(packet_t)) {
				if((info->pkt = realloc(info->pkt, info->pkt->size)) == NULL)
					exit_failure("realloc");
			}
		
		} else 
			return;

		/* Is there an entire packet in the buffer? */
		if (evbuffer_get_length(input) >= (info->pkt->size - sizeof(packet_header_t))) {

			n = evbuffer_remove(input, info->pkt->data, info->pkt->size - sizeof(packet_header_t));

			switch (info->pkt->type) {
			case DECT_PACKET:
				/* Dump the packet. */
				printf("[RDECT][%04d] - ", info->pkt->size - sizeof(packet_header_t));
				for (i=0 ; i<info->pkt->size - sizeof(packet_header_t); i++)
					printf("%02x ", info->pkt->data[i]);
				printf("\n");

				handle_dect_packet(info->pkt->data);
				break;
			default:
				handle_client_packet(bev, (client_packet *)info->pkt);
				break;
			}

			/* Reset packet struct */
			free(info->pkt);
			info->pkt = calloc(sizeof(packet_t), 1);

		} else
			return;
	}
}


void handle_dect_packet(unsigned char *buf) {

	int i;
	RosPrimitiveType primitive = ((recDataType *) buf)->PrimitiveIdentifier;
	struct packet *p = (struct packet *)buf;

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

	case API_FP_FWU_DEVICE_NOTIFY_IND:
		printf("API_FP_FWU_DEVICE_NOTIFY_IND\n");
		break;

	case API_FP_CC_CONNECT_CFM:
		printf("API_FP_CC_CONNECT_CFM\n");
		break;

	case API_FP_CC_CONNECT_IND:
		printf("API_FP_CC_CONNECT_IND\n");
		connect_ind(buf);
		break;

	case API_FP_CC_INFO_IND:
		printf("API_FP_CC_INFO_IND\n");
		break;

	case API_FP_CC_ALERT_IND:
		printf("API_FP_CC_ALERT_IND\n");
		break;

	case API_FP_LINUX_NVS_UPDATE_IND:
		printf("API_FP_LINUX_NVS_UPDATE_IND\n");
		break;

	case API_FP_MM_HANDSET_PRESENT_IND:
		printf("API_FP_MM_HANDSET_PRESENT_IND\n");
		present_ind(buf);
		break;

	case API_FP_MM_SET_REGISTRATION_MODE_CFM:
		printf("API_FP_MM_SET_REGISTRATION_MODE_CFM\n");
		break;

	case API_FP_MM_GET_HANDSET_IPUI_CFM:
		printf("API_FP_MM_GET_HANDSET_IPUI_CFM\n");
		handset_ipui_cfm(buf);
		break;

	case API_FP_MM_GET_REGISTRATION_COUNT_CFM:
		printf("API_FP_MM_GET_REGISTRATION_COUNT_CFM\n");
		registration_count_cfm(buf);
		break;

	case API_FP_MM_DELETE_REGISTRATION_CFM:
		printf("API_FP_MM_DELETE_REGISTRATION_CFM\n");
		delete_registration_cfm(buf);
		break;

	case API_FP_MM_REGISTRATION_COMPLETE_IND:
		printf("API_FP_MM_REGISTRATION_COMPLETE_IND\n");
		list_handsets();
		break;

	case API_FP_LINUX_INIT_CFM:
		printf("API_FP_LINUX_INIT_CFM\n");
		list_handsets();
		break;

	case API_FP_GET_EEPROM_CFM:
		printf("API_FP_GET_EEPROM_CFM\n");
		break;

	default:
		printf("Unknown packet\n");
		break;
	}
}


void handle_client_packet(struct bufferevent *bev, client_packet *p) {

	switch (p->type) {

	case GET_STATUS:
		printf("GET_STATUS\n");
		get_status(bev);
		break;

	case REGISTRATION:
		printf("REGISTRATION\n");
		status.reg_mode = ENABLED;
		registration(bev, p);
		break;

	case PING_HSET:
		printf("PING_HSET %d\n", p->data);
		ping_handset(p->data);
		break;

	case DELETE_HSET:
		printf("DELETE_HSET %d\n", p->data);
		delete_hset(p->data);
		break;

	default:
		printf("unknown packet\n");
		break;
	}
}






static void run(void) {

	evutil_socket_t listener;
	struct sockaddr_in sin;
	struct event *listener_event;

	/* Init status */
	init_status();

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


	
