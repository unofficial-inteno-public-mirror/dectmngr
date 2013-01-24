

#include <stdio.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <endpointdrv.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

/* globals */
int apifd;

/* defines */
#define DECTDBG_FILE "/dev/dectdbg"
#define API_FILE "/dev/dect"

#define API_FP_MM_SET_REGISTRATION_MODE_REQ 0x4105



/* Global mail primitives:                                                                                                                             
  API_FP_MM_GET_ID_REQ = 0x4004,                                                                                                                       
  API_FP_MM_GET_ID_CFM = 0x4005,                                                                                                                       
  API_FP_MM_GET_MODEL_REQ = 0x4006,                                                                                                                    
  API_FP_MM_GET_MODEL_CFM = 0x4007,                                                                                                                    
  API_FP_MM_SET_ACCESS_CODE_REQ = 0x4008,                                                                                                              
  API_FP_MM_SET_ACCESS_CODE_CFM = 0x4009,                                                                                                              
  API_FP_MM_GET_ACCESS_CODE_REQ = 0x400A,                                                                                                              
  API_FP_MM_GET_ACCESS_CODE_CFM = 0x400B,                                                                                                              
  API_FP_MM_GET_REGISTRATION_COUNT_REQ = 0x4100,                                                                                                       
  API_FP_MM_GET_REGISTRATION_COUNT_CFM = 0x4101,                                                                                                       
  API_FP_MM_GET_HANDSET_IPUI_REQ = 0x4109,                                                                                                             
  API_FP_MM_GET_HANDSET_IPUI_CFM = 0x410A,                                                                                                             
  API_FP_MM_HANDSET_PRESENT_IND = 0x4108,                                                                                                              
  API_FP_MM_STOP_PROTOCOL_REQ = 0x410B,                                                                                                                
  API_FP_MM_START_PROTOCOL_REQ = 0x410D,                                                                                                               
  API_FP_MM_HANDSET_DETACH_IND = 0x410E,                                                                                                               
  API_FP_MM_EXT_HIGHER_LAYER_CAP2_REQ = 0x410C,                                                                                                        
  API_FP_MM_SET_REGISTRATION_MODE_REQ = 0x4105,                                                                                                        
  API_FP_MM_SET_REGISTRATION_MODE_CFM = 0x4106,                                                                                                        
  API_FP_MM_REGISTRATION_COMPLETE_IND = 0x4107,                                                                                                        
  API_FP_MM_DELETE_REGISTRATION_REQ = 0x4102,                                                                                                          
  API_FP_MM_DELETE_REGISTRATION_CFM = 0x4103,                                                                                                          
  API_FP_MM_HANDSET_DEREGISTED_IND = 0x410F,                                                                                                           
  API_FP_MM_UNITDATA_REQ = 0x4180,                                                                                                                     
  API_FP_MM_ALERT_BROADCAST_REQ = 0x4182,                                                                                                              
  The global mail primitives MUST be defined in Global.h! */










/* function prototypes */
int register_handsets_start(void);
int register_handsets_stop(void);
int ping_handsets(void);
static void dectDrvWrite(void *data, int size);
int write_frame(unsigned char *fr);




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


/* Do we need to do this? */

/* int dectCtlImplHwInit(void) */
/* { */
/*    int status = 0; */
/*    int fileHandle; */
/*    DECTSHIMDRV_INIT_PARAM parm; */

/*    fileHandle = open("/dev/dectshim", O_RDWR); */

/*    if( fileHandle == -1 ) */
/*    { */
/*       printf( "%s: open error %d\n", __FUNCTION__, errno ); */
/*       return -1; */
/*    } */
/*    else if(( status = ioctl( fileHandle, DECTSHIMIOCTL_INIT_CMD, &parm )) != 0 ) */
/*    { */
/*       cmsLog_error("%s: error during ioctl", __FUNCTION__); */
/*    } */

/*    close(fileHandle); */

