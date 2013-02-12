

#include <stdio.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <endpointdrv.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <dectshimdrv.h>
#include <ApiFpProject.h>
#include <endpointdrv.h>
#include <dectUtils.h>


/* globals */
int apifd;
int fd_max;
int num_endpoints = 6;
VRG_ENDPT_STATE endptObjState[6];
static int endpoint_fd = -1;
static int endpoint_country = VRG_COUNTRY_NORTH_AMERICA;

extern unsigned char DectNvsData[];

/* defines */
#define DECTDBG_FILE "/dev/dectdbg"
#define API_FILE "/dev/dect"


#define API_FP_MM_SET_REGISTRATION_MODE_REQ 0x4105
#define EPSTATUS_DRIVER_ERROR       EPSTATUS_UNKNOWN_TYPE



/* function prototypes */
int register_handsets_start(void);
int register_handsets_stop(void);
int ping_handsets(void);
static void dectDrvWrite(void *data, int size);
int write_frame(unsigned char *fr);
int daemonize(void);
static int open_file(int *fd_ptr, const char *filename);
static void update_max(int sd);
void handle_packet(unsigned char *buf);
void setup_ind(unsigned char *buf);
static void brcm_create_endpoints(void);
int endpt_init(void);
void signal_dialtone(int i);
void connect_cfm(unsigned char *buf);


EPSTATUS vrgEndptSendCasEvtToEndpt( ENDPT_STATE *endptState, CAS_CTL_EVENT_TYPE eventType, CAS_CTL_EVENT event );
EPSTATUS vrgEndptCreate( int physId, int lineId, VRG_ENDPT_STATE *endptState );

EPSTATUS vrgEndptInit
(
 VRG_ENDPT_INIT_CFG        *endptInitCfg,
 endptEventCallback         notifyCallback,
 endptPacketCallback        packetCallback,
 endptGetProvCallback       getProvisionCallback,
 endptSetProvCallback       setProvisionCallback,
 endptPacketReleaseCallback packetReleaseCallback,
 endptTaskShutdownCallback  taskShutdownCallback
 );


EPSTATUS ovrgEndptSignal
(
 ENDPT_STATE   *endptState,
 int            cnxId,
 EPSIG          signal,
 unsigned int   value,
 int            duration,
 int            period,
 int            repetition
 );


EPSTATUS vrgEndptDriverOpen(void);



static int open_file(int *fd_ptr, const char *filename)
{
  //  *fd_ptr = open(filename, O_RDWR);
  *fd_ptr = open(filename, O_RDWR | O_CREAT);

  if (*fd_ptr == -1) {
    fprintf(stderr, "open");
    perror(filename);

    return -1;
  }

  update_max(*fd_ptr);

  return 0;
}


