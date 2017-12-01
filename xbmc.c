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

#define _WIN32_WINNT 0x501

#ifndef VC6
#include "winsock2.h"
#endif

#include <windows.h>
#include <winuser.h>
#include <io.h>
#include <direct.h>
#include <stdio.h>
#include <sys/timeb.h>
#include <tlhelp32.h>

#endif

#include <stdio.h>

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

typedef int DWORD;
#define closesocket		close

#endif


#include "remote.h"
#include "global.h"
#include "network.h"
#include "lowlevel.h"
#include "dbstruct.h"




#define BTN_USE_NAME   0x01
#define BTN_DOWN       0x02
#define BTN_UP         0x04
#define BTN_USE_AMOUNT 0x08
#define BTN_QUEUE      0x10
#define BTN_NO_REPEAT  0x20
#define BTN_VKEY       0x40
#define BTN_AXIS       0x80

#define PT_HELO         0x01
#define PT_BYE          0x02
#define PT_BUTTON       0x03
#define PT_MOUSE        0x04
#define PT_PING         0x05
#define PT_BROADCAST    0x06
#define PT_NOTIFICATION 0x07
#define PT_BLOB         0x08
#define PT_LOG          0x09
#define PT_ACTION       0x0A
#define PT_DEBUG        0xFF

#define ICON_NONE       0x00
#define ICON_JPEG       0x01
#define ICON_PNG        0x02
#define ICON_GIF        0x03

#define MAX_PACKET_SIZE  1024
#define HEADER_SIZE      32
#define MAX_PAYLOAD_SIZE (MAX_PACKET_SIZE - HEADER_SIZE)

#define MAJOR_VERSION 2
#define MINOR_VERSION 0

extern byte xbmc_mode;

#define ACTION_EXECBUILTIN		1
#define ACTION_BUTTON			2



DWORD xbmc_pid;
unsigned int xbmc_uid;
unsigned int xbmc_seq;
unsigned long XBMC_last_ping;
int xbmc_remote;
byte xbmc_init;

SOCKET xbmc_socket;




void XBMC_SendBye ()
{
	int res;
	byte data[1024];

	if (!xbmc_init) return;

	res = BuildXBMCHeader (data,PT_BYE,0);
	send (xbmc_socket,(const char *)data,res,0);
}

void XBMC_SendPing ()
{
	int res;
	byte data[1024];

	if (!xbmc_init) return;

	res = BuildXBMCHeader (data,PT_PING,0);
	send (xbmc_socket,(const char *)data,res,0);

	XBMC_last_ping = time (0);
}

void StartXBMC (APP *app)
{
	int i;
	char *progdir;
	char prog[1024],appname[1024];
	if (!xbmc_mode) return;

	if (xbmc_init) closesocket (xbmc_socket);
	xbmc_init = 0;
	xbmc_pid = 0;

#ifdef WIN32
	if (app->appname[0] == '%') {
		strcpy (appname,app->appname);
		i = 1;
		while (appname[i] && appname[i] != '%') i++;
		appname[i++] = 0;
		progdir = getenv (appname + 1);
		if (!progdir) progdir = getenv ("ProgramFiles");
		if (!progdir) return;
		sprintf (prog,"%s%s",progdir,appname+i);
	}
	else strcpy (prog,app->appname);
	
	WinExec (prog,SW_SHOWMAXIMIZED);
#endif

#ifdef LINUX
	system (app->appname);
#endif

}


#ifdef WIN32

byte XBMC_CheckRunning (void)
{
	HANDLE pHandle;

	pHandle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, xbmc_pid);
    
	if (pHandle == NULL) {
		xbmc_pid = 0;
		return (0);
	}
	
	CloseHandle(pHandle);

	return (1);
}

DWORD XBMC_GetPID (void)
{   
	HANDLE hProcessSnap;
	PROCESSENTRY32 peStruct;   

	xbmc_pid = 0;

	hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);  
	if(hProcessSnap == INVALID_HANDLE_VALUE) return (0);

	peStruct.dwSize = sizeof(PROCESSENTRY32);
    
	if(Process32First(hProcessSnap, &peStruct) == FALSE){
		CloseHandle(hProcessSnap);
		return (0);
	}

	do {
		if (!strcmp (peStruct.szExeFile,"xbmc.exe") || !strcmp (peStruct.szExeFile,"XBMC.exe") || !strcmp (peStruct.szExeFile,"XBMC.EXE")) {
			xbmc_pid = peStruct.th32ProcessID;
			break;
		}
	}
    while(Process32Next(hProcessSnap, &peStruct));

	CloseHandle(hProcessSnap);

	return xbmc_pid;
}

#endif



#ifdef LINUX

byte XBMC_CheckRunning (void)
{
	if (kill (xbmc_pid,0)) {
		xbmc_pid = 0;
		return (0);
	}
	return (1);
}

