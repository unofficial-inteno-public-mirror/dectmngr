

#include <stdio.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <endpointdrv.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
//#include <codec.h>

//#include <endpoint_user.h>





#define NOT_INITIALIZED -1
#define EPSTATUS_DRIVER_ERROR -1
#define MAX_NUM_LINEID 2
typedef void (*rtpDropPacketResetCallback)(void);

typedef struct
{
   endptEventCallback         pEventCallBack;
   endptPacketCallback        pPacketCallBack;
   rtpDropPacketResetCallback pRtpDropPacketResetCallBack;
   int                        fileHandle;
   int                        logFileHandle;

} ENDPTUSER_CTRLBLOCK;



EPSTATUS vrgEndptSignal
(
   ENDPT_STATE   *endptState,
   int            cnxId,
   EPSIG          signal,
   unsigned int   value,
   int            duration,
   int            period,
   int            repetition
 );


ENDPTUSER_CTRLBLOCK endptUserCtrlBlock = {NULL, NULL, NULL, NOT_INITIALIZED, NOT_INITIALIZED};
VRG_ENDPT_STATE endptObjState[MAX_NUM_LINEID];
int fd;


EPSTATUS vrgEndptDriverOpen(void);
int endpt_init(void);


void get_stats()
{
	int i,j;
	/* Endpoint */
   ENDPOINTDRV_ENDPOINTCOUNT_PARM endpointCount;
	/* Capabilities */
   ENDPOINTDRV_CAP_PARM tCapParm;
   EPZCAP capabilities = {};
	/* Hook stat */
	ENDPOINTDRV_HOOKSTAT_CMD_PARM tHookstatParm;

	/* Get number of endpoints */
   endpointCount.size = sizeof(ENDPOINTDRV_ENDPOINTCOUNT_PARM);
   if ( ioctl( fd, ENDPOINTIOCTL_ENDPOINTCOUNT, &endpointCount ) != IOCTL_STATUS_SUCCESS )
   {
      printf("%s: error during ioctl", __FUNCTION__);
   } else {
      printf("\n\nNumber of endpoints: %d\n\n\n", endpointCount.endpointNum);
   }

	/* get capabilities */
typedef struct EPZCAP   /* Endpoint Capabilities */
{
   VRG_UINT16  pCap[2];      /* Packetazation period data (msec). MS-Bit contains "1" if a range is defined */
   VRG_UINT8   codecCap[CODEC_MAX_TYPES];   /* Codec caps for the given endpoint */
   VRG_UINT32  nteCap;       /* Bit encoded value representing supported named telephone events */
   VRG_UINT16  eCap;         /* Echo cancelation (0 if NOT supported) */
   VRG_UINT16  sCap;         /* Silence Suppression parameter (0 if NOT supported) */
   int         mCap[16];     /* Connection Modes. Each char codes the connection mode coded as MGECMODE enum type */
   EPTYPE      endptType;    /* Endpoint type */
   EPCLIDTYPE  clidType;     /* CLID type */
   VRG_UINT8   clidData[2];  /* Extra data.  Used for EPCLIDTYPE_DTMF to carry head/tail information */
   VRG_UINT8   vbdCap;     /* Bit encoded value representing supported voice band data capabilities. */
   VRG_UINT8   redLvl;     /* Maximum number of redundancy level(s) supported per RFC 2198. */
} EPZCAP;


	for ( i = 0; i < vrgEndptGetNumEndpoints(); i++ ) {
		sleep(1);
	   tCapParm.capabilities= &capabilities;
	   tCapParm.state       = (ENDPT_STATE*)&endptObjState[i];
	   tCapParm.epStatus    = EPSTATUS_DRIVER_ERROR;
	   tCapParm.size        = sizeof(ENDPOINTDRV_CAP_PARM);

	   if ( ioctl( fd, ENDPOINTIOCTL_ENDPT_CAPABILITIES, &tCapParm ) != IOCTL_STATUS_SUCCESS )
	   {
	      printf("%s: error during ioctl", __FUNCTION__);
	   } else {
			sleep(1);
			printf("\nioctl succeeded on enpoint %d:.\n\n\n",i);
			printf("pCap[0] = %d, pCap[1] = %d\n", capabilities.pCap[0], capabilities.pCap[1]);
			printf("eCap = %d\n", capabilities.eCap);
			printf("sCap = %d\n", capabilities.sCap);
			for (j=0 ;  j<CODEC_MAX_TYPES ; j++) {
				printf("codecCap[%d] = %d\n",j, capabilities.codecCap[j]);
			}
	   }

		/* Get hook stat, doesn't seem to work */
		tHookstatParm.epStatus   = EPSTATUS_DRIVER_ERROR;
	   tHookstatParm.hookstat   = EPCAS_ONHOOK;
	   tHookstatParm.lineId     = endptObjState[i].lineId;
	   tHookstatParm.size       = sizeof(ENDPOINTDRV_HOOKSTAT_CMD_PARM);

	   if ( ioctl( fd, ENDPOINTIOCTL_HOOKSTAT, &tHookstatParm ) != IOCTL_STATUS_SUCCESS ) {
		  printf("%s: error during ioctl", __FUNCTION__);
	   } else {
			printf("hook stat is: %d\n",tHookstatParm.hookstat);
		}
	}
	sleep(2);
}



