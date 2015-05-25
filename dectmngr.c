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
#include <dectshimdrv.h>

#include <json/json.h>

#include "dect.h"
#include "ucix.h"


/* Globals */
struct bufferevent *dect;
struct event_base *base;
struct info *dect_info;
struct status_packet status;
static struct uci_context *uci_ctx = NULL;
struct config config;

ApiCallReferenceType CallReference;

char *hotplug_cmd_path = DEFAULT_HOTPLUG_PATH;

#define ULP_DLC_CTRL_UNACKNOWLEDGED  0x01
#define ULP_DLC_CTRL_ACKNOWLEDGED    0x02

#define EARLY_BIT        (1 << 6)
#define PAGING_ON        (1 << 7)
#define DECT_NVS_SIZE 4096


void handle_dect_packet(unsigned char *buf);
void packet_read(struct bufferevent *bev, void *ctx);
void handle_client_packet(struct bufferevent *bev, client_packet *p);
void ApiFreeInfoElement(ApiInfoElementType **IeBlockPtr);
void ApiBuildInfoElement(ApiInfoElementType **IeBlockPtr,
                         rsuint16 *IeBlockLengthPtr,
                         ApiIeType Ie,
                         rsuint8 IeLength,
                         rsuint8 *IeData);
static void registration(struct bufferevent *bev, client_packet *p);

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
		case LED_ON :
			setenv("ACTION", "led_on", 1);
			break;
		case LED_OFF :
			setenv("ACTION", "led_off", 1);
			break;
		case LED_BLINK :
			setenv("ACTION", "led_blink", 1);
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


static void control_led(void)
{
	if(status.radio == ENABLED) {
		if(status.reg_mode == ENABLED) {
			call_hotplug(LED_BLINK);
		} else {
			call_hotplug(LED_ON);
		}
	} else {
		call_hotplug(LED_OFF);
	}
}


static void start_protocol(void)
{
	unsigned char o_buf[3];


	*(o_buf + 0) = ((API_FP_MM_START_PROTOCOL_REQ & 0xff00) >> 8);
	*(o_buf + 1) = ((API_FP_MM_START_PROTOCOL_REQ & 0x00ff) >> 0);
	*(o_buf + 2) = 0;
	
	printf("API_FP_MM_START_PROTOCOL_REQ\n");
	write_dect(o_buf, 3);

	status.radio = ENABLED;
	control_led();
}


static void stop_protocol(void)
{
	unsigned char o_buf[3];


	*(o_buf + 0) = ((API_FP_MM_STOP_PROTOCOL_REQ & 0xff00) >> 8);
	*(o_buf + 1) = ((API_FP_MM_STOP_PROTOCOL_REQ & 0x00ff) >> 0);
	*(o_buf + 2) = 0;
	
	printf("API_FP_MM_STOP_PROTOCOL_REQ\n");
	write_dect(o_buf, 3);

	status.radio = DISABLED;
	control_led();
}



static void dect_radio(int enable) {
	if (enable && status.radio == DISABLED) {
		start_protocol();
	} else if (!enable && status.radio == ENABLED) {
		stop_protocol();
	}
}



static void init_cfm(void) {

	ApiFpCcFeaturesReqType *t = NULL;

	t = (ApiFpCcFeaturesReqType*) malloc(sizeof(ApiFpCcFeaturesReqType));

	t->Primitive = API_FP_CC_FEATURES_REQ;
	t->ApiFpCcFeature = API_FP_CC_EXTENDED_TERMINAL_ID_SUPPORT;

	write_dect(t, sizeof(ApiFpCcFeaturesReqType));
	free(t);

	status.dect_init = true;
}



/* Schedule for later to refresh the list of
 * registered handsets. Used when we know the
 * stack is busy working so we need a small delay. */
static void list_handsets_schedule(void) {
	struct event *timeout;
	struct timeval tv;

	tv.tv_sec = 1;
	tv.tv_usec = 250000u;
	timeout = event_new(base, -1, EV_TIMEOUT, (void *)init_cfm, NULL);
	event_add(timeout, &tv);
}


static void list_handsets(void) {
	
  ApiFpMmGetRegistrationCountReqType m = { .Primitive = API_FP_MM_GET_REGISTRATION_COUNT_REQ, .StartTerminalId = 0};
	printf("list_handsets\n");
	write_dect(&m, sizeof(m));
}


/* When the radio is set to "auto" mode we turn
 * it of if we have no registered handsets. */
