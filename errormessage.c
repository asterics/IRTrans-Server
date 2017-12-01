/*
 * Copyright (c) 2007, IRTrans GmbH
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer. 
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution. 
 *     * Neither the name of IRTrans GmbH nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY IRTrans GmbH ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 * DISCLAIMED. IN NO EVENT SHALL IRTrans GmbH BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF 
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */



#ifdef WIN32
#include "winsock2.h"
#include <windows.h>
#include <time.h>
#include <sys/timeb.h>

#define MSG_NOSIGNAL	0

#endif

#ifdef WINCE
#include "winsock2.h"
#include <windows.h>
#endif

#ifdef LINUX
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <time.h>
#include <sys/timeb.h>
#endif

#include <stdio.h>

#include "remote.h"
#include "network.h"
#include "lowlevel.h"
#include "global.h"
#include "server.h"

#include "errcode.h"

#ifdef WIN32
#include "winio.h"
#include "winusbio.h"
#endif

void SwapNetworkheader (NETWORKSTATUS *ns);
void	CloseIRSocket (int client);


extern NETWORKCLIENT sockinfo[CLIENT_COUNT];


void log_print (char msg[],int level)
{
	FILE *fp;
	time_t tv;
	int ms = 0;
	struct tm *tmpnt;
#ifdef WIN32
	struct _timeb tb;
#endif
#ifdef LINUX
	struct timeb tb;
#endif

	if (logfp) fp = logfp;
	else fp = stderr;

	if (!fp) return;

	if (level > (int)(mode_flag & LOG_MASK)) return;

	if (mode_flag & TIMESTAMP) {
		tv = time (0);
#ifdef WIN32
		_ftime (&tb);
#endif
#ifdef LINUX
		ftime (&tb);
#endif
		tv = tb.time;
		ms = tb.millitm;
		tmpnt = localtime (&tv);

		fprintf (fp,"%4d-%02d-%02d %02d:%02d:%02d.%03d  %s",tmpnt->tm_year+1900,tmpnt->tm_mon+1,tmpnt->tm_mday,tmpnt->tm_hour,tmpnt->tm_min,tmpnt->tm_sec,ms,msg);
	}
	else fprintf (fp,"%s",msg);

	fflush (fp);

//	if (level <= LOG_ERROR) NetworkClientMessage (msg,0xffff);
}

void NetworkClientMessage (char msg[],int num)
{
	int i,res;

	NETWORKLOG nl;
		
	memset (&nl,0,sizeof (NETWORKLOG));
	nl.clientid = 0;
	nl.statuslen = sizeof (NETWORKLOG);
	nl.statustype = STATUS_LOG;
	strcpy (nl.message,msg);

	SwapNetworkheader ((NETWORKSTATUS *)&nl);

	if (num != 0xffff) {
		res = send (sockinfo[num].fd,(char *)&nl,sizeof (NETWORKLOG),MSG_NOSIGNAL);
		if (res <= 0) CloseIRSocket (num);
	}
	else {
		i = 0;
		while (i < CLIENT_COUNT) {
			if ((sockinfo[i].type == SELECT_SERVER || sockinfo[i].type == SELECT_REOPEN) && sockinfo[i].msg_mode) {
				res = send (sockinfo[i].fd,(char *)&nl,sizeof (NETWORKLOG),MSG_NOSIGNAL);
				if (res <= 0) CloseIRSocket (i);

			}
			i++;
		}
	}
}

