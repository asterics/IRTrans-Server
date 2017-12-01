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
#include <io.h>
#include <direct.h>
#include <sys/types.h>
#include <sys/stat.h> 
#include <time.h>
#include <sys/timeb.h>

#define MSG_NOSIGNAL	0
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
#include <signal.h>
#include <stdint.h>
#include <time.h>

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
#include "flash.h"

#ifdef WIN32
#include "winio.h"
#include "winusbio.h"
BOOL WINAPI ShutdownHandler (DWORD type);
extern WSAEVENT IrtLanEvent;
#endif

extern int protocol_version;
extern int AnalyzeUDPString (char *st,int *netcom,char *remote,char *command,char *ccf,int *netmask,int *bus,int *led,int *port,int *wait);

void	CloseIRSocket (int client);
int		FlashHTML (byte *command,byte result[]);
int		GetHTMLFileList (byte *command,byte result[]);
int		IRTransLanFlash (DEVICEINFO *dev,IRDATA_LAN_FLASH *ird,int len,uint32_t ip);
int		SendASCII (byte *command,byte result[],int asciimode);
int		GetRemotelist (byte *data,byte result[]);
int		GetCommandlist (byte *data,byte result[]);
int		GetDevicelist (byte *data,byte result[]);
int		LearnASCII (byte command[],char result[]);
int		ASCIIErrorMessage (byte result[],byte errcode,byte errparm[]);
int		EncodeHex (byte data[],int len,byte result[]);
int		ASCIISendHex (byte command[],char result[]);

void	SwapWordN (word *pnt);
void	SwapIntN (int32_t *pnt);
byte	GetHexDigit (char v);
byte	HToI (char *st,byte fac);


// Tolower ASCII String								OK
// Auto Init ohne ASCI								OK
// ASCII Command ohne "A" am Anfang					OK
// ASCII serial/serialx Command
// ASCII Hex Learn									OK
// ASCII Hex Send									OK
// Fehlermeldungen ASCII							OK
// ASCII Commands UDP

void DoExecuteASCIICommand (byte command[],SOCKET sockfd,int client,int asciimode)
{
	int len,errcnt,sz,res;
	char result[65536];

	ConvertLcase (command,strlen (command));

	memset (result,0,sizeof (result));
	if (!strncmp (command,"GET_HTML_FILELIST",17)) {
		len = GetHTMLFileList (command+17,result);
	}
	else if (!strncmp (command,"FLASH_HTML",10)) {
		len = FlashHTML (command+10,result);
	}
	else if (!strncmp (command,"sndhex",6)) {
		len = ASCIISendHex (command,result);
	}
	else if (!strncmp (command,"snd",3)) {
		len = SendASCII (command,result,asciimode);
	}
	else if (!strncmp (command,"getremotes",10)) {
		len = GetRemotelist (command,result);
	}
	else if (!strncmp (command,"getcommands",11)) {
		len = GetCommandlist (command,result);
	}
	else if (!strncmp (command,"getdevices",10)) {
		len = GetDevicelist (command,result);
	}
	else if (!strncmp (command,"learn",5)) {
		len = LearnASCII (command,result);
	}

	else {
		sprintf (result,"**00000 RESULT ASCII Format Error\n");
		len = strlen (result);
	}

	sprintf (result+2,"%05d",len);
	result[7] = ' ';
	log_print (result,LOG_INFO);

	errcnt = sz = 0;
	while (sz < len && errcnt < 20) {
		res = send (sockfd,result + sz,len - sz,MSG_NOSIGNAL);
		if (res > 0) sz += res;
		if (res == -1) {
			msSleep (100);
			errcnt++;
		}
	}

	if (res <= 0 && len) {
		CloseIRSocket (client);
		sprintf (result,"IP Connection lost\n");
		log_print (result,LOG_ERROR);
	}
} 


