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
#include <stdio.h>

#include "remote.h"
#include "errcode.h"
#include "network.h"
#include "lowlevel.h"
#include "global.h"


#include "serio.h"

HANDLE hCom;
HANDLE hComEvent;

int ReadSerialStringEx (DEVICEINFO *dev,byte pnt[],int len,word timeout);


// Low Level Routinen

// 1. Comm-Device Bereich öffnen
// 2. Daten Lesen mit Timeout Wert:	SetCommTimeouts
// 3. Transceiver zurücksetzen

extern char baudrate[10];


void msSleep (int time)
{
	Sleep (time);
}

int WriteSerialStringEx (DEVICEINFO *dev,byte pnt[],int len)
{
	DWORD bytes;

	memset (&dev->io.ov_write,0,sizeof (OVERLAPPED));
	dev->io.ov_write.hEvent = dev->io.event;

	WriteFile(dev->io.comport,pnt,len,&bytes,&dev->io.ov_write);
	WaitForSingleObject (dev->io.event,100);
	GetOverlappedResult (dev->io.comport,&dev->io.ov_write,&bytes,FALSE);
	ResetEvent (dev->io.event);

	if ((int)bytes != len) return (ERR_TIMEOUT);
	else return (0);
}

int GetSerialAvailableEx (DEVICEINFO *dev)
{
	return (0);
}

int ReadSerialStringEx_ITo (DEVICEINFO *dev,byte pnt[],int len,word timeout)
{
	int cnt,rcnt;

	cnt = 0;

	while (cnt < len) {
		rcnt = ReadSerialStringEx (dev,pnt + cnt,len - cnt,timeout);

		if (!rcnt) return cnt;

		cnt += rcnt;
	}

	return cnt;
}

void SetSerialTimeoutComio (DEVICEINFO *dev,word time)
{
	COMMTIMEOUTS to;

	memset (&to,0,sizeof (to));

	to.ReadIntervalTimeout = time;
	to.ReadTotalTimeoutConstant = time;
	SetCommTimeouts (dev->virtual_comport,&to);
}

int ReadSerialStringComio (DEVICEINFO *dev,byte pnt[],int len,word timeout)
{
	int res,i = 0;
	DWORD bytes = 0,dummy;

	memset (pnt,0,len);

	memset (&dev->io.ov_read,0,sizeof (OVERLAPPED));
	dev->io.ov_read.hEvent = dev->com_event;

	SetSerialTimeoutComio (dev,timeout);
	ReadFile(dev->virtual_comport ,pnt,len,&dummy,&dev->io.ov_read);
	res = WaitForSingleObject (dev->com_event,5);
	if (res == WAIT_TIMEOUT) res = WaitForSingleObject (dev->com_event,timeout);

	if (res != WAIT_TIMEOUT) GetOverlappedResult (dev->virtual_comport,&dev->io.ov_read,&bytes,FALSE);

	ResetEvent (dev->com_event);
	return (bytes);
}


int ReadSerialStringEx (DEVICEINFO *dev,byte pnt[],int len,word timeout)
{
	int res,i = 0;
	DWORD bytes = 0,dummy;

	memset (pnt,0,len);

	memset (&dev->io.ov_read,0,sizeof (OVERLAPPED));
	dev->io.ov_read.hEvent = dev->io.event;

	SetSerialTimeoutEx (dev,timeout);
	ReadFile(dev->io.comport,pnt,len,&dummy,&dev->io.ov_read);
	res = WaitForSingleObject (dev->io.event,5);
	if (res == WAIT_TIMEOUT) res = WaitForSingleObject (dev->io.event,timeout);

	if (res != WAIT_TIMEOUT) GetOverlappedResult (dev->io.comport,&dev->io.ov_read,&bytes,FALSE);

	ResetEvent (dev->io.event);
	return (bytes);
}


void FlushComEx(HANDLE fp)
{
	PurgeComm(fp,PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);
}


void SetSerialTimeoutEx (DEVICEINFO *dev,word time)
{
	COMMTIMEOUTS to;

	memset (&to,0,sizeof (to));

	to.ReadIntervalTimeout = time;
	to.ReadTotalTimeoutConstant = time;
	SetCommTimeouts (dev->io.comport,&to);
}

void SetSerialTimeoutEx_ITo (DEVICEINFO *dev,word time)
{
	COMMTIMEOUTS to;

	memset (&to,0,sizeof (to));

	to.ReadIntervalTimeout = time;
//	to.ReadTotalTimeoutConstant = time;
	SetCommTimeouts (dev->io.comport,&to);
}