void endptEventCb()
{
  printf("Received callback event.\n");
}


void ingressPktRecvCb( ENDPT_STATE *endptState, int cnxId, EPPACKET *epPacketp, int length )
{
  	printf("Received packetCallback event.\n");

}

void endpointPacketReleaseCallback(ENDPT_STATE *endpt, int cxid, unsigned int   bufDesc)
{

	printf("Received packet release callback event: [%d]: %d %x\n",endpt->lineId, cxid, bufDesc);
	free((void*)bufDesc);
}


void event_loop()
{

   	ENDPOINTDRV_EVENT_PARM tEventParm;
   	ENDPT_STATE endptState;
   	int rc = IOCTL_STATUS_FAILURE;
	int event_cnt = 20;

   	tEventParm.size = sizeof(ENDPOINTDRV_EVENT_PARM);

	while (event_cnt>0) {
      	tEventParm.length = 0;

		printf("Getting event, %d left before exit\n", event_cnt);
		event_cnt--;

      	/* Get the event from the endpoint driver. */
      	rc = ioctl( fd, ENDPOINTIOCTL_ENDPT_GET_EVENT, &tEventParm);
      	if( rc == IOCTL_STATUS_SUCCESS )
      	{
        	endptState.lineId = tEventParm.lineId;
			switch (tEventParm.event) {
				case EPEVT_OFFHOOK:
					printf("EPEVT_OFFHOOK detected\n");
					break;
				case EPEVT_ONHOOK:
					printf("EPEVT_ONHOOK detected\n");
					break;

				case EPEVT_DTMF0: printf("EPEVT_DTMF0 detected\n"); break;
				case EPEVT_DTMF1: printf("EPEVT_DTMF1 detected\n"); break;
				case EPEVT_DTMF2: printf("EPEVT_DTMF2 detected\n"); break;
				case EPEVT_DTMF3: printf("EPEVT_DTMF3 detected\n"); break;
				case EPEVT_DTMF4: printf("EPEVT_DTMF4 detected\n"); break;
				case EPEVT_DTMF5: printf("EPEVT_DTMF5 detected\n"); break;
				case EPEVT_DTMF6: printf("EPEVT_DTMF6 detected\n"); break;
				case EPEVT_DTMF7: printf("EPEVT_DTMF7 detected\n"); break;
				case EPEVT_DTMF8: printf("EPEVT_DTMF8 detected\n"); break;
				case EPEVT_DTMF9: printf("EPEVT_DTMF9 detected\n"); break;
				case EPEVT_DTMFS: printf("EPEVT_DTMFS detected\n"); break;
				case EPEVT_DTMFH: printf("EPEVT_DTMFH detected\n"); break;
				default:
					break;
			}
		}
		sleep(1);
	}

}


