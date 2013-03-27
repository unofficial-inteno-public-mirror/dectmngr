
#include <dectctl.h>
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
#include <dectUtils.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dectNvsCtl.h>

#include "endpt.h"
#include "las.h"



/* globals */
int apifd;
int fd_max;
int num_endpoints = 6;
VRG_ENDPT_STATE endptObjState[6];
extern int endpoint_fd;
static int endpoint_country = VRG_COUNTRY_NORTH_AMERICA;
extern unsigned char DectNvsData[];
int proxy;
unsigned char nvs_buf[API_FP_LINUX_NVS_SIZE];

/* defines */
#define DECTDBG_FILE "/dev/dectdbg"
#define API_FILE "/dev/dect"
#define BUF_SIZE 2048

#define API_FP_MM_SET_REGISTRATION_MODE_REQ 0x4105
#define DECT_MAX_HANDSET 6



/* function prototypes */
int register_handsets_start(void);
int register_handsets_stop(void);
int ping_handsets(void);
void dectDrvWrite(void *data, int size);
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
EPSTATUS vrgEndptConsoleCmd( ENDPT_STATE *endptState, EPCONSOLECMD cmd, EPCMD_PARMS *consoleCmdParams );
void dectSetupPingingCall(int handset);
void nvs_update_ind(unsigned char *buf);


void RevertByteOrder(int length, unsigned char* data) 
{

}

static void dectCtlImplEepromCmd(DECT_EEPROMCMD_TYPE cmd, int address, int length, int data )
{

   unsigned char *tempPtr = NULL;
   switch (cmd)
   {
      case DECT_EEPROMCMD_GET_DATA:
      {
         tempPtr = (unsigned char*) malloc (7);
         if (tempPtr == NULL)
         {
            cmsLog_error("No more memory!!!");
            return;
         }
         *(tempPtr+0) = 0; // Length MSB
         *(tempPtr+1) = 5; // Length LSB
         *(tempPtr+2) = (unsigned char) ((API_FP_GET_EEPROM_REQ&0xff00)>>8);  // Primitive MSB
         *(tempPtr+3) = (unsigned char)  (API_FP_GET_EEPROM_REQ&0x00ff);      // Primitive LSB
         *(tempPtr+4) = (unsigned char)   (address&0x00ff);                   // EEPROM Address
         *(tempPtr+5) = (unsigned char)  ((address&0xff00)>>8);               // EEPROM Address
         *(tempPtr+6) = (unsigned char)   length;                             // Command Length
      }
      break;
      case DECT_EEPROMCMD_SET_DATA:
      {
         tempPtr = (unsigned char*) malloc (8);
         if (tempPtr == NULL)
         {
            cmsLog_error("No more memory!!!");
            return;
         }
         *(tempPtr+0) = 0; // Length MSB
         *(tempPtr+1) = 6; // Length LSB
         *(tempPtr+2) = (unsigned char) ((API_FP_SET_EEPROM_REQ&0xff00)>>8);  // Primitive MSB
         *(tempPtr+3) = (unsigned char)  (API_FP_SET_EEPROM_REQ&0x00ff);      // Primitive LSB
         *(tempPtr+4) = (unsigned char)  (address&0x00ff);                    // EEPROM Address
         *(tempPtr+5) = (unsigned char) ((address&0xff00)>>8);                // EEPROM Address
         *(tempPtr+6) = (unsigned char)   length;                             // Command Length
         *(tempPtr+7) = (unsigned char)   data;                               // Command Data
      }
      break;
      default:
      break;
   }

   if (tempPtr != NULL)
   {
      write_frame(tempPtr);
   }
}


static void dectCtlImplGetDectMode( void )
{
	int i, addr;

	
	for (i = 0; i < API_FP_LINUX_NVS_SIZE; i += 128) {

		addr |= (i & 0x00ff) << 8;                    // EEPROM Address
		addr |= (i & 0xff00) >> 8;                // EEPROM Address

		dectCtlImplEepromCmd(DECT_EEPROMCMD_GET_DATA, addr, 128, 0 );
	}
}

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
	dectSetupPingingCall(1);

	return 0;
}