static void perhaps_disable_protocol(void) {
	int handsetCount, i;

	handsetCount = 0;
	
	// Is the radio in "auto" mode?
	if(config.radio == AUTO) {
		for(i = 0; i < MAX_NR_HANDSETS; i++) {
			if(status.handset[i].registered) handsetCount++;
		}

		dect_radio(handsetCount > 0);
	}

	control_led();
}


static int load_cfg_file(void) {
  
  const char *radio;
  
  uci_ctx = ucix_init("dect");
  
  if(!uci_ctx) {
    exit_failure("Error loading config file\n");
  }
  
  radio = ucix_get_option(uci_ctx, "dect", "dect", "radio");
  
  if (!strcmp(radio, "on")) {
    config.radio = ENABLED;
  } else if (!strcmp(radio, "off")) {
    config.radio = DISABLED;
  } else if (!strcmp(radio, "auto")) {
    config.radio = AUTO;
  } else {
    exit_failure("Bad config parameter\n");
  }

  return 1;
}


static void reload_config(void)
{
  load_cfg_file();
  
  /* Only start radio protocol if radio is enabled in config */
  if (config.radio == ENABLED) {
    dect_radio(1);
    list_handsets_schedule();
  } else if (config.radio == AUTO) {
    init_cfm();
  } else {
    dect_radio(0);
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


static void ule_pvc_config_ind(ApiFpUlePvcConfigIndType *m) {

	ApiFpUleProtocol_t Protocol= { API_ULE_PROTOCOL_FUN_1, API_ULE_PROTOCOL_VERSION_0, 0};	
	int size = sizeof(ApiFpUlePvcConfigResType) - sizeof(ApiFpUleProtocol_t) + sizeof(ApiFpUleProtocol_t);
	ApiFpUlePvcConfigResType* r = (ApiFpUlePvcConfigResType*) malloc(size);

	r->Primitive = API_FP_ULE_PVC_CONFIG_RES;
	r->TerminalId = m->TerminalId;
	r->Initiator = API_ULE_PP_INITIATOR;
	r->MtuPtSize = API_ULE_MTU_SIZE_MAX;
	r->MtuFtSize = API_ULE_MTU_SIZE_MAX;
	r->MtuLifetime = API_ULE_MTU_LIFETIME_DEFAULT;
	r->WindowSize = API_ULE_WINDOW_SIZE_DEFAULT;
	r->ProtocolCount = 1;
	memcpy(r->Protocol,&Protocol,sizeof(ApiFpUleProtocol_t));
	write_dect(r, size);
	free(r);
	printf("API_FP_ULE_PVC_CONFIG_RES\n");
}

static void ule_pvc_data_pending_ind(ApiFpUlePvcConfigIndType *m) {
		ApiFpUlePage_t Page[2]= {{API_ULE_PAGING_CH, 17, 2},{0,0,0}};
		int size = sizeof(ApiFpUlePvcPendingResType) - sizeof(ApiFpUlePage_t) + sizeof(ApiFpUlePage_t)*1;
		ApiFpUlePvcPendingResType* r2 = (ApiFpUlePvcPendingResType*)malloc(size);

		r2->Primitive = API_FP_ULE_PVC_PENDING_RES;
		r2->TerminalId = m->TerminalId;
		r2->Status = API_FP_ULE_ERR_NO_ERROR;
		r2->PageChCount = 1;
		memcpy(r2->PageChannel,Page,sizeof(ApiFpUlePage_t)*1);
		write_dect(r2, size);
		free(r2);
		printf("API_FP_ULE_PVC_PENDING_RES\n");

}


static void ule_pvc_data_iwu_ind(ApiFpUlePvcIwuDataIndType *m) {

	ApiFpUlePage_t Page[2]= {{API_ULE_PAGING_CH, 17, 2},{0,0,0}};
	int size = sizeof(ApiFpUlePvcIwuDataReqType) - 1 + m->InfoElementLength;
	ApiFpUlePvcIwuDataReqType* r = (ApiFpUlePvcIwuDataReqType*) malloc(size);
	

	r->Primitive = API_FP_ULE_PVC_IWU_DATA_REQ;
	r->TerminalId = m->TerminalId;
	r->InfoElementLength = m->InfoElementLength;
	memcpy(r->InfoElement,m->InfoElement,m->InfoElementLength);
	write_dect(r, size);
	free(r);
	printf("API_FP_ULE_PVC_IWU_DATA_REQ\n");
	
	size = sizeof(ApiFpUlePvcPendingResType) - sizeof(ApiFpUlePage_t) + sizeof(ApiFpUlePage_t)*1;
	ApiFpUlePvcPendingResType* r2 = (ApiFpUlePvcPendingResType*)malloc(size);
	
	r2->Primitive = API_FP_ULE_PVC_PENDING_RES;
	r2->TerminalId = m->TerminalId;
	r2->Status = API_FP_ULE_ERR_NO_ERROR;
	r2->PageChCount = 1;
	memcpy(r2->PageChannel,Page,sizeof(ApiFpUlePage_t)*1);
	write_dect(r2, size);
	free(r2);
	printf("API_FP_ULE_PVC_PENDING_RES\n");
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
		if (handset > 0) {
			get_handset_ipui(handset);
		} else {
			perhaps_disable_protocol();
		}
	}
}


static void ping_handset_stop(struct event *ev, short error, void *arg) {

	int *handset = arg;
	ApiFpCcReleaseReqType *m;
	
	printf("ping_handset_stop\n");
	if (bad_handsetnr(*handset)) {
		free(handset);
		return;
	}

	if (status.handset[(*handset) - 1].pinging == TRUE) {
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
	free(handset);
}



static void setup_cfm(unsigned char *b) {

	ApiFpCcSetupResType *m = (ApiFpCcSetupResType *)b;
	
	CallReference = m->CallReference;
	printf("Status: %d\n", m->Status);
	printf("Status: %d\n", m->Status);
}




static void register_handsets_start(void) {
	
	ApiFpMmSetRegistrationModeReqType m = { .Primitive = API_FP_MM_SET_REGISTRATION_MODE_REQ, .RegistrationEnabled = true, .DeleteLastHandset = false};

	if (status.radio == ENABLED) {
	  printf("register_handsets_start\n");
	  status.reg_mode = ENABLED;
	  control_led();
	  write_dect(&m, sizeof(m));
	} else {
	  printf("can't start regmode: radio not enabled\n");
	}
}


static void nvs_get_data( unsigned char *pNvsData )
{
	int fd, ret;
	
	if (pNvsData == NULL) {
		
		printf("%s: error %d\n", __FUNCTION__, errno);
		return;
	}

	
	fd = open("/etc/dect/nvs", O_RDONLY);
	if (fd == -1) {
		perror("open");
		exit(EXIT_FAILURE);
	}

	ret = read(fd, pNvsData, DECT_NVS_SIZE);
	if (ret == -1) {
		perror("read");
		exit(EXIT_FAILURE);
	}

	ret = close(fd);
	if (ret == -1) {
		perror("close");
		exit(EXIT_FAILURE);
	}


}



static int dect_init(void)
{
	int fd, r;
	ApiLinuxInitReqType *t = NULL;
	DECTSHIMDRV_INIT_PARAM parm;
	
	fd = open("/dev/dectshim", O_RDWR);
  
	if (fd == -1) {
		printf("%s: open error %d\n", __FUNCTION__, errno);
		return -1;
	}


	r = ioctl(fd, DECTSHIMIOCTL_INIT_CMD, &parm);
	if (r != 0) {
		printf("%s: ioctl error %d\n", __FUNCTION__, errno);
	}

	close(fd);
  
	printf("sizeof(ApiLinuxInitReqType): %d\n", sizeof(ApiLinuxInitReqType));

	/* download the eeprom values to the DECT driver*/
	t = (ApiLinuxInitReqType*) malloc(RSOFFSETOF(ApiLinuxInitReqType, Data) + DECT_NVS_SIZE);
	t->Primitive = API_LINUX_INIT_REQ;
	t->LengthOfData = DECT_NVS_SIZE;
	nvs_get_data(t->Data);

	write_dect(t, RSOFFSETOF(ApiLinuxInitReqType, Data) + DECT_NVS_SIZE);
	

	return r;
}


static void register_handsets_stop(void) {

	ApiFpMmSetRegistrationModeReqType m = { .Primitive = API_FP_MM_SET_REGISTRATION_MODE_REQ, .RegistrationEnabled = false, .DeleteLastHandset = false};

	status.reg_mode = DISABLED;

	if (status.radio == ENABLED ) {
	  printf("register_handsets_stop\n");
	  control_led();
	  write_dect(&m, sizeof(m));
	} else {
	  control_led();
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


static void ule_data_ind(ApiFpUleDataIndType *m) {

	printf("TerminalId: %d\n", m->TerminalId);
	printf("Length: %d\n", m->Length);
	printf("%s\n", m->Data);
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
	if (status.handset[handset].registered == TRUE) {
		get_handset_ipui(handset + 1);
	} else {
		// Finished querying each handset
		perhaps_disable_protocol();
	}
	
}


/* After the radio has been enabled, trigger
 * start of registration. */
static void registration_delayed_start(void) {
	if(status.reg_mode == DELAYED_START) registration(NULL, NULL);
}


/* Start registration and arm a timer for possible timeout. */
static void registration(struct bufferevent *bev, client_packet *p) {

	struct timeval tv;
	struct event *timeout;

	if(config.radio == DISABLED) return;

	// Enable the radio protocol if necessary
	if(status.radio == DISABLED) {
		printf("radio is off; registration delayed\n");
		dect_radio(1);
		status.reg_mode = DELAYED_START;
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		timeout = event_new(base, -1, EV_TIMEOUT, 
			(void *)registration_delayed_start, NULL);
		event_add(timeout, &tv);
	} else if(status.reg_mode != ENABLED) {
		printf("enable registration\n");
		register_handsets_start();
		tv.tv_sec = 180;
		tv.tv_usec = 0;
		timeout = event_new(base, -1, EV_TIMEOUT, 
			(void *)register_handsets_stop, NULL);
		event_add(timeout, &tv);
	}
}


static void ping_handset(int handset) {

	struct timeval tv = {20,0};
	struct event *timeout;
	
	if (bad_handsetnr(handset))
		return;

	if(config.radio == DISABLED) return;

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
		//printf("client connection closed\n");
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
		//printf("accepted client connection\n");

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
		printf("error connecting to dectproxy\n");
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
	status.radio = DISABLED;
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
if(status.reg_mode == DISABLED) init_cfm();
		break;

	case API_FP_MM_SET_REGISTRATION_MODE_CFM:
		printf("API_FP_MM_SET_REGISTRATION_MODE_CFM\n");
if(status.reg_mode == DISABLED) init_cfm();
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
list_handsets_schedule();
		break;

	case API_FP_MM_REGISTRATION_COMPLETE_IND:
		printf("API_FP_MM_REGISTRATION_COMPLETE_IND\n");
		register_handsets_stop();
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

		/* Only start radio protocol if radio is enabled in config */
		if (config.radio == ENABLED) {
		  dect_radio(1);
		}
		
		list_handsets();
 		break;

	case API_FP_ULE_INIT_CFM:
		printf("API_FP_ULE_INIT_CFM\n");
		ule_init_cfm((ApiFpUleInitCfmType *) buf);
		break;

	case API_FP_ULE_DATA_IND:
		printf("API_FP_ULE_DATA_IND\n");
		ule_data_ind((ApiFpUleDataIndType *) buf);
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

	case API_FP_ULE_PVC_CONFIG_REJ:
		printf("API_FP_ULE_PVC_CONFIG_REJ\n");
		break;

	case API_FP_ULE_PVC_CONFIG_IND:
		printf("API_FP_ULE_PVC_CONFIG_IND\n");
		ule_pvc_config_ind((ApiFpUlePvcConfigIndType *)buf);
		break;

	case API_FP_ULE_PVC_PENDING_IND:
		printf("API_FP_ULE_PVC_PENDING_IND\n");
		ule_pvc_data_pending_ind((ApiFpUlePvcConfigIndType *)buf);
		break;

	case API_FP_ULE_PVC_IWU_DATA_IND:
		printf("API_FP_ULE_PVC_IWU_DATA_IND\n");
		ule_pvc_data_iwu_ind((ApiFpUlePvcIwuDataIndType *)buf);
		break;
		
	default:
		printf("Unknown packet\n");
		break;
	}
}

void handle_client_packet(struct bufferevent *bev, client_packet *p) {

	switch (p->type) {

	case GET_STATUS:
		//printf("GET_STATUS\n");
		get_status(bev);
		break;

	case REGISTRATION:
		printf("REGISTRATION\n");
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

	case RADIO:
		printf("RADIO %d\n", p->data);
		dect_radio(p->data);
		break;

	case RELOAD_CONFIG:
	  printf("RELOAD_CONFIG\n");
	  reload_config();
		break;

	case DELETE_HSET:
		printf("DELETE_HSET %d\n", p->data);
		delete_hset(p->data);
		break;

	case LIST_HANDSETS:
		printf("LIST_HANDSETS\n");
		init_cfm();
		break;

	case ULE_START:
		printf("ULE_START\n");
		ule_start();
		break;

	case INIT:
		printf("dect_init\n");
		dect_init();
		break;

	case BUTTON:
		printf("button %d\n", p->data);
		if(p->data && (config.radio == ENABLED || 
				config.radio == AUTO)) {
			registration(bev, p);
		}
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

	load_cfg_file();

	if ((base = event_base_new()) == NULL)
		exit_failure("event_base_new");

	/* Connect to dectproxy */
	if ((dect = create_connection(INADDR_ANY, 7777)) == NULL)
		exit_failure("create_connection");

	/* Open listening socket. */
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_LOOPBACK;
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


	
