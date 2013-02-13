
#include "endpt.h"



int endpoint_fd = 1;

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