int ASCIISendHex (byte command[],char result[])
{
	int bus,res;
	byte *pnt;
	word smask,car;
	byte adr,rc,rp,rpt;
	word pos;
	IRDATA ir_data;

	smask = 0xffff;
	rc = rp = 0;
	car = 0;
	adr = 0;
	rpt = 0;
	bus = 0;


	pnt = command + 6;

	if (*pnt == 'r') {
		rpt = 1;
		pnt++;
	}

	pnt++;
	        
	while (*pnt && *pnt != 'h') {
		while (*pnt && *pnt < 'A') pnt++;
	    
		if (*pnt == 'l') {						   // LED Select
			pnt++;
			if (*pnt == 'i') adr |= INTERNAL_LEDS;
			else if (*pnt == 'e') adr |= EXTERNAL_LEDS;
			else if (*pnt == 'b') adr |= EXTERNAL_LEDS | INTERNAL_LEDS;
			else if (*pnt >= '1' && *pnt <= '9') adr |= atoi (pnt) * 4;

			pnt++;
		} 
		else if (*pnt == 'm') {                    // Sendmask
			pnt++;
			smask = atoi (pnt);
		} 
		else if (*pnt == 'c') {                    // IR Carrier
			pnt++;
			car = atoi (pnt);
			if (car > 500) car = 255;
			else if (car > 127) car = (car / 4) | 128;
		} 
		else if (*pnt == 'r') {                    // Repeat Count
			pnt++;
			rc = (byte)atoi (pnt);
		} 
		else if (*pnt == 'b') {                    // Bus ID
			pnt++;
			bus = (byte)atoi (pnt);
		} 
		else if (*pnt == 'p' || *pnt == 'd') {                    // Repeat Pause
			pnt++;
			rp = (byte)atoi (pnt);
		} 
		else if (*pnt != 'h') pnt++;
	}

	if (*pnt != 'h') return (ASCIIErrorMessage (result,ERR_ASCIIFORMAT,NULL));

	pnt++;
	pos = 0;

	memset (&ir_data,0,sizeof (ir_data));
	ir_data.len = 10;

	while (pos < ir_data.len) {
		if (!pnt[2 * pos] || !pnt[2 * pos + 1]) return (ASCIIErrorMessage (result,ERR_ASCIIFORMAT,NULL));
		((byte *)&ir_data)[pos] = HToI (pnt + 2 * pos,16);
		pos++;  
	}

	if (!ir_data.len) return (ASCIIErrorMessage (result,ERR_ASCIIFORMAT,NULL));

	res = CheckLEDValid (adr,bus);
	if (res) return (ASCIIErrorMessage (result,res,NULL));

	force_swap_irdata (&ir_data);

	if (res = CheckIRCode (&ir_data)) return (ASCIIErrorMessage (result,res,NULL));

	swap_word (smask);

	ir_data.command = HOST_SEND + rpt;
	ir_data.address = adr;
	ir_data.target_mask = smask;
	if (car) ir_data.transmit_freq = (byte)car;
	if (rc) ir_data.ir_repeat = rc;
	if (rp) ir_data.repeat_pause = rp;

	res = WriteTransceiverEx (&IRDevices[bus],&ir_data);

	return (ASCIIErrorMessage (result,res,NULL));
}


