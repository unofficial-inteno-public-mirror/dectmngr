
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <Api/CodecList/ApiCodecList.h>
#include <Api/FpCc/ApiFpCc.h>
#include <Api/FpMm/ApiFpMm.h>
#include <Api/FpNoEmission/ApiFpNoEmission.h>
#include <Api/GenEveNot/ApiGenEveNot.h>
#include <Api/Las/ApiLas.h>
#include <Api/Linux/ApiLinux.h>
#include <Api/FpUle/ApiFpUle.h>

#include "dect.h"
#include "fp_mm.h"



/* Globals */
struct bufferevent *dect;
struct event_base *base;
struct info *dect_info;



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



static void dect_event(struct bufferevent *bev, short events, void *ptr) {

        if (events & BEV_EVENT_CONNECTED) {
		printf("connected to dectproxy\n");
        } else if (events & BEV_EVENT_ERROR) {
                printf("error\n");
	}
}




void handle_dect_packet(unsigned char *buf) {

	int i;
	RosPrimitiveType primitive = ((ApifpccEmptySignalType *) buf)->Primitive;
	struct packet *p = (struct packet *)buf;
	uint8_t type = primitive >> 2;

	switch (primitive) {

	case API_FP_MM_GET_ID_REQ:
	case API_FP_MM_GET_ID_CFM:
	case API_FP_MM_GET_MODEL_REQ:
	case API_FP_MM_GET_MODEL_CFM:
	case API_FP_MM_SET_ACCESS_CODE_REQ:
	case API_FP_MM_SET_ACCESS_CODE_CFM:
	case API_FP_MM_GET_ACCESS_CODE_REQ:
	case API_FP_MM_GET_ACCESS_CODE_CFM:
	case API_FP_MM_GET_REGISTRATION_COUNT_REQ:
	case API_FP_MM_GET_REGISTRATION_COUNT_CFM:
	case API_FP_MM_DELETE_REGISTRATION_REQ:
	case API_FP_MM_DELETE_REGISTRATION_CFM:
	case API_FP_MM_REGISTRATION_FAILED_IND:
	case API_FP_MM_SET_REGISTRATION_MODE_REQ:
	case API_FP_MM_SET_REGISTRATION_MODE_CFM:
	case API_FP_MM_REGISTRATION_COMPLETE_IND:
	case API_FP_MM_HANDSET_PRESENT_IND:
	case API_FP_MM_GET_HANDSET_IPUI_REQ:
	case API_FP_MM_GET_HANDSET_IPUI_CFM:
	case API_FP_MM_STOP_PROTOCOL_REQ:
	case API_FP_MM_EXT_HIGHER_LAYER_CAP2_REQ:
	case API_FP_MM_START_PROTOCOL_REQ:
	case API_FP_MM_HANDSET_DETACH_IND:
	case API_FP_MM_HANDSET_DEREGISTERED_IND:
	case API_FP_MM_GET_NAME_REQ:
	case API_FP_MM_GET_NAME_CFM:
	case API_FP_MM_SET_NAME_REQ:
	case API_FP_MM_SET_NAME_CFM:
	case API_FP_MM_FEATURES_REQ:
	case API_FP_MM_FEATURES_CFM:
	case API_FP_MM_GET_FP_CAPABILITIES_REQ:
	case API_FP_MM_GET_FP_CAPABILITIES_CFM:
	case API_FP_MM_UNITDATA_REQ:
	case API_FP_MM_ALERT_BROADCAST_REQ:
	case API_FP_MM_START_PROTOCOL_PCM_SYNC_REQ:

		printf("API_FP_MM\n");
		break;

	case API_FP_GENEVENOT_EVENT_REQ:
	case API_FP_GENEVENOT_EVENT_IND:
	case API_FP_GENEVENOT_PP_EVENT_IND:
	case API_PP_GENEVENOT_EVENT_REQ:
	case API_PP_GENEVENOT_EVENT_IND:
	case API_FP_GENEVENOT_FEATURES_REQ:
	case API_FP_GENEVENOT_FEATURES_CFM:

		api_fp_genevenot((ApifpccEmptySignalType *) buf);
		break;

	default:
		printf("Unknown packet: %x\n", type);
		break;
	}
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
				break;
			}

			/* Reset packet struct */
			free(info->pkt);
			info->pkt = calloc(sizeof(packet_t), 1);

		} else
			return;
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






static void run(void) {

	evutil_socket_t listener;
	struct sockaddr_in sin;
	struct event *listener_event;


	if ((base = event_base_new()) == NULL)
		exit_failure("event_base_new");

	/* Connect to dectproxy */
	if ((dect = create_connection(INADDR_ANY, 7777)) == NULL)
		exit_failure("create_connection");

	event_base_dispatch(base);
}




int main(void) {

	setvbuf(stdout, NULL, _IONBF, 0);

	run();
	return 0;
}



