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



#ifdef LINUX

#include <sys/time.h>
#include <sys/types.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <termios.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <stdlib.h>

#include "remote.h"
#include "errcode.h"
#include "network.h"
#include "lowlevel.h"
#include "serio.h"
#include "global.h"


#define BAUDRATE	B38400

char SerialDevice[256];
extern char baudrate[10];

int hCom;

void msSleep (int time)
{
	struct timeval tv;

	tv.tv_sec = time / 1000;
	tv.tv_usec = (time % 1000) * 1000;

	select (0,NULL,NULL,NULL,&tv);
}

void WriteSerialString (byte pnt[],int len)
{
	int res,stat;

	res = write (hCom,pnt,len);
	if (res != len) {
		log_print ("IRTrans Connection lost. Aborting ...\n",LOG_FATAL);
		exit (-1);
	}
}

int ReadSerialString (byte pnt[],int len,word timeout)
{
	int bytes,total = 0;
	struct timeval tv;
	fd_set fs;

	while (total < len) {
		FD_ZERO (&fs);
		FD_SET (hCom,&fs);
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;
		bytes = select (hCom+1,&fs,NULL,NULL,&tv);
		if (!bytes) return (total);
		bytes = read (hCom,pnt+total,len-total);
		total += bytes;
		}
	return (total);
}

int WriteSerialStringEx (DEVICEINFO *dev,byte pnt[],int len)
{
	int res,stat = 1;

	res = write (dev->io.comport,pnt,len);
	if (res != len) {
		if (dev->io.if_type == IF_USB && !(mode_flag & NO_RECONNECT)) {
			close (dev->io.comport);
			while (stat) {
				stat = OpenSerialPortEx (dev->io.node,&dev->io.comport,0);
				if (stat) sleep (10);
			}
		}
		else {
			log_print ("IRTrans Connection lost. Aborting ...\n",LOG_FATAL);
			exit (-1);
		}
		return (ERR_TIMEOUT);
	}
	return (0);
}


int ReadSerialStringEx (DEVICEINFO *dev,byte pnt[],int len,word timeout)
{
	char st[80];
	int bytes,total = 0;
	struct timeval tv;
	fd_set fs;

	while (total < len) {
		FD_ZERO (&fs);
		FD_SET (dev->io.comport,&fs);
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;
		bytes = select (dev->io.comport+1,&fs,NULL,NULL,&tv);
		if (!bytes) return (total);
		bytes = read (dev->io.comport,pnt+total,len-total);
		if (!bytes) return (total);
		total += bytes;
		}
	return (total);
}

void FlushCom ()
{
}



void FlushComEx(HANDLE fp)
{
	int bytes;
	struct timeval tv;
	fd_set fs;
	char dummy[256];

	FD_ZERO (&fs);
	FD_SET (fp,&fs);
	tv.tv_sec = 0;
	tv.tv_usec = 10000;
	bytes = select (fp+1,&fs,NULL,NULL,&tv);
	if (!bytes) return;
	bytes = read (fp,dummy,256);

}


int WritePort (DEVICEINFO *dev,byte pnt[],int len)
{
	int res;

	res = write (dev->virtual_comport,pnt,len);
	if (res != len) return (ERR_TIMEOUT);
	else return (0);
}


int OpenVirtualComport (char Pname[],int *port)
{
	if ((*port = open(Pname, O_RDWR | O_NOCTTY)) < 0) return (ERR_OPEN);
	return (0);
}


int OpenSerialPort(char Pname[])
{
	int parnum = 0,res,flg;
	struct termios portterm;

	strcpy (SerialDevice,Pname);
	if ((hCom = open(Pname, O_RDWR | O_NOCTTY)) < 0) return (ERR_OPEN);
 
	if (!isatty(hCom)) {
		close(hCom);
		return (ERR_OPEN);
	}

#ifndef DBOX
	if (flock(hCom, LOCK_EX | LOCK_NB) < 0) {
		close(hCom);
		return (ERR_FLOCK);
	}
#endif

	portterm.c_cflag = CS8 | CREAD | CLOCAL;

	portterm.c_cc[VMIN] = 1; 
	portterm.c_cc[VTIME] = 0;

  	cfsetispeed(&portterm, BAUDRATE);
	cfsetospeed(&portterm, BAUDRATE);

	portterm.c_lflag = 0;

	portterm.c_iflag = IGNBRK;
	portterm.c_oflag = 0;


	tcflush(hCom, TCIOFLUSH);
	if (tcsetattr(hCom, TCSANOW, &portterm) < 0) {
		close(hCom);
		return (ERR_STTY);
	}
	msSleep (1000);  
	
	tcflush(hCom, TCIOFLUSH);

	return (0);
}



int OpenSerialPortEx (char Pname[],int *port,int wait)
{
	int res,flg;
	struct termios portterm;
	if ((*port = open(Pname, O_RDWR | O_NOCTTY)) < 0) return (ERR_OPEN);
 
	if (!isatty(*port)) {
		close(*port);
		return (ERR_OPEN);
	}

#ifndef DBOX
	if (flock(*port, LOCK_EX | LOCK_NB) < 0) {
		close(*port);
		return (ERR_FLOCK);
	}
#endif

	portterm.c_cflag = CS8 | CREAD | CLOCAL;

	portterm.c_cc[VMIN] = 1; 
	portterm.c_cc[VTIME] = 0;


	if (!strcmp (baudrate,"4800")) {
  		cfsetispeed(&portterm, B4800);
		cfsetospeed(&portterm, B4800);
	}

	else if (!strcmp (baudrate,"9600")) {
  		cfsetispeed(&portterm, B9600);
		cfsetospeed(&portterm, B9600);
	}

	else if (!strcmp (baudrate,"19200")) {
  		cfsetispeed(&portterm, B19200);
		cfsetospeed(&portterm, B19200);
	}
	else if (!strcmp (baudrate,"57600")) {
  		cfsetispeed(&portterm, B57600);
		cfsetospeed(&portterm, B57600);
	}
	else if (!strcmp (baudrate,"115200")) {
  		cfsetispeed(&portterm, B115200);
		cfsetospeed(&portterm, B115200);
		portterm.c_cflag = CS8 | CREAD | CLOCAL | CSTOPB;
	}
	else {
  		cfsetispeed(&portterm, BAUDRATE);
		cfsetospeed(&portterm, BAUDRATE);
	}

	portterm.c_lflag = 0;

	portterm.c_iflag = IGNBRK;
	portterm.c_oflag = 0;


	tcflush(*port, TCIOFLUSH);
	if (tcsetattr(*port, TCSANOW, &portterm) < 0) {
		close(*port);
		return (ERR_STTY);
	}
	msSleep (1000);  
	
	tcflush(*port, TCIOFLUSH);

	return (0);
}


#ifdef DBOX

tcflush (int fd,int mode)
{
	char st[1024];
	do {
	} while (ReadSerialString (st,1000,10) == 1000);
}

#endif


#endif

