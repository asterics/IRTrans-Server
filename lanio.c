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
#include <ws2tcpip.h>
#endif

#ifdef WINCE
#include "winsock2.h"
#include <windows.h>
#include <time.h>
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
#include <net/if.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/timeb.h>

typedef int DWORD;
#define closesocket		close
#endif


#include "remote.h"
#include "errcode.h"
#include "network.h"
#include "dbstruct.h"
#include "lowlevel.h"
#include "fileio.h"
#include "global.h"

#ifdef WIN32
#include "winio.h"
#include "winusbio.h"
BOOL WINAPI ShutdownHandler (DWORD type);
#endif

#ifdef WIN32
WSAEVENT IrtLanEvent;
LARGE_INTEGER counterRes;
#endif


SOCKET irtlan_outbound;
SOCKET irtlan_socket;


int GetInterfaces (unsigned int ips[]);



int	IRTransLanFlash (DEVICEINFO *dev,IRDATA_LAN_FLASH *ird,int len,uint32_t ip)
{
	int res;
	byte bcast = 0;
	struct sockaddr_in target;

	
	if (bcast) {
		memset (&target,0,sizeof (struct sockaddr));
		target.sin_family = AF_INET;
		target.sin_addr.s_addr = INADDR_BROADCAST;
		target.sin_port = htons ((word)IRTRANS_PORT);
	}

	else if (dev) memcpy (&target,&dev->io.IPAddr[0],sizeof (struct sockaddr_in));


	if ((bcast || dev) && connect (irtlan_outbound,(struct sockaddr *)&target,sizeof (struct sockaddr_in)) < 0) return (ERR_BINDSOCKET);


	res = send (irtlan_outbound,(char *)ird,len + 5,0);

	msSleep (30);
	
	return (rcv_status_timeout (500,ip));
}



