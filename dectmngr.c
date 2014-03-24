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
#include <stdbool.h>
//#include <ApiFpProject.h>
//#include <dectUtils.h>
//
/* #include <Api/RsStandard.h> */
/* #include <Api/Types/ApiTypes.h> */

#include <Api/CodecList/ApiCodecList.h>
#include <Api/FpCc/ApiFpCc.h>
//#include <Api/FpFwu/ApiFpFwu.h>
#include <Api/FpMm/ApiFpMm.h>
#include <Api/FpNoEmission/ApiFpNoEmission.h>
#include <Api/GenEveNot/ApiGenEveNot.h>
#include <Api/Las/ApiLas.h>
#include <Api/Linux/ApiLinux.h>
//#include <Api/Project/ApiProject.h>
#include <Api/FpUle/ApiFpUle.h>


#include <json/json.h>

#include "dect.h"


/* Globals */
struct bufferevent *dect;
struct event_base *base;
struct info *dect_info;
struct status_packet status;
ApiCallReferenceType CallReference;

char *hotplug_cmd_path = DEFAULT_HOTPLUG_PATH;

#define ULP_DLC_CTRL_UNACKNOWLEDGED  0x01
#define ULP_DLC_CTRL_ACKNOWLEDGED    0x02

#define EARLY_BIT        (1 << 6)
#define PAGING_ON        (1 << 7)


void handle_dect_packet(unsigned char *buf);
void packet_read(struct bufferevent *bev, void *ctx);
void handle_client_packet(struct bufferevent *bev, client_packet *p);
void ApiFreeInfoElement(ApiInfoElementType **IeBlockPtr);
void ApiBuildInfoElement(ApiInfoElementType **IeBlockPtr,
                         rsuint16 *IeBlockLengthPtr,
                         ApiIeType Ie,
                         rsuint8 IeLength,
                         rsuint8 *IeData);

int switch_state_on = 1;





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


static void call_hotplug(uint8_t action)
{
	char *argv[3];
	int pid;

	pid = fork();
	if (pid > 0)
		return;
	
	if (pid == 0) {

		/* Child process */
		switch (action) {
		case DECT_INIT :
			setenv("ACTION", "dect_init", 1);
			break;
		case REG_START :
			setenv("ACTION", "reg_start", 1);
			break;
		case REG_STOP :
			setenv("ACTION", "reg_stop", 1);
			break;
		default:
			printf("Unknown action\n");
			return;
		}
	
		argv[0] = hotplug_cmd_path;
		argv[1] = "dect";
		argv[2] = NULL;
		execvp(argv[0], argv);
		exit(127);
	}
}


static int bad_handsetnr(int handset) {

	if ((handset <= 0) || (handset > MAX_NR_HANDSETS)) {
		printf("Bad handset nr: %d\n", handset);
		return 1;
	}
	return 0;
}


static send_client(struct bufferevent *bev, uint8_t status) {
	
	packet_t *p;

	if ((p = (packet_t *)malloc(sizeof(packet_t))) == NULL)
		perror("failure");

	p->type = RESPONSE;
	bufferevent_write(bev, p, sizeof(packet_t));
	free(p);
}