int endpt_init(void)
{
  int num_endpts;
  VRG_ENDPT_INIT_CFG   vrgEndptInitCfg;
  int rc, i;
  
  printf("Initializing endpoint interface\n");

  vrgEndptDriverOpen();

  vrgEndptInitCfg.country = VRG_COUNTRY_NORTH_AMERICA;
  vrgEndptInitCfg.currentPowerSource = 0;

  /* Intialize endpoint */
  rc = vrgEndptInit( &vrgEndptInitCfg,
		     endptEventCb,
		     ingressPktRecvCb,
		     NULL,
		     NULL,
		     endpointPacketReleaseCallback,
		     NULL );

  num_endpts = vrgEndptGetNumEndpoints();
  
  printf("\n\nNum endpoints: %d\n\n\n", num_endpts);


  /* Creating Endpt */
  for ( i = 0; i < vrgEndptGetNumEndpoints(); i++ )
    {
      rc = vrgEndptCreate( i, i,(VRG_ENDPT_STATE *)&endptObjState[i] );
    }

  return 0;
}

void create_connection() {

	int i, loop_state=1500;
   	int buf_pos_idx = 0;
	#define PACKET_BUFFER_SIZE 172*50
   	UINT8 packet_buffer[PACKET_BUFFER_SIZE] = {0};
	int tcounter = 0;


  	for ( i = 0; i < vrgEndptGetNumEndpoints(); i++ ) {
		ENDPOINTDRV_CONNECTION_PARM tConnectionParm;
		EPZCNXPARAM epCnxParms = {0};
//		CODECLIST  codecListLocal = {0};
//		CODECLIST  codecListRemote = {0};

		/* Enable sending a receving G711 */
		epCnxParms.cnxParmList.recv.numCodecs = 3;
		epCnxParms.cnxParmList.recv.codecs[0].type = CODEC_PCMU;
		epCnxParms.cnxParmList.recv.codecs[0].rtpPayloadType = RTP_PAYLOAD_PCMU;
		epCnxParms.cnxParmList.recv.codecs[1].type = CODEC_PCMA;
		epCnxParms.cnxParmList.recv.codecs[1].rtpPayloadType = RTP_PAYLOAD_PCMA;
		epCnxParms.cnxParmList.recv.codecs[2].type = CODEC_G726_32;
		epCnxParms.cnxParmList.recv.codecs[2].rtpPayloadType = RTP_PAYLOAD_G726_32;

		epCnxParms.cnxParmList.send.numCodecs = 3;
		epCnxParms.cnxParmList.send.codecs[0].type = CODEC_PCMU;
		epCnxParms.cnxParmList.send.codecs[0].rtpPayloadType = RTP_PAYLOAD_PCMU;
		epCnxParms.cnxParmList.send.codecs[1].type = CODEC_PCMA;
		epCnxParms.cnxParmList.send.codecs[1].rtpPayloadType = RTP_PAYLOAD_PCMA;
		epCnxParms.cnxParmList.send.codecs[2].type = CODEC_G726_32;
		epCnxParms.cnxParmList.send.codecs[2].rtpPayloadType = RTP_PAYLOAD_G726_32;

		// Set 20ms packetization period
		epCnxParms.cnxParmList.send.period[0] = 20;
         epCnxParms.mode  =   EPCNXMODE_SNDRX;
//         epCnxParms.cnxParmList.recv = codecListLocal;
//         epCnxParms.cnxParmList.send = codecListRemote;
//         epCnxParms.period = 20;
         epCnxParms.echocancel = 1;
         epCnxParms.silence = 0;
//         epCnxParms.pktsize = CODEC_G711_PAYLOAD_BYTE;   /* Not used ??? */


	   	tConnectionParm.cnxId      = i;
	   	tConnectionParm.cnxParam   = &epCnxParms;
	   	tConnectionParm.state      = (ENDPT_STATE*)&endptObjState[i];
	   	tConnectionParm.epStatus   = EPSTATUS_DRIVER_ERROR;
	   	tConnectionParm.size       = sizeof(ENDPOINTDRV_CONNECTION_PARM);

	   	if ( ioctl( fd, ENDPOINTIOCTL_ENDPT_CREATE_CONNECTION, &tConnectionParm ) != IOCTL_STATUS_SUCCESS ){
		  printf("%s: error during ioctl", __FUNCTION__);
		} else {
			printf("\n\nConnection %d created\n\n",i);
		}
	}

	sleep(2);

	/* get rtp packets */
	while(loop_state) {
//		ENDPOINTDRV_EVENT_PARM tEventParm;
//		int rc1 = IOCTL_STATUS_FAILURE;
//		int print_packet = 0;

	   	ENDPOINTDRV_PACKET_PARM tPacketParm;
	   	ENDPOINTDRV_PACKET_PARM tPacketParm_send;
	   	UINT8 data[1024] = {0};
	   	int rc2 = IOCTL_STATUS_SUCCESS;
	   	EPPACKET epPacket;
		EPPACKET epPacket_send;
		tcounter++;
//		int send_buf_pos_idx = 0;

/*		tEventParm.length = 0;
      	rc1 = ioctl( fd, ENDPOINTIOCTL_ENDPT_GET_EVENT, &tEventParm);
      	if( rc1 == IOCTL_STATUS_SUCCESS ) {
			if (tEventParm.event == EPEVT_DTMF5) {
				printf("EPEVT_DTMF5 detected, exiting while loop\n");
				loop_state = 0;
			}
			if (tEventParm.event == EPEVT_DTMF7) {
				printf("EPEVT_DTMF7 detected, printing packet\n");
				print_packet = 1;
			}
      	}
*/
		/* send rtp packet to the endpoint */
		epPacket_send.mediaType   = 0;
		epPacket_send.packetp     = malloc(172);
		memcpy(epPacket_send.packetp, &packet_buffer[buf_pos_idx], 172);

	   	tPacketParm_send.cnxId       = 0;
   		tPacketParm_send.state       = (ENDPT_STATE*)&endptObjState[0];
   		tPacketParm_send.length      = 172;
   		tPacketParm_send.bufDesc     = (int)&epPacket_send;
   		tPacketParm_send.epPacket    = &epPacket_send;
   		tPacketParm_send.epStatus    = EPSTATUS_DRIVER_ERROR;
   		tPacketParm_send.size        = sizeof(ENDPOINTDRV_PACKET_PARM);

		if (tcounter>51) {
	   		if ( ioctl( fd, ENDPOINTIOCTL_ENDPT_PACKET, &tPacketParm_send ) != IOCTL_STATUS_SUCCESS ) {
		  		printf("%s: error during ioctl", __FUNCTION__);
	   		} else {
				printf("Sent packet [%d]\n",tcounter-50);
			}

		}

		/* get rtp packets from endpoint */
		epPacket.mediaType   = 0;
		epPacket.packetp     = &data[0];
		tPacketParm.epPacket = &epPacket;
		tPacketParm.cnxId    = 0;
		tPacketParm.length   = 0;

		rc2 = ioctl( fd, ENDPOINTIOCTL_ENDPT_GET_PACKET, &tPacketParm);
      	if( rc2 == IOCTL_STATUS_SUCCESS )
      	{
			unsigned short sn = (unsigned short)(data[3] | data[2] <<8);
			if (tPacketParm.cnxId == 0 && tPacketParm.length == 172) {
				printf("Got packet [%d] l:%d cndId:%d sn:%d bpi=%d\n",1500-loop_state,tPacketParm.length,
						tPacketParm.cnxId, sn, buf_pos_idx);
				memcpy(&packet_buffer[buf_pos_idx], &data[0], tPacketParm.length);
				buf_pos_idx += tPacketParm.length;
				if (buf_pos_idx >= PACKET_BUFFER_SIZE)
					buf_pos_idx = 0;
			}
      	}
		loop_state--;
//		sleep(1);
	}

	/* Close connection */
	for ( i = 0; i < vrgEndptGetNumEndpoints(); i++ ) {
		ENDPOINTDRV_DELCONNECTION_PARM tDelConnectionParm;

		tDelConnectionParm.cnxId      = i;
   		tDelConnectionParm.state      = (ENDPT_STATE*)&endptObjState[i];
   		tDelConnectionParm.epStatus   = EPSTATUS_DRIVER_ERROR;
   		tDelConnectionParm.size       = sizeof(ENDPOINTDRV_DELCONNECTION_PARM);

		if ( ioctl( fd, ENDPOINTIOCTL_ENDPT_DELETE_CONNECTION, &tDelConnectionParm ) != IOCTL_STATUS_SUCCESS )
   		{
      		printf("%s: error during ioctl", __FUNCTION__);
   		} else {
			printf("\n\nConnection %d closed\n\n",i);
		}
	}
}