int LearnASCII (byte command[],char result[])
{
	IRDATA ir_data;
	char msg[255];
	int res;	

	byte *pnt,len;
	word to = 10;

	int bus = 0;

	do {
		pnt = command + 6;

		memset (&ir_data,0,7);
	  
		ir_data.command = HOST_LEARNIR;				// Hier Mode ergänzen
		ir_data.ir_length = LRN_BO_MODE_SELECT | LRN_NOSORT_SELECT;
	  
		while (*pnt) {
			while (*pnt && *pnt < 'A') pnt++;
	    
			if (*pnt == 'i') {                         // IR Timeout
				pnt++;
				if (*pnt > '0') ir_data.target_mask |= ((*pnt - 49) & 7) | LRN_TIMEOUT_SELECT;   // IR Timeoutwert maskieren und übernehmen
			} 
			else if (*pnt == 'm') {												// Learnmode
				pnt++;
				if (*pnt == '1') ir_data.command = HOST_LEARNIRREPEAT;
				if (*pnt == '2') ir_data.command = HOST_LEARNIRRAW;
				if (*pnt == '3') ir_data.command = HOST_LEARNIRRAWREPEAT;
			} 
			else if (*pnt == 'x') {                    // Long IR Codes
				pnt++;
				if (*pnt >= '1' && *pnt <= '3') {
					ir_data.target_mask |= (*pnt & 3) << 4; 
				}
				else ir_data.target_mask |= 16;
			} 

			else if (*pnt == 'r') {                    // Receiver Select
				pnt++;
				while (*pnt >= '0') {
					if (     *pnt == '3') ir_data.target_mask |= LRN_RCV_38;
					else if (*pnt == '4') ir_data.target_mask |= LRN_RCV_455;
					else if (*pnt == '5') ir_data.target_mask |= LRN_RCV_56;
					else if (*pnt == 'p') ir_data.target_mask |= LRN_RCV_455_PLASMA;
					else if (*pnt == 'c' || *pnt == 'u') ir_data.target_mask |= LRN_RCV_CARRIER;
					else if (*pnt == 'E') ir_data.target_mask |= LRN_RCV_EXTERNAL;
					else if (*pnt == 'e') ir_data.target_mask |= LRN_RCV_HI | LRN_RCV_NR_SELECT;
					else if (*pnt == '1') ir_data.target_mask |= LRN_RCV_NR_SELECT;
					else if (*pnt == '2') ir_data.target_mask |= LRN_RCV_HI | LRN_RCV_NR_SELECT;
					pnt++;															
				}
			} 
			else if (*pnt == 't') {                     // IR Toleranz
				  pnt++;
				  if (*pnt >= '0') ir_data.target_mask |= (((*pnt - '0') & 7) << 13) | LRN_TOLERANCE_SELECT;
			}
			else if (*pnt == 'b') {                                          // B&O Mode
				pnt++;
				ir_data.ir_length |= LRN_BO_MODE;            // B&O Modus setzen
			}
			else if (*pnt == 's') {                     // No Sort
				pnt++;
				ir_data.ir_length |= LRN_NOSORT;
			}
			else if (*pnt == 'l') {                     // Long Button press
				pnt++;
				ir_data.ir_length |= LRN_LONG_CODE;
			}
			else if (*pnt == 'w') {                     // Learn Timeout
				pnt++;
				to = (byte)atoi ((char *)pnt);
				if (to < 1 || to > 60) to = 10;
			}
			else if (*pnt == 'p') pnt += 2;
			else pnt++;
		}
		ir_data.len = 7;


		res = WriteTransceiverEx (IRDevices + bus,&ir_data);
		if (res) return (ASCIIErrorMessage (result,res,NULL));

		if (IRDevices[bus].io.if_type == IF_LAN) len = sizeof (IRDATA);

		else {
			res = ReadIRStringEx (IRDevices + bus,&len,1,to * 1000);

			if (!res) {
				CancelLearnEx (IRDevices + bus);
				return (ASCIIErrorMessage (result,ERR_TIMEOUT,NULL));
			}
		}

	} while (len <= sizeof (IRDATA) - CODE_LEN);

	if (IRDevices[bus].io.if_type == IF_LAN) {
		if (ReadLearndataLAN (IRDevices + bus,(byte *)&ir_data,to * 1000)) return (ASCIIErrorMessage (result,ERR_TIMEOUT,NULL));
		len = ir_data.len;
	}
	else {
		if (ReadIRStringEx (IRDevices + bus,&ir_data.checksumme,len-1,200) != len-1) return (ASCIIErrorMessage (result,ERR_TIMEOUT,NULL));
		ir_data.len = len;
	}

	sprintf (msg,"Learn Status: %d\n",ir_data.command);
	log_print (msg,LOG_DEBUG);

	if (mode_flag & DEBUG_TIMING) showDebugTiming (&ir_data);

	if (TIME_LEN != time_len) ConvertToIRTRANS4 ((IRDATA3 *)&ir_data);

	ir_data.checksumme = 1;

	force_swap_irdata (&ir_data);

	sprintf ((char *)result,"**00000 LEARN ");
	res = EncodeHex ((byte *)&ir_data,len,result + 14) + 14;

	return (res);
}