/****************************************************************************
*
*  FUNCTION:   dectSetupPingingCall
*
*  PURPOSE:    Setup internal pinging call without involve the
*              PCM bus.
*
*  PARAMETERS:
*     handset -- The handset id to setup the call on.
*  RETURNS:
*     Nothing
*
*  NOTES:   Pinging involves sending a API_FP_CC_SETUP_REQ with a API_IE_CALLING_PARTY_NAME
*           info element with presentationInd set to API_PRESENTATION_HANSET_LOCATOR
****************************************************************************/
void dectSetupPingingCall(int handset)
{
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

	if( pCallingNameIe != NULL )
		{
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

			if( pingIeBlockPtr == NULL )
				{
					printf("dectCallMgrSetupPingingCall:  ApiBuildInfoElement FAILED for API_IE_CALLING_PARTY_NAME!!\n");
					return;
				}
		}
	else
		{
			printf("dectCallMgrSetupPingingCall: malloc FAILED for API_IE_CALLING_PARTY_NAME!!\n");
			return;
		}

	/*****************************************************
	 * create API_FP_CC_SETUP_REQ mail *
	 *****************************************************/
	if( pingIeBlockLength > 0 )
		{
			/* Allocate memory for mail */
			pingMailPtr = (ApiFpCcSetupReqType *) malloc( (sizeof(ApiFpCcSetupReqType)-1) + pingIeBlockLength );
			if (pingMailPtr != NULL)
				{
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

					/* Size must be in little endian  */
					((ApiFpCcSetupReqType *) pingMailPtr)->InfoElementLength = pingIeBlockLength;
				}
			else
				{
					printf("dectCallMgrSetupPingingCall: No more memory available for API_FP_CC_SETUP_REQ!!!\n");
					return;
				}
		}
	else
		{
			printf("dectCallMgrSetupPingingCall: zero pingIeBlockLength!!!\n");
			ApiFreeInfoElement( &pingIeBlockPtr );
			return;
		}


	/* Send the mail */
	printf("OUTPUT: API_FP_CC_SETUP_REQ (ping)\n");
	dectUtilSendMail(USER_TASK, ( (sizeof(ApiFpCcSetupReqType)-1) + pingIeBlockLength ), (unsigned char *)pingMailPtr );

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
void dectDrvWrite(void *data, int size)
{   
    int i;
    unsigned char* cdata = (unsigned char*)data;
    printf("\n[WDECT][%04d] - ",size);

    for (i=0 ; i<size ; i++) {
        printf("%02x ",cdata[i]);
    }
    printf("\n");

   if (-1 == write(proxy, data, size))
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


static void joe_dectNvsCtlGetData( unsigned char *pNvsData )
{
	int fd, ret;
	
	if (pNvsData == NULL) {
		
		printf("%s: error %d\n", __FUNCTION__, errno);
		return;
	}

	
	fd = open("/root/nvs", O_RDONLY);
	if (fd == -1) {
		perror("open");
		exit(EXIT_FAILURE);
	}

	ret = read(fd, pNvsData, API_FP_LINUX_NVS_SIZE);
	if (ret == -1) {
		perror("read");
		exit(EXIT_FAILURE);
	}

	ret = close(fd);
	if (ret == -1) {
		perror("close");
		exit(EXIT_FAILURE);
	}




	/* printf("ret: %d\n", ret); */
	/* memcpy( pNvsData, &DectNvsData[0], API_FP_LINUX_NVS_SIZE); */
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
  
  printf("sizeof(ApiFpLinuxInitReqType): %d\n", sizeof(ApiFpLinuxInitReqType));

  /* download the eeprom values to the DECT driver*/
  t = (ApiFpLinuxInitReqType*) malloc(sizeof(ApiFpLinuxInitReqType));
  t->Primitive = API_FP_LINUX_INIT_REQ;
  joe_dectNvsCtlGetData(t->NvsData);

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
ApiInfoElementType* ApiGetNextInfoElement(ApiInfoElementType *IeBlockPtr,
                                          rsuint16 IeBlockLength,
                                          ApiInfoElementType *IePtr)
{
	ApiInfoElementType *pEnd = (ApiInfoElementType*)((rsuint8*)IeBlockPtr + IeBlockLength);

	if (IePtr == NULL)
		{
			// return the first info element                                                                                                                   
			IePtr = IeBlockPtr;
		}
	else
		{
			// calc the address of the next info element                                                                                                       
			IePtr = (ApiInfoElementType*)((rsuint8*)IePtr + RSOFFSETOF(ApiInfoElementType, IeData) + IePtr->IeLength);
		}

	if (IePtr < pEnd)
		{
			return IePtr; // return the pointer to the next info element                                                                                       
		}
	return NULL; // return NULL to indicate that we have reached the end                                                                                 
}


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


/* void signal_dialtone(int i) { */
/*   ovrgEndptSignal( (ENDPT_STATE*)&endptObjState[i], -1, EPSIG_DIAL, 1, -1, -1 , -1); */
/* } */





/* int endpt_init(void) */
/* { */
/* 	VRG_ENDPT_INIT_CFG   vrgEndptInitCfg; */
/* 	int rc; */

/* 	printf("Initializing endpoint interface\n"); */

/* 	vrgEndptDriverOpen(); */

/* 	vrgEndptInitCfg.country = endpoint_country; */
/* 	vrgEndptInitCfg.currentPowerSource = 0; */

/* 	/\* Intialize endpoint *\/ */
/* 	rc = vrgEndptInit( &vrgEndptInitCfg, */
/* 			   NULL, */
/* 			   NULL, */
/* 			   NULL, */
/* 			   NULL, */
/* 			   NULL, */
/* 			   NULL ); */

/* 	return 0; */
/* } */





/* static void brcm_create_endpoints(void) */
/* { */
/* 	int i, rc; */

/* 	/\* Creating Endpt *\/ */
/* 	for ( i = 0; i < num_endpoints; i++ ) */
/* 	{ */
/* 		rc = vrgEndptCreate(i, i,(VRG_ENDPT_STATE *)&endptObjState[i]); */
/* 	} */
/* } */


void setup_ind(unsigned char *buf) {

  int endpt = 1;
  int len;
  unsigned char o_buf[5];
  unsigned char ast_buf[5];
  unsigned char *newMailPtr;
  int newMailSize;
  unsigned short IeBlockLength;
  ApiInfoElementType *IeBlockPtr = NULL;
  ApiCodecListType codecList;
  EPSIG signal;
  ENDPT_STATE endptState;


  /* Tell Asterisk about offhook event */
  len = send(proxy, buf, API_FP_LINUX_MAX_MAIL_SIZE, 0);


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
         ((ApiFpCcConnectReqType *) newMailPtr)->InfoElementLength = IeBlockLength;

         /* Send mail */
         printf("OUTPUT: API_FP_CC_CONNECT_REQ");
	 dectDrvWrite(newMailPtr, newMailSize);

         free(newMailPtr);


	 /* Signal offhook to endpoint */
	 /* endptState.lineId = 0; */
	 /* vrgEndptSendCasEvtToEndpt( &endptState, CAS_CTL_DETECT_EVENT, CAS_CTL_EVENT_OFFHOOK ); */
  
	 /* Dialtone */
	 /* signal_dialtone(1); */



	 
       }
   }
}









void connect_cfm(unsigned char *buf) {  

  ApiHandsetIdType handset;
  
  handset = ((ApiFpCcConnectCfmType*) buf)->CallReference.HandsetId;
  printf("Connected to handset %d\n", handset );
}


void release_ind(unsigned char *buf) {  

  ApiHandsetIdType handset;
  unsigned char o_buf[5];
  unsigned char ast_buf[5];
  ENDPT_STATE endptState;
  int len;

  handset = ((ApiFpCcConnectCfmType*) buf)->CallReference.HandsetId;

  /* Tell Asterisk about onhook event */
  len = send(proxy, buf, API_FP_LINUX_MAX_MAIL_SIZE, 0);


  /* write endpoint id to device */
  *(o_buf + 0) = ((API_FP_CC_RELEASE_RES & 0xff00) >> 8);
  *(o_buf + 1) = ((API_FP_CC_RELEASE_RES & 0x00ff) >> 0);
  *(o_buf + 2) = handset;
  *(o_buf + 3) = 0;
  *(o_buf + 4) = 0;

  printf("API_FP_CC_RELEASE_RES\n");
  dectDrvWrite(o_buf, 5);




  /* Signal onhook to endpoint */
  /* endptState.lineId = 0; */
  /* vrgEndptSendCasEvtToEndpt( &endptState, CAS_CTL_DETECT_EVENT, CAS_CTL_EVENT_OFFHOOK ); */


}



void nvs_update_ind(unsigned char *MailPtr) {

	/*Update the NVS*/
	int i;
	DECT_NVS_DATA nvsData;
	cmsLog_notice("INPUT: API_FP_LINUX_NVS_UPDATE_IND\n");

	nvsData.offset = ((ApiFpLinuxNvsUpdateIndType*) MailPtr)->NvsOffset;
	nvsData.nvsDataLength = ((ApiFpLinuxNvsUpdateIndType*) MailPtr)->NvsDataLength;
	nvsData.nvsDataPtr = (unsigned char *)&(((ApiFpLinuxNvsUpdateIndType*) MailPtr)->NvsData);

	printf("Offset: %x\n", nvsData.offset);
	printf("Length: %x\n", nvsData.nvsDataLength);
	
	memcpy(nvs_buf + nvsData.offset, nvsData.nvsDataPtr, nvsData.nvsDataLength);
	
	/* write data to flash */
	nvs_save();

	/* save NVS data to local copy */
	/* dectNvsCtlSetData(&nvsData); */

}







static void get_eeprom_cfm(unsigned char *MailPtr)
{
	int i = 0;
	unsigned char *Data;

	if ( ((ApiFpSetEepromCfmType*) MailPtr)->Status == RSS_SUCCESS ) {
		printf("bah 1\n");
		printf("%d byte(s) from EEPROM address 0x%04X fetched successfully !",
			      ((ApiFpGetEepromCfmType*) MailPtr)->DataLength,
			      ((ApiFpGetEepromCfmType*) MailPtr)->Address);
		
		Data = (unsigned char *)malloc(((((ApiFpGetEepromCfmType*) MailPtr)->DataLength)*sizeof(unsigned char)) + 1); 
		memcpy(Data,((ApiFpGetEepromCfmType*) MailPtr)->Data, 
		       (((ApiFpGetEepromCfmType*) MailPtr)->DataLength)*sizeof(unsigned char));
		
		memcpy(nvs_buf + ((ApiFpGetEepromCfmType*) MailPtr)->Address, ((ApiFpGetEepromCfmType*) MailPtr)->Data, 
		       (((ApiFpGetEepromCfmType*) MailPtr)->DataLength)*sizeof(unsigned char));
		nvs_dump();
		printf("\nByte(s) fetched");
		printf("address: %d\n", ((ApiFpGetEepromCfmType*) MailPtr)->Address);

		if(Data != NULL) {
			for(; i <((ApiFpGetEepromCfmType*) MailPtr)->DataLength ;i++) {
				if ((i % 8) == 0)
					printf("\n");
				printf("0x%02X\t",*(Data + i));
			}
			printf("\n");
		}
	} else {

		printf("Failed to get %d byte(s) from EEPROM address %04X ! error code = %d",
		       ((ApiFpGetEepromCfmType*) MailPtr)->DataLength,
		       ((ApiFpGetEepromCfmType*) MailPtr)->Address,
		       ((ApiFpGetEepromCfmType*) MailPtr)->Status);
	}
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
    send(proxy, buf, API_FP_LINUX_MAX_MAIL_SIZE, 0);
    break;

  case API_FP_LINUX_NVS_UPDATE_IND:
    printf("API_FP_LINUX_NVS_UPDATE_IND\n");
    nvs_update_ind(buf);
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

  case API_FP_GET_EEPROM_CFM:
    printf("API_FP_GET_EEPROM_CFM\n");
    get_eeprom_cfm(buf);
    break;

  default:
    printf("Unknown packet\n");
    break;
  }
  

}