int signal_ringing(void)
{
  int i;


   /* Check whether value is on or off */
  for ( i = 0; i < vrgEndptGetNumEndpoints(); i++ )
     vrgEndptSignal( (ENDPT_STATE*)&endptObjState[i], -1, EPSIG_RINGING, -1, -1, -1 , -1);

  return 0;
}


int main(int argc, char **argv)
{
    int iflag = 0;
    int rflag = 0;
    int dflag = 0;
    int eflag = 0;
	int sflag = 0;
	int pflag = 0;
    int index;
    int c;

  if((endptUserCtrlBlock.fileHandle = open("/dev/bcmendpoint0", O_RDWR)) < 0) {
    printf("Endpint: open error %d\n", errno);
    return -1;
  }


  while ((c = getopt (argc, argv, "ierdsp")) != -1)
    switch (c)
      {
      case 'i':
	iflag=1;
	break;
      case 'r':
	rflag=1;
	break;
      case 'd':
	dflag=1;
	break;
	  case 'e':
    eflag=1;
	break;
	  case 'p':
	pflag=1;
	break;
	  case 's':
	sflag=1;
	break;
      case '?':
	if (optopt == 'c')
	  fprintf (stderr, "Option -%c requires an argument.\n", optopt);
	else if (isprint (optopt))
	  fprintf (stderr, "Unknown option `-%c'.\n", optopt);
	else
	  fprintf (stderr,
		   "Unknown option character `\\x%x'.\n",
		   optopt);
	return 1;
      default:
	abort ();
      }


  for (index = optind; index < argc; index++)
    printf ("Non-option argument %s\n", argv[index]);


  if(iflag)
    endpt_init();

  if(rflag)
    signal_ringing();

  if(eflag)
	event_loop();

  if(sflag)
    get_stats();

  if(pflag)
	create_connection();

  if(dflag)
    vrgEndptDeinit();
  

  return 0;
}