int OpenSerialPortEx (char Pname[],HANDLE *port,int wait)
{
	int res;
	char nm[50];
	DCB dcb={0};

	char sDCB[40];

	strcpy (sDCB,"38400,n,8,1");
	sprintf (nm,"\\\\.\\%s",Pname);

	if (!strcmp (baudrate,"4800")) strcpy (sDCB,"4800,n,8,1");
	if (!strcmp (baudrate,"9600")) strcpy (sDCB,"9600,n,8,1");
	if (!strcmp (baudrate,"19200")) strcpy (sDCB,"19200,n,8,1");
	if (!strcmp (baudrate,"57600")) strcpy (sDCB,"57600,n,8,1");
	if (!strcmp (baudrate,"115200")) strcpy (sDCB,"115200,n,8,2");

	*port=CreateFile(nm,
                    GENERIC_READ | GENERIC_WRITE,
                    0,
                    NULL,
                    OPEN_EXISTING,
                    FILE_FLAG_OVERLAPPED,
				    NULL);
	if (*port == INVALID_HANDLE_VALUE) return (ERR_OPEN);

	res = BuildCommDCB(sDCB, &dcb);

	dcb.fDtrControl = DTR_CONTROL_ENABLE;
	dcb.fRtsControl = RTS_CONTROL_ENABLE;
	dcb.fOutxCtsFlow = 0;
	dcb.fOutxDsrFlow = 0;
	res = SetCommState(*port,&dcb);

	if (mode_flag & NO_RESET) EscapeCommFunction(*port,SETRTS);
	else EscapeCommFunction(*port, CLRRTS);
	EscapeCommFunction(*port, SETDTR);
	Sleep (wait);
	FlushComEx (*port);
	return (0);
}

int WritePort (DEVICEINFO *dev,byte pnt[],int len)
{
	DWORD bytes;

	memset (&dev->com_ov,0,sizeof (OVERLAPPED));
	dev->com_ov.hEvent = dev->com_event;
	WriteFile(dev->virtual_comport,pnt,len,&bytes,&dev->com_ov);

	WaitForSingleObject (dev->com_event ,100);
	GetOverlappedResult (dev->com_event,&dev->com_ov,&bytes,FALSE);
	ResetEvent (dev->com_event);
	if ((int)bytes != len) return (ERR_TIMEOUT);
	else return (0);
}

int OpenVirtualComport (char Pname[],HANDLE *port)
{

	char nm[50];

	sprintf (nm,"\\\\.\\%s",Pname);
	*port = CreateFile(nm,
                    GENERIC_READ | GENERIC_WRITE,
                    0,
                    NULL,
                    OPEN_EXISTING,
                    FILE_FLAG_OVERLAPPED,
  				    NULL);

	if (*port == INVALID_HANDLE_VALUE) return (ERR_OPEN);

	return (0);
}


int OpenSerialPort(char Pname[])
{
	DCB dcb={0};

	char sDCB[]="38400,n,8,1";

	hCom=CreateFile(Pname,
                    GENERIC_READ | GENERIC_WRITE,
                    0,
                    NULL,
                    OPEN_EXISTING,
                    FILE_FLAG_OVERLAPPED,
				    NULL);
	
	BuildCommDCB(sDCB, &dcb);
	dcb.fDtrControl = DTR_CONTROL_ENABLE;
	dcb.fRtsControl = RTS_CONTROL_ENABLE;
	dcb.fOutxCtsFlow = 0;
	dcb.fOutxDsrFlow = 0;
	SetCommState(hCom,&dcb);
	if (mode_flag & NO_RESET) 	EscapeCommFunction(hCom,SETRTS);
	else EscapeCommFunction(hCom, CLRRTS);
	EscapeCommFunction(hCom, SETDTR);
	Sleep(1000);
	FlushCom ();

	hComEvent = CreateEvent (NULL,TRUE,FALSE,NULL);
	return (0);
}


void WriteSerialString (byte pnt[],int len)
{

	DWORD bytes;
	OVERLAPPED ov;

	memset (&ov,0,sizeof (ov));
	ov.hEvent = hComEvent;

	WriteFile(hCom,pnt,len,&bytes,&ov);
	WaitForSingleObject (hComEvent,100);
	GetOverlappedResult (hCom,&ov,&bytes,FALSE);
	ResetEvent (hComEvent);
}


int ReadSerialString (byte pnt[],int len,word timeout)
{

	DWORD bytes;
	OVERLAPPED ov;

	memset (&ov,0,sizeof (ov));
	ov.hEvent = hComEvent;

	SetSerialTimeout (timeout);
	ReadFile(hCom,pnt,len,&bytes,&ov);
	WaitForSingleObject (hComEvent,timeout);
	GetOverlappedResult (hCom,&ov,&bytes,FALSE);
	ResetEvent (hComEvent);
	return (bytes);
}

void FlushCom(void)
{
	PurgeComm(hCom,PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);
}

void SetSerialTimeout (word time)
{
	COMMTIMEOUTS to;

	memset (&to,0,sizeof (to));

	to.ReadIntervalTimeout = time;
	to.ReadTotalTimeoutConstant = time;
	SetCommTimeouts (hCom,&to);
}



#endif