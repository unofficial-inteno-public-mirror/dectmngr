

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


static void dectNvsCtlGetData( unsigned char *pNvsData )
{
   if ( pNvsData == NULL ) 
   {   
     printf("%s: error %d\n", __FUNCTION__, errno);
      return;
   }

   memcpy( pNvsData, &DectNvsData[0], API_FP_LINUX_NVS_SIZE);
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
  t = (unsigned char*) malloc(sizeof(ApiFpLinuxInitReqType));
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


/* typedef struct { */
/*   int packet_type; */
/*   unsigned char data[API_FP_LINUX_MAX_MAIL_SIZE]; */
/* } packet; */


/* packet classify_packet(unsigned char buf); */


/* packet classify_packet(unsigned char buf) { */

  

/* } */



int daemonize(void) 
{

  fd_set rd_fdset;
  unsigned char buf[API_FP_LINUX_MAX_MAIL_SIZE];
  int res, len, i;
  fd_set rfds;
  RosPrimitiveType primitive;
  

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

      primitive = ((recDataType *) buf)->PrimitiveIdentifier;
      
      switch (primitive) {
	
      case API_FP_CC_SETUP_IND:
	printf("API_FP_CC_SETUP_IND\n");
	break;
	
      default:
	printf("Unknown packet\n");
	break;
      }

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