/*    // *** Auto unload the module */
/*    // grep dect /proc/modules &>/dev/null ||  - no grep on the system */
/*    status = system("insmod /lib/modules/2.6.30/extra/dect.ko 2>/dev/null"); */
/*    if (status == -1) */
/*    { */
/*       cmsLog_error("Module load command failed"); */
/*    } */
/*    else if (WEXITSTATUS(status)) */
/*    { */
/*       cmsLog_error("dect.ko module not loaded (already loaded or not found)"); */
/*    } */
/*    else */
/*    { */
/*       cmsLog_notice("dect kernel module auto loaded"); */
/*    } */
/*    return( status ); */
/* } */




/****************************************************************************
*
*  FUNCTION:   dectCtlImplEepromCmd
*
*  PURPOSE:    Send EEPROM cmd to the DECT base module
*
*  PARAMETERS:
*     cmd     - Get or Set command
*     address - Address of EEPROM
*     length  - data length
*     data    - New data for Set operation
*  RETURNS:
*     Nothing
*
*  NOTES:
*
****************************************************************************/
/* void dectCtlImplEepromCmd(DECT_EEPROMCMD_TYPE cmd, int address, int length, int data ) */
/* { */

/*    unsigned char *tempPtr = NULL; */
/*    switch (cmd) */
/*    { */
/*       case DECT_EEPROMCMD_GET_DATA: */
/*       { */
/*          tempPtr = (unsigned char*) malloc (7); */
/*          if (tempPtr == NULL) */
/*          { */
/*             cmsLog_error("No more memory!!!"); */
/*             return; */
/*          } */
/*          *(tempPtr+0) = 0; // Length MSB */
/*          *(tempPtr+1) = 5; // Length LSB */
/*          *(tempPtr+2) = (unsigned char) ((API_FP_GET_EEPROM_REQ&0xff00)>>8);  // Primitive MSB */
/*          *(tempPtr+3) = (unsigned char)  (API_FP_GET_EEPROM_REQ&0x00ff);      // Primitive LSB */
/*          *(tempPtr+4) = (unsigned char)   (address&0x00ff);                   // EEPROM Address */
/*          *(tempPtr+5) = (unsigned char)  ((address&0xff00)>>8);               // EEPROM Address */
/*          *(tempPtr+6) = (unsigned char)   length;                             // Command Length */
/*       } */
/*       break; */
/*       case DECT_EEPROMCMD_SET_DATA: */
/*       { */
/*          tempPtr = (unsigned char*) malloc (8); */
/*          if (tempPtr == NULL) */
/*          { */
/*             cmsLog_error("No more memory!!!"); */
/*             return; */
/*          } */
/*          *(tempPtr+0) = 0; // Length MSB */
/*          *(tempPtr+1) = 6; // Length LSB */
/*          *(tempPtr+2) = (unsigned char) ((API_FP_SET_EEPROM_REQ&0xff00)>>8);  // Primitive MSB */
/*          *(tempPtr+3) = (unsigned char)  (API_FP_SET_EEPROM_REQ&0x00ff);      // Primitive LSB */
/*          *(tempPtr+4) = (unsigned char)  (address&0x00ff);                    // EEPROM Address */
/*          *(tempPtr+5) = (unsigned char) ((address&0xff00)>>8);                // EEPROM Address */
/*          *(tempPtr+6) = (unsigned char)   length;                             // Command Length */
/*          *(tempPtr+7) = (unsigned char)   data;                               // Command Data */
/*       } */
/*       break; */
/*       default: */
/*       break; */
/*    } */

/*    if (tempPtr != NULL) */
/*    { */
/*       bosMutexAcquire(&semMailFrameQueueSem); */
/*       EnQueue(&mailFrameQueue, tempPtr); */
/*       bosMutexRelease(&semMailFrameQueueSem); */
/*    } */
/* } */

