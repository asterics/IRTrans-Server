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
#include <winbase.h>

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
#include <stdint.h>
#include "WinTypes.h"
#endif

#include <stdio.h>
#include <signal.h>

#include "errcode.h"
#include "remote.h"
#include "network.h"
#include "lowlevel.h"
#include "global.h"
#include "winusbio.h"


FT_HANDLE usb;


char usbcomports[20][5];
int usbcomcnt;

#ifdef WIN32

int GetOSInfo (void)
{
	int ver;
	OSVERSIONINFO os;
	char *cpu;

	memset (&os,0,sizeof (OSVERSIONINFO));
	os.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
	GetVersionEx (&os);
	ver = os.dwMajorVersion * 100 + os.dwMinorVersion * 10;

	cpu = getenv ("PROCESSOR_ARCHITEW6432");

	if (cpu) ver |= 1;

	return (ver);
}


void GetComPorts (void)
{
	int res,pos,cnt;
	char buf[20000];

	res = QueryDosDevice (NULL,buf,20000);

	cnt = pos = 0;

	while (pos < res) {
		if (!memcmp (buf+pos,"COM",3)) cnt++;
		while (buf[pos]) pos++;
		pos++;
	}

	if (cnt > 20) cnt = 20;
	usbcomcnt = cnt;

	pos = 0;

	while (pos < res) {
		if (!memcmp (buf+pos,"COM",3)) {
			strcpy (usbcomports[cnt-1],buf+pos);
			cnt--;
		}
		while (buf[pos]) pos++;
		pos++;
	}
}

#endif


int OpenUSBPort (void)
{
	FT_STATUS stat;

	stat = F_OpenEx ("IRTrans USB",FT_OPEN_BY_DESCRIPTION,&usb);
	if (stat) stat = F_OpenEx ("FB401",FT_OPEN_BY_DESCRIPTION,&usb);
	if (stat) stat = F_OpenEx ("IRTrans USB B",FT_OPEN_BY_DESCRIPTION,&usb);
	if (stat) stat = F_OpenEx ("IRTrans WiFi",FT_OPEN_BY_DESCRIPTION,&usb);
	if (stat) stat = F_OpenEx ("Breakout Box",FT_OPEN_BY_DESCRIPTION,&usb);

	if (stat) return (ERR_OPENUSB);

	F_SetLatencyTimer (usb,2);

	return (0);
}


int WriteUSBString (byte pnt[],int len)
{
	DWORD num;
	FT_STATUS stat;

	stat = F_Write (usb,pnt,len,&num);
	if (stat) return (0);

	return (num);
}


int	ReadUSBString (byte pnt[],int len,long timeout)
{
	DWORD num;
	FT_STATUS stat;

	F_SetTimeouts (usb,timeout,0);

	stat = F_Read (usb,pnt,len,&num);

	if (stat) return (0);

	return (num);
}


int GetUSBAvailableEx (DEVICEINFO *dev)
{
	DWORD num;
	FT_STATUS stat;

	stat = F_GetQueueStatus (dev->io.usbport,&num);

	if (stat) return (0);
	
	return (num);
}


int ReadUSBStringEx_ITo (DEVICEINFO *dev,byte pnt[],int len,word timeout)
{
	int cnt = 0;
	int rcnt;

	DWORD wstat;
	DWORD num;


	SetUSBEventEx (dev,FT_EVENT_RXCHAR);
	while (cnt < len) {
#ifdef WIN32
		if (!cnt) wstat = WaitForSingleObject(dev->io.event,timeout * 4);
		else wstat = WaitForSingleObject(dev->io.event,timeout);

		if (wstat == WAIT_TIMEOUT) return (cnt);

		ResetEvent (dev->io.event);
#endif

		F_GetQueueStatus (dev->io.usbport,&num);

		if (num > (DWORD)(len - cnt)) num = len - cnt;

		rcnt = ReadUSBStringEx (dev,pnt + cnt,num,100);
		cnt += rcnt;
	}

	return (cnt);
}