void GetError (int res,char st[])
{

	switch (res) {
	case ERR_OPEN:
		sprintf (st,"Error opening COM/USB Port / LAN Device\n");
		break;
	case ERR_RESET:
		sprintf (st,"No IR Transceiver found (Reset not possible)\n");
		break;
	case ERR_READVERSION:
		sprintf (st,"Could not get Transceiver Version\n");
		break;
	case ERR_VERSION:
		sprintf (st,"Wrong IR Transceiver SW Version. Minimum Version: %s\n",MINIMUM_SW_VERSION);
		break;
	case ERR_TIMEOUT:
		sprintf (st,"Timeout (Connection lost ?)\n");
		break;
	case ERR_OPENUSB:
		sprintf (st,"Error opening USB Device / Device not found\n");
		break;
	case ERR_DBOPENINPUT:
		sprintf (st,"Error opening Database file (Access rights ?)\n");
		break;
	case ERR_REMOTENOTFOUND:
		sprintf (st,"Specified Remote Control [%%s] not found\n");
		break;
	case ERR_COMMANDNOTFOUND:
		sprintf (st,"Specified Remote Command [%%s] not found\n");
		break;
	case ERR_TIMINGNOTFOUND:
		sprintf (st,"Specified Remote Timing not found\n");
		break;
	case ERR_OPENASCII:
		sprintf (st,"Could not create new Remote file (Access rights ?)\n");
		break;
	case ERR_NODATABASE:
		sprintf (st,"Could not open Remote Database (No folder 'remotes' / Access rights ?)\n");
		break;
	case ERR_TOGGLE_DUP:
		sprintf (st,"Could not record Toggle Command (No commands yet learned ?)\n");
		break;
	case ERR_DBOPENINCLUDE:
		sprintf (st,"Specified Include File not found\n");
		break;
	case ERR_NOFILEOPEN:
		sprintf (st,"No Remote opened to learn new commands\n");
		break;
	case ERR_FLOCK:
		sprintf (st,"Could not lock input file (USB / TTY)\n");
		break;
	case ERR_STTY:
		sprintf (st,"Could not set serial parameters\n");
		break;
	case ERR_OPENSOCKET:
		sprintf (st,"Could not open IP socket\n");
		break;
	case ERR_BINDSOCKET:
		sprintf (st,"Could not bind to IP socket (Another server running ?)\n");
		break;
	case ERR_HOTCODE:
		sprintf (st,"Hotcode %%s-%%s not found\n");
		break;
	case ERR_NOTIMING:
		sprintf (st,"No timing learned for new commands\n");
		break;
	case ERR_TEMPCOMMAND:
		sprintf (st,"Illegal Temparature Command: %%s\n");
		break;
	case ERR_OPENTRANS:
		sprintf (st,"Error opening translation table file\n");
		break;
	case ERR_WRONGBUS:
		sprintf (st,"Specified Bus does not exist\n");
		break;
	case ERR_ISMACRO:
		sprintf (st,"Cannot get the Device Data for a macro\n");
		break;
	case ERR_DEVICEUNKNOWN:
		sprintf (st,"IR Code received from unknown device: %%s\n");
		break;
	case ERR_BINDWEB:
		sprintf (st,"Cannot bind to Web Port. Another Webserver running ? Try -no_web.\n");
		break;
	case ERR_OVERWRITE:
		sprintf (st,"File already exists. Overwrite ?\n");
		break;
	case ERR_NO_RS232:
		sprintf (st,"No IRTrans Device with AUX RS232 port connected\n");
		break;
	case ERR_CCF:
		sprintf (st,"Illegal CCF Code\n");
		break;
	case ERR_CCFSYNTAX:
		sprintf (st,"Syntax Error in CCF Code\n");
		break;
	case ERR_CCFLEN:
		sprintf (st,"Wrong CCF Code length\n");
		break;
	case ERR_SSID_WLAN:
		sprintf (st,"WLAN Radio status can only be retrieved via USB\n");
		break;
	case ERR_LEARN_LENGTH:
		sprintf (st,"IR Code length > max. length - Code might not work\n");
		break;
	case ERR_LEARN_RAWLEN:
		sprintf (st,"IR RAW Code length > max. length - Code might not work\n");
		break;
	case ERR_LEARN_TIMECNT:
		sprintf (st,"Number of timings to large - use RAW Code - Code might not work\n");
		break;
	case ERR_LEARN_TC_LEN:
		sprintf (st,"IR Code too long for large number of timings - Code might not work\n");
		break;
	case ERR_SEND_LED:
		sprintf (st,"Selected IR Output does not exist\n");
		break;
	case ERR_OUTPUT_BUSY:
		sprintf (st,"IR Output Busy\n");
		break;
	case ERR_MACRO_LENGTH:
		sprintf (st,"Resulting Macro data too long\n");
		break;
	case ERR_MACRO_COUNT:
		sprintf (st,"More than 10 commands in the Macro\n");
		break;
	case ERR_NO_MACRO:
		sprintf (st,"Selected IRTrans device does not support Macros\n");
		break;
	case ERR_LONGCODE:
		sprintf (st,"IR Code length not supported by this device\n");
		break;
	case ERR_IRCODE_LENGTH:
		sprintf (st,"Illegal IR Code length\n");
		break;
	case ERR_IRCODE_TIMING:
		sprintf (st,"Illegal IR Timing\n");
		break;
	case ERR_IRCODE_DATA:
		sprintf (st,"IR Code does not match IR Timings\n");
		break;
	case ERR_NOSTATEINPUT:
		sprintf (st,"No State Input for this IR Output defined\n");
		break;
	default:
		sprintf (st,"Error %d\n",res);
		break;
	}

}