static void write_dect(void *data, int size) {

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


static void list_handsets(void) {
	
  ApiFpMmGetRegistrationCountReqType m = { .Primitive = API_FP_MM_GET_REGISTRATION_COUNT_REQ, .StartTerminalId = 0};
	printf("list_handsets\n");
	write_dect(&m, sizeof(m));
}


static void ule_start(void) {

	ApiFpUleInitReqType *m;
	
	if(!(m = (ApiFpUleInitReqType *)calloc(1, sizeof(ApiFpUleInitReqType))))
		exit_failure("malloc");

	m->Primitive = API_FP_ULE_INIT_REQ;
	m->MaxUlpDevices = 0x4;

	printf("ule_start\n");
	write_dect(m, sizeof(ApiFpUleInitReqType));
}


void ApiFreeInfoElement(ApiInfoElementType **IeBlockPtr) {

	free((void*)*IeBlockPtr);
	*IeBlockPtr = NULL;
}


static void ule_init_cfm(ApiFpUleInitCfmType *m) {

	printf("Status: %d\n", m->Status);
	printf("MaxUlpDevices: %d\n", m->MaxUlpDevices);
	printf("UpLinkBuffers: %d\n", m->UpLinkBuffers);
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

	ApiFpCcSetupReqType* m;
	ApiFpCcAudioIdType Audio;

	m = (ApiFpCcSetupReqType*) malloc(sizeof(ApiFpCcSetupReqType));

	CallReference.Value = 0;
	CallReference.Instance.Host = 0;
	CallReference.Instance.Fp = 1;

	Audio.IntExtAudio = API_IEA_INT;
	Audio.SourceTerminalId = 0;
	

	m->Primitive = API_FP_CC_SETUP_REQ;
	m->CallReference = CallReference;
	m->TerminalId = handset;
	m->AudioId = Audio;
	m->BasicService = API_BASIC_SPEECH;
	m->CallClass = API_CC_NORMAL;
	m->Signal = API_CC_SIGNAL_ALERT_ON_PATTERN_2;
	m->InfoElementLength = 0;
					  
	write_dect(m, sizeof(ApiFpCcSetupReqType));
	free(m);
	  
}




static void get_handset_ipui(int handset)
{
	ApiFpMmGetHandsetIpuiReqType* m = (ApiFpMmGetHandsetIpuiReqType*)malloc(sizeof(ApiFpMmGetHandsetIpuiReqType));

	m->Primitive = API_FP_MM_GET_HANDSET_IPUI_REQ;
	m->TerminalId = handset;
	write_dect((unsigned char *)m, sizeof(ApiFpMmGetHandsetIpuiReqType));

}



static void registration_count_cfm(unsigned char *mail) {

	int i, handset;
	
	printf("INPUT: API_FP_MM_GET_REGISTRATION_COUNT_CFM\n");
	


	if (((ApiFpMmGetRegistrationCountCfmType*) mail)->Status == RSS_SUCCESS) {

		printf("Max Number of Handset allowed: %d\n", ((ApiFpMmGetRegistrationCountCfmType*) mail)->MaxNoHandsets);

		printf("TerminalIdCount: %d\n", ((ApiFpMmGetRegistrationCountCfmType*) mail)->TerminalIdCount);
		 
		for (i = 0 ; i < (((ApiFpMmGetRegistrationCountCfmType*) mail)->TerminalIdCount ) ; i++ ) {
			 
			handset = ((ApiFpMmGetRegistrationCountCfmType*) mail)->TerminalId[i];

			if (bad_handsetnr(handset))
				return;

			status.handset[handset - 1].registered = TRUE;
		}
		 
		/* Get the ipui of the first handset. For some damn
		   reason we can't to all of them at once. */
		handset = ((ApiFpMmGetRegistrationCountCfmType*) mail)->TerminalId[0];
		if (handset > 0)
			get_handset_ipui(handset);
	}
}


static void ping_handset_stop(struct event *ev, short error, void *arg) {

	int *handset = arg;
	ApiFpCcReleaseReqType *m;

	if (bad_handsetnr(handset))
		return;


	status.handset[(*handset) - 1].pinging = FALSE;

	m = (ApiFpCcReleaseReqType *) malloc(sizeof(ApiFpCcReleaseReqType));
	
	m->Primitive = API_FP_CC_RELEASE_REQ;
	m->CallReference = CallReference;
	m->Reason = API_RR_UNEXPECTED_MESSAGE;
	m->InfoElementLength = 0;
	m->InfoElement[1] = NULL;

	printf("API_FP_CC_RELEASE_REQ\n");
	write_dect(m, sizeof(ApiFpCcReleaseReqType));
	free(m);
}



static void setup_cfm(unsigned char *b) {

	ApiFpCcSetupResType *m = (ApiFpCcSetupResType *)b;
	
	CallReference = m->CallReference;
	printf("Status: %d\n", m->Status);
	printf("Status: %d\n", m->Status);
}




static void register_handsets_start(void) {
	
	ApiFpMmSetRegistrationModeReqType m = { .Primitive = API_FP_MM_SET_REGISTRATION_MODE_REQ, .RegistrationEnabled = true, .DeleteLastHandset = false};

	printf("register_handsets_start\n");
	if (status.dect_init) {
		call_hotplug(REG_START);
		write_dect(&m, sizeof(m));
	}
}


static void init_cfm(void) {

	status.dect_init = true;
	call_hotplug(DECT_INIT);

}


static void register_handsets_stop(void) {

	ApiFpMmSetRegistrationModeReqType m = { .Primitive = API_FP_MM_SET_REGISTRATION_MODE_REQ, .RegistrationEnabled = false, .DeleteLastHandset = false};


	printf("register_handsets_stop\n");
	if (status.dect_init) {
		status.reg_mode = DISABLED;
		call_hotplug(REG_STOP);
		write_dect(&m, sizeof(m));
	}
}


static void delete_hset(int handset) {  

	ApiFpUleDeleteRegistrationReqType m = { .Primitive = API_FP_MM_DELETE_REGISTRATION_REQ, .TerminalId = handset};

	if (status.dect_init) {
		printf("delete handset: %d\n", handset);
		write_dect(&m, sizeof(m));
	}
}


static void present_ind(unsigned char *mail) {  

	ApiTerminalIdType handset;
	int length;
	ApiFpMmHandsetPresentIndType *t = NULL;

	handset = ((ApiFpMmHandsetPresentIndType*) mail)->TerminalId;
	length = ((ApiFpMmHandsetPresentIndType*) mail)->InfoElementLength;
	printf("INPUT: API_FP_MM_HANDSET_PRESENT_IND from handset (%d)\n", handset);
	
	if (bad_handsetnr(handset))
		return;

	status.handset[handset - 1].present = TRUE;
}


static void delete_registration_cfm(unsigned char *mail) {  

	int handset;

	handset = ((ApiFpMmDeleteRegistrationCfmType*) mail)->TerminalId;
	
	if (bad_handsetnr(handset))
		return;

	printf("deleted handset: %d\n", handset);
	status.handset[handset - 1].registered = FALSE;
	status.handset[handset - 1].present = FALSE;
}


static void connect_ind(ApiFpCcConnectIndType *m) {

	int handset;
	struct brcm_pvt *p;
	struct brcm_subchannel *sub;
	unsigned char o_buf[5];

	handset = m->CallReference.Instance.Fp;

	if (bad_handsetnr(handset))
		return;
	
	if (status.handset[(handset) - 1].pinging == TRUE)
		status.handset[(handset) - 1].pinging = FALSE;
}

static void ule_data_req(int switch_on) {
	ApiFpUleDataReqType *r = calloc(1, sizeof(ApiFpUleDataReqType) + 18);
	
	//	0x18 0x45 0 X4 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0

	rsuint8 switch_timer_p[19] =  { 0x18,0x45,0x0,0xff,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0 };
	rsuint8 switch_on_p[19] =  { 0x31,0x45,0x0,0x01,0x01,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0 };

	rsuint8 switch_off_p[19] = { 0x31,0x45,0x0,0x1,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0 };

	r->Primitive = API_FP_ULE_DATA_REQ;
	r->TerminalId = 7;
	r->DlcCtrl = PAGING_ON | ULP_DLC_CTRL_ACKNOWLEDGED;
	//	r->DlcCtrl = PAGING_ON | ULP_DLC_CTRL_UNACKNOWLEDGED;
	r->Length = 20;
	
	if (switch_on == 1) {
		printf("switch on\n");
		memcpy(r->Data, switch_on_p, 19);
		switch_state_on = 1;
	} else if (switch_on == 0){
		printf("switch off\n");
		memcpy(r->Data, switch_off_p, 19);
		switch_state_on = 0;
 	} else if (switch_on == 2){
		printf("switch timer\n");
		memcpy(r->Data, switch_timer_p, 19);
	}


	printf("API_FP_ULE_DATA_REQ\n");
	write_dect((unsigned char *)r, sizeof(ApiFpUleDataReqType) + 18);
	free(r);

	
}


static void ule_data_cfm(ApiFpUleDataCfmType *m) {

	printf("TerminalId: %d\n", m->TerminalId);
	printf("Status: %d\n", m->Status);

}



/* static void ule_data_ind(unsigned char *buf) { */

/* 	ApiFpUleDataIndType *m = (ApiFpUleDataIndType *) buf; */
	
/* 	printf("DataType: 0x%2.2x\n", m->DataType); */
/* 	printf("Sensor: 0x%2.x\n", m->Sensor); */
/* 	printf("Counter: %d\n", m->Counter); */
/* 	printf("State: %d\n", m->State); */
/* 	printf("Power_h: %d\n", m->Power_h); */
/* 	printf("Power_l: %d\n", m->Power_l); */
/* 	printf("RMSVoltage: %d\n", m->RMSVoltage); */
/* 	printf("RMSCurrent: %d\n", m->RMSCurrent); */
/* 	printf("EnergyFwd: %d\n", m->EnergyFwd); */
/* 	printf("EnergyRev: %d\n\n", m->EnergyRev); */
	
/* } */


/* static void ule_service_ind(unsigned char *buf) { */

/* 	ApiFpUleServiceResType *r = calloc(1, sizeof(ApiFpUleServiceResType)); */
/* 	ApiFpUleServiceIndType *m = (ApiFpUleServiceIndType *) buf; */

/* 	printf("PpNumber: %d\n", m->PpNumber); */
/* 	printf("Bandwidth: %d\n", m->Bandwidth); */
/* 	printf("DownlinkRedundant: %d\n", m->DownlinkRedundant); */
/* 	printf("ContentionLatency: %d\n", m->ContentionLatency); */
/* 	printf("MaxDutyCycle: %d\n", m->MaxDutyCycle); */
/* 	printf("AField: %d\n", m->AField); */
/* 	printf("Bfield_Full: %d\n", m->Bfield_Full); */
/* 	printf("Bfield_Long: %d\n", m->Bfield_Long); */
/* 	printf("Bfield_Double: %d\n", m->Bfield_Double); */

/* 	/\* Response *\/ */
/* 	r->Primitive = API_FP_ULE_SERVICE_RES; */
/* 	r->Status = RSS_SUCCESS; */
/* 	r->PpNumber = m->PpNumber; */

/* 	if ((m->Bandwidth == 0) && (m->DownlinkRedundant == 0) && (m->ContentionLatency == 0) && (m->MaxDutyCycle == 0)) { */
/* 		r->UlpFrameLenDown = 0; */
/* 		r->DownlinkFrame = 0; */
/* 		r->DownLinkRedundant = 0; */
/* 		r->UlpFrameLenUp = 0; */
/* 		r->UplinkStartFrame = 0; */
/* 		r->UplinkEndFrame = 0; */
/* 		r->Slotsize = 1; */
		
/* 	} else { */
/* 		r->UlpFrameLenDown = m->MaxDutyCycle; */
/* 		r->DownlinkFrame = (rsuint16) (m->PpNumber * 2); */
/* 		r->DownLinkRedundant = 1; */
/* 		r->UlpFrameLenUp = 8; */
/* 		r->UplinkStartFrame = 128; */
/* 		r->UplinkEndFrame = 255; */
/* 		r->Slotsize = 1; */
/* 	} */
	
/* 	printf("API_FP_ULE_SERVICE_RES\n"); */
/* 	_write_dect((unsigned char *)r, sizeof(ApiFpUleServiceResType)); */
/* 	free(r); */
/* } */


static void handset_ipui_cfm(unsigned char *mail) {  

	int handset, i;

	handset = ((ApiFpMmGetHandsetIpuiCfmType *) mail)->TerminalId;

	if (bad_handsetnr(handset))
		return;


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
	
	if (bad_handsetnr(handset))
		return;

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
	RosPrimitiveType primitive = ((ApifpccEmptySignalType *) buf)->Primitive;
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

	case API_FWU_DEVICE_NOTIFY_IND:
		printf("API_FP_FWU_DEVICE_NOTIFY_IND\n");
		break;

	case API_FP_CC_CONNECT_CFM:
		printf("API_FP_CC_CONNECT_CFM\n");
		break;

	case API_FP_CC_CONNECT_IND:
		printf("API_FP_CC_CONNECT_IND\n");
		connect_ind((ApiFpCcConnectIndType *)buf);
		break;

	case API_FP_CC_INFO_IND:
		printf("API_FP_CC_INFO_IND\n");
		break;

	case API_FP_CC_ALERT_IND:
		printf("API_FP_CC_ALERT_IND\n");
		break;

	case API_LINUX_NVS_UPDATE_IND:
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

	case API_LINUX_INIT_CFM:
		printf("API_FP_LINUX_INIT_CFM\n");
		init_cfm();
		ule_start();
		break;

	case API_FP_CC_RELEASE_CFM:
		printf("API_FP_CC_RELEASE_CFM\n");
		break;

	case API_FP_CC_SETUP_CFM:
		printf("API_FP_CC_SETUP_CFM\n");
		setup_cfm(buf);
 		break;

	case API_FP_CC_FEATURES_CFM:
		printf("API_FP_CC_FEATURES_CFM\n");
		list_handsets();
 		break;

	case API_FP_ULE_INIT_CFM:
		printf("API_FP_ULE_INIT_CFM\n");
		ule_init_cfm((ApiFpUleInitCfmType *) buf);
		break;

	case API_FP_ULE_DATA_IND:
		printf("API_FP_ULE_DATA_IND\n");
		break;

	case API_FP_ULE_DATA_CFM:
		printf("API_FP_ULE_DATA_CFM\n");
		ule_data_cfm((ApiFpUleDataCfmType *)buf);
		break;

	case API_FP_ULE_DTR_IND:
		printf("API_FP_ULE_DTR_IND\n");
		break;

	case API_FP_ULE_GET_REGISTRATION_COUNT_CFM:
		printf("API_FP_ULE_GET_REGISTRATION_COUNT_CFM\n");
		break;

	case API_FP_ULE_GET_DEVICE_IPUI_CFM:
		printf("API_FP_ULE_GET_DEVICE_IPUI_CFM\n");
		break;

	case API_FP_ULE_ABORT_DATA_CFM:
		printf("API_FP_ULE_ABORT_DATA_CFM\n");
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

	case ZWITCH:
		printf("SWITCH %d\n", p->data);
		ule_data_req(p->data);
		break;

	case DELETE_HSET:
		printf("DELETE_HSET %d\n", p->data);
		delete_hset(p->data);
		break;

	case LIST_HANDSETS:
		printf("LIST_HANDSETS\n");
		init_cfm();
		list_handsets();
		break;

	case ULE_START:
		printf("ULE_START\n");
		ule_start();
		break;

	case INIT:
		printf("INIT\n");
		init_cfm();
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


	