DWORD XBMC_GetPID (void)
{   
	int i;
	char ln[200];
	FILE *fp;

	xbmc_pid = 0;

	fp = popen( "ps aux|grep -v grep|grep -i xbmc.bin", "r");

	fgets (ln,sizeof (ln),fp);	

	pclose (fp);

	i = 0;
	while (ln[i] && ln[i] != ' ' && ln[i] != '\t') i++;
	
	if (!ln[i]) return;

	while (ln[i] == ' ' || ln[i] == '\t') i++;

	if (!ln[i]) return;

	xbmc_pid = atoi (ln + i);

	printf ("PID: %d\n",xbmc_pid);

	return xbmc_pid;
}

#endif




void SendXBMC (APPCOMMAND *appcom)
{
	int res;
	byte data[1024];

	if (!xbmc_mode) return;

	if (!xbmc_init || !XBMC_CheckRunning ()) {
		res = InitXBMC ();
		if (res) return;
	}
	
	res = 0;

	if (appcom->type[0] == TYPE_STR || appcom->type[0] == TYPE_XBMC_BUTTON) {
		res = BuildXBMCKey (data,appcom->function.name);
	}
	else if (appcom->type[0] == TYPE_XBMC_ACTION) {
		res = BuildXBMCAction (data,ACTION_BUTTON,appcom->function.name);
	}
	else if (appcom->type[0] == TYPE_XBMC_ACTION_BUILTIN) {
		res = BuildXBMCAction (data,ACTION_EXECBUILTIN,appcom->function.name);
	}
	
	if (!res) return;

	send (xbmc_socket,(const char *)data,res,0);

	XBMC_last_ping = time (0);
}


int InitXBMC (void)
{
	int res;
	byte data[1024];
	struct sockaddr_in serv_addr;

	if (!xbmc_mode) return (1);

	if (!XBMC_GetPID ()) return (1);

	xbmc_socket = socket (PF_INET,SOCK_DGRAM,0);
	if (xbmc_socket < 0) return (1);

	memset (&serv_addr,0,sizeof (serv_addr));
	serv_addr.sin_family = AF_INET;

	serv_addr.sin_addr.s_addr = inet_addr ("127.0.0.1");
	serv_addr.sin_port = htons ((word)9777);

	res = connect (xbmc_socket,(struct sockaddr *)&serv_addr,sizeof (serv_addr));
	if (res) return (res);

	xbmc_uid = time (0);

	res = BuildXBMCHelo (data,"IRTrans Server");
	send (xbmc_socket,(const char *)data,res,0);

	xbmc_init = 1;

	XBMC_last_ping = time (0);

	return (0);
}


int BuildXBMCHeader (byte *data,short type,int payload_len)
{
	unsigned short u16;
	unsigned int u32;

	memset (data,0,HEADER_SIZE + payload_len);

	memcpy (data +  0,"XBMC",4);				// Signature

	data[4] = MAJOR_VERSION;					// Major Version
	data[5] = MINOR_VERSION;					// Minor Version

	u16 = htons (type);
	memcpy (data +  6,&u16,2);					// Packet Type

	u32 = htonl (xbmc_seq++);
	memcpy (data +  8,&u32,4);					// SEQ Number

	u32 = htonl (1);
	memcpy (data + 12,&u32,4);					// Number of packets

	u16 = htons ((word)payload_len);
	memcpy (data + 16,&u16,2);					// LEN of Payload

	u32 = htonl (xbmc_uid);
	memcpy (data + 18,&u32,4);					// UID

	return (payload_len + HEADER_SIZE);
}


int BuildXBMCHelo (byte *data,char *name)
{
	int len;

	len = BuildXBMCHeader (data,PT_HELO,strlen (name) + 12);

	strcpy ((char *)data + HEADER_SIZE,name);

	return (len);
}


int BuildXBMCAction (byte *data,byte type,char *action)
{
	int len;
	unsigned short u16;

	len = BuildXBMCHeader (data,PT_ACTION,strlen (action) + 2);
	data[HEADER_SIZE] = type;
	strcpy ((char *)data + HEADER_SIZE +  1,action);

	return (len);
}


int BuildXBMCKey (byte *data,char *button)
{
	int len;
	unsigned short u16;

	len = BuildXBMCHeader (data,PT_BUTTON,strlen (button) + 10);


	u16 = htons (0);
	memcpy (data + HEADER_SIZE +  0,&u16,2);							// Button Code

	u16 = htons (0x2b);
	memcpy (data + HEADER_SIZE +  2,&u16,2);							// Flags

	u16 = htons (0);
	memcpy (data + HEADER_SIZE +  4,&u16,2);							// Amount

	strcpy ((char *)data + HEADER_SIZE +  6,"R1");								// Device Map

	strcpy ((char *)data + HEADER_SIZE +  9,button);							// Button Name

	return (len);
}