/****************************************************************************
*
*  FUNCTION:   dectCtlImplRegistrationCmd
*
*  PURPOSE:    Send Registration cmd to the DECT base module
*
*  PARAMETERS:
*     cmd  - Registration command
*     dataPtr - pointer to data related with each command
*     length - data length
*  RETURNS:
*     Nothing
*
*  NOTES:
*
****************************************************************************/
/* void dectCtlImplRegistrationCmd(DECT_REGCMD_TYPE cmd, unsigned char* dataPtr, int length) */
/* { */
/*    unsigned char *tempPtr = NULL; */

/*    switch (cmd) */
/*    { */
/*       /\* case DECT_REGCMD_DELETE_HANDSET: *\/ */
/*       /\* { *\/ */
/*       /\*    if (length < 1) *\/ */
/*       /\*    { *\/ */
/*       /\*       cmsLog_error("No handset is specify to delete!"); *\/ */
/*       /\*    } *\/ */

/*       /\*    tempPtr = (unsigned char*) malloc(5); *\/ */
/*       /\*    if (tempPtr == NULL) *\/ */
/*       /\*    { *\/ */
/*       /\*       cmsLog_error("No more memory!!!"); *\/ */
/*       /\*       return; *\/ */
/*       /\*    } *\/ */

/*       /\*    *(tempPtr+0) = 0; // Length MSB *\/ */
/*       /\*    *(tempPtr+1) = 3; // Length LSB *\/ */
/*       /\*    *(tempPtr+2) = (unsigned char) ((API_FP_MM_DELETE_REGISTRATION_REQ&0xff00)>>8); // Primitive MSB *\/ */
/*       /\*    *(tempPtr+3) = (unsigned char) (API_FP_MM_DELETE_REGISTRATION_REQ&0x00ff);      // Primitive LSB *\/ */
/*       /\*    *(tempPtr+4) = (unsigned char) dataPtr[0];      // Handset Id *\/ */
/*       /\* } *\/ */
/*       /\* break; *\/ */

/*       case DECT_REGCMD_PING_HANDSET: */
/*       { */
/*          if (length < 1) */
/*          { */
/*             cmsLog_error("No handset specifed to ping!"); */
/*             return; */
/*          } */

/*          int handset = (int)dataPtr[0]; */
/*          /\* if the handset ID is 0, we want to ping all the handsets *\/ */
/*          if( handset == 0 ) */
/*          { */
/*             int temp; */
/*             /\* set each ping timeout *\/ */
/*             for( temp = 0; temp < DECT_MAX_HANDSET ; temp++ ) */
/*             { */
/*                pingHandset[temp] = TRUE; */
/*             } */
/*          } */
/*          else */
/*          { */
/*             pingHandset[handset-1] = TRUE; */
/*          } */
/*       } */
/*       break; */

/*       case DECT_REGCMD_START_REG: */
/*       { */
/*          tempPtr = (unsigned char*) malloc(5); */
/*          if (tempPtr == NULL) */
/*          { */
/*             cmsLog_error("No more memory!!!"); */
/*             return; */
/*          } */
/*          *(tempPtr+0) = 0; // Length MSB */
/*          *(tempPtr+1) = 3; // Length LSB */
/*          *(tempPtr+2) = (unsigned char) ((API_FP_MM_SET_REGISTRATION_MODE_REQ&0xff00)>>8); // Primitive MSB */
/*          *(tempPtr+3) = (unsigned char) (API_FP_MM_SET_REGISTRATION_MODE_REQ&0x00ff);      // Primitive LSB */
/*          *(tempPtr+4) = 1; //enable registration */

/*          /\* Start the registration timer *\/ */
/*          bosTimeGetMs( &dectRegTimeout ); */

/*          /\* Attempt to open the registration window. */
/*          *\/ */
/*          regWndOpening = TRUE; */
/*       } */
/*       break; */