int	IRTransLanSend (DEVICEINFO *dev,IRDATA *ird)
{
	int res,cnt,len;
	char st[255];
	IRDATA_LAN_LARGE irdlan;
	struct sockaddr_in target;
	unsigned long timediff;
#ifdef WIN32
	LARGE_INTEGER time_start;
	LARGE_INTEGER time_ack;
#endif
#ifdef LINUX
	struct timeb tb;
	long long int time_start;
	long long int time_ack;
#endif

	
	memset (&irdlan,0,sizeof (IRDATA_LAN_LARGE));

	if (ird->command == START_FLASH_MODE) {
		if (ird->len == 0) {
			irdlan.netcommand = COMMAND_FLASH_END;
			ird->len = 3;
		}
		else irdlan.netcommand = COMMAND_FLASH_START;
	}

	else if (ird->command == TRANSFER_FLASH) {
		irdlan.netcommand = COMMAND_FLASH_DATA;
	}

	else if (ird->len == 0) {
		irdlan.netcommand = COMMAND_LAN_PING;
	}
	else {
		irdlan.netcommand = COMMAND_LAN;
	}

	if (ird->command == HOST_SEND_MACRO) len = ((IRDATA_MACRO *)ird)->total_len;
	else len = ird->len;

	memcpy (&(irdlan.ir_data),ird,len);

	if (dev) {
		if (dev->io.socket != 0) {
			cnt = 0;
retry_tcp:
			res = send (dev->io.socket,(char *)&irdlan,len + sizeof (IRDATA_LAN) - sizeof (IRDATA),0);
			if (res <= 0) {
				cnt++;
				sprintf (st,"IRTRans LAN Connection with %s-%s lost [%d]\n",inet_ntoa (dev->io.IPAddr[0].sin_addr),dev->device_node,dev->io.tcp_reconnect);
				log_print (st,LOG_ERROR);
				if (mode_flag & TCP_RECONNECT) {
#ifdef WIN32
					WSACloseEvent (dev->io.event);
#endif
					closesocket (dev->io.socket);
					if (cnt <= 1) {
						if (!open_irtrans_tcp (dev)) {
							dev->io.tcp_reconnect++;
							goto retry_tcp;
						}
					}
				}
				return (ERR_NETIO);
			}
			dev->io.last_time = time (0);
		
			if (ird->command == HOST_SEND || ird->command == HOST_RESEND || ird->command == HOST_SEND_LEDMASK || ird->command == HOST_RESEND_LEDMASK || ird->command == HOST_SEND_MACRO) {
				res = rcv_status_timeout_tcp (dev,2000);

				if (res < 0) {
					res = send (dev->io.socket,(char *)&irdlan,1,0);
		
					if (res <= 0) {
						cnt++;
						sprintf (st,"IRTRans LAN Connection with %s-%s lost [%d]\n",inet_ntoa (dev->io.IPAddr[0].sin_addr),dev->device_node,dev->io.tcp_reconnect);
						log_print (st,LOG_ERROR);
						if (mode_flag & TCP_RECONNECT) {
#ifdef WIN32
							WSACloseEvent (dev->io.event);
#endif
							closesocket (dev->io.socket);
							if (cnt <= 1) {
								if (!open_irtrans_tcp (dev)) {
									dev->io.tcp_reconnect++;
									goto retry_tcp;
								}
							}
						}
						return (ERR_NETIO);
					}
				}

				if (res != COMMAND_SEND_ACK) {
					sprintf (st,"IRTRans LAN IRSend ACK: %d\n",res);
					log_print (st,LOG_ERROR);
				}
				
				if (res == -1) return (ERR_TIMEOUT);
				if (res == COMMAND_SEND_ACK_BUSY) {
					return (ERR_OUTPUT_BUSY);
				}
			}
			else msSleep (30);
			return (0);
		}

		memcpy (&target,&dev->io.IPAddr[0],sizeof (struct sockaddr_in));
		if (dev && connect (irtlan_outbound,(struct sockaddr *)&target,sizeof (struct sockaddr_in)) < 0) return (ERR_BINDSOCKET);
	}

	if (dev && dev->lan_version[0] == 'L' && memcmp (dev->lan_version+1,"1.07.60",7) >= 0 && (ird->command == HOST_SEND || ird->command == HOST_RESEND || ird->command == HOST_SEND_LEDMASK || ird->command == HOST_RESEND_LEDMASK || ird->command == HOST_SEND_MACRO)) {
		if (dev->io.io_seq_mode && dev->io.lan_io_sequence == 0) dev->io.lan_io_sequence++;
		irdlan.mode = dev->io.lan_io_sequence++;
		dev->io.io_seq_mode = 1;
	}

	res = send (irtlan_outbound,(char *)&irdlan,len + sizeof (IRDATA_LAN) - sizeof (IRDATA),0);

	if (dev && dev->lan_version[0] == 'L' && memcmp (dev->lan_version+1,"1.07.60",7) >= 0 && (ird->command == HOST_SEND || ird->command == HOST_RESEND || ird->command == HOST_SEND_LEDMASK || ird->command == HOST_RESEND_LEDMASK || ird->command == HOST_SEND_MACRO)) {

#ifdef WIN32
		QueryPerformanceCounter (&time_start);

		do {
			res = rcv_status_timeout (50,target.sin_addr.S_un.S_addr);
			if (res == COMMAND_SEND_ACK2) {
				sprintf (st,"LAN STAT: %d\n",res);
				log_print (st,LOG_ERROR);
			}
		} while (res == COMMAND_SEND_ACK2);

		QueryPerformanceCounter (&time_ack);
		timediff = (unsigned long)((time_ack.QuadPart - time_start.QuadPart) / counterRes.QuadPart);
#else
		ftime (&tb);
		time_start = tb.time * 10000 + tb.millitm * 10;
		do {
			res = rcv_status_timeout (50,target.sin_addr.s_addr);
		} while (res == COMMAND_SEND_ACK2);

		ftime (&tb);
		time_ack = tb.time * 10000 + tb.millitm * 10;
		timediff = time_ack - time_start;
#endif
	
		if (res == -1 && irdlan.mode) { // No ACK received
			sprintf (st,"LAN SEND - RESEND [%f ms]\n",timediff / 10.0);
			log_print (st,LOG_ERROR);
			res = send (irtlan_outbound,(char *)&irdlan,len + sizeof (IRDATA_LAN) - sizeof (IRDATA),0);
#ifdef WIN32
			res = rcv_status_timeout (50,target.sin_addr.S_un.S_addr);

			QueryPerformanceCounter (&time_ack);
			timediff = (unsigned long)((time_ack.QuadPart - time_start.QuadPart) / counterRes.QuadPart);
#else
			res = rcv_status_timeout (50,target.sin_addr.s_addr);

			ftime (&tb);
			time_ack = tb.time * 10000 + tb.millitm * 10;
			timediff = time_ack - time_start;
#endif
			if (res == -1) return (ERR_TIMEOUT);
		}

		if (res == COMMAND_SEND_ACK_WAIT) {
			sprintf (st,"Time ACK Wait: %f ms[SEQ: %d]\n",timediff / 10.0,irdlan.mode);
			log_print (st,LOG_ERROR);
#ifdef WIN32
			res = rcv_status_timeout (2000,target.sin_addr.S_un.S_addr);
			QueryPerformanceCounter (&time_ack);
			timediff = (unsigned long)((time_ack.QuadPart - time_start.QuadPart) / counterRes.QuadPart);
#else
			res = rcv_status_timeout (2000,target.sin_addr.s_addr);

			ftime (&tb);
			time_ack = tb.time * 10000 + tb.millitm * 10;
			timediff = time_ack - time_start;
#endif
		}
		if (res == COMMAND_SEND_ACK_BUSY) {
			return (ERR_OUTPUT_BUSY);
		}
		if (res != COMMAND_SEND_ACK && res != COMMAND_SEND_ACK2) {
			sprintf (st,"ACK2 Error: %d!\n",res);
			log_print (st,LOG_ERROR);
			return (ERR_TIMEOUT);
		}
		sprintf (st,"Time ACK End : %f ms\n",timediff / 10.0);
		log_print (st,LOG_ERROR);
		return (0);
	}

	if (dev && dev->fw_capabilities2 & FN2_SEND_ACK && (ird->command == HOST_SEND || 
		((dev->lan_version[0] != 'L' || memcmp (dev->lan_version+1,"1.07.24",7) > 0) &&  (ird->command == HOST_RESEND || ird->command == HOST_SEND_LEDMASK || ird->command == HOST_RESEND_LEDMASK || ird->command == HOST_SEND_MACRO)))) {
#ifdef WIN32
		res = rcv_status_timeout (2000,target.sin_addr.S_un.S_addr);
#else
		res = rcv_status_timeout (2000,target.sin_addr.s_addr);
#endif
			
		if (res != COMMAND_SEND_ACK) {
			sprintf (st,"IRTRans LAN IRSend ACK: %d\n",res);
			log_print (st,LOG_ERROR);
		}
		if (res == -1) return (ERR_TIMEOUT);
	}

	else msSleep (30);

	return (0);
}