int EncodeHex (byte pnt[],int len,byte result[])
{
	int i;

	for (i=0;i < len;i++) sprintf (result + i * 2,"%02X",pnt[i]);

	result[i * 2] = '\n';
	return (i * 2) + 1;
}

int ASCIIErrorMessage (byte result[],byte errcode,byte errparm[])
{
	switch (errcode) {
		case 0:
			sprintf (result,"**00000 RESULT OK\n");
			break;
		case ERR_TIMEOUT:
			sprintf (result,"**00000 RESULT LEARN TIMEOUT ERROR\n");
			break;
		case ERR_ASCIIFORMAT:
			sprintf (result,"**00000 RESULT ASCII Format Error\n");
			break;
		case ERR_WRONGBUS:
			sprintf (result,"**00000 RESULT ERROR Specified Bus does not exist\n");
			break;
		case ERR_SEND_LED:
			sprintf (result,"**00000 RESULT ERROR Selected IR Output does not exist\n");
			break;
		case ERR_IRCODE_LENGTH:
			sprintf (result,"**00000 RESULT ERROR Illegal IR Code length\n");
			break;
		case ERR_IRCODE_TIMING:
			sprintf (result,"**00000 RESULT ERROR Illegal IR Timing\n");
			break;
		case ERR_IRCODE_DATA:
			sprintf (result,"**00000 RESULT ERROR IR Code does not match IR Timings\n");
			break;
		default:
			sprintf (result,"**00000 RESULT ERROR: %d\n",errcode);
			break;
	}
	return (strlen (result));
}

int GetCommandlist (byte *data,byte result[])
{
	int i,j,res;
	COMMANDBUFFER cb;

	i = 12;

	while (data[i] && data[i] != ',') i++;
	if (data[i]) data[i++] = 0;
	
	res = GetCommandDatabase (&cb,data+12,atoi (data+i));
	if (res) {
		sprintf (result,"**00000 RESULT ERROR: Remote %s not found\n",data+12);
		log_print (result+15, LOG_ERROR);
		return ((int)strlen (result));
	}

	sprintf (result,"**00000 COMMANDLIST %d,%d,%d",cb.offset,cb.count_total,cb.count_buffer);

	for (i=0;i < cb.count_buffer;i++) {
		strcat (result,",");
		j = 19;
		while (j && cb.commands[i][j] == ' ') j--;
		cb.commands[i][j+1] = 0;
		strcat (result,cb.commands[i]);
	}
	strcat (result,"\n");

	return ((int)strlen (result));

}

int GetRemotelist (byte *data,byte result[])
{
	int i,j;
	REMOTEBUFFER rb;
	GetRemoteDatabase (&rb,atoi (data + 11));

	sprintf (result,"**00000 REMOTELIST %d,%d,%d",rb.offset,rb.count_total,rb.count_buffer);

	for (i=0;i < rb.count_buffer;i++) {
		strcat (result,",");
		j = 79;
		while (j && rb.remotes[i].name[j] == ' ') j--;
		rb.remotes[i].name[j+1] = 0;
		strcat (result,rb.remotes[i].name);
	}
	strcat (result,"\n");

	return ((int)strlen (result));
}

int GetDevicelist (byte *data,byte result[])
{
	int i,p;
	char st[10];

	sprintf (result,"**00000 DEVICELIST %d",device_cnt);

	for (i=0;i < device_cnt;i++) {
		sprintf (st,",%d ",i);
		strcat (result,st);
		if (IRDevices[i].io.if_type == IF_RS232) strcat (result,"SER ");
		else if (IRDevices[i].io.if_type == IF_USB) strcat (result,"USB ");
		else if (IRDevices[i].io.if_type == IF_LAN) {
			strcat (result,"LAN ");
			strcat (result,IRDevices[i].usb_serno);
			strcat (result," ");
			strcat (result,IRDevices[i].device_node);
			strcat (result," ");
		}
		p = 1;
		if (IRDevices[i].fw_capabilities & FN_MULTISEND2) p = 2;
		else if (IRDevices[i].fw_capabilities & FN_MULTISEND4) p = 4;
		else if (IRDevices[i].fw_capabilities & FN_MULTISEND8) p = 8;
		else if (IRDevices[i].fw_capabilities2 & FN2_MULTISEND16) p = 16;

		if (IRDevices[i].fw_capabilities3 & FN3_MULTISTREAM) sprintf (st,"%02d MULTI",p);
		else sprintf (st,"%02d PORT",p);
		strcat (result,st);
	}
	strcat (result,"\n");

	return ((int)strlen (result));
}