/*       case DECT_REGCMD_STOP_REG: */
/*       { */
/*          tempPtr = (unsigned char*) malloc(5); */
/*          if (tempPtr == NULL) */
/*          { */
/*             cmsLog_error("No more memory!!!"); */
/*             return; */
/*          } */
/*          *(tempPtr+0) = 0; // Length MSB */
/*          *(tempPtr+1) = 3; // Length LSB */
/*          *(tempPtr+2) = (unsigned char) ((API_FP_MM_SET_REGISTRATION_MODE_REQ&0xff00)>>8); // Primitive MSB */
/*          *(tempPtr+3) = (unsigned char) (API_FP_MM_SET_REGISTRATION_MODE_REQ&0x00ff);      // Primitive LSB */
/*          *(tempPtr+4) = 0; //disable registration */

/*          /\* Stop the registration timer *\/ */
/*          dectRegTimeout = 0; */

/*          /\* Attempt to close the registration window. */
/*          *\/ */
/*          regWndOpening = FALSE; */
/*       } */
/*       break; */

   /*    case DECT_REGCMD_GET_REG_COUNT: */
   /*    { */
   /*       tempPtr = (unsigned char*) malloc(4); */
   /*       if (tempPtr == NULL) */
   /*       { */
   /*          cmsLog_error("No more memory!!!"); */
   /*          return; */
   /*       } */
   /*       *(tempPtr+0) = 0; // Length MSB */
   /*       *(tempPtr+1) = 2; // Length LSB */
   /*       *(tempPtr+2) = (unsigned char) ((API_FP_MM_GET_REGISTRATION_COUNT_REQ&0xff00)>>8); // Primitive MSB */
   /*       *(tempPtr+3) = (unsigned char) (API_FP_MM_GET_REGISTRATION_COUNT_REQ&0x00ff);      // Primitive LSB */
   /*    } */
   /*    break; */

   /*    case DECT_REGCMD_SET_ACCESS_CODE: */
   /*    { */
   /*       if (length != 4 ) */
   /*       { */
   /*          cmsLog_error("Access code shoule be 4 digits!!! Input access code is %s\n", dataPtr); */
   /*       } */

   /*       tempPtr = (unsigned char*) malloc(6); */
   /*       if (tempPtr == NULL) */
   /*       { */
   /*          cmsLog_error("No more memory!!!"); */
   /*          return; */
   /*       } */

   /*       *(tempPtr+0) = 0; // Length MSB */
   /*       *(tempPtr+1) = 4; // Length LSB */
   /*       *(tempPtr+2) = (unsigned char) ((API_FP_MM_SET_ACCESS_CODE_REQ&0xff00)>>8); // Primitive MSB */
   /*       *(tempPtr+3) = (unsigned char) (API_FP_MM_SET_ACCESS_CODE_REQ&0x00ff);      // Primitive LSB */
   /*       *(tempPtr+4) = ((dataPtr[0]&0x0F) <<4); */
   /*       *(tempPtr+4) += (dataPtr[1]&0x0F); */
   /*       *(tempPtr+5) = ((dataPtr[2]&0x0F) <<4); */
   /*       *(tempPtr+5) += (dataPtr[3]&0x0F); */

   /*       /\* Cache the new access code until we have confirmation it has been */
   /*       ** successfully set. */
   /*       *\/ */
   /*       memset ( newAccessCode, 0, sizeof( newAccessCode ) ); */
   /*       strncpy ( newAccessCode, */
   /*                 (char *)dataPtr, */
   /*                 ( length < DECT_AC_MAX_LEN ) ? length : DECT_AC_MAX_LEN  ); */
   /*    } */
   /*    break; */

   /*    case DECT_REGCMD_GET_ACCESS_CODE: */
   /*    { */
   /*       tempPtr = (unsigned char*) malloc(4); */
   /*       if (tempPtr == NULL) */
   /*       { */
   /*          cmsLog_error("No more memory!!!"); */
   /*          return; */
   /*       } */
   /*       *(tempPtr+0) = 0; // Length MSB */
   /*       *(tempPtr+1) = 2; // Length LSB */
   /*       *(tempPtr+2) = (unsigned char) ((API_FP_MM_GET_ACCESS_CODE_REQ&0xff00)>>8); // Primitive MSB */
   /*       *(tempPtr+3) = (unsigned char) (API_FP_MM_GET_ACCESS_CODE_REQ&0x00ff);      // Primitive LSB */
   /*    } */
   /*    break; */

   /*    case DECT_REGCMD_GET_ID: */
   /*    { */
   /*       tempPtr = (unsigned char*) malloc(4); */
   /*       if (tempPtr == NULL) */
   /*       { */
   /*          cmsLog_error("No more memory!!!"); */
   /*          return; */
   /*       } */
   /*       *(tempPtr+0) = 0; // Length MSB */
   /*       *(tempPtr+1) = 2; // Length LSB */
   /*       *(tempPtr+2) = (unsigned char) ((API_FP_MM_GET_ID_REQ&0xff00)>>8); // Primitive MSB */
   /*       *(tempPtr+3) = (unsigned char) (API_FP_MM_GET_ID_REQ&0x00ff);      // Primitive LSB */
   /*    } */
   /*    break; */

   /*    case DECT_REGCMD_GET_FW_VERSION: */
   /*    { */
   /*       tempPtr = (unsigned char*) malloc(4); */
   /*       if (tempPtr == NULL) */
   /*       { */
   /*          cmsLog_error("No more memory!!!"); */
   /*          return; */
   /*       } */
   /*       *(tempPtr+0) = 0; // Length MSB */
   /*       *(tempPtr+1) = 2; // Length LSB */
   /*       *(tempPtr+2) = (unsigned char) ((API_FP_GET_FW_VERSION_REQ&0xff00)>>8); // Primitive MSB */
   /*       *(tempPtr+3) = (unsigned char) (API_FP_GET_FW_VERSION_REQ&0x00ff);      // Primitive LSB */
   /*    } */
   /*    break; */

   /*    case DECT_REGCMD_GET_MODEL: */
   /*    { */
   /*       tempPtr = (unsigned char*) malloc(4); */
   /*       if (tempPtr == NULL) */
   /*       { */
   /*          cmsLog_error("No more memory!!!"); */
   /*          return; */
   /*       } */
   /*       *(tempPtr+0) = 0; // Length MSB */
   /*       *(tempPtr+1) = 2; // Length LSB */
   /*       *(tempPtr+2) = (unsigned char) ((API_FP_MM_GET_MODEL_REQ&0xff00)>>8); // Primitive MSB */
   /*       *(tempPtr+3) = (unsigned char) (API_FP_MM_GET_MODEL_REQ&0x00ff);      // Primitive LSB */
   /*    } */
   /*    break; */

   /*    case DECT_REGCMD_GET_HSET_IPUI: */
   /*    { */
   /*       if ( length < 1 ) */
   /*       { */
   /*          cmsLog_error("No handset is specify to retrieve IPUI!"); */
   /*       } */

   /*       tempPtr = (unsigned char*) malloc(5); */
   /*       if (tempPtr == NULL) */
   /*       { */
   /*          cmsLog_error("No more memory!!!"); */
   /*          return; */
   /*       } */

   /*       *(tempPtr+0) = 0; // Length MSB */
   /*       *(tempPtr+1) = 3; // Length LSB */
   /*       *(tempPtr+2) = (unsigned char) ((API_FP_MM_GET_HANDSET_IPUI_REQ&0xff00)>>8); // Primitive MSB */
   /*       *(tempPtr+3) = (unsigned char) (API_FP_MM_GET_HANDSET_IPUI_REQ&0x00ff);      // Primitive LSB */
   /*       *(tempPtr+4) = (unsigned char) dataPtr[0];      // Handset Id */
   /*    } */
   /*    break; */

   /*    case DECT_REGCMD_SYNC_DATE_TIME: */
   /*    { */
   /*       unsigned short mailSize = sizeof(ApiFpSetTimeReqType); */
   /*       unsigned char * mailPtr = NULL; */

   /*       time_t currentTime; */

   /*       // get current time */
   /*       time (&currentTime); */

   /*       struct tm * ptm= localtime(&currentTime); */

   /*       cmsLog_debug("Current time is: %d-%d-%d %d:%d:%d \n", */
   /*                     (ptm->tm_year) + 1900, */
   /*                     (ptm->tm_mon)+1, */
   /*                     ptm->tm_mday, */
   /*                     ptm->tm_hour, */
   /*                     ptm->tm_min, */
   /*                     ptm->tm_sec); */

   /*       tempPtr = (unsigned char*) malloc(mailSize + 2); */
   /*       mailPtr = tempPtr + 2; */

   /*       if (tempPtr == NULL) */
   /*       { */
   /*          cmsLog_error("No more memory!!!"); */
   /*          return; */
   /*       } */

   /*       *(tempPtr+0) = (unsigned char)(mailSize >> 8);     // Length MSB */
   /*       *(tempPtr+1) = (unsigned char)(mailSize & 0x00FF); // Length LSB */

   /*       ((ApiFpSetTimeReqType *)mailPtr)->Primitive = API_FP_SET_TIME_REQ; */
   /*       ((ApiFpSetTimeReqType *)mailPtr)->Coding = API_CODING_TIME_DATE; */
   /*       ((ApiFpSetTimeReqType *)mailPtr)->Interpretation = API_INTER_CURRENT_TIME_DATE; */
   /*       ((ApiFpSetTimeReqType *)mailPtr)->ApiTimeDateCode.Year = ptm->tm_year; */
   /*       ((ApiFpSetTimeReqType *)mailPtr)->ApiTimeDateCode.Month = (ptm->tm_mon)+1; */
   /*       ((ApiFpSetTimeReqType *)mailPtr)->ApiTimeDateCode.Day = ptm->tm_mday; */
   /*       ((ApiFpSetTimeReqType *)mailPtr)->ApiTimeDateCode.Hour = ptm->tm_hour; */
   /*       ((ApiFpSetTimeReqType *)mailPtr)->ApiTimeDateCode.Minute = ptm->tm_min; */
   /*       ((ApiFpSetTimeReqType *)mailPtr)->ApiTimeDateCode.Second = ptm->tm_sec; */
   /*       ((ApiFpSetTimeReqType *)mailPtr)->ApiTimeDateCode.TimeZone = 0; //TBD. To get the timezone from the system. */
   /*    } */
   /*    break; */

   /*    default: */
   /*    break; */
   /* } */

/* if (tempPtr != NULL) */
/*    { */
/*       /\* bosMutexAcquire(&semMailFrameQueueSem); *\/ */
/*       /\* EnQueue(&mailFrameQueue, tempPtr); *\/ */
/*       /\* bosMutexRelease(&semMailFrameQueueSem); *\/ */

/*      unsigned short bSenderSize = 256*tempPtr[0] + tempPtr[1]; */

/*      dectDrvWrite((void *)(tempPtr+2), bSenderSize); */
/*    } */
/* } */




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




int main(int argc, char **argv)
{

  int rflag = 0;
  int sflag = 0;
  int pflag = 0;
  int index;
  int c;

  if((apifd = open("/dev/dect", O_RDWR)) < 0) {
    printf("dectmngr: open error %d\n", errno);
    return -1;
  }

  printf("dectmngr\n");

  /* bosInit(); */

  while ((c = getopt (argc, argv, "rsp")) != -1)
    switch (c)
      {
      case 'r':
	rflag=1;
	break;
      case 's':
	sflag=1;
	break;
      case 'p':
	pflag=1;
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


  if(rflag)
    register_handsets_start();

  if(sflag)
    register_handsets_stop();

  if(pflag)
    ping_handsets();


  return 0;
}




