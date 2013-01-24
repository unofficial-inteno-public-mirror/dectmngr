

#include <stdio.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <endpointdrv.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

/* defines */




/* function prototypes */
int register_handsets_start(void);
int register_handsets_stop(void);
int ping_handsets(void);






int register_handsets_start(void) {
  
  printf("register_handsets_start\n");

  return 0;
}


int register_handsets_stop(void) {
  
  printf("register_handsets_stop\n");

  return 0;
}


int ping_handsets(void) {
  
  printf("ping_handsets\n");

  return 0;
}




int main(int argc, char **argv)
{
  int fd;
  int rflag = 0;
  int sflag = 0;
  int pflag = 0;
  int index;
  int c;

  if((fd = open("/dev/dect", O_RDWR)) < 0) {
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