static void update_max(int sd)
{
  if (sd > fd_max)
    fd_max = sd;
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


int ping_handsets(void) {
  
  printf("ping_handsets\n");

  return 0;
}






/****************************************************************************
*
*  FUNCTION:   dectDrvWrite
*
*  PURPOSE:    Write data to the DECT driver.
*
*  PARAMETERS: data - Data to be written
*              size - Size of data
*
*  RETURNS:    none
*
****************************************************************************/
static void dectDrvWrite(void *data, int size)
{   
    int i;
    unsigned char* cdata = (unsigned char*)data;
    printf("\n[WDECT][%04d] - ",size);
    for (i=0 ; i<size ; i++) {
        printf("%02x ",cdata[i]);
    }
    printf("\n");

   if (-1 == write(apifd, data, size))
   {
      perror("write to API failed");
      return;
   }

   return;
}




static void dectDrvWrite_debug(void *data, int size)
{   
    int i;
    unsigned char* cdata = (unsigned char*)data;
    printf("\n[WDECT][%04d] - ",size);
    for (i=0 ; i<size ; i++) {
        printf("%02x ",cdata[i]);
    }
    printf("\n");


   return;
}


static void dectNvsCtlGetData( unsigned char *pNvsData )
{
   if ( pNvsData == NULL ) 
   {   
     printf("%s: error %d\n", __FUNCTION__, errno);
      return;
   }

   memcpy( pNvsData, &DectNvsData[0], API_FP_LINUX_NVS_SIZE);
}


EPSTATUS vrgEndptSendCasEvtToEndpt( ENDPT_STATE *endptState, CAS_CTL_EVENT_TYPE eventType, CAS_CTL_EVENT event )
{
   ENDPOINTDRV_SENDCASEVT_CMD_PARM tCasCtlEvtParm;
   int fileHandle;

   tCasCtlEvtParm.epStatus      = EPSTATUS_DRIVER_ERROR;
   tCasCtlEvtParm.casCtlEvtType = eventType;
   tCasCtlEvtParm.casCtlEvt     = event;
   tCasCtlEvtParm.lineId        = endptState->lineId;
   tCasCtlEvtParm.size          = sizeof(ENDPOINTDRV_SENDCASEVT_CMD_PARM);
/*
   tHookstatParm.epStatus   = EPSTATUS_DRIVER_ERROR;
   tHookstatParm.hookstat   = casState;
   tHookstatParm.lineId     = endptState->lineId;
   tHookstatParm.size       = sizeof(ENDPOINTDRV_HOOKSTAT_CMD_PARM);
*/
//printf("tHookstatParm.size =%d, endpointdrv_hookstat_cmd_parm=%d\n", tHookstatParm.size, sizeof(ENDPOINTDRV_HOOKSTAT_CMD_PARM));

   fileHandle = open("/dev/bcmendpoint0", O_RDWR);

   if ( ioctl( fileHandle, ENDPOINTIOCTL_SEND_CAS_EVT, &tCasCtlEvtParm ) != IOCTL_STATUS_SUCCESS )
   {
      printf(("%s: error during ioctl", __FUNCTION__));
   }

   close(fileHandle);
   return( tCasCtlEvtParm.epStatus );
}



EPSTATUS vrgEndptConsoleCmd( ENDPT_STATE *endptState, EPCONSOLECMD cmd, EPCMD_PARMS *consoleCmdParams )
{
   ENDPOINTDRV_CONSOLE_CMD_PARM tConsoleParm;
   int fileHandle;

   fileHandle = open("/dev/bcmendpoint0", O_RDWR);

   tConsoleParm.state      = endptState;
   tConsoleParm.cmd        = cmd;
   tConsoleParm.lineId     = endptState->lineId;
   tConsoleParm.consoleCmdParams = consoleCmdParams;
   tConsoleParm.epStatus   = EPSTATUS_DRIVER_ERROR;
   tConsoleParm.size       = sizeof(ENDPOINTDRV_CONSOLE_CMD_PARM);

   if ( ioctl( fileHandle, ENDPOINTIOCTL_ENDPT_CONSOLE_CMD, &tConsoleParm ) != IOCTL_STATUS_SUCCESS )
   {
      printf(("%s: error during ioctl", __FUNCTION__));
   }

   close(fileHandle);

   return( tConsoleParm.epStatus );
}




static int dect_init(void)
{
  int fd, r;
  ApiFpLinuxInitReqType *t = NULL;
  DECTSHIMDRV_INIT_PARAM parm;
  ENDPT_STATE    endptState;
  EPCMD_PARMS    consoleCmdParams;


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

  
  /* download the eeprom values to the DECT driver*/
  t = (ApiFpLinuxInitReqType*) malloc(sizeof(ApiFpLinuxInitReqType));
  t->Primitive = API_FP_LINUX_INIT_REQ;
  dectNvsCtlGetData(t->NvsData);
  dectDrvWrite(t, sizeof(ApiFpLinuxInitReqType));



  /* initialize dect procesing in dect enpoint driver */
  memset( &consoleCmdParams,0, sizeof(consoleCmdParams) );
  memset( &endptState, 0, sizeof(endptState) );

  endptState.lineId = 0;

  /* if ( vrgEndptGetNumDectEndpoints() > 0 ) */
  /*   { */
  vrgEndptConsoleCmd( &endptState,
		      EPCMD_DECT_START_BUFF_PROC,
		      &consoleCmdParams );
  /* } */
  
  return r;
}



void RevertByteOrder(int length, unsigned char* data)
{
#if 0
   int i;
   unsigned char tmp;

   if (length <= 1 ) 
   {
      return;
   }

   for (i=0; i<length/2; i++)
   {
      tmp = data[i];
      data[i]=data[length-i-1];
	   data[length-i-1]=tmp;
   }
#endif
}


/****************************************************************************
*                              Macro definitions                              
****************************************************************************/


#ifndef RSOFFSETOF
  /*! \def RSOFFSETOF(type, field)
  * Computes the byte offset of \a field from the beginning of \a type. */
  #define RSOFFSETOF(type, field) ((size_t)(&((type*)0)->field))
#endif


/****************************************************************************
*                     Enumerations/Type definitions/Structs                   
****************************************************************************/


/****************************************************************************
*                            Global variables/const                           
****************************************************************************/

/****************************************************************************
*                                Implementation
****************************************************************************/


/***************************************************************************/
void ApiBuildInfoElement(ApiInfoElementType **IeBlockPtr,
                         rsuint16 *IeBlockLengthPtr,
                         ApiIeType Ie,
                         rsuint8 IeLength,
                         rsuint8 *IeData)
{
  rsuint16 newLength = *IeBlockLengthPtr + RSOFFSETOF(ApiInfoElementType, IeData) + IeLength;

  /* Ie is in little endian inside the infoElement list while all arguments to function are in bigEndian */
  rsuint16 targetIe = Ie;
  RevertByteOrder( sizeof(ApiIeType),(rsuint8*)&targetIe   );          

  /* Allocate / reallocate a heap block to store (append) the info elemte in. */
  ApiInfoElementType *p = malloc(newLength);
  if (p == NULL)
  {
    // We failed to get e new block.
    // We free the old and return with *IeBlockPtr == NULL.
    ApiFreeInfoElement(IeBlockPtr);
    *IeBlockLengthPtr = 0;
  }
  else
  {
    // Update *IeBlockPointer with the address of the new block
    //     *IeBlockPtr = p;
    if( *IeBlockPtr != NULL )
    {
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

/***************************************************************************/
void ApiFreeInfoElement(ApiInfoElementType **IeBlockPtr)
{
  free((void*)*IeBlockPtr);
  *IeBlockPtr = NULL;
}


void signal_dialtone(int i) {
  ovrgEndptSignal( (ENDPT_STATE*)&endptObjState[i], -1, EPSIG_DIAL, 1, -1, -1 , -1);
}


EPSTATUS vrgEndptDriverOpen(void)
{
	/* Open and initialize Endpoint driver */
	if( ( endpoint_fd = open("/dev/bcmendpoint0", O_RDWR) ) == -1 )
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

	tStartupParam.endptInitCfg = endptInitCfg;
	tStartupParam.epStatus     = EPSTATUS_DRIVER_ERROR;
	tStartupParam.size         = sizeof(ENDPOINTDRV_INIT_PARAM);

	/* Check if kernel driver is opened */
	if ( ioctl( endpoint_fd, ENDPOINTIOCTL_ENDPT_INIT, &tStartupParam ) != IOCTL_STATUS_SUCCESS )
		return ( tStartupParam.epStatus );

	return ( tStartupParam.epStatus );
}



int endpt_init(void)
{
	VRG_ENDPT_INIT_CFG   vrgEndptInitCfg;
	int rc;

	printf("Initializing endpoint interface\n");

	vrgEndptDriverOpen();

	vrgEndptInitCfg.country = endpoint_country;
	vrgEndptInitCfg.currentPowerSource = 0;

	/* Intialize endpoint */
	rc = vrgEndptInit( &vrgEndptInitCfg,
			   NULL,
			   NULL,
			   NULL,
			   NULL,
			   NULL,
			   NULL );

	return 0;
}



EPSTATUS vrgEndptCreate( int physId, int lineId, VRG_ENDPT_STATE *endptState )
{
	ENDPOINTDRV_CREATE_PARM tInitParm;

	tInitParm.physId     = physId;
	tInitParm.lineId     = lineId;
	tInitParm.endptState = endptState;
	tInitParm.epStatus   = EPSTATUS_DRIVER_ERROR;
	tInitParm.size       = sizeof(ENDPOINTDRV_CREATE_PARM);

	/* Check if kernel driver is opened */

	if ( ioctl( endpoint_fd, ENDPOINTIOCTL_ENDPT_CREATE, &tInitParm ) != IOCTL_STATUS_SUCCESS )
		{
		}

	return( tInitParm.epStatus );
}


static void brcm_create_endpoints(void)
{
	int i, rc;

	/* Creating Endpt */
	for ( i = 0; i < num_endpoints; i++ )
	{
		rc = vrgEndptCreate(i, i,(VRG_ENDPT_STATE *)&endptObjState[i]);
	}
}


void setup_ind(unsigned char *buf) {

  int endpt = 1;
  unsigned char o_buf[5];
  unsigned char *newMailPtr;
  int newMailSize;
  unsigned short IeBlockLength;
  ApiInfoElementType *IeBlockPtr = NULL;
  ApiCodecListType codecList;
  EPSIG signal;
  ENDPT_STATE state;  

  /* write endpoint id to device */
  
  *(o_buf + 0) = ((API_FP_CC_SETUP_RES & 0xff00) >> 8);
  *(o_buf + 1) = ((API_FP_CC_SETUP_RES & 0x00ff) >> 0);
  *(o_buf + 2) = 1;
  *(o_buf + 3) = 0;
  *(o_buf + 4) = 0;

  printf("API_FP_CC_SETUP_RES\n");
  dectDrvWrite(o_buf, 5);


  /* dectOffhookAck */

  //  ApiCodecListType codecList;
  //  ApiCodecInfoType  Codec;

  codecList.NegotiationIndicator = API_NI_POSSIBLE;
  codecList.NoOfCodecs = 1;
  codecList.Codec[0].Codec = API_CT_G722; /*  = 0x03 !< G.722, information transfer rate 64 kbit/s */
  codecList.Codec[0].MacDlcService = API_MDS_1_MD; //   = 0x00, /*!< DLC service LU1, MAC service: In_minimum_delay */
  codecList.Codec[0].CplaneRouting = API_CPR_CS; //    = 0x00, /*!< CS only */
  codecList.Codec[0].SlotSize = API_SS_LS640; // = 0x01, /*!< Long slot; j = 640 */
  

  
  /* Build API_IE_CODEC_LIST infoElement with a single codec in our list*/
     IeBlockLength = 0;
   ApiBuildInfoElement( &IeBlockPtr,
                        &IeBlockLength,
                        API_IE_CODEC_LIST,
                        sizeof(ApiCodecListType),
                        (unsigned char*)(&codecList));

   if( IeBlockPtr != NULL ) {
     
     /* Send connect request */
     newMailSize = (sizeof(ApiFpCcConnectReqType)-1) + IeBlockLength;
     newMailPtr = (unsigned char *) malloc( newMailSize );

     if (newMailPtr != NULL) {

         ((ApiFpCcConnectReqType *) newMailPtr)->Primitive                 = API_FP_CC_CONNECT_REQ;
         ((ApiFpCcConnectReqType *) newMailPtr)->CallReference.HandsetId   = 1; //handsetId;
	 

         /* Copy over infoElements */
         memcpy( &(((ApiFpCcConnectReqType *) newMailPtr)->InfoElement[0]), IeBlockPtr, IeBlockLength );
         ApiFreeInfoElement( &IeBlockPtr );

         /* Size must be in little endian  */
         RevertByteOrder( sizeof(unsigned short),(unsigned char*)&IeBlockLength   );
         ((ApiFpCcConnectReqType *) newMailPtr)->InfoElementLength = IeBlockLength;

         /* Send mail */
         printf("OUTPUT: API_FP_CC_CONNECT_REQ");
	 dectDrvWrite(newMailPtr, newMailSize);

         free(newMailPtr);



	 /* Signal dialtone */
	 signal_dialtone(1);
       }
   }
}






EPSTATUS ovrgEndptSignal
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

	if ( ioctl( endpoint_fd, ENDPOINTIOCTL_ENDPT_SIGNAL, &tSignalParm ) != IOCTL_STATUS_SUCCESS )
		{
		}

	return( tSignalParm.epStatus );
}



void connect_cfm(unsigned char *buf) {  
  ApiHandsetIdType handset;
  
  handset = ((ApiFpCcConnectCfmType*) buf)->CallReference.HandsetId;
  printf("Connected to handset %d\n", handset );
}


void release_ind(unsigned char *buf) {  
  ApiHandsetIdType handset;
  unsigned char o_buf[5];
  handset = ((ApiFpCcConnectCfmType*) buf)->CallReference.HandsetId;

  /* write endpoint id to device */
  
  *(o_buf + 0) = ((API_FP_CC_RELEASE_RES & 0xff00) >> 8);
  *(o_buf + 1) = ((API_FP_CC_RELEASE_RES & 0x00ff) >> 0);
  *(o_buf + 2) = handset;
  *(o_buf + 3) = 0;
  *(o_buf + 4) = 0;

  printf("API_FP_CC_RELEASE_RES\n");
  dectDrvWrite(o_buf, 5);

}





void handle_packet(unsigned char *buf) {

  RosPrimitiveType primitive;
  
  primitive = ((recDataType *) buf)->PrimitiveIdentifier;
      
  switch (primitive) {
	
  case API_FP_CC_SETUP_IND:
    printf("API_FP_CC_SETUP_IND\n");
    setup_ind(buf);
    break;

  case API_FP_CC_REJECT_IND:
    printf("API_FP_CC_REJECT_IND\n");
    break;

  case API_FP_CC_RELEASE_IND:
    printf("API_FP_CC_RELEASE_IND\n");
    release_ind(buf);
    break;

  case API_FP_CC_CONNECT_CFM:
    printf("API_FP_CC_CONNECT_CFM\n");
    connect_cfm(buf);
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

  case API_FP_MM_REGISTRATION_COMPLETE_IND:
    printf("API_FP_MM_REGISTRATION_COMPLETE_IND\n");
    break;

  case API_FP_LINUX_INIT_CFM:
    printf("API_FP_LINUX_INIT_CFM\n");
    break;




  default:
    printf("Unknown packet\n");
    break;
  }
  

}



int daemonize(void) 
{

  fd_set rd_fdset;
  unsigned char buf[API_FP_LINUX_MAX_MAIL_SIZE];
  int res, len, i;
  fd_set rfds;
  ENDPT_STATE    endptState;
  
  /* endpt_init(); */
  /* brcm_create_endpoints(); */

  /* signal_dialtone(1);   */
  /* endptState.lineId = 1; */
  /* vrgEndptSendCasEvtToEndpt( &endptState, CAS_CTL_DETECT_EVENT, CAS_CTL_EVENT_OFFHOOK ); */


  FD_SET(apifd, &rd_fdset);

  /* main loop */
  while (1) {
    
    memcpy(&rfds, &rd_fdset, sizeof(fd_set));;
    
    if (res = select(fd_max + 1, &rfds, NULL, NULL, NULL) < 0) {
      perror("select");
      return -1;
    }

    if (FD_ISSET(apifd, &rfds)) {
    
      len = read(apifd, buf, sizeof(buf));
      
      if (len > 0) {

	/* debug printout */
	printf("\n[RDECT][%04d] - ", len);
	for (i = 0; i < len; i++)
	  printf("%02x ", buf[i]);
	printf("\n");

      }
      
      handle_packet(buf);

    }

  }

  return 0;
}





int main(int argc, char **argv)
{
  int iflag = 0;
  int rflag = 0;
  int sflag = 0;
  int pflag = 0;
  int dflag = 0;
  int index, ret , c;


  //  printf("dectmngr1 \n");
  if (ret = open_file(&apifd, "/dev/dect") < 0)
    return -1;

  printf("dectmngr\n");

  /* bosInit(); */

  while ((c = getopt (argc, argv, "rispd")) != -1)
    switch (c)
      {
      case 'r':
	rflag=1;
	break;
      case 'i':
	iflag=1;
	break;
      case 's':
	sflag=1;
	break;
      case 'p':
	pflag=1;
	break;
      case 'd':
	dflag=1;
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
    dect_init();

  if(rflag)
    register_handsets_start();

  if(sflag)
    register_handsets_stop();

  if(pflag)
    ping_handsets();

  if(dflag)
    daemonize();


  return 0;
}