int open_irtrans_tcp (DEVICEINFO *dev)
{
#ifdef WIN32
	int res;
	char msg[256];
	unsigned int adr;
	struct sockaddr_in serv_addr;
	struct hostent *he;
	struct in_addr addr;
	BOOL val;

	memset (&dev->io.IPAddr,0,sizeof (dev->io.IPAddr));
	dev->io.IPAddr[0].sin_family = AF_INET;

	adr = inet_addr (dev->device_node);
	if (adr == INADDR_NONE) {
		he = (struct hostent *)gethostbyname (dev->device_node);
		if (he == NULL) return (ERR_OPEN);
		memcpy(&addr, he->h_addr_list[0], sizeof(struct in_addr));
		adr = addr.s_addr;
	}

	dev->io.socket = socket (PF_INET,SOCK_STREAM,0);
	if (dev->io.socket < 0) return (ERR_OPEN);


	memset (&serv_addr,0,sizeof (serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = adr;
	serv_addr.sin_port = htons (TCP_PORT);

	if (connect (dev->io.socket,(struct sockaddr *)&serv_addr,sizeof (serv_addr)) < 0) {
		sprintf (msg,"TCP Connect Error\n");
		log_print (msg,LOG_ERROR);
		return (ERR_OPEN);
	}

	val = TRUE;
	res = setsockopt (dev->io.socket,IPPROTO_TCP, TCP_NODELAY,(char *)&val,sizeof (BOOL));
	res = setsockopt (dev->io.socket,SOL_SOCKET, SO_KEEPALIVE,(char *)&val,sizeof (BOOL));

	if (send (dev->io.socket,"BINA",4,0) < 0) {
		sprintf (msg,"TCP Send Error [Init]\n");
		log_print (msg,LOG_ERROR);
		return (ERR_OPEN);
	}
	msSleep (500);
	dev->io.event = WSACreateEvent ();
	WSAEventSelect (dev->io.socket,dev->io.event,FD_READ);
	dev->io.IPAddr[0].sin_addr.s_addr = adr;
	dev->io.IPAddr[0].sin_port = htons ((word)IRTRANS_PORT);
#endif
	return (0);
}
					  
int rcv_status_timeout_tcp (DEVICEINFO *dev,int timeout)
{
	char st[255];
	byte data[1000];
	int stat,sz,val;
	int res = -1;
	struct sockaddr_in from;

#ifdef LINUX
	fd_set events;
	int maxfd,wait,flags;
	struct timeval tv;

	flags = fcntl (irtlan_socket,F_GETFL);
#endif

#ifdef WIN32
	stat = WaitForSingleObject (dev->io.event,timeout);
	if (stat == WAIT_TIMEOUT)  return (-1);
#endif

#ifdef LINUX
	FD_ZERO (&events);

	FD_SET (irtlan_socket,&events);
	maxfd = irtlan_socket + 1;

	tv.tv_sec = timeout / 1000;
	tv.tv_usec = (timeout % 1000) * 1000;

	stat = select (maxfd,&events,NULL,NULL,&tv);
	if (stat == 0)  return (-1);

	fcntl (irtlan_socket,F_SETFL,flags | O_NONBLOCK);
#endif

	sz = sizeof (from);
	val = recvfrom (dev->io.socket,data,1000,0,(struct sockaddr *)&from,&sz);
	if (val <= 0) return (-2);
	res = data[0];

#ifdef WIN32
	WSAResetEvent (dev->io.event);
#endif

#ifdef LINUX
	fcntl (irtlan_socket,F_SETFL,flags);
#endif
	if (res == COMMAND_SEND_ACK_BUSY) {
		sprintf (st,"IR Output Busy: 0x%x\n",*((word *)(data+1)));
		log_print (st,LOG_ERROR);
	}

	return (res);
}



int rcv_status_timeout (int timeout,uint32_t ip)
{
	char st[256];
	byte data[1000];
	int stat,sz,val;
	int res = -1;
	struct sockaddr_in from;

#ifdef LINUX
	fd_set events;
	int maxfd,wait,flags;
	struct timeval tv;

	flags = fcntl (irtlan_socket,F_GETFL);
#endif

retry:
#ifdef WIN32
	stat = WaitForSingleObject (IrtLanEvent,timeout);
	if (stat == WAIT_TIMEOUT)  return (-1);
#endif

#ifdef LINUX
	FD_ZERO (&events);

	FD_SET (irtlan_socket,&events);
	maxfd = irtlan_socket + 1;

	tv.tv_sec = timeout / 1000;
	tv.tv_usec = (timeout % 1000) * 1000;

	stat = select (maxfd,&events,NULL,NULL,&tv);
	if (stat == 0)  return (-1);

	fcntl (irtlan_socket,F_SETFL,flags | O_NONBLOCK);
#endif

	do { 
		sz = sizeof (from);

		val = recvfrom (irtlan_socket,data,1000,0,(struct sockaddr *)&from,&sz);
#ifdef WIN32
		if (val > 0) {
			if (!ip || from.sin_addr.S_un.S_addr == ip) res = data[0];
			else {
				sprintf (st,"RCV Stat IP Error: %x\n",from.sin_addr.S_un.S_addr);
				log_print (st,LOG_ERROR);
			}
		}
#else
		if (val > 0 && (!ip || from.sin_addr.s_addr == ip)) res = data[0];
#endif
	} while (val > 0);

#ifdef WIN32
	WSAResetEvent (IrtLanEvent);
#endif
	if (res == -1) goto retry;

	if (res == COMMAND_SEND_ACK_BUSY) {
		sprintf (st,"IR Output Busy: 0x%x\n",*((word *)(data+1)));
		log_print (st,LOG_ERROR);
	}

#ifdef LINUX
	fcntl (irtlan_socket,F_SETFL,flags);
#endif

	return (res);
}


int rcv_status ()
{
	byte val,res = 0;

	val = recv (irtlan_socket,&res,1,0);

	return res;
}

void InitWinsock ()
{
#ifdef WIN32
	int err;
    WORD	wVersionRequired;
    WSADATA	wsaData;
    wVersionRequired = MAKEWORD(2,2);
    err = WSAStartup(wVersionRequired, &wsaData);
    if (err != 0) exit(1);

#endif
}

extern int irtrans_udp_port;

void CloseIRTransLANSocket (void)
{
	if (irtlan_outbound > 0) closesocket (irtlan_outbound);
	if (irtlan_socket > 0) closesocket (irtlan_socket);

}

int OpenIRTransLANSocket ()
{
	int res;
	struct sockaddr_in serv_addr;

	irtlan_outbound = socket (PF_INET,SOCK_DGRAM,0);
	if (irtlan_outbound < 0) return (ERR_OPENSOCKET);

	res = 1;
	setsockopt (irtlan_outbound,SOL_SOCKET,SO_BROADCAST,(char *)&res,sizeof (int));

	memset (&serv_addr,0,sizeof (serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = 0;
	serv_addr.sin_port = htons (0);
	if (bind (irtlan_outbound,(struct sockaddr *)&serv_addr,sizeof (serv_addr)) < 0) {
		fprintf (stderr,"\n\nError binding send socket ... Abort [%d].\n",errno);
		exit (-1);
	}
	
	irtlan_socket = socket (PF_INET,SOCK_DGRAM,0);
	if (irtlan_socket < 0) return (ERR_OPENSOCKET);

	memset (&serv_addr,0,sizeof (serv_addr));
	serv_addr.sin_family = AF_INET;

	serv_addr.sin_addr.s_addr = htonl (INADDR_ANY);
	serv_addr.sin_port = htons ((short)irtrans_udp_port);

	if (bind (irtlan_socket,(struct sockaddr *)&serv_addr,sizeof (serv_addr)) < 0) {
		fprintf (stderr,"\n\nError binding socket ... Abort [%d].\n",errno);
		exit (-1);
	}

#ifdef WIN32
	QueryPerformanceFrequency (&counterRes);
	counterRes.QuadPart /= 10000;						// Resolution = 100µs
#endif

	return (0);
}

SOCKET irt_bcast[32];
int if_count;


int OpenIRTransBroadcastSockets (void)
{
	int res,i;
	unsigned int ips[32];

	struct sockaddr_in serv_addr;

	if_count = GetInterfaces (ips);

	for (i=0;i < if_count;i++) {

		irt_bcast[i] = socket (PF_INET,SOCK_DGRAM,0);
		if (irt_bcast[i] < 0) return (ERR_OPENSOCKET);

		res = 1;
		setsockopt (irt_bcast[i],SOL_SOCKET,SO_BROADCAST,(char *)&res,sizeof (int));

		memset (&serv_addr,0,sizeof (serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = ips[i];
		serv_addr.sin_port = htons (0);
		if (bind (irt_bcast[i],(struct sockaddr *)&serv_addr,sizeof (serv_addr)) < 0) {
			fprintf (stderr,"\n\nError binding send socket ... Abort [%d].\n",errno);
			exit (-1);
		}
		memset (&serv_addr,0,sizeof (serv_addr));
		serv_addr.sin_family = AF_INET;

		serv_addr.sin_addr.s_addr = htonl (INADDR_BROADCAST);
		serv_addr.sin_port = htons ((short)irtrans_udp_port);

		if (connect (irt_bcast[i],(struct sockaddr *)&serv_addr,sizeof (struct sockaddr_in)) < 0) return (ERR_BINDSOCKET);
	}

	return (0);
}

#ifdef WIN32

int GetInterfaces (unsigned int ips[])
{

	int i,num,cnt;
	unsigned long len;
	INTERFACE_INFO InterfaceList[32];

    SOCKET sd = WSASocket(AF_INET, SOCK_DGRAM, 0, 0, 0, 0);
    if (sd == SOCKET_ERROR) return (0);

    if (WSAIoctl(sd, SIO_GET_INTERFACE_LIST, 0, 0, &InterfaceList, sizeof(InterfaceList), &len, 0, 0) == SOCKET_ERROR) return (0);

    num = len / sizeof(INTERFACE_INFO);

	cnt = 0;
    for (i = 0; i < num; i++) if ((InterfaceList[i].iiFlags & (IFF_LOOPBACK | IFF_POINTTOPOINT)) == 0 && (InterfaceList[i].iiFlags & IFF_UP)) {
		ips[cnt++] = InterfaceList[i].iiAddress.AddressIn.sin_addr.s_addr;
    }
	return cnt;
}

#endif

#ifdef LINUX

int GetInterfaces (unsigned int ips[])
{
	int i,j,cnt;
	FILE *fp;
	char *pnt,ln[256];
	struct sockaddr_in *sinp;
	struct ifreq ifr;
	int s; /* Socket */
	char local_ip_addr[16];

	fp = fopen ("/proc/net/dev","r");
	if (!fp) return (0);
	s = socket(AF_INET, SOCK_DGRAM, 0);

	cnt = 0;
	pnt = fgets (ln,sizeof (ln),fp);
	while (pnt) {
		i = 0;
		while (ln[i] == ' ') i++;
		if (!memcmp (ln+i,"eth",3) || !memcmp (ln+i,"wlan",4)) {
			j = i;
			while ((ln[j] >= '0' && ln[j] <= '9') || (ln[j] >= 'a' && ln[j] <= 'z') || (ln[j] >= 'A' && ln[j] <= 'Z')) j++;
			ln[j] = 0;
			memset (&ifr,0,sizeof (ifr));
			strcpy(ifr.ifr_name, ln+i);
			ioctl(s, SIOCGIFADDR, &ifr);
			sinp = (struct sockaddr_in*)&ifr.ifr_addr;
			ips[cnt++] = sinp->sin_addr.s_addr;
		}
		pnt = fgets (ln,sizeof (ln),fp);
	}

	close (s);
	fclose (fp);

	return (cnt);
}

#endif