int ReadUSBStringAvailable (DEVICEINFO *dev,byte pnt[],int len,word timeout)
{
	DWORD wstat;
	DWORD num;

	F_GetQueueStatus (dev->io.usbport,&num);

	if (!num) {
#ifdef WIN32
		SetUSBEventEx (dev,FT_EVENT_RXCHAR);

		wstat = WaitForSingleObject(dev->io.event,timeout);
		if (wstat == WAIT_TIMEOUT) return (0);
		ResetEvent (dev->io.event);
#endif
	}

	F_GetQueueStatus (dev->io.usbport,&num);

	if (num > (DWORD)(len)) num = len;

	return (ReadUSBStringEx (dev,pnt,num,100));
}

int	ReadUSBStringEx (DEVICEINFO *dev,byte pnt[],int len,word timeout)
{
	DWORD num;
	FT_STATUS stat;

	F_SetTimeouts (dev->io.usbport,timeout,0);

	stat = F_Read (dev->io.usbport,pnt,len,&num);

	if (stat) return (0);

	return (num);
}



void FlushUSB (void)
{
	F_Purge (usb,FT_PURGE_RX | FT_PURGE_TX);
}


void FlushUSBEx (FT_HANDLE hndl)
{
	F_Purge (hndl,FT_PURGE_RX | FT_PURGE_TX);
}

void WriteUSBStringEx (DEVICEINFO *dev,byte pnt[],int len)
{
	DWORD num;
	FT_STATUS stat;

	stat = F_Write (dev->io.usbport,pnt,len,&num);
	if (stat == 4) {

		F_Close (dev->io.usbport);
		if (mode_flag & NO_RECONNECT) {
			log_print ("IRTrans Connection lost. Aborting ...\n",LOG_FATAL);
			exit (-1);
		}
		while (stat) {
			log_print ("Trying reconnect ...\n",LOG_DEBUG);
#ifdef WIN32
			F_Reload (0x403,0xfc60);
			F_Reload (0x403,0xfc61);
			Sleep (1000);
#endif
			stat = F_OpenEx (dev->usb_serno,FT_OPEN_BY_SERIAL_NUMBER,&dev->io.usbport);
#ifdef WIN32
			Sleep (2000);
#endif
			if (!stat) log_print ("Reconnected ...\n",LOG_DEBUG);
		}
	}
}


void SetUSBEventEx (DEVICEINFO *dev,DWORD mask)
{
	F_SetEventNotification (dev->io.usbport,mask,dev->io.event);
}

void break_signal (int sig)
{
	log_print ("Abort ...\n",LOG_FATAL);
	exit (0);
}

#ifdef WIN32

void cleanup_exit (void)
{
#ifndef _STANDALONE
	int i;
	for (i=0;i < device_cnt;i++) if (IRDevices[i].io.if_type == IF_USB) F_Close (IRDevices[i].io.usbport);
#endif
	if (hdll) FreeLibrary(hdll);
}