int SendASCII (byte *data,byte result[],int asciimode)
{
	int i,res,flag = 0;
	int cmd_num,adr;
	word framelen;
	char err[2048],txt[2048];
	char remote[80],command[512],ccf[2048];
	int netcom,netmask,bus,led,port,wait;
	unsigned int ledval;

	strcpy (txt,data);
	res = AnalyzeUDPString (data,&netcom,remote,command,ccf,&netmask,&bus,&led,&port,&wait);
	if (res) {
		sprintf (err,"Illegal IRTrans ASCII Command [%d][<%s>]\n",res,txt);
		log_print (err, LOG_ERROR);
		sprintf (result,"**00000 RESULT FORMAT ERROR: %d\n",res);
		return ((int)strlen (result));
	}

	sprintf (txt,"IRTrans ASCII Command: %d %s,%s,%d,%d,%d,%d\n", netcom,remote,command,bus,led,netmask,port);
	log_print (txt,LOG_DEBUG);
	
	adr = 0;

	if (netmask) adr |= 0x10000 | netmask;

	if (led > 3) {
		led -= 4;

		ledval = ((led & 7) << 27) | 0x80000000;
		if (led > 7) {
            led = (led & 24) / 8;
            ledval |= ((led & 3) << 17);
		}
		adr |= ledval;

	}
	else adr |= (led & 3) << 17;

	if (bus == 255) {
		if (strcmp (IRDevices[0].version+1,"6.08.15") >= 0) flag = 1;
		adr |= 0x40000000;
	}
	else {
		if (strcmp (IRDevices[bus].version+1,"6.08.15") >= 0) flag = 1;
		adr |= bus << 19;
	}
	protocol_version = 210;

	if (netcom == COMMAND_SEND || netcom == COMMAND_SENDMASK || netcom == COMMAND_SENDMACRO) {
		if (netcom == COMMAND_SENDMACRO) {
			int cmd_array[16];
			word pause_array[16];
				
			if (!netmask) adr |= 0x10000 | 0xffff;

			for (i=0;i < 16;i++) pause_array[i] = wait;

			res = DBFindRemoteMacro (remote,command,cmd_array,pause_array);
				
			for (i=0;i < 16;i++) if (pause_array[i] > 2500) pause_array[i] = 2500;
			if (!res) res = SendIRMacro (cmd_array,adr,pause_array,&framelen);
		}

		if (netcom == COMMAND_SEND || netcom == COMMAND_SENDMASK) {
			res = DBFindRemoteCommand (remote,command,&cmd_num,NULL);
			if (!res) res = SendIR (cmd_num,adr,(byte)netcom,&framelen);
		}
		
		if (res) {
			GetError (res, txt);
			switch(res) {
				case ERR_REMOTENOTFOUND:
					sprintf (err, txt, remote);
					break;
				case ERR_COMMANDNOTFOUND:
					sprintf (err, txt, command);
					break;
				default:
					sprintf (err, txt);
					break;
			}
			sprintf (result,"**00000 RESULT ERROR: %s",err);
			log_print (err, LOG_ERROR);
			return ((int)strlen (result));
		}
		if (asciimode == MODE_ASCII_TIME) {
			sprintf (result,"**00000 RESULT OK: %d ms\n",framelen);
		}
		else {
			strcpy (result,"**00000 RESULT OK\n");
		}
		if (!flag) msSleep (25);
	}
	return ((int)strlen (result));
}

// Bus übergeben !!!