int connect_proxy(void) {

  int len;
  struct sockaddr_in remote_addr;
  char buf[BUF_SIZE];

  memset(&remote_addr, 0, sizeof(remote_addr));
  remote_addr.sin_family = AF_INET;
  remote_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  remote_addr.sin_port = htons(7777);
  
  if ((proxy = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    return -1;
  }

  if (connect(proxy, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr)) < 0) {
    perror("connect");
    return -1;
  }

  printf("connected to proxy\n");

  return 0;
}



int daemonize(void) 
{

  fd_set rd_fdset;
  unsigned char buf[API_FP_LINUX_MAX_MAIL_SIZE];
  int res, len, i;
  fd_set rfds;
  ENDPT_STATE endptState;
  int pkt_len;

  dect_init();

  joe_dectNvsCtlGetData(nvs_buf);
  /* if (connect_proxy() < 0) { */
  /*   perror("connect_proxy"); */
  /*   return -1; */
  /* } */

  /* Read loop */
  FD_SET(proxy, &rd_fdset);
  fd_max = proxy;
  
  while (1) {
    
    memcpy(&rfds, &rd_fdset, sizeof(fd_set));

    if (res = select(fd_max + 1, &rfds, NULL, NULL, NULL) < 0) {
      perror("select");
      return -1;
    }

    if (FD_ISSET(proxy, &rfds)) {
	    
	    len = read(proxy, buf, 2);
	    pkt_len = buf[0] << 8; // MSB
	    pkt_len |= buf[1];     // LSB

	    printf("packet len: %d\n", pkt_len);
	    len = read(proxy, buf, pkt_len);

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


void las(void)
{
	
	dectLasInit();

}


void nvs_dump(void)
{
	int fd, ret;
	unsigned char *buf;

	fd = open("/root/nvs_dump", O_RDWR | O_CREAT);
	if (ret == -1) {
		perror("open");
		exit(EXIT_FAILURE);
	}

	buf = (ApiFpLinuxInitReqType*) malloc(API_FP_LINUX_NVS_SIZE);

	joe_dectNvsCtlGetData(buf);
	/* memcpy( buf, &DectNvsData[0], API_FP_LINUX_NVS_SIZE); */

	write(fd, (void *)(nvs_buf), API_FP_LINUX_NVS_SIZE);
}



void nvs_save(void)
{
	int fd, ret;
	unsigned char *buf;

	fd = open("/root/nvs", O_RDWR | O_CREAT);
	if (ret == -1) {
		perror("open");
		exit(EXIT_FAILURE);
	}

	write(fd, (void *)(nvs_buf), API_FP_LINUX_NVS_SIZE);
	if (ret == -1) {
		perror("write");
		exit(EXIT_FAILURE);
	}
	
	close(fd);
}


static void get_eeprom_data(void)
{
	dectCtlImplGetDectMode();

}


int main(int argc, char **argv)
{
  int iflag = 0;
  int rflag = 0;
  int sflag = 0;
  int pflag = 0;
  int dflag = 0;
  int lflag = 0;
  int nflag = 0;
  int eflag = 0;
  int index, ret , c;



  if (connect_proxy() < 0)
    return -1;

  printf("dectmngr\n");

  /* bosInit(); */

  while ((c = getopt (argc, argv, "rispdlne")) != -1)
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
      case 'l':
	lflag=1;
	break;
      case 'n':
	nflag=1;
	break;
      case 'e':
	eflag=1;
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

  if(lflag)
	  las();

  if(nflag)
	  nvs_dump();

  if(eflag)
	  get_eeprom_data();


  return 0;
}