int LoadUSBLibrary (void)
{

	char msg[256];

	atexit (cleanup_exit);
	signal (SIGINT,break_signal);

	hdll = LoadLibrary("Ftd2xx.dll");	
	if(hdll == NULL)
	{
		sprintf (msg,"Error: Can't Load ftd2xx.dll\n");
		log_print (msg,LOG_FATAL);
		return (-1);
	}

	m_pListDevices = (PtrToListDevices)GetProcAddress(hdll, "FT_ListDevices");

	m_pOpen = (PtrToOpen)GetProcAddress(hdll, "FT_Open");

	m_pOpenEx = (PtrToOpenEx)GetProcAddress(hdll, "FT_OpenEx");

	m_pRead = (PtrToRead)GetProcAddress(hdll, "FT_Read");

	m_pClose = (PtrToClose)GetProcAddress(hdll, "FT_Close");

	m_pGetQueueStatus = (PtrToGetQueueStatus)GetProcAddress(hdll, "FT_GetQueueStatus");

	m_pWrite = (PtrToWrite)GetProcAddress(hdll, "FT_Write");

	m_pCyclePort = (PtrToCyclePort)GetProcAddress(hdll, "FT_CyclePort");

	m_pReload = (PtrToReload)GetProcAddress(hdll,"FT_Reload");

	m_pResetDevice = (PtrToResetDevice)GetProcAddress(hdll, "FT_ResetDevice");

	m_pPurge = (PtrToPurge)GetProcAddress(hdll, "FT_Purge");

	m_pSetTimeouts = (PtrToSetTimeouts)GetProcAddress(hdll, "FT_SetTimeouts");

	m_pSetEvent = (PtrToSetEvent)GetProcAddress(hdll, "FT_SetEventNotification");

	m_pGetDeviceInfo = (PtrToGetDeviceInfo)GetProcAddress(hdll, "FT_GetDeviceInfo");

	m_pSetLatencyTimer = (PtrToSetLatencyTimer)GetProcAddress(hdll, "FT_SetLatencyTimer");

	return (0);
}

enum FT_STATUS F_GetDeviceInfo(FT_HANDLE usb,FT_DEVICE *device,DWORD *id,char *serno,char *desc,PVOID dummy)
{
	return (*m_pGetDeviceInfo)(usb, device, id, serno, desc, dummy);
}

enum FT_STATUS F_SetEventNotification(FT_HANDLE usb,DWORD mask,PVOID event)
{
	return (*m_pSetEvent)(usb, mask, event);
}


enum FT_STATUS F_ListDevices(PVOID pArg1, PVOID pArg2, DWORD dwFlags)
{
	return (*m_pListDevices)(pArg1, pArg2, dwFlags);
}	

enum FT_STATUS F_Open(PVOID pvDevice,FT_HANDLE *usb)
{
	return (*m_pOpen)(pvDevice, usb );
}	

enum FT_STATUS F_OpenEx(PVOID pArg1, DWORD dwFlags,FT_HANDLE *usb)
{
	return (*m_pOpenEx)(pArg1, dwFlags, usb);
}	

enum FT_STATUS F_Read(FT_HANDLE usb,LPVOID lpvBuffer, DWORD dwBuffSize, LPDWORD lpdwBytesRead)
{
	return (*m_pRead)(usb, lpvBuffer, dwBuffSize, lpdwBytesRead);
}	

enum FT_STATUS F_Close(FT_HANDLE usb)
{
	return (*m_pClose)(usb);
}	

enum FT_STATUS F_GetQueueStatus(FT_HANDLE usb,LPDWORD lpdwAmountInRxQueue)
{
	return (*m_pGetQueueStatus)(usb, lpdwAmountInRxQueue);
}	

enum FT_STATUS F_Write(FT_HANDLE usb,LPVOID lpvBuffer, DWORD dwBuffSize, LPDWORD lpdwBytes)
{
	return (*m_pWrite)(usb, lpvBuffer, dwBuffSize, lpdwBytes);
}	

enum FT_STATUS F_Reload (WORD wVID, WORD wPID)
{
	return (*m_pReload)(wVID, wPID);
}

enum FT_STATUS F_CyclePort(FT_HANDLE usb)
{
	return (*m_pCyclePort)(usb);
}	

enum FT_STATUS F_ResetDevice(FT_HANDLE usb)
{
	return (*m_pResetDevice)(usb);
}	

enum FT_STATUS F_Purge(FT_HANDLE usb,ULONG dwMask)
{
	return (*m_pPurge)(usb, dwMask);
}	


enum FT_STATUS F_SetTimeouts(FT_HANDLE usb,ULONG dwReadTimeout, ULONG dwWriteTimeout)
{
	return (*m_pSetTimeouts)(usb, dwReadTimeout, dwWriteTimeout);
}	

enum FT_STATUS F_SetLatencyTimer (FT_HANDLE usb, UCHAR ucTimer)
{
	return (*m_pSetLatencyTimer)(usb, ucTimer);
}

#endif