int FlashHTML (byte *command,byte result[])
{
	int i,p,pos;
	struct stat fst;
	word flashpage,adr;
	word cnt = 0;
	unsigned int mem[65536];
	char fname[256];
	HTTP_DIRECTORY *dir;
	IRDATA_LAN_FLASH ird;
	FILE *fp;
	
	while (*command == ' ') command++;

	p = 0;
	cnt = 0;

	memset (mem,0,sizeof (mem));
	dir = (HTTP_DIRECTORY *)mem;
	memset (&ird,0,sizeof (IRDATA_LAN_FLASH));

	while (command[p]) {
		i = p;
	
		while (command[i] != ';') i++;
		command[i] = 0;

#ifdef WIN32
		sprintf (fname,"..\\html\\%s",command+p);
		fp = fopen (fname,"rb");
#else
		sprintf (fname,"../html/%s",command+p);
		fp = fopen (fname,"r");
#endif
		if (fp) {
			strncpy (dir->dir[cnt].name,command+p,22);
			dir->dir[cnt].name[22] = 0;

#ifdef WIN32
			fstat (_fileno(fp),&fst);
#else
			fstat (fileno(fp),&fst);
#endif

			dir->dir[cnt].timestamp = (unsigned int)(fst.st_mtime + ((unsigned int)70 * 365 * 24 * 3600) + ((unsigned int)17 * 24 * 3600)); // Umrechnung auf NTP Format
			SwapIntN (&dir->dir[cnt].timestamp);

			if (fst.st_size > 0xffff) dir->dir[cnt].len = 0xffff;
			else dir->dir[cnt].len = (word)fst.st_size;

			if (!strcmp (dir->dir[cnt].name + strlen (dir->dir[cnt].name) - 4,".txt")) dir->dir[cnt].filetype = CONTENT_PLAIN | EXTERNAL_FILE;
			else if (!strcmp (dir->dir[cnt].name + strlen (dir->dir[cnt].name) - 4,".htm")) dir->dir[cnt].filetype = CONTENT_HTML | EXTERNAL_FILE;
			else if (!strcmp (dir->dir[cnt].name + strlen (dir->dir[cnt].name) - 5,".html")) dir->dir[cnt].filetype = CONTENT_HTML | EXTERNAL_FILE;
			else if (!strcmp (dir->dir[cnt].name + strlen (dir->dir[cnt].name) - 4,".jpg")) dir->dir[cnt].filetype = CONTENT_JPEG | EXTERNAL_FILE;
			else if (!strcmp (dir->dir[cnt].name + strlen (dir->dir[cnt].name) - 5,".jpeg")) dir->dir[cnt].filetype = CONTENT_JPEG | EXTERNAL_FILE;
			else if (!strcmp (dir->dir[cnt].name + strlen (dir->dir[cnt].name) - 4,".gif")) dir->dir[cnt].filetype = CONTENT_GIF | EXTERNAL_FILE;
			else dir->dir[cnt].filetype = EXTERNAL_FILE;
			cnt++;
			fclose (fp);
		}
		
		p = i + 1;
	}

	dir->count = (word)cnt;
	dir->magic = F_MAGIC;
	pos = (cnt * sizeof (HTTP_DIRENTRY)) / 4 + 1;

	for (cnt=0;cnt < dir->count && pos < 32768;cnt++) {
#ifdef WIN32
		sprintf (fname,"..\\html\\%s",dir->dir[cnt].name);
		fp = fopen (fname,"rb");
#else
		sprintf (fname,"../html/%s",dir->dir[cnt].name);
		fp = fopen (fname,"r");
#endif
		if (!fp) continue;
		
		fread (&mem[pos],1,dir->dir[cnt].len,fp);
		fclose (fp);

		dir->dir[cnt].adr = pos;
		pos += (dir->dir[cnt].len + 3) / 4;

		SwapWordN (&dir->dir[cnt].adr);
		SwapWordN (&dir->dir[cnt].len);
	}
	SwapWordN (&dir->count);
	SwapWordN (&dir->magic);

	sprintf (fname,"HTML Size: %d\n",pos * 4);
	log_print (fname,LOG_INFO);

	if (pos >= 32768) {
		sprintf (fname,"HTML Size %d (Max. is 128K)\n",pos * 4);
		log_print (fname,LOG_ERROR);
		sprintf (result,"**00000 RESULT_HTML_FLASH E%s\n",fname);
		return ((int)strlen (result));
	}
	
	flashpage = 128;

	p = adr = 0;
	while (adr < pos && p != 'E') {
		i = 0;
		do {
			ird.adr = adr;
			ird.len = flashpage;
		
			SwapWordN (&ird.adr);
			SwapWordN (&ird.len);

			memcpy (ird.data,mem + adr,flashpage);
			ird.netcommand = COMMAND_FLASH_HTML;
#ifdef WIN32
			p = IRTransLanFlash (IRDevices,&ird,flashpage,IRDevices[0].io.IPAddr[0].sin_addr.S_un.S_addr);
#else
			p = IRTransLanFlash (IRDevices,&ird,flashpage,IRDevices[0].io.IPAddr[0].sin_addr.s_addr);
#endif
			if (!p) p = 'E';
			i++;
		} while (p == 'E' && i < 5);
		adr += flashpage / 4;
	}
	
	if (p == 'E') {
		strcpy (result,"**00000 RESULT_HTML_FLASH E\n");
	}
	else {
		sprintf (result,"**00000 RESULT_HTML_FLASH O%d\n",pos * 4);
	}
	return ((int)strlen (result));
}



int GetHTMLFileList (byte *command,byte result[])
{
	word cnt = 0,len;
	char fname[256];
	FILE *fp;
	struct stat fst;
    char st[256];
	struct tm *atime;

#ifdef WIN32
    struct _finddata_t c_file;
#ifdef _M_X64
    intptr_t hFile;
#else
    int hFile;
#endif
#endif

#ifdef LINUX
    int fd,pos,lend;
    long off;
	struct dirent *di;
    char mem[2048];
#endif

	while (*command == ' ') command++;

	memcpy (result,"**00000 RESULT_HTML_FILELIST ",29);
	len = 29;
	
#ifdef WIN32
	if((hFile = _findfirst( "..\\html\\*.*", &c_file )) != -1L) {
		do if (c_file.attrib != _A_SUBDIR) {
			sprintf (fname,"..\\html\\%s",c_file.name);
			fp = fopen (fname,"r");
			if (fp) {
				fstat (_fileno(fp),&fst);
				strncpy (result+len,c_file.name,22);
				strcat (result,";");

				atime = localtime (&fst.st_mtime);
				sprintf (st,"%d;%02d.%02d.%04d %02d:%02d;",fst.st_size,atime->tm_mday,atime->tm_mon+1,atime->tm_year + 1900,atime->tm_hour,atime->tm_min);
				strcat (result,st);

				len = (word)strlen (result);
				cnt++;
				fclose (fp);
			}
		} while( _findnext( hFile, &c_file ) == 0);
		_findclose( hFile );
	}
#endif

#ifdef LINUX
        fd = open ("../html",0);
        do {
			lend = getdirentries (fd,mem,2048,&off);
			pos = 0;
			while (pos < lend) {
				di = (struct dirent *)&mem[pos];

				sprintf (fname,"../html/%s",di->d_name);
				fp = fopen (fname,"r");
				if (fp && !fstat (fileno(fp),&fst) && S_ISREG (fst.st_mode)) {
					strncpy (result+len,di->d_name,22);
					strcat (result,";");

					atime = localtime (&fst.st_mtime);
					sprintf (st,"%d;%02d.%02d.%04d %02d:%02d;",fst.st_size,atime->tm_mday,atime->tm_mon+1,atime->tm_year + 1900,atime->tm_hour,atime->tm_min);
					strcat (result,st);
					
					len = strlen (result);
					cnt++;
					fclose (fp);
				}
			
				pos += di -> d_reclen;
			}
        } while (lend);

        close (fd);
#endif

	strcat (result,"\n");
	len++;
	return (len);
}

byte HToI (char *st,byte fac) 
{
  return (GetHexDigit (st[0]) * fac) + GetHexDigit (st[1]);
}


byte GetHexDigit (char v) 
{
  if (v >= '0' && v <= '9') return (v - '0');
  if (v >= 'a' && v <= 'k') return (v - ('a' - 10));
  if (v >= 'A' && v <= 'K') return (v - ('A' - 10));
  
  return (0);
}