/*
*****************************************************************************
** FUNCTION:   vrgEndptDriverOpen
**
** PURPOSE:    Opens the Linux kernel endpoint driver.
**             This function should be the very first call used by the
**             application before isssuing any other endpoint APIs because
**             the ioctls for the endpoint APIs won't reach the kernel
**             if the driver is not successfully opened.
**
** PARAMETERS:
**
** RETURNS:    EPSTATUS
**
*****************************************************************************
*/
EPSTATUS vrgEndptDriverOpen(void)
{
   /* Open and initialize Endpoint driver */
   if( ( fd = open("/dev/bcmendpoint0", O_RDWR) ) == -1 )
   {
      printf( "%s: open error %d\n", __FUNCTION__, errno );
      return ( EPSTATUS_DRIVER_ERROR );
   }
   else
   {
      printf( "%s: Endpoint driver open success\n", __FUNCTION__ );
   }

   return ( EPSTATUS_SUCCESS );
}


/*
*****************************************************************************
** FUNCTION:   vrgEndptDriverClose
**
** PURPOSE:    Close endpoint driver
**
** PARAMETERS: None
**
** RETURNS:    EPSTATUS
**
** NOTE:
*****************************************************************************
*/
EPSTATUS vrgEndptDriverClose()
{
   if ( close( fd ) == -1 )
   {
      printf("%s: close error %d", __FUNCTION__, errno);
      return ( EPSTATUS_DRIVER_ERROR );
   }

   fd = NOT_INITIALIZED;

   return( EPSTATUS_SUCCESS );
}





/*
*****************************************************************************
** FUNCTION:   vrgEndptInit
**
** PURPOSE:    Module initialization for the VRG endpoint. The endpoint
**             module is responsible for controlling a set of endpoints.
**             Individual endpoints are initialized using the vrgEndptInit() API.
**
** PARAMETERS: country           - Country type
**             notifyCallback    - Callback to use for event notification
**             packetCallback           - Callback to use for endpt packets
**             getProvisionCallback     - Callback to get provisioned values.
**                                        May be set to NULL.
**             setProvisionCallback     - Callback to get provisioned values.
**                                        May be set to NULL.
**             packetReleaseCallback    - Callback to release ownership of
**                                        endpt packet back to caller
**             taskShutdownCallback     - Callback invoked to indicate endpt
**                                        task shutdown
**
** RETURNS:    EPSTATUS
**
** NOTE:       getProvisionCallback, setProvisionCallback,
**             packetReleaseCallback, and taskShutdownCallback are currently not used within
**             the DSL framework and should be set to NULL when
**             invoking this function.
**
*****************************************************************************
*/
EPSTATUS vrgEndptInit
(
   VRG_ENDPT_INIT_CFG        *endptInitCfg,
   endptEventCallback         notifyCallback,
   endptPacketCallback        packetCallback,
   endptGetProvCallback       getProvisionCallback,
   endptSetProvCallback       setProvisionCallback,
   endptPacketReleaseCallback packetReleaseCallback,
   endptTaskShutdownCallback  taskShutdownCallback
)
{
   ENDPOINTDRV_INIT_PARAM tStartupParam;
   /* BOS_TASK_ID eventTaskId    = 0; */
   /* BOS_TASK_ID packetTaskId   = 0; */

#if defined(BRCM_IDECT_CALLMGR) || defined(BRCM_EDECT_CALLMGR)
   int dect_fd;
   struct termios attrS0;
   struct termios attrS1;
   char write_buf[7] = "qwerty";
   char read_buf[7] = "\0";
   int count = 0;
#endif /* BRCM_IDECT_CALLMGR || BRCM_EDECT_CALLMGR */

   tStartupParam.endptInitCfg = endptInitCfg;
   tStartupParam.epStatus     = EPSTATUS_DRIVER_ERROR;
   tStartupParam.size         = sizeof(ENDPOINTDRV_INIT_PARAM);

   endptUserCtrlBlock.pEventCallBack   = notifyCallback;
   endptUserCtrlBlock.pPacketCallBack  = packetCallback;

#if defined(BRCM_IDECT_CALLMGR) || defined(BRCM_EDECT_CALLMGR)

   memset(&attrS1, 0, sizeof(attrS1));
#if 0
   attrS1.c_iflag = IXOFF | IXON | ICRNL;
   attrS1.c_oflag = OPOST;
   attrS1.c_cflag = B115200 | CS8 | CREAD | CLOCAL | HUPCL;
   attrS1.c_lflag = ISIG | ICANON | IEXTEN;
#else
   attrS1.c_cflag = B115200 | CS8 | CREAD | CLOCAL | HUPCL;
#endif
   attrS1.c_cc[ VTIME ] = 0;
   attrS1.c_cc[ VMIN ] = 1;

   if( ( dect_fd = open("/dev/ttyS1", O_RDWR) ) == -1 )
   {
   }
   else
   {
      if ( tcsetattr( dect_fd, TCSANOW, &attrS1 ) < 0 )
      {
      }

      close(dect_fd);
   }

#endif /* BRCM_IDECT_CALLMGR || BRCM_EDECT_CALLMGR */

   /* bGlobalTaskExit = FALSE; */

   /* Check if kernel driver is opened */

   if ( ioctl( endptUserCtrlBlock.fileHandle, ENDPOINTIOCTL_ENDPT_INIT, &tStartupParam ) != IOCTL_STATUS_SUCCESS )
   {
      return ( tStartupParam.epStatus );
   }
   else
   {
      /* Endpoint initialization was OK. Clear the deinit flag */
      /* endptDeinitialized = 0; */
   }

   /* Start the task for reading events from the endpoint driver */
   /* bosTaskCreate( "eptEvent", */
   /*                BOS_CFG_TASK_LINUX_DEFAULT_STACK_SIZE, */
   /*                BOS_CFG_TASK_HIGH_VAL, */
   /*                &EndpointEventTask, */
   /*                0, */
   /*                &eventTaskId ); */

   /* Start the task for reading packets from the endpoint driver */
   /* bosTaskCreate( "eptPacket", */
   /*                BOS_CFG_TASK_LINUX_DEFAULT_STACK_SIZE, */
   /*                BOS_CFG_TASK_HIGH_VAL, */
   /*                &EndpointPacketTask, */
   /*                0, */
   /*                &packetTaskId ); */

   /* if( (eventTaskId == 0) || (packetTaskId==0) ) */
   /* { */
   /*    return ( EPSTATUS_DRIVER_ERROR ); */
   /* } */

   return ( tStartupParam.epStatus );
}


/*
*****************************************************************************
** FUNCTION:   vrgEndptDeinit
**
** PURPOSE:    VRG endpoint module shutdown - call once during system shutdown.
**             This will shutdown all endpoints and free all resources used by
**             the VRG endpt manager. (i.e. this function should free all resources
**             allocated in vrgEndptInit() and vrgEndptCreate()).
**
** PARAMETERS: none
**
** RETURNS:    EPSTATUS
**             This function should only return an error under catastrophic
**             circumstances. i.e. Something that cannot be fixed by re-invoking
**             the module initialization function.
**
** NOTE:       It is assumed that this function is only called after all endpoint
**             tasks have been notified of a pending application reset, and each
**             one has acknowledged the notification. This implies that each endpoint
**             task is currently blocked, waiting to be resumed so that they may
**             complete the shutdown procedure.
**
**             It is also assumed that no task is currently blocked on any OS
**             resource that was created in the module initialization functions.
**
*****************************************************************************
*/
EPSTATUS vrgEndptDeinit( void )
{
   int filehandle = open("/dev/bcmendpoint0", O_RDWR);
   if ( filehandle == -1 )
   {
      return( EPSTATUS_DRIVER_ERROR );
   }

   if ( ioctl( fd, ENDPOINTIOCTL_ENDPT_DEINIT, NULL ) != IOCTL_STATUS_SUCCESS )
   {
   }

   /* bGlobalTaskExit = TRUE; */

   close( filehandle );

   /* endptDeinitialized = 1; */

   return( EPSTATUS_SUCCESS );
}





/*****************************************************************************
*  FUNCTION:   vrgEndptSignal
*
*  PURPOSE:    Generate a signal on the endpoint (or connection)
*
*  PARAMETERS: endptState  - state of the endpt object
*              cnxId       - connection identifier (-1 if not applicable)
*              signal      - signal type code (see EPSIG)
*              value       - signal value
*                          BR signal types - 1
*                          OO signal types - 0 == off, 1 == on
*                          TO signal types - 0 = stop/off, 1= start/on
*                          String types - (char *) cast to NULL-term string value
*
*  RETURNS:    EPSTATUS
*
*****************************************************************************/
EPSTATUS vrgEndptSignal
(
   ENDPT_STATE   *endptState,
   int            cnxId,
   EPSIG          signal,
   unsigned int   value,
   int            duration,
   int            period,
   int            repetition
)
{
   ENDPOINTDRV_SIGNAL_PARM tSignalParm;

   tSignalParm.cnxId    = cnxId;
   tSignalParm.state    = endptState;
   tSignalParm.signal   = signal;
   tSignalParm.value    = value;
   tSignalParm.epStatus = EPSTATUS_DRIVER_ERROR;
   tSignalParm.duration = duration;
   tSignalParm.period   = period;
   tSignalParm.repetition = repetition;
   tSignalParm.size     = sizeof(ENDPOINTDRV_SIGNAL_PARM);

   /* Check if kernel driver is opened */

   if ( ioctl( fd, ENDPOINTIOCTL_ENDPT_SIGNAL, &tSignalParm ) != IOCTL_STATUS_SUCCESS )
   {
   }

   return( tSignalParm.epStatus );
}


/*
*****************************************************************************
** FUNCTION:   vrgEndptGetNumEndpoints
**
** PURPOSE:    Retrieve the number of endpoints
**
** PARAMETERS: None
**
** RETURNS:    Number of endpoints
**
*****************************************************************************
*/
int vrgEndptGetNumEndpoints( void )
{
   ENDPOINTDRV_ENDPOINTCOUNT_PARM endpointCount;
   int retVal = 0;
   int filehandle = open("/dev/bcmendpoint0", O_RDWR);

   endpointCount.size = sizeof(ENDPOINTDRV_ENDPOINTCOUNT_PARM);

   if ( ioctl( filehandle, ENDPOINTIOCTL_ENDPOINTCOUNT, &endpointCount ) != IOCTL_STATUS_SUCCESS )
   {
   }
   else
   {
      retVal = endpointCount.endpointNum;
   }

   close(filehandle);

   return( retVal );
}


/*
*****************************************************************************
** FUNCTION:   vrgEndptCreate
**
** PURPOSE:    This function is used to create an VRG endpoint object.
**
** PARAMETERS: physId      (in)  Physical interface.
**             lineId      (in)  Endpoint line identifier.
**             endptState  (out) Created endpt object.
**
** RETURNS:    EPSTATUS
**
** NOTE:
*****************************************************************************
*/
EPSTATUS vrgEndptCreate( int physId, int lineId, VRG_ENDPT_STATE *endptState )
{
   ENDPOINTDRV_CREATE_PARM tInitParm;

   tInitParm.physId     = physId;
   tInitParm.lineId     = lineId;
   tInitParm.endptState = endptState;
   tInitParm.epStatus   = EPSTATUS_DRIVER_ERROR;
   tInitParm.size       = sizeof(ENDPOINTDRV_CREATE_PARM);

   /* Check if kernel driver is opened */

   if ( ioctl( fd, ENDPOINTIOCTL_ENDPT_CREATE, &tInitParm ) != IOCTL_STATUS_SUCCESS )
   {
   }

   return( tInitParm.epStatus );
}


/*
*****************************************************************************
** FUNCTION:   vrgEndptDestroy
**
** PURPOSE:    This function is used to destroy VRG endpoint object
**             (previously created with vrgEndptCreate)
**
** PARAMETERS: endptState (in) Endpt object to be destroyed.
**
** RETURNS:    EPSTATUS
**
** NOTE:
*****************************************************************************
*/
EPSTATUS vrgEndptDestroy( VRG_ENDPT_STATE *endptState )
{
   ENDPOINTDRV_DESTROY_PARM tInitParm;

   tInitParm.endptState = endptState;
   tInitParm.epStatus   = EPSTATUS_DRIVER_ERROR;
   tInitParm.size       = sizeof(ENDPOINTDRV_DESTROY_PARM);

   /* Check if kernel driver is opened */

   if ( ioctl( fd, ENDPOINTIOCTL_ENDPT_DESTROY, &tInitParm ) != IOCTL_STATUS_SUCCESS )
   {
   }

   return( tInitParm.epStatus );
}
