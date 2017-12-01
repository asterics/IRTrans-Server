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
#endif

#ifdef WINCE
#include "winsock2.h"
#include <windows.h>
#endif


#ifdef LINUX
typedef int DWORD;
#define closesocket		close
extern int hCom;
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>
#endif

#include <stdio.h>

#include "remote.h"
#include "errcode.h"
#include "dbstruct.h"
#include "network.h"
#include "lowlevel.h"
#include "global.h"
#include "fileio.h"

#ifdef WIN32
#include "winio.h"
#include "winusbio.h"
#endif


extern IRREMOTE *rem_pnt;
extern int rem_cnt;
extern IRCOMMAND *cmd_pnt;
extern int cmd_cnt;
extern IRTIMING *tim_pnt;
extern int tim_cnt;


#include "flash.h"

#define INITIAL_VALUE 0x92f3
#define INITIAL_VALUE_V2 0x143a
#define INITIAL_VALUE_V4 0x3b79


int CopyTimingIRDB (IRDB_TIMING *irt,int num,int bus);
void set_entry (char entry[],int pos,FLASH_CONTENT_OLD *content,byte type,byte remote,byte group,byte shift,byte setup,word sourcemask,word acc_timeout,word acc_repeat,char *version,byte nmflag,char *remname,char *comname,byte rcv_len);
word FindIRDBCommand (byte remote[],byte command[],IRDB_HEADER *pnt,byte vers);
word FindIRDBRemote (byte remote[],IRDB_HEADER *pnt,byte vers);
void ReadTraReceive (char tp[]);
void ReadTraConfig ();
word CRC (byte *Data, int Length,word init);
void strip (char *st,int len);


word flash_data[65536];
FILE *fptrans;

TRANSLATECOMMAND trans[10000];
IRDBCOMMAND irdb[10000];
IRDBHEADER irdbheader;

int trans_num;

int StoreDbItem (IRDBCOMMAND *db)
{
	int res;
	char remcmp[100];
	char cmdcmp[100];
	static int errcnt;

	trans_num = db->number;

	if (db->type == IRDB_REM || (db->type >= IRDB_TYPEACTION && db->type <= IRDB_TYPEACTION_7)) {

		memset (remcmp,0,100);
		memset (cmdcmp,0,100);

		strcpy (remcmp,db->remote);
		strcpy (cmdcmp,db->command);
		ConvertLcase (remcmp,(int)strlen (remcmp));
		ConvertLcase (cmdcmp,(int)strlen (cmdcmp));

		res = DBFindRemote (remcmp);
		if (res == -1) {
			errcnt++;
			return (ERR_REMOTENOTFOUND);
		}
		db->remote_num = res;

		if (db->type >= IRDB_TYPEACTION && db->type <= IRDB_TYPEACTION_7) db->command_num = DBFindCommand (cmdcmp,&db->remote_num);
	}

	if (db->type == IRDB_TYPEHEADER || (db->type >= IRDB_TYPEHEADER_32K && db->type <= IRDB_TYPEHEADER_96K)) {
		memcpy (&irdbheader,db,sizeof (IRDBHEADER));
		errcnt = 0;
	}
	memcpy (&irdb[trans_num-errcnt],db,sizeof (IRDBCOMMAND));

	return (0);
}

int initIRDBPointer (void)
{
	int i,res,pnt,errcnt = 0;
	char remcmp[100];
	char cmdcmp[100];


	pnt = 0;


	for (i=0;i <= 3;i++) {
		strip (irdbheader.minremote[i],80);
		strip (irdbheader.maxremote[i],80);
		strip (irdbheader.mincommand[i],20);
		strip (irdbheader.mincommand[i],20);
	}

	strip (irdbheader.default_action,100);
	strip (irdbheader.ok_status,20);
	strip (irdbheader.err_status,20);


	while (pnt <= trans_num) {

		if (irdb[pnt].type == IRDB_REM || (irdb[pnt].type >= IRDB_TYPEACTION && irdb[pnt].type <= IRDB_TYPEACTION_7)) {

			memset (remcmp,0,100);
			memset (cmdcmp,0,100);
			strip (irdb[pnt].remote,80);
			strip (irdb[pnt].command,20);
			strip (irdb[pnt].action,256);
			memcpy (remcmp,irdb[pnt].remote,80);
			memcpy (cmdcmp,irdb[pnt].command,20);
			ConvertLcase (remcmp,(int)strlen (remcmp));
			ConvertLcase (cmdcmp,(int)strlen (cmdcmp));

			res = DBFindRemote (remcmp);
			if (res == -1) {
				errcnt++;
			}
			else {
				irdb[pnt].remote_num = res;
				if (irdb[pnt].type >= IRDB_TYPEACTION && irdb[pnt].type <= IRDB_TYPEACTION_7) irdb[pnt].command_num = DBFindCommand (cmdcmp,&irdb[pnt].remote_num);
			}
		}

	pnt++;
	}
	return (0);
}

void strip (char *st,int len)
{
	len--;
	while (len > 0 && st[len] == ' ') st[len--] = 0;

	if (!len && st[len] == ' ') st[len] = 0;

}

void StoreTransItem (TRANSLATECOMMAND *tr)
{
	trans_num = tr->number;
	memcpy (&trans[trans_num],tr,sizeof (TRANSLATECOMMAND));
}


// RCV IRDB String (ohne Offset !)
// Umstellen auf separaten Speicherbereich für Offset Codes


int SetIRDBEx (byte bus,int iradr,STATUSBUFFER *stat)
{
	char st[255];
	byte vers,calflag,rcv_start,rcv_len,cmp_len,cmp_start,toggleflag;
	int i,j,flashwordsize,len,lenf,ntim,act_cnt,size;
	int k,timings[1000];
	word adr,cmd_cnt,rem_cnt,com_adr,hash_cnt,start_adr;
	IRDB_HEADER *pnt;
	IRDB_HEADER_V3 *pnt_v3;
	IRDB_HEADER_V4 *pnt_v4;
	IRDB_REMOTE *rem;
	IRDB_REMOTE_COMPACT *remc;
	IRDB_REMOTE_EX *remex;
	HASH_ENTRY *hash;
	IRDB_COMMAND *com;
	IRDB_COMMAND_COMPACT *comc;
	IRDB_IRCOMMAND *ircom;
	IRDB_ACTION *act;
	IRDB_ACTION_EX *actex;
	IRDB_ACTION_HEADER *actheader;
	IRDB_ACTION_DATA *actdata;
	word rem_adr_det[4000],cadr;
	HASH_ENTRY *chash;
	IRDB_FLASHINFO *finfo;


	finfo = (IRDB_FLASHINFO *)stat;
	memset (flash_data,0,sizeof (flash_data));
	pnt = (IRDB_HEADER *)flash_data;
	pnt_v3 = (IRDB_HEADER_V3 *)flash_data;
	pnt_v4 = (IRDB_HEADER_V4 *)flash_data;

	act_cnt = 0;
	vers = 0;

	if (IRDevices[bus].version[0] == 0) return (ERR_WRONGBUS);
	
	if (strcmp (IRDevices[bus].version+1,"5.04.17") >= 0) vers = 1;		// Erste IRDB Command Struktur
	if (strcmp (IRDevices[bus].version+1,"5.04.28") >= 0) vers = 2;		// Geänderte IRDB Command Struktur: Compact Structs
	if (strcmp (IRDevices[bus].version+1,"5.05.04") >= 0) vers = 3;		// EX IRDB Struktur (IPs, RS232 separat, Remote Action
	if (strcmp (IRDevices[bus].version+1,"5.08.25") >= 0) vers = 4;		// Erweiterte Actions / Action Input
	if (strcmp (IRDevices[bus].version+1,"5.10.06") >= 0) vers = 5;		// ADC für 1wire I/O
	if (strcmp (IRDevices[bus].version+1,"5.11.04") >= 0) vers = 6;		// 
	calflag = (IRDevices[bus].fw_capabilities & FN_CALIBRATE) != 0;
	toggleflag = (IRDevices[bus].io.toggle_support) != 0;


	if (vers >= 5) {
		adr = pnt->table.remote_adr = IRDB_HEADER_LEN;
	}
	else {
		if (vers >= 4) {
			adr = pnt->table.remote_adr = IRDB_HEADER_LEN_V4;
		}
		else {
			adr = pnt->table.remote_adr = IRDB_HEADER_LEN_V3;
		}
	}

	if (irdb[0].type == IRDB_TYPEHEADER || (irdb[0].type >= IRDB_TYPEHEADER_32K && irdb[0].type <= IRDB_TYPEHEADER_96K)) {
		if (irdb[0].type == IRDB_TYPEHEADER_32K) {
			start_adr = 98304 / 2;
			pnt = (IRDB_HEADER *)(flash_data + 98304 / 2);
			adr = pnt->table.remote_adr = IRDB_HEADER_LEN + 98304 / 2;
		}
		else if (irdb[0].type == IRDB_TYPEHEADER_64K) {
			start_adr = 65536 / 2;
			pnt = (IRDB_HEADER *)(flash_data + 65536 / 2);
			adr = pnt->table.remote_adr = IRDB_HEADER_LEN + 65536 / 2;
		}
		else if (irdb[0].type == IRDB_TYPEHEADER_96K) {
			start_adr = 32768 / 2;
			pnt = (IRDB_HEADER *)(flash_data + 32768 / 2);
			adr = pnt->table.remote_adr = IRDB_HEADER_LEN + 32768 / 2;
		}
		else start_adr = 0;
	
		pnt->type = FLASH_TYPE_IRDB;

		memcpy (pnt->table.default_action,irdbheader.default_action,100);
		memcpy (pnt->table.return_stat_ok,irdbheader.ok_status,20);
		memcpy (pnt->table.return_stat_err,irdbheader.err_status,20);

		if (vers >= 4) {
			pnt->table.target_ip = irdbheader.ip;
			pnt->table.target_port = irdbheader.port;
			for (i=0;i <= 3;i++) {
				if (vers >= 5) {
					pnt->table.adc_margin[i].mode = irdbheader.inputmode[i];
					pnt->table.adc_margin[i].low_value = irdbheader.min[i];
					pnt->table.adc_margin[i].high_value = irdbheader.max[i];
					pnt->table.adc_margin[i].hysteresis = irdbheader.hyst[i];
					pnt->table.adc_irdata[i].low_ledsel = irdbheader.minledsel[i];
					pnt->table.adc_irdata[i].high_ledsel = irdbheader.maxledsel[i];
					pnt->table.adc_irdata[i].low_mask = irdbheader.minmask[i];
					pnt->table.adc_irdata[i].high_mask = irdbheader.maxmask[i];
				}
				else {
					pnt_v4->table.adc[i].mode = irdbheader.inputmode[i];
					pnt_v4->table.adc[i].low_value = irdbheader.min[i];
					pnt_v4->table.adc[i].high_value = irdbheader.max[i];
					pnt_v4->table.adc[i].hysteresis = irdbheader.hyst[i];
					pnt_v4->table.adc[i].low_ledsel = irdbheader.minledsel[i];
					pnt_v4->table.adc[i].high_ledsel = irdbheader.maxledsel[i];
					pnt_v4->table.adc[i].low_mask = irdbheader.minmask[i];
					pnt_v4->table.adc[i].high_mask = irdbheader.maxmask[i];
				}
			}
		}
		else {
			for (i=0;i <= 3;i++) {
				pnt_v3->table.adc[i].mode = irdbheader.inputmode[i];
				pnt_v3->table.adc[i].low_value = irdbheader.min[i];
				pnt_v3->table.adc[i].high_value = irdbheader.max[i];
				pnt_v3->table.adc[i].hysteresis = irdbheader.hyst[i];
				pnt_v3->table.adc[i].low_ledsel = irdbheader.minledsel[i];
				pnt_v3->table.adc[i].high_ledsel = irdbheader.maxledsel[i];
				pnt_v3->table.adc[i].low_mask = irdbheader.minmask[i];
				pnt_v3->table.adc[i].high_mask = irdbheader.maxmask[i];
			}
		}

 		if (!(irdbheader.active & 1)) pnt->type = 0xffff;			// Ausschalten der IRDB
		else {
			if ((irdbheader.active & 2) && vers >= 3) pnt->type = FLASH_TYPE_IRDBAUX;
			if ((irdbheader.active & 4) && vers >= 3) pnt->type |= FLASH_MODE_LAN_DB;
			if ((irdbheader.active & 8) && vers >= 3) pnt->type |= FLASH_MODE_LANRELAIS;
			if ((irdbheader.active & 16) && vers >= 4) pnt->type |= FLASH_MODE_IRDB_SENDONLY;
			if ((irdbheader.active & 32) && vers >= 4) pnt->type |= FLASH_MODE_SEND_LINE;
			if ((irdbheader.active & 64) && vers >= 4) pnt->type |= FLASH_MODE_SEND_BINARY;
		}
		if (vers >= 4) {
			pnt->table.rs232_config[0] = irdbheader.rs232_config[0];
			pnt->table.rs232_config[1] = irdbheader.rs232_config[1];
			pnt->table.rs232_config[2] = irdbheader.rs232_config[2];
			pnt->table.rs232_config[3] = irdbheader.rs232_config[3];
		}
	}

	rem_cnt = 0;
	rem = (IRDB_REMOTE *)(flash_data + adr);
	remc = (IRDB_REMOTE_COMPACT *)(flash_data + adr);
	remex = (IRDB_REMOTE_EX *)(flash_data + adr);

	for (i=0;i <= trans_num;i++) {
		if (irdb[i].type == IRDB_REM) {
			rem_adr_det[rem_cnt] = adr;

			if (vers >= 3) {
				remex->ip = irdb[i].ip;
				remex->port = irdb[i].port;
				memcpy (remex->name,irdb[i].remote,80);
				len = (int)strlen (irdb[i].remote);
				if (len > 80) len = 80;
				remex->namelen = len;

				remex->modeflags = irdb[i].action_type;

				memcpy (remex->name+len,irdb[i].action,50);
				lenf = (int)strlen (irdb[i].action);
				
				if (lenf > 50) lenf = 50;
				remex->defactlen = lenf;

				adr += (word)((IRDB_REMOTE_LEN_EX + len + lenf) / 2);
				remex = (IRDB_REMOTE_EX *)(flash_data + adr);
			}
			else if (vers >= 2) {
				memcpy (remc->name,irdb[i].remote,80);
				len = (int)strlen (irdb[i].remote);
				if (len > 80) len = 80;
				remc->namelen = len;

				adr += (word)((IRDB_REMOTE_LEN_COMPACT + len) / 2);
				remc = (IRDB_REMOTE_COMPACT *)(flash_data + adr);
			}
			else {
				memcpy (rem->name,irdb[i].remote,80);
				adr += IRDB_REMOTE_LEN;
				rem = (IRDB_REMOTE *)(flash_data + adr);
			}
			
			rem_cnt++;

		}
	}
	pnt->table.remote_cnt = rem_cnt;

	rem_cnt = 0;
	pnt->table.remote_hash_adr = adr;

	for (i=0;i <= trans_num;i++) {
		if (irdb[i].type == IRDB_REM) {
			hash = (HASH_ENTRY *)(flash_data + adr);
			len = (int)strlen (irdb[i].remote);
			if (len > 80) len = 80;
			hash->hashcode = get_hashcode (irdb[i].remote,(byte)len);
			hash->adr = rem_adr_det[rem_cnt];

			adr += HASH_LEN;
			rem_cnt++;
		}
	}


	pnt->table.ircommand_hash_adr = adr;
	hash = (HASH_ENTRY *)(flash_data + adr);
	cmd_cnt = 0;
	rcv_start = rcv_len = 0;

//  Bei Wechsel der REM prüfen auf geänderte Start / LEN
//	Bei geänderten Werten HASH_ENTRY_OFFSET schreiben
//	Offsetwerte bei Hashbestimmung berücksichtigen

	for (i=0;i <= trans_num;i++) {
		if (irdb[i].type == IRDB_REM) {
			j = rem_pnt[irdb[i].remote_num].command_start;
			if (vers >= 6 && (rem_pnt[irdb[i].remote_num].rcv_start != rcv_start || rem_pnt[irdb[i].remote_num].rcv_len != rcv_len)) {
				rcv_start = rem_pnt[irdb[i].remote_num].rcv_start;
				rcv_len = rem_pnt[irdb[i].remote_num].rcv_len;
				((HASH_ENTRY_OFFSET *)hash)->rcv_start = rcv_start;
				((HASH_ENTRY_OFFSET *)hash)->rcv_len = rcv_len;
				hash->adr = 0;
				adr += HASH_LEN;
				hash = (HASH_ENTRY *)(flash_data + adr);
				cmd_cnt++;
			}
			while (j < rem_pnt[irdb[i].remote_num].command_end) {
				if (rcv_len && rcv_len < cmd_pnt[j].ir_length) cmp_len = rcv_len;
				else cmp_len = (byte)cmd_pnt[j].ir_length;
				if (rcv_start < cmp_len) cmp_start = rcv_start;
				else cmp_start = 0;

				k = 0;
				if (cmd_pnt[j].data[0] < '0') cmp_start += (k = cmd_pnt[j].data[0]);
				if (cmd_pnt[j].data[k] == 'X' && cmd_pnt[j].data[k+1] == 'l') cmp_start += 2;					// Offsets für "XL" + evtl. Code Offsets

				hash->hashcode = get_hashcode (cmd_pnt[j].data + cmp_start,(byte)(cmp_len - cmp_start));
				hash->adr = 1;
				adr += HASH_LEN;
				hash = (HASH_ENTRY *)(flash_data + adr);
				cmd_cnt++;
				j++;
			}

		}
	}

	for (i=0;i < tim_cnt;i++) tim_pnt[i].flash_adr = 0;

	for (i=0;i <= trans_num;i++) {
		if (irdb[i].type == IRDB_REM) {
			j = rem_pnt[irdb[i].remote_num].timing_start;

			while (j < rem_pnt[irdb[i].remote_num].timing_end) {
				if (tim_pnt[j].link_count) {
					tim_pnt[j].flash_adr = adr;

					adr += CopyTimingIRDB ((IRDB_TIMING *)(flash_data + adr),j,bus);
				}
				j++;
			}
		}
	}


	for (i=0;i < 1000;i++) timings[i] = -1;

	for (i=0;i <= trans_num;i++) {
		if (irdb[i].type == IRDB_REM) {
			j = rem_pnt[irdb[i].remote_num].command_start;
			while (j < rem_pnt[irdb[i].remote_num].command_end) {
				if (cmd_pnt[j].timing == LINK_IRCOMMAND && tim_pnt[cmd_pnt[cmd_pnt[j].command_link].timing].flash_adr == 0) {	// Timing noch nicht da
					k = 0;
					while (timings[k] != cmd_pnt[cmd_pnt[j].command_link].timing && timings[k] != -1 && k < 999) k++;
					if (timings[k] == -1) {
						timings[k] = cmd_pnt[cmd_pnt[j].command_link].timing + rem_pnt[cmd_pnt[j].remote_link].timing_start;
					}
				}
				j++;
			}
		}
	}

	i = 0;
	while (timings[i] != -1 && i < 999) {
		tim_pnt[timings[i]].flash_adr = adr;
		adr += CopyTimingIRDB ((IRDB_TIMING *)(flash_data + adr),timings[i],bus);
		i++;
	}
	
	pnt->table.command_cnt = cmd_cnt;
	com_adr = adr;
	rem_cnt = 0;
	
	for (i=0;i <= trans_num;i++) {
		if (irdb[i].type == IRDB_REM) {
			j = rem_pnt[irdb[i].remote_num].command_start;
			cmd_cnt = 0;
			while (j < rem_pnt[irdb[i].remote_num].command_end) {
				com = (IRDB_COMMAND *)(flash_data + adr);
				comc = (IRDB_COMMAND_COMPACT *)(flash_data + adr);

				if (cmd_pnt[j].timing == LINK_IRCOMMAND) k = cmd_pnt[j].command_link;
				else k = j;

				if (vers >= 2) {
					memset (comc,0,sizeof (IRDB_COMMAND_COMPACT));
					comc->remote_adr = rem_adr_det[rem_cnt];
					len = (int)strlen (cmd_pnt[j].name);
					if (len > 20) len = 20;
					memcpy (comc->name,cmd_pnt[j].name,len);

					ntim = rem_pnt[cmd_pnt[k].remote].timing_start + cmd_pnt[k].timing;
					comc->timing_adr = tim_pnt[ntim].flash_adr;

					remc = (IRDB_REMOTE_COMPACT *)(flash_data + comc->remote_adr);
					if (!cmd_cnt) remc->command_adr = adr;

					comc->toggle_seq = cmd_pnt[j].toggle_seq;
					comc->namelen = len;

					adr += (word)((IRDB_COMMAND_LEN_COMPACT + len) / 2);
				}
				else {
					com->remote_adr = rem_adr_det[rem_cnt];
					len = (int)strlen (cmd_pnt[j].name) + 1;
					if (len > 20) len = 20;
					memset (com->name,' ',20);
					memcpy (com->name,cmd_pnt[j].name,len);

					ntim = rem_pnt[cmd_pnt[k].remote].timing_start + cmd_pnt[k].timing;
					com->timing_adr = tim_pnt[ntim].flash_adr;

					rem = (IRDB_REMOTE *)(flash_data + com->remote_adr);
					if (!cmd_cnt) rem->command_adr = adr;

					if (vers >= 1) {
						com->toggle_seq = cmd_pnt[k].toggle_seq;
						adr += IRDB_COMMAND_LEN;
					}
					else adr += IRDB_COMMAND_LEN_1;
				}

				j++;
				cmd_cnt++;
			}
			if (vers >= 2) 	remc->command_cnt = cmd_cnt;
			else rem->command_cnt = cmd_cnt;
			rem_cnt++;
		}
	}


	rem_cnt = 0;
	
	for (i=0;i <= trans_num;i++) {
		if (irdb[i].type == IRDB_REM) {
			j = rem_pnt[irdb[i].remote_num].command_start;
			cmd_cnt = 0;
			rem = (IRDB_REMOTE *)(flash_data + rem_adr_det[rem_cnt]);
			remc = (IRDB_REMOTE_COMPACT *)(flash_data + rem_adr_det[rem_cnt]);
			cadr = remc->command_adr;

			while (j < rem_pnt[irdb[i].remote_num].command_end) {
				hash = (HASH_ENTRY *)(flash_data + adr);

				len = (int)strlen (cmd_pnt[j].name);
				if (len > 20) len = 20;

				hash->hashcode = get_hashcode (cmd_pnt[j].name,(byte)len);

				if (vers >= 2) hash->adr = cadr;
				else if (vers >= 1) hash->adr = rem->command_adr + cmd_cnt * IRDB_COMMAND_LEN;
				else hash->adr = rem->command_adr + cmd_cnt * IRDB_COMMAND_LEN_1;


				if (!cmd_cnt) {
					if (vers >= 2) remc->command_hash = adr;
					else rem->command_hash = adr;
				}

				adr += HASH_LEN;
				j++;
				cmd_cnt++;
				cadr += (word)((IRDB_COMMAND_LEN_COMPACT + len) / 2);
			}
			rem_cnt++;
		}
	}

	cmd_cnt = 0;
	rem_cnt = 0;
	hash_cnt = 0;
	hash = (HASH_ENTRY *)(flash_data + pnt->table.ircommand_hash_adr);
	pnt->table.ircommand_adr = adr;
	ircom = (IRDB_IRCOMMAND *)(flash_data + adr);

	for (i=0;i <= trans_num;i++) {
		if (irdb[i].type == IRDB_REM) {
			j = rem_pnt[irdb[i].remote_num].command_start;
			rem = (IRDB_REMOTE *)(flash_data + rem_adr_det[rem_cnt]);
			remc = (IRDB_REMOTE_COMPACT *)(flash_data + rem_adr_det[rem_cnt]);
			rem_cnt++;
			if (vers >= 2) chash = (HASH_ENTRY *)(flash_data + remc->command_hash);
			else chash = (HASH_ENTRY *)(flash_data + rem->command_hash);

			while (j < rem_pnt[irdb[i].remote_num].command_end) {
				com = (IRDB_COMMAND *)(flash_data + chash->adr);
				comc = (IRDB_COMMAND_COMPACT *)(flash_data + chash->adr);

				if (cmd_pnt[j].timing == LINK_IRCOMMAND) k = cmd_pnt[j].command_link;
				else k = j;

				if (!(cmd_pnt[k].mode & RAW_DATA) && ((calflag && cmd_pnt[k].ir_length_cal > CODE_LEN) || cmd_pnt[k].ir_length > CODE_LEN)) {			// Long Codes packen
					int ii,pp;
					ii = pp = 0;

					if (calflag) {
						while (pp < cmd_pnt[k].ir_length_cal) {
							if (cmd_pnt[k].data_cal[pp] & 128) ircom->ir_data_ir[ii++] = cmd_pnt[k].data_cal[pp++];
							else {
								ircom->ir_data_ir[ii] = (cmd_pnt[k].data_cal[pp++] & 7);
								if (pp < cmd_pnt[k].ir_length_cal) ircom->ir_data_ir[ii] |= ((cmd_pnt[k].data_cal[pp] & 7) << 4);
								pp++;
								ii++;
							}
						}
						
						ircom->ir_data_ir[0] |= LONG_CODE_FLAG;
						if (pp > cmd_pnt[k].ir_length_cal) ircom->ir_data_ir[0] |= LONG_CODE_LEN;
						ircom->ir_length = ii;
					}
					else {
						while (pp < cmd_pnt[k].ir_length) {
							if (cmd_pnt[k].data[pp] & 128) pp++;
							else {
								ircom->ir_data_ir[ii] = (cmd_pnt[k].data[pp++] & 7);
								if (pp < cmd_pnt[k].ir_length) ircom->ir_data_ir[ii] |= ((cmd_pnt[k].data[pp] & 7) << 4);
								pp++;
								ii++;
							}
						}
						
						ircom->ir_data_ir[0] |= LONG_CODE_FLAG;
						if (pp > cmd_pnt[k].ir_length) ircom->ir_data_ir[0] |= LONG_CODE_LEN;
						ircom->ir_length = ii;
					}
				}
				else {
					if (calflag && !(cmd_pnt[k].mode & RAW_DATA)) {
						if (!toggleflag) {
							memcpy (ircom->ir_data_ir,cmd_pnt[k].data_cal,(byte)cmd_pnt[k].ir_length_cal);
							ircom->ir_length = (byte)cmd_pnt[k].ir_length_cal;
						}
						else {
							memcpy (ircom->ir_data_ir,cmd_pnt[k].data_offset,(byte)cmd_pnt[k].ir_length_offset);
							ircom->ir_length = (byte)cmd_pnt[k].ir_length_offset;

						}
					}
					else {
						memcpy (ircom->ir_data_ir,cmd_pnt[k].data,(byte)cmd_pnt[k].ir_length);
						ircom->ir_length = (byte)cmd_pnt[k].ir_length;
					}
				}

				ircom->command_adr = chash->adr;

				if (vers >= 2) comc->ird_adr = adr;
				else com->ird_adr = adr;
				
				if (cmd_pnt[k].timing == RS232_IRCOMMAND) {
					if (vers >= 2) comc->timing_adr = (word)cmd_pnt[k].pause;
					ircom->command_adr = 1;
				}

				else if (cmd_pnt[k].mode & RAW_DATA) {
					if (vers >= 2) comc->timing_adr = (word)cmd_pnt[k].timing;
					else com->timing_adr = (word)cmd_pnt[k].timing;
					ircom->command_adr = 0;
				}
				else {
					if (hash[cmd_cnt + hash_cnt].adr == 0) hash_cnt++;
					hash[cmd_cnt + hash_cnt].adr = adr;
				}

				adr += (sizeof (IRDB_IRCOMMAND) + ircom->ir_length) / 2; // +1 steckt bereits in der Länge 1 von data in IRDB_IRCOMMAND
				ircom = (IRDB_IRCOMMAND *)(flash_data + adr);

				cmd_cnt++;
				chash++;
				j++;
			}
		}
	}

	if (vers >= 4) {
		pnt->table.action_adr = adr;
		actheader = (IRDB_ACTION_HEADER *)(flash_data + adr);
		for (i=0;i <= trans_num;i++) {
			if (irdb[i].type == IRDB_TYPEACTION) {
				int l,n,p;
				l = 0;
				actheader->relais = irdb[i].relais;
				for (n = 0;n <= 7 && irdb[i+n].type == IRDB_TYPEACTION + n;n++) {
					p = 0;
					actdata = (IRDB_ACTION_DATA *)(actheader->data + l);
					if (irdb[i+n].action_len || irdb[i+n].ip || irdb[i+n].port || irdb[i+n].action_type != 2) {
						if (irdb[i+n].action_len > 245) irdb[i+n].action_len = 245;
						actheader->action_cnt++;
						actdata->action_len = irdb[i+n].action_len;
						actdata->action_type = irdb[i+n].action_type;

						if (irdb[i+n].ip) {
							memcpy (actdata->action + p,&irdb[i+n].ip,4);
							p += 4;
							actdata->action_type |= ACTION_IP;
						}
						if (irdb[i+n].port) {
							memcpy (actdata->action + p,&irdb[i+n].port,2);
							p += 2;
							actdata->action_type |= ACTION_PORT;
						}
						memcpy (actdata->action + p,irdb[i+n].action,irdb[i+n].action_len);
						p += irdb[i+n].action_len;
						if (irdb[i+n].action_len & 1) p++;
						l += p + 2;
					}
				}
				j = FindIRDBCommand (irdb[i].remote,irdb[i].command,pnt,vers);
				if (j) {
					comc = (IRDB_COMMAND_COMPACT *)(flash_data + j);
					comc->action_adr = adr;
					actheader->command_adr = j;
				}
				
				adr += (actheader->total_length = (l + sizeof (IRDB_ACTION_HEADER) - 1) / 2);

				actheader = (IRDB_ACTION_HEADER *)(flash_data + adr);
				act_cnt++;
			}
		}
		pnt->table.action_cnt = act_cnt;
	}
	else if (vers >= 3) {
		actex = (IRDB_ACTION_EX *)(flash_data + adr);
		for (i=0;i <= trans_num;i++) {
			if (irdb[i].type == IRDB_TYPEACTION_2) {
				int n,p;
				actex->relais = irdb[i-2].relais;
				i -= 2;
				p = 0;
				for (n=0;n <= 2;n++) {
					actex->action_type[n] = irdb[i+n].action_type;
					actex->action_len[n] = irdb[i+n].action_len;

					if (irdb[i+n].ip) {
						memcpy (actex->action + p,&irdb[i+n].ip,4);
						p += 4;
						actex->action_type[n] |= ACTION_IP;
					}
					if (irdb[i+n].port) {
						memcpy (actex->action + p,&irdb[i+n].port,2);
						p += 2;
						actex->action_type[n] |= ACTION_PORT;
					}
					memcpy (actex->action + p,irdb[i+n].action,irdb[i+n].action_len);
					p += irdb[i+n].action_len;
					if (irdb[i+n].action_len & 1) p++;
				}

				j = FindIRDBCommand (irdb[i].remote,irdb[i].command,pnt,vers);
				if (j) {
					comc = (IRDB_COMMAND_COMPACT *)(flash_data + j);
					comc->action_adr = adr;
				}

				adr += (word)((sizeof (IRDB_ACTION_EX) + p) / 2); // +1 steckt bereits in der Länge 1 von action in IRDB_ACTION
				actex = (IRDB_ACTION_EX *)(flash_data + adr);
				act_cnt++;
				i += 2;
			}
		}
	}
	else {
		act = (IRDB_ACTION *)(flash_data + adr);
		for (i=0;i <= trans_num;i++) {
			if (irdb[i].type == IRDB_TYPEACTION) {
				act->action_len = irdb[i].action_len;
				act->action_type = irdb[i].action_type;
				act->relais = irdb[i].relais;
				memcpy (act->action,irdb[i].action,irdb[i].action_len);

				j = FindIRDBCommand (irdb[i].remote,irdb[i].command,pnt,vers);
				if (j) {
					com = (IRDB_COMMAND *)(flash_data + j);
					comc = (IRDB_COMMAND_COMPACT *)(flash_data + j);
					if (vers >= 2) comc->action_adr = adr;
					else com->action_adr = adr;
				}

				adr += (sizeof (IRDB_ACTION) + act->action_len) / 2; // +1 steckt bereits in der Länge 1 von action in IRDB_ACTION
				act = (IRDB_ACTION *)(flash_data + adr);
				act_cnt++;
			}
		}
	}


	if (irdb[0].type == IRDB_TYPEHEADER || (irdb[0].type >= IRDB_TYPEHEADER_32K && irdb[0].type <= IRDB_TYPEHEADER_96K)) {
		for (i=0;i <= 3;i++) {
			if (irdbheader.minremote[i][0] && irdbheader.mincommand[i][0]) {
				j = FindIRDBRemote (irdbheader.minremote[i],pnt,vers);
				if (!j) {
					if (vers >= 5) pnt->table.adc_irdata[i].low_remote = 0;
					else {
						if (vers >= 4) pnt_v4->table.adc[i].low_remote = 0;
						else pnt_v3->table.adc[i].low_remote = 0;
					}
					sprintf (st,"Minremote %d [%s] not found in Flash IRDB\n",i,irdbheader.minremote[i]);
					log_print (st,LOG_ERROR);
				}
				else {
					if (vers >= 5) {
						pnt->table.adc_irdata[i].low_remote = j;
						memcpy (pnt->table.adc_irdata[i].low_commands,irdbheader.mincommand[i],sizeof (irdbheader.mincommand[i]));
					}
					else {
						if (vers >= 4) {
							pnt_v4->table.adc[i].low_remote = j;
							memcpy (pnt_v4->table.adc[i].low_commands,irdbheader.mincommand[i],sizeof (irdbheader.mincommand[i]));
						}
						else {
							pnt_v3->table.adc[i].low_remote = j;
							memcpy (pnt_v3->table.adc[i].low_commands,irdbheader.mincommand[i],sizeof (irdbheader.mincommand[i]));
						}
					}
				}
			}
			else {
				if (vers >= 5) pnt->table.adc_irdata[i].low_remote = 0;
				else {
					if (vers >= 4) pnt_v4->table.adc[i].low_remote = 0;
					else pnt_v3->table.adc[i].low_remote = 0;
				}
			}

			if (irdbheader.maxremote[i][0] && irdbheader.maxcommand[i][0]) {
				j = FindIRDBRemote (irdbheader.maxremote[i],pnt,vers);
				if (!j) {
					if (vers >= 5) pnt->table.adc_irdata[i].high_remote = 0;
					else {
						if (vers >= 4) pnt_v4->table.adc[i].high_remote = 0;
						else pnt_v3->table.adc[i].high_remote = 0;
					}
					sprintf (st,"Maxremote %d [%s] not found in Flash IRDB\n",i,irdbheader.maxremote[i]);
					log_print (st,LOG_ERROR);
				}
				else {
					if (vers >= 5) {
						pnt->table.adc_irdata[i].high_remote = j;
						memcpy (pnt->table.adc_irdata[i].high_commands,irdbheader.maxcommand[i],sizeof (irdbheader.maxcommand[i]));
					}
					else {
						if (vers >= 4) {
							pnt_v4->table.adc[i].high_remote = j;
							memcpy (pnt_v4->table.adc[i].high_commands,irdbheader.maxcommand[i],sizeof (irdbheader.maxcommand[i]));
						}
						else {
							pnt_v3->table.adc[i].high_remote = j;
							memcpy (pnt_v3->table.adc[i].high_commands,irdbheader.maxcommand[i],sizeof (irdbheader.maxcommand[i]));
						}
					}
				}
			}
			else {
				if (vers >= 5) pnt->table.adc_irdata[i].high_remote = 0;
				else {
					if (vers >= 4) pnt_v4->table.adc[i].high_remote = 0;
					else pnt_v3->table.adc[i].high_remote = 0;
				}
			}
		}
	}

	pnt->len = adr - start_adr;

	if (vers >= 4) pnt->crc = CRC ((byte *)(flash_data + start_adr + 1),((pnt->len - 1) * 2),INITIAL_VALUE_V4);
	else if (vers >= 2) pnt->crc = CRC ((byte *)(flash_data + start_adr + 1),((pnt->len - 1) * 2),INITIAL_VALUE_V2);
	else pnt->crc = CRC ((byte *)(flash_data + start_adr + 1),((pnt->len - 1) * 2),INITIAL_VALUE);
	

	sprintf (st,"Flash IRDB Size: %d Words (%d KBytes).\n%-3d Remotes\n%-3d Commands\n%-3d Actions\n%d Start Adr\n",pnt->len,(pnt->len) / 512,pnt->table.remote_cnt,pnt->table.command_cnt,act_cnt,start_adr); 
	log_print (st,LOG_DEBUG);

	finfo->statuslen = sizeof (IRDB_FLASHINFO);
	finfo->statustype = STATUS_IRDBFLASH;
	sprintf (finfo->memsize,"%d Words (%d KBytes)",pnt->len,(pnt->len)/512);
	sprintf (finfo->remotes,"%d",pnt->table.remote_cnt);
	sprintf (finfo->commands,"%d",pnt->table.command_cnt);
	sprintf (finfo->actions,"%d",act_cnt);
	if (IRDevices[bus].fw_capabilities & FN_FLASH128) {
		if (start_adr == (32768/2)) sprintf (finfo->flashsize,"128/96K");
		else if (start_adr == (65536/2)) sprintf (finfo->flashsize,"128/64K");
		else if (start_adr == (98304/2)) sprintf (finfo->flashsize,"128/32K");
		else sprintf (finfo->flashsize,"128K");
		size = 65535 * 2;
	}
	else {
		sprintf (finfo->flashsize,"64K");
		size = 65535;
	}

	if ((adr * 2) > size) return (0);
	
	flashwordsize = 128;

	adr = 0;
	while (adr < pnt->len) {
		if ((adr + flashwordsize /2) < pnt->len) TransferFlashdataEx (bus,flash_data + start_adr + adr,adr + start_adr,flashwordsize,0,iradr);
		else TransferFlashdataEx (bus,flash_data + start_adr + adr,adr + start_adr,flashwordsize,1,iradr);
		adr += flashwordsize / 2;
	}

	return (0);
}


int CopyTimingIRDB (IRDB_TIMING *irt,int num,int bus)
{
	if (tim_pnt[num].mode == TIMECOUNT_18) {
		IRDB_TIMING_18 *irt18 = (IRDB_TIMING_18 *)irt;

		memcpy (&irt18->ir_length,&tim_pnt[num].ir_length,3);
		memcpy (irt18->pause_len,tim_pnt[num].pause_len,TIME_LEN_18 * 2);
		memcpy (irt18->pulse_len,tim_pnt[num].pulse_len,TIME_LEN_18 * 2);
		memcpy (&irt18->time_cnt,&tim_pnt[num].time_cnt,3);
		irt18->time_cnt = tim_pnt[num].timecount_mode;;

		return (IRDB_TIMING_18_LEN);
	}
	else if ((tim_pnt[num].mode & SPECIAL_IR_MODE) == PULSE_200) {
		int i;
		IRDB_TIMING_SINGLE *irs = (IRDB_TIMING_SINGLE *)irt;
		
		if (!(IRDevices[bus].fw_capabilities2 & FN2_PULSE200)) {
			IRDB_TIMING_18 *irt18 = (IRDB_TIMING_18 *)irt;

			memcpy (&irt18->ir_length,&tim_pnt[num].ir_length,3);
			irt18->mode = TIMECOUNT_18;
			memcpy (irt18->pause_len,tim_pnt[num].pause_len,TIME_LEN_18 * 2);
			for (i=0;i < TIME_LEN_18;i++) irt18->pulse_len[i] = tim_pnt[num].pulse_len[0];
			memcpy (&irt18->time_cnt,&tim_pnt[num].time_cnt,3);
		
			return (IRDB_TIMING_18_LEN);
		}

		else {
			memcpy (&irs->ir_length,&tim_pnt[num].ir_length,3);
			memcpy (irs->multi_len,tim_pnt[num].pause_len,TIME_LEN_SINGLE * 2);
			memcpy (&irs->single_len,tim_pnt[num].pulse_len,2);
			memcpy (&irs->time_cnt,&tim_pnt[num].time_cnt,3);

			return (IRDB_TIMING_SINGLE_LEN);
		}
	}
	else {
		memcpy (&irt->ir_length,&tim_pnt[num].ir_length,3);
		memcpy (irt->pause_len,tim_pnt[num].pause_len,TIME_LEN * 2);
		memcpy (irt->pulse_len,tim_pnt[num].pulse_len,TIME_LEN * 2);
		memcpy (&irt->time_cnt,&tim_pnt[num].time_cnt,3);
		irt->time_cnt = tim_pnt[num].timecount_mode;
		
		return (IRDB_TIMING_LEN);
	}
}


word FindIRDBCommand (byte remote[],byte command[],IRDB_HEADER *pnt,byte vers)
{
	word i,cnt,adr,len;
	IRDB_REMOTE *rem;
	IRDB_COMMAND *com;
	IRDB_COMMAND_COMPACT *comc;
	IRDB_REMOTE_COMPACT *remc;
	IRDB_REMOTE_EX *remex;

	if (vers >= 3) {
		adr = pnt->table.remote_adr;

		len = (word)strlen (remote);

		for (i=0;i < pnt->table.remote_cnt;i++)	{
			remex = (IRDB_REMOTE_EX *)(flash_data + adr);
			if (len == remex->namelen && !memcmp (remex->name,remote,len)) break;
			adr += (IRDB_REMOTE_LEN_EX + remex->namelen) / 2;
		}

		if (i == pnt->table.remote_cnt) return (0);

		cnt = remex->command_cnt;
		adr = remex->command_adr;

		len = (word)strlen (command);

		for (i=0;i < cnt;i++) {
			comc = (IRDB_COMMAND_COMPACT *)(flash_data + adr);
			if (len == comc->namelen && !memcmp (comc->name,command,len)) return (adr);

			adr += (IRDB_COMMAND_LEN_COMPACT + comc->namelen) / 2;
		}
	}

	else if (vers >= 2) {
		adr = pnt->table.remote_adr;

		len = (word)strlen (remote);

		for (i=0;i < pnt->table.remote_cnt;i++)	{
			remc = (IRDB_REMOTE_COMPACT *)(flash_data + adr);
			if (len == remc->namelen && !memcmp (remc->name,remote,len)) break;
			adr += (IRDB_REMOTE_LEN_COMPACT + remc->namelen) / 2;
		}


		if (i == pnt->table.remote_cnt) return (0);

		cnt = remc->command_cnt;
		adr = remc->command_adr;

		len = (word)strlen (command);

		for (i=0;i < cnt;i++) {
			comc = (IRDB_COMMAND_COMPACT *)(flash_data + adr);
			if (len == comc->namelen && !memcmp (comc->name,command,len)) return (adr);

			adr += (IRDB_COMMAND_LEN_COMPACT + comc->namelen) / 2;
		}
	}
	
	else {
		rem = (IRDB_REMOTE *)(flash_data + pnt->table.remote_adr);

		for (i=0;i < pnt->table.remote_cnt;i++)	if (!memcmp (rem[i].name,remote,80)) break;
		
		if (i == pnt->table.remote_cnt) return (0);

		cnt = rem[i].command_cnt;
		adr = rem[i].command_adr;

		for (i=0;i < cnt;i++) {
			com = (IRDB_COMMAND *)(flash_data + adr);
			if (!memcmp (com->name,command,20)) return (adr);

			if (vers >= 1) adr += IRDB_COMMAND_LEN;
			else adr += IRDB_COMMAND_LEN_1;
		}
	}

	return (0);
}


word FindIRDBRemote (byte remote[],IRDB_HEADER *pnt,byte vers)
{
	word i,adr,len;
	IRDB_REMOTE *rem;
	IRDB_REMOTE_COMPACT *remc;
	IRDB_REMOTE_EX *remex;

	if (vers >= 3) {
		adr = pnt->table.remote_adr;

		len = (word)strlen (remote);

		for (i=0;i < pnt->table.remote_cnt;i++)	{
			remex = (IRDB_REMOTE_EX *)(flash_data + adr);
			if (len == remex->namelen && !memcmp (remex->name,remote,len)) break;
			adr += (IRDB_REMOTE_LEN_EX + remex->namelen) / 2;
		}

		if (i == pnt->table.remote_cnt) return (0);
		return (adr);
	}

	else if (vers >= 2) {
		adr = pnt->table.remote_adr;

		len = (word)strlen (remote);


		for (i=0;i < pnt->table.remote_cnt;i++)	{
			remc = (IRDB_REMOTE_COMPACT *)(flash_data + adr);
			if (len == remc->namelen && !memcmp (remc->name,remote,len)) break;
			adr += (IRDB_REMOTE_LEN_COMPACT + remc->namelen) / 2;
		}

		if (i == pnt->table.remote_cnt) return (0);

		return (adr);
	}
	
	else {
		rem = (IRDB_REMOTE *)(flash_data + pnt->table.remote_adr);

		for (i=0;i < pnt->table.remote_cnt;i++)	if (!memcmp (rem[i].name,remote,80)) break;
		
		if (i == pnt->table.remote_cnt) return (0);

		return ((word)((word *)(&rem[i]) - flash_data));
	}

	return (0);
}



int FileTransData (char nm[],byte dbtype,byte filemode)
{
	int i;
	char st[255];

	strcpy (st,nm);
	if (dbtype == 1) strcat (st,".tra");
	if (dbtype == 2) strcat (st,".irdb");
	if (filemode == FILE_MODE_SAVEAS) {
		fptrans = fopen (st,"r");
		if (fptrans) {
			fclose (fptrans);
			return (ERR_OVERWRITE);
		}
	}

	fptrans = fopen (st,"w");
	if (fptrans == NULL) return ERR_OPENTRANS;

	for (i=0;i <= trans_num;i++) {
		if (dbtype == 2) { 
			if (irdb[i].type == IRDB_TYPEHEADER || (irdb[i].type >= IRDB_TYPEHEADER_32K && irdb[i].type <= IRDB_TYPEHEADER_96K)) {
				int j;
				fprintf (fptrans,"[DEFAULTACTION]%s\n",irdbheader.default_action);
				fprintf (fptrans,"  [RESULT_ERR]%s\n",irdbheader.err_status);
				fprintf (fptrans,"  [RESULT_OK]%s\n",irdbheader.ok_status);
				fprintf (fptrans,"  [ACTIVE]%d\n",irdbheader.active);
				fprintf (fptrans,"  [H_PORT]%d\n",irdbheader.port);
				fprintf (fptrans,"  [H_IP]%03d.%03d.%03d.%03d\n",(irdbheader.ip >> 24) & 255,(irdbheader.ip >> 16) & 255,(irdbheader.ip >> 8) & 255,irdbheader.ip & 255);
				fprintf (fptrans,"  [RS232_0]%d\n",irdbheader.rs232_config[0]);
				fprintf (fptrans,"  [RS232_1]%d\n",irdbheader.rs232_config[1]);
				fprintf (fptrans,"  [RS232_2]%d\n",irdbheader.rs232_config[2]);
				fprintf (fptrans,"  [RS232_3]%d\n",irdbheader.rs232_config[3]);
				for (j=0;j <= 3;j++) {
					fprintf (fptrans,"  [INPUTMODE_%d]%d\n",j,irdbheader.inputmode[j]);
					fprintf (fptrans,"  [MIN_%d]%d\n",j,irdbheader.min[j]);
					fprintf (fptrans,"  [MAX_%d]%d\n",j,irdbheader.max[j]);
					fprintf (fptrans,"  [HYST_%d]%d\n",j,irdbheader.hyst[j]);
					fprintf (fptrans,"  [MINREMOTE_%d]%s\n",j,irdbheader.minremote[j]);
					fprintf (fptrans,"  [MINCOMMAND_%d]%s\n",j,irdbheader.mincommand[j]);
					fprintf (fptrans,"  [MAXREMOTE_%d]%s\n",j,irdbheader.maxremote[j]);
					fprintf (fptrans,"  [MAXCOMMAND_%d]%s\n",j,irdbheader.maxcommand[j]);
					fprintf (fptrans,"  [MINLED_%d]%d\n",j,irdbheader.minledsel[j]);
					fprintf (fptrans,"  [MINMASK_%d]%d\n",j,irdbheader.minmask[j]);
					fprintf (fptrans,"  [MAXLED_%d]%d\n",j,irdbheader.maxledsel[j]);
					fprintf (fptrans,"  [MAXMASK_%d]%d\n",j,irdbheader.maxmask[j]);
				}
				if (irdb[i].type == IRDB_TYPEHEADER_32K) fprintf (fptrans,"  [START_ADR]96\n");
				else if (irdb[i].type == IRDB_TYPEHEADER_64K) fprintf (fptrans,"  [START_ADR]64\n");
				else if (irdb[i].type == IRDB_TYPEHEADER_96K) fprintf (fptrans,"  [START_ADR]32\n");
				else fprintf (fptrans,"  [START_ADR]0\n");
				fprintf (fptrans,"[END]\n");
			}
			if (irdb[i].type == IRDB_REM) {
				fprintf (fptrans,"[DBREMOTE2]%s\n",irdb[i].remote);

				fprintf (fptrans,"  [IP]%03d.%03d.%03d.%03d\n",(irdb[i].ip >> 24) & 255,(irdb[i].ip >> 16) & 255,(irdb[i].ip >> 8) & 255,irdb[i].ip & 255);
				fprintf (fptrans,"  [PORT]%d\n",irdb[i].port);
				fprintf (fptrans,"  [FLAGS]%d\n",irdb[i].action_type);
				fprintf (fptrans,"  [RACTION]%s\n",irdb[i].action);
				fprintf (fptrans,"[END]\n");
			}
			if (irdb[i].type >= IRDB_TYPEACTION && irdb[i].type <= IRDB_TYPEACTION_7) {
				fprintf (fptrans,"[COMACTION_%d]\n  [ACREMOTE]%s\n",irdb[i].type - IRDB_TYPEACTION,irdb[i].remote);
				fprintf (fptrans,"  [ACCOMMAND]%s\n",irdb[i].command);
				fprintf (fptrans,"  [ACFLAGS]%d\n",irdb[i].action_type);
				if (irdb[i].type == IRDB_TYPEACTION)
					fprintf (fptrans,"  [RELAIS]%c%d\n",irdb[i].relais & 125,(irdb[i].relais & 128)>>6 | (irdb[i].relais & 2)>>1);
				fprintf (fptrans,"  [ACLEN]%d\n",irdb[i].action_len);
				fprintf (fptrans,"  [IP]%03d.%03d.%03d.%03d\n",(irdb[i].ip >> 24) & 255,(irdb[i].ip >> 16) & 255,(irdb[i].ip >> 8) & 255,irdb[i].ip & 255);
				fprintf (fptrans,"  [PORT]%d\n",irdb[i].port);
				fprintf (fptrans,"  [ACTION]");
				fwrite (irdb[i].action,1,irdb[i].action_len,fptrans);
				fprintf (fptrans,"\n[END]\n");
			}
		}

		if (dbtype == 1) {
			if (trans[i].type == F_CONFIG) {
				fprintf (fptrans,"[CONFIG]\n  [NAME]%s\n",trans[i].remote);

				fprintf (fptrans,"  [SETTINGS]0x%x\n",trans[i].setup);
				fprintf (fptrans,"  [SOURCEMASK]0x%x\n",trans[i].source_mask);
				fprintf (fptrans,"  [TARGETMASK]0x%x\n",trans[i].target_mask);
				fprintf (fptrans,"[END]\n");
			}
			if ((trans[i].type >= F_COMMAND && trans[i].type <= F_VOLUMEMACROD) || 
				(trans[i].type >= F_TOGGLE && trans[i].type < (F_TOGGLE + 6*4)) || trans[i].type >= F_MACRO) {
				if (trans[i].type == F_COMMAND) fprintf (fptrans,"[RCVCOMMAND]\n");
				if (trans[i].type == F_VOLUMEMACRO) fprintf (fptrans,"[VOLUMEMACRO]\n");
				if (trans[i].type == F_VOLUMEMACROD) fprintf (fptrans,"[VOLUMEMACROD]\n");
				if (trans[i].type >= F_MACRO) fprintf (fptrans,"[MACRO %d]\n",trans[i].type - F_MACRO);
				else if (trans[i].type >= F_TOGGLE) fprintf (fptrans,"[TOGGLE %d]\n",trans[i].type - F_TOGGLE);

				fprintf (fptrans,"  [REMOTE]%s\n",trans[i].remote);
				fprintf (fptrans,"  [COMMAND]%s\n",trans[i].command);
				fprintf (fptrans,"  [SETTINGS]0x%x\n",trans[i].setup);
				fprintf (fptrans,"  [SOURCEMASK]0x%x\n",trans[i].source_mask);
				fprintf (fptrans,"  [ACC_TIMEOUT]%d\n",trans[i].accelerator_timeout);
				fprintf (fptrans,"  [ACC_SETUP]0x%x\n",trans[i].accelerator_repeat);
				fprintf (fptrans,"  [REMOTE_NUM]%d\n",trans[i].remote_num);
				fprintf (fptrans,"  [GROUP_NUM]%d\n",trans[i].group_num);
				fprintf (fptrans,"  [SHIFT_NUM]%d\n",trans[i].multi_num);
				fprintf (fptrans,"  [INCLUDE_NAMES]%d\n",trans[i].include_names);
				fprintf (fptrans,"[END]\n");
			}
			if (trans[i].type == F_SEND) {
				fprintf (fptrans,"[SEND]\n");

				fprintf (fptrans,"  [REMOTE]%s\n",trans[i].remote);
				fprintf (fptrans,"  [COMMAND]%s\n",trans[i].command);
				fprintf (fptrans,"  [TARGETMASK]0x%x\n",trans[i].target_mask);
				fprintf (fptrans,"  [TIMEOUT]%d\n",trans[i].wait_timeout);
				fprintf (fptrans,"  [REMOTE_NUM]%d\n",trans[i].remote_num);
				fprintf (fptrans,"  [GROUP_NUM]%d\n",trans[i].group_num);
				fprintf (fptrans,"  [SHIFT_NUM]%d\n",trans[i].multi_num);
				fprintf (fptrans,"  [IO_INPUT]%d\n",trans[i].io_input);
				fprintf (fptrans,"  [IO_VALUE]%d\n",trans[i].io_value);
				fprintf (fptrans,"  [LED_SELECT]%d\n",trans[i].setup);
				fprintf (fptrans,"[END]\n");
			}
			if (trans[i].type == F_REMOTE) {
				fprintf (fptrans,"[REMOTE]\n  [NAME]%s\n",trans[i].remote);
				fprintf (fptrans,"  [SETTINGS]0x%x\n",trans[i].setup);
				fprintf (fptrans,"  [SOURCEMASK]0x%x\n",trans[i].source_mask);
				fprintf (fptrans,"  [TARGETMASK]0x%x\n",trans[i].target_mask);
				fprintf (fptrans,"  [REMOTE_NUM]%d\n",trans[i].remote_num);
				fprintf (fptrans,"[END]\n");
			}
			if (trans[i].type == F_ENABLEGROUP) {
				fprintf (fptrans,"[GROUP]\n");
				fprintf (fptrans,"  [REMOTE]%s\n",trans[i].remote);
				fprintf (fptrans,"  [COMMAND]%s\n",trans[i].command);
				fprintf (fptrans,"  [SETTINGS]0x%x\n",trans[i].setup);
				fprintf (fptrans,"  [SOURCEMASK]0x%x\n",trans[i].source_mask);
				fprintf (fptrans,"  [REMOTE_NUM]%d\n",trans[i].remote_num);
				fprintf (fptrans,"  [GROUP_NUM]%d\n",trans[i].group_num);
				fprintf (fptrans,"  [SHIFT_NUM]%d\n",trans[i].multi_num);
				fprintf (fptrans,"[END]\n");
			}
			if (trans[i].type == F_PREKEY) {
				fprintf (fptrans,"[SHIFT]\n");
				fprintf (fptrans,"  [REMOTE]%s\n",trans[i].remote);
				fprintf (fptrans,"  [COMMAND]%s\n",trans[i].command);
				fprintf (fptrans,"  [SETTINGS]0x%x\n",trans[i].setup);
				fprintf (fptrans,"  [SOURCEMASK]0x%x\n",trans[i].source_mask);
				fprintf (fptrans,"  [REMOTE_NUM]%d\n",trans[i].remote_num);
				fprintf (fptrans,"  [GROUP_NUM]%d\n",trans[i].group_num);
				fprintf (fptrans,"  [SHIFT_NUM]%d\n",trans[i].multi_num);
				fprintf (fptrans,"[END]\n");
			}
		}
	}

	fclose (fptrans);
	return (0);
}

int LoadIRDB (IRDBBUFFER *db,char nm[],word offset)
{
	int i;
	char ln[2048],*st;

	if (!offset) {
		IRDBHEADERBUFFER *dbh;
		dbh = (IRDBHEADERBUFFER *)db;

		strcpy (ln,nm);
		strcat (ln,".irdb");
		fptrans = fopen (ln,"r");
		if (fptrans == NULL) return ERR_OPENTRANS;
		trans_num = 0;
		memset (&irdbheader,0,sizeof(IRDBHEADER));
		while (st = DBReadString (ln,fptrans,0)) {
			ReadTraReceive (st);
		}
		fclose (fptrans);

		memset (dbh,0,sizeof (IRDBHEADERBUFFER));
		dbh->statustype = STATUS_IRDB;
		dbh->statuslen = sizeof (IRDBHEADERBUFFER);
		dbh->offset = offset;
		memcpy (&dbh->header,&irdbheader,sizeof (IRDBHEADER));
		dbh->count_buffer = 1;
		dbh->count_total = trans_num;
		dbh->count_remaining = (short)(trans_num - offset);

		return (0);
	}

	i = 0;
	memset (db,0,sizeof (IRDBBUFFER));
	db->statustype = STATUS_IRDB;
	db->statuslen = sizeof (IRDBBUFFER);
	db->offset = offset;

	while (i < 12 && offset < trans_num) {
		memcpy (&db->dbdata[i],&irdb[offset],sizeof (IRDBCOMMAND));
		i++;
		offset++;
	}

	db->count_buffer = i;
	db->count_total = trans_num;
	if (i == 12) db->count_remaining = (short)(trans_num - offset);
	else db->count_remaining = 0;
	return (0);
}

int LoadTranslation (TRANSLATEBUFFER *tb,char nm[],word offset)
{
	int i;
	char ln[2048],*st;

	if (!offset) {
		strcpy (ln,nm);
		strcat (ln,".tra");
		fptrans = fopen (ln,"r");
		if (fptrans == NULL) return ERR_OPENTRANS;
		trans_num = 0;
		while (st = DBReadString (ln,fptrans,0)) {
			ReadTraReceive (st);
		}
		fclose (fptrans);
	}

	i = 0;
	memset (tb,0,sizeof (TRANSLATEBUFFER));
	tb->statustype = STATUS_TRANSLATE;
	tb->statuslen = sizeof (TRANSLATEBUFFER);
	tb->offset = offset;

	while (i < 30 && offset < trans_num) {
		memcpy (&tb->trdata[i],&trans[offset],sizeof (TRANSLATECOMMAND));
		i++;
		offset++;
	}

	tb->count_buffer = i;
	tb->count_total = trans_num;
	if (i == 30) tb->count_remaining = (short)(trans_num - offset);
	else tb->count_remaining = 0;
	return (0);
}


void ReadTraReceive (char tp[])
{
	int pos;
	char ln[2048],*st;

	memset (&trans[trans_num],0,sizeof (TRANSLATECOMMAND));
	memset (&irdb[trans_num],0,sizeof (IRDBCOMMAND));

	irdb[trans_num].number = trans_num;
	trans[trans_num].number = trans_num;


	if (!strncmp (tp,"[DBREMOTE]",10)) {
		irdb[trans_num].type = IRDB_REM;
		memset (irdb[trans_num].remote,' ',80);
		memcpy (irdb[trans_num].remote,tp + 10,strlen (tp + 10));
		irdb[trans_num].action_type = 1;
		trans_num++;
		return;
	}

	if (!strcmp (tp,"[RCVCOMMAND]")) trans[trans_num].type = F_COMMAND;
	if (!strcmp (tp,"[VOLUMEMACRO]")) trans[trans_num].type = F_VOLUMEMACRO;
	if (!strcmp (tp,"[VOLUMEMACROD]")) trans[trans_num].type = F_VOLUMEMACROD;
	if (!strncmp (tp,"[MACRO ",7)) trans[trans_num].type = F_MACRO + (byte)strtoul (tp + 7,0,0);
	if (!strncmp (tp,"[TOGGLE ",8)) trans[trans_num].type = F_TOGGLE + (byte)strtoul (tp + 8,0,0);
	if (!strcmp (tp,"[SEND]")) trans[trans_num].type = F_SEND;
	if (!strcmp (tp,"[CONFIG]")) trans[trans_num].type = F_CONFIG;
	if (!strcmp (tp,"[REMOTE]")) trans[trans_num].type = F_REMOTE;
	if (!strcmp (tp,"[GROUP]")) trans[trans_num].type = F_ENABLEGROUP;
	if (!strcmp (tp,"[SHIFT]")) trans[trans_num].type = F_PREKEY;
	if (!strcmp (tp,"[COMACTION]")) irdb[trans_num].type = IRDB_TYPEACTION;
	if (!strcmp (tp,"[COMACTION_0]")) irdb[trans_num].type = IRDB_TYPEACTION;
	if (!strcmp (tp,"[COMACTION_1]")) irdb[trans_num].type = IRDB_TYPEACTION_1;
	if (!strcmp (tp,"[COMACTION_2]")) irdb[trans_num].type = IRDB_TYPEACTION_2;
	if (!strcmp (tp,"[COMACTION_3]")) irdb[trans_num].type = IRDB_TYPEACTION_3;
	if (!strcmp (tp,"[COMACTION_4]")) irdb[trans_num].type = IRDB_TYPEACTION_4;
	if (!strcmp (tp,"[COMACTION_5]")) irdb[trans_num].type = IRDB_TYPEACTION_5;
	if (!strcmp (tp,"[COMACTION_6]")) irdb[trans_num].type = IRDB_TYPEACTION_6;
	if (!strcmp (tp,"[COMACTION_7]")) irdb[trans_num].type = IRDB_TYPEACTION_7;
	if (!strncmp (tp,"[DBREMOTE2]",11)) {
		irdb[trans_num].type = IRDB_REM;
		memset (irdb[trans_num].remote,' ',80);
		memcpy (irdb[trans_num].remote,tp + 11,strlen (tp + 11));
	}
	if (!strncmp (tp,"[DEFAULTACTION]",15)) {
		irdb[trans_num].type = IRDB_TYPEHEADER;
		irdbheader.type = IRDB_TYPEHEADER;
		memset (irdbheader.default_action ,' ',100);
		tp[115] = 0;
		memcpy (irdbheader.default_action,tp + 15,strlen (tp + 15));
	}
	pos = ftell (fptrans);
	while ((st = DBReadString (ln,fptrans,0)) && strcmp (st,"[END]")) {
		if (!strncmp (st,"[NAME]",6)) {
			memset (trans[trans_num].remote,' ',80);
			memcpy (trans[trans_num].remote,st + 6,strlen (st + 6));
		}
		if (!strncmp (st,"[REMOTE]",8)) {
			memset (trans[trans_num].remote,' ',80);
			memcpy (trans[trans_num].remote,st + 8,strlen (st + 8));
		}
		if (!strncmp (st,"[PORT]",6)) irdb[trans_num].port = (word)strtoul (st + 6,0,0);
		if (!strncmp (st,"[FLAGS]",7)) irdb[trans_num].action_type = (byte)strtoul (st + 7,0,0);
		if (!strncmp (st,"[IP]",4)) {
			st[7] = 0;
			irdb[trans_num].ip = (byte)strtoul (st + 4,0,10) << 24;
			st[11] = 0;
			irdb[trans_num].ip |= (byte)strtoul (st + 8,0,10) << 16;
			st[15] = 0;
			irdb[trans_num].ip |= (byte)strtoul (st + 12,0,10) << 8;
			st[19] = 0;
			irdb[trans_num].ip |= (byte)strtoul (st + 16,0,10);
		}
		if (!strncmp (st,"[RACTION]",9)) {
			memset (irdb[trans_num].action ,' ',256);
			st[59] = 0;
			memcpy (irdb[trans_num].action,st + 9,strlen (st + 9));
		}

		if (!strncmp (st,"[COMMAND]",9)) {
			memset (trans[trans_num].command ,' ',20);
			memcpy (trans[trans_num].command ,st + 9,strlen (st + 9));
		}
		if (!strncmp (st,"[RESULT_ERR]",12)) {
			memset (irdbheader.err_status,' ',sizeof (irdbheader.err_status));
			memcpy (irdbheader.err_status,st + 12,strlen (st + 12));
		}
		if (!strncmp (st,"[RESULT_OK]",11)) {
			memset (irdbheader.ok_status,' ',sizeof (irdbheader.ok_status));
			memcpy (irdbheader.ok_status,st + 11,strlen (st + 11));
		}
		if (!strncmp (st,"[ACREMOTE]",10)) {
			memset (irdb[trans_num].remote,' ',80);
			memcpy (irdb[trans_num].remote,st + 10,strlen (st + 10));
		}
		if (!strncmp (st,"[ACCOMMAND]",11)) {
			memset (irdb[trans_num].command ,' ',20);
			memcpy (irdb[trans_num].command ,st + 11,strlen (st + 11));
		}
		if (!strncmp (st,"[ACTION]",8)) {
			fseek (fptrans,pos+10,SEEK_SET);
			fread (irdb[trans_num].action,1,irdb[trans_num].action_len,fptrans);
		}
		if (!strncmp (st,"[RELAIS]",8)) irdb[trans_num].relais = st[8] + ((st[9] & 2) << 6) + ((st[9] & 1) << 1);
		if (!strncmp (st,"[ACTYPE]",8)) {
			if (st[8] == 'A') irdb[trans_num].action_type = 130;
			if (st[8] == 'B') irdb[trans_num].action_type = 131;
		}
		if (!strncmp (st,"[ACFLAGS]",9)) irdb[trans_num].action_type = atoi (st+9);
		if (!strncmp (st,"[ACLEN]",7)) irdb[trans_num].action_len = (byte)strtoul (st + 7,0,0);
				
		if (!strncmp (st,"[RS232_0]",9)) irdbheader.rs232_config[0] = (byte)strtoul (st + 9,0,0);
		if (!strncmp (st,"[RS232_1]",9)) irdbheader.rs232_config[1] = (byte)strtoul (st + 9,0,0);
		if (!strncmp (st,"[RS232_2]",9)) irdbheader.rs232_config[2] = (byte)strtoul (st + 9,0,0);
		if (!strncmp (st,"[RS232_3]",9)) irdbheader.rs232_config[3] = (byte)strtoul (st + 9,0,0);

		if (!strncmp (st,"[START_ADR]96",13)) irdbheader.type = IRDB_TYPEHEADER_32K;
		if (!strncmp (st,"[START_ADR]64",13)) irdbheader.type = IRDB_TYPEHEADER_64K;
		if (!strncmp (st,"[START_ADR]32",13)) irdbheader.type = IRDB_TYPEHEADER_96K;

		if (!strncmp (st,"[INPUTMODE_0]",13)) irdbheader.inputmode[0] = (byte)strtoul (st + 13,0,0);
		if (!strncmp (st,"[INPUTMODE_1]",13)) irdbheader.inputmode[1] = (byte)strtoul (st + 13,0,0);
		if (!strncmp (st,"[INPUTMODE_2]",13)) irdbheader.inputmode[2] = (byte)strtoul (st + 13,0,0);
		if (!strncmp (st,"[INPUTMODE_3]",13)) irdbheader.inputmode[3] = (byte)strtoul (st + 13,0,0);
		if (!strncmp (st,"[MIN_0]",7)) irdbheader.min[0] = (byte)strtoul (st + 7,0,0);
		if (!strncmp (st,"[MIN_1]",7)) irdbheader.min[1] = (byte)strtoul (st + 7,0,0);
		if (!strncmp (st,"[MIN_2]",7)) irdbheader.min[2] = (byte)strtoul (st + 7,0,0);
		if (!strncmp (st,"[MIN_3]",7)) irdbheader.min[3] = (byte)strtoul (st + 7,0,0);
		if (!strncmp (st,"[MAX_0]",7)) irdbheader.max[0] = (byte)strtoul (st + 7,0,0);
		if (!strncmp (st,"[MAX_1]",7)) irdbheader.max[1] = (byte)strtoul (st + 7,0,0);
		if (!strncmp (st,"[MAX_2]",7)) irdbheader.max[2] = (byte)strtoul (st + 7,0,0);
		if (!strncmp (st,"[MAX_3]",7)) irdbheader.max[3] = (byte)strtoul (st + 7,0,0);
		if (!strncmp (st,"[HYST_0]",8)) irdbheader.hyst[0] = (byte)strtoul (st + 8,0,0);
		if (!strncmp (st,"[HYST_1]",8)) irdbheader.hyst[1] = (byte)strtoul (st + 8,0,0);
		if (!strncmp (st,"[HYST_2]",8)) irdbheader.hyst[2] = (byte)strtoul (st + 8,0,0);
		if (!strncmp (st,"[HYST_3]",8)) irdbheader.hyst[3] = (byte)strtoul (st + 8,0,0);
		if (!strncmp (st,"[MINLED_0]",10)) irdbheader.minledsel[0] = (byte)strtoul (st + 10,0,0);
		if (!strncmp (st,"[MINLED_1]",10)) irdbheader.minledsel[1] = (byte)strtoul (st + 10,0,0);
		if (!strncmp (st,"[MINLED_2]",10)) irdbheader.minledsel[2] = (byte)strtoul (st + 10,0,0);
		if (!strncmp (st,"[MINLED_3]",10)) irdbheader.minledsel[3] = (byte)strtoul (st + 10,0,0);
		if (!strncmp (st,"[MAXLED_0]",10)) irdbheader.maxledsel[0] = (byte)strtoul (st + 10,0,0);
		if (!strncmp (st,"[MAXLED_1]",10)) irdbheader.maxledsel[1] = (byte)strtoul (st + 10,0,0);
		if (!strncmp (st,"[MAXLED_2]",10)) irdbheader.maxledsel[2] = (byte)strtoul (st + 10,0,0);
		if (!strncmp (st,"[MAXLED_3]",10)) irdbheader.maxledsel[3] = (byte)strtoul (st + 10,0,0);
		if (!strncmp (st,"[MINMASK_0]",11)) irdbheader.minmask[0] = strtoul (st + 11,0,0);
		if (!strncmp (st,"[MINMASK_1]",11)) irdbheader.minmask[1] = strtoul (st + 11,0,0);
		if (!strncmp (st,"[MINMASK_2]",11)) irdbheader.minmask[2] = strtoul (st + 11,0,0);
		if (!strncmp (st,"[MINMASK_3]",11)) irdbheader.minmask[3] = strtoul (st + 11,0,0);
		if (!strncmp (st,"[MAXMASK_0]",11)) irdbheader.maxmask[0] = strtoul (st + 11,0,0);
		if (!strncmp (st,"[MAXMASK_1]",11)) irdbheader.maxmask[1] = strtoul (st + 11,0,0);
		if (!strncmp (st,"[MAXMASK_2]",11)) irdbheader.maxmask[2] = strtoul (st + 11,0,0);
		if (!strncmp (st,"[MAXMASK_3]",11)) irdbheader.maxmask[3] = strtoul (st + 11,0,0);
		if (!strncmp (st,"[MINREMOTE_",11)) {
			st[13 + 80] = 0;
			memset (irdbheader.minremote[st[11] - '0'],' ',80);
			memcpy (irdbheader.minremote[st[11] - '0'],st + 13,strlen (st + 13));
		}
		if (!strncmp (st,"[MAXREMOTE_",11)) {
			st[13 + 80] = 0;
			memset (irdbheader.maxremote[st[11] - '0'],' ',80);
			memcpy (irdbheader.maxremote[st[11] - '0'],st + 13,strlen (st + 13));
		}
		if (!strncmp (st,"[MINCOMMAND_",12)) {
			st[14 + 50] = 0;
			memset (irdbheader.mincommand[st[12] - '0'],' ',50);
			memcpy (irdbheader.mincommand[st[12] - '0'],st + 14,strlen (st + 14));
		}
		if (!strncmp (st,"[MAXCOMMAND_",11)) {
			st[14 + 50] = 0;
			memset (irdbheader.maxcommand[st[12] - '0'],' ',50);
			memcpy (irdbheader.maxcommand[st[12] - '0'],st + 14,strlen (st + 14));
		}
		if (!strncmp (st,"[ACTIVE]",8)) irdbheader.active = (byte)strtoul (st + 8,0,0);
		if (!strncmp (st,"[H_IP]",6)) {
			st[9] = 0;
			irdbheader.ip = (byte)strtoul (st + 6,0,10) << 24;
			st[13] = 0;
			irdbheader.ip |= (byte)strtoul (st + 10,0,10) << 16;
			st[17] = 0;
			irdbheader.ip |= (byte)strtoul (st + 14,0,10) << 8;
			st[21] = 0;
			irdbheader.ip |= (byte)strtoul (st + 18,0,10);
		}
		if (!strncmp (st,"[H_PORT]",8)) irdbheader.port = (word)strtoul (st + 8,0,0);

		
		if (!strncmp (st,"[LED_SELECT]",12)) trans[trans_num].setup = (byte)strtoul (st + 12,0,0);
		if (!strncmp (st,"[SETTINGS]",10)) trans[trans_num].setup = (byte)strtoul (st + 10,0,0);
		if (!strncmp (st,"[SOURCEMASK]",12)) trans[trans_num].source_mask = strtoul (st + 12,0,0);
		if (!strncmp (st,"[TARGETMASK]",12)) trans[trans_num].target_mask = strtoul (st + 12,0,0);
		if (!strncmp (st,"[REMOTE_NUM]",12)) trans[trans_num].remote_num = (byte)strtoul (st + 12,0,0);
		if (!strncmp (st,"[GROUP_NUM]",11)) trans[trans_num].group_num = (byte)strtoul (st + 11,0,0);
		if (!strncmp (st,"[SHIFT_NUM]",11)) trans[trans_num].multi_num = (byte)strtoul (st + 11,0,0);
		if (!strncmp (st,"[ACC_TIMEOUT]",13)) trans[trans_num].accelerator_timeout = (byte)strtoul (st + 13,0,0);
		if (!strncmp (st,"[ACC_SETUP]",11)) trans[trans_num].accelerator_repeat = (byte)strtoul (st + 11,0,0);
		if (!strncmp (st,"[TIMEOUT]",9)) trans[trans_num].wait_timeout = (word)strtoul (st + 9,0,0);
		if (!strncmp (st,"[INCLUDE_NAMES]",15)) trans[trans_num].include_names = (byte)strtoul (st + 15,0,0);
		if (!strncmp (st,"[IO_INPUT]",10)) trans[trans_num].io_input = (byte)strtoul (st + 10,0,0);
		if (!strncmp (st,"[IO_VALUE]",10)) trans[trans_num].io_value = (byte)strtoul (st + 10,0,0);
		pos = ftell (fptrans);
	}
	trans_num++;
}

int SetFlashdataEx (byte bus,int iradr,STATUSBUFFER *stat)
{
	char st[255];
	byte cal,toggle,led;
	byte *cmdpnt;
	char remname[100];
	int rcv_cnt,snd_cnt,grp_cnt;
	int res,adr,i,j,flashwordsize,nrem;
	word val;
	IRDATA_LARGE ird;
	FLASH_CONTENT_OLD *pnt;
	int cmd_count = 0;
	int cmd_num = 0;
	IRDB_FLASHINFO *finfo;


	finfo = (IRDB_FLASHINFO *)stat;

	led = (strcmp (IRDevices[bus].version+1,"6.08.05") >= 0);
	cal = (IRDevices[bus].fw_capabilities & FN_CALIBRATE) != 0;
	toggle = (IRDevices[bus].io.toggle_support) != 0;

	for (i=0;i <= trans_num;i++) if ((trans[i].type >= F_COMMAND && trans[i].type <= F_VOLUMEMACROD) || 
									 (trans[i].type >= F_TOGGLE && trans[i].type < (F_TOGGLE + 6*4)) || 
									  trans[i].type >= F_MACRO || trans[i].type == F_ENABLEGROUP || trans[i].type == F_PREKEY) {
		if (res = DBFindRemoteCommandEx (trans[i].remote,trans[i].command,(IRDATA *)&ird,cal,toggle)) {
			if (res == ERR_REMOTENOTFOUND) strcpy (err_remote,trans[i].remote);
			if (res == ERR_COMMANDNOTFOUND) strcpy (err_command,trans[i].command);
			return (res);
		}
		else cmd_count++;
	}

	memset (flash_data,0,sizeof (flash_data));
	pnt = (FLASH_CONTENT_OLD *)flash_data;
	pnt->dir_cnt = cmd_count;
	pnt->magic = F_MAGIC;
	pnt->checksum = 0;
	grp_cnt = rcv_cnt = snd_cnt = 0;

	pnt->rcv_len = 0;
	for (i=0;i < REMOTE_CNT;i++) pnt->group_flags[i] = 1;
	pnt->trans_setup[0] = trans[0].setup;
	pnt->source_mask[0] = (word)trans[0].source_mask;
	pnt->target_mask[0] = (word)trans[0].target_mask;
	pnt->end_pnt = pnt->data_pnt = CONTENT_LEN;
	cmd_num = 0;

	for (i=0;i <= trans_num;i++) if ((trans[i].type >= F_COMMAND && trans[i].type <= F_VOLUMEMACROD) || 
									 (trans[i].type >= F_TOGGLE && trans[i].type < (F_TOGGLE + 6*4)) || 
									  trans[i].type >= F_MACRO || trans[i].type == F_ENABLEGROUP || trans[i].type == F_PREKEY) {
		memset (remname,0,sizeof(remname));
		strcpy (remname,trans[i].remote);
		nrem = DBFindRemote (remname);
		if (nrem >= 0) if (rem_pnt[nrem].rcv_len && (!pnt->rcv_len || rem_pnt[nrem].rcv_len < pnt->rcv_len)) pnt->rcv_len = rem_pnt[nrem].rcv_len;
	}


	for (i=0;i <= trans_num;i++) if ((trans[i].type >= F_COMMAND && trans[i].type <= F_VOLUMEMACROD) || 
									 (trans[i].type >= F_TOGGLE && trans[i].type < (F_TOGGLE + 6*4)) || 
									  trans[i].type >= F_MACRO || trans[i].type == F_ENABLEGROUP || trans[i].type == F_PREKEY) {
	    rcv_cnt++;
		DBFindRemoteCommandEx (trans[i].remote,trans[i].command,(IRDATA *)&ird,cal,toggle);

		if (ird.mode == TIMECOUNT_18) cmdpnt = ((IRDATA_18 *)&ird)->data;
		else if ((ird.mode & SPECIAL_IR_MODE) == PULSE_200) cmdpnt = ((IRDATA_SINGLE *)&ird)->data;
		else cmdpnt = ird.data;
		
		cmdpnt[ird.ir_length] = 0;

		if (*cmdpnt < '0') cmdpnt += *cmdpnt;
		if (cmdpnt[0] == 'X' && cmdpnt[1] == 'l') cmdpnt += 2;

		set_entry (cmdpnt,cmd_num,pnt,trans[i].type,trans[i].remote_num,trans[i].group_num,trans[i].multi_num,trans[i].setup,
					(word)trans[i].source_mask,trans[i].accelerator_timeout,trans[i].accelerator_repeat,remote_statusex[bus].stat[iradr].version+1,trans[i].include_names,trans[i].remote,trans[i].command,pnt->rcv_len);
		cmd_num++;
	}

	cmd_num = 0;
	for (i=0;i <= trans_num;i++) {
		if (trans[i].type == F_REMOTE) {
			pnt->trans_setup[trans[i].remote_num] = trans[i].setup;
			pnt->target_mask[trans[i].remote_num] = (word)trans[i].target_mask;
			pnt->source_mask[trans[i].remote_num] = (word)trans[i].source_mask;
		}
		if (trans[i].type == F_ENABLEGROUP) cmd_num++;

		if (trans[i].type == F_PREKEY) cmd_num++;

		if (trans[i].type >= F_COMMAND && trans[i].type <= F_VOLUMEMACROD) {
			if (trans[i+1].type == F_SEND) {
				snd_cnt++;
				DBFindRemoteCommandEx (trans[i+1].remote,trans[i+1].command,(IRDATA *)&ird,cal,toggle);

				if (ird.command == HOST_SEND_RS232) {
					if (led) ird.command = TRANSLATE_SEND_RS232;
				}
				else {
					if (led) {
						ird.command = TRANSLATE_SEND;
						ird.command |= trans[i+1].setup;
					}
					ird.target_mask = (word)trans[i+1].target_mask;
				}
				set_commanddata (cmd_num,(IRDATA *)&ird,bus,i+1);
				if (trans[i].type == F_VOLUMEMACROD) {
					snd_cnt++;
					if (trans[i+2].type == F_SEND) {
						if (res = DBFindRemoteCommandEx (trans[i+2].remote,trans[i+2].command,(IRDATA *)&ird,cal,toggle)) {
							if (res == ERR_REMOTENOTFOUND) strcpy (err_remote,trans[i+2].remote);
							if (res == ERR_COMMANDNOTFOUND) strcpy (err_command,trans[i+2].command);
							return (res);
						}
						if (ird.command == HOST_SEND_RS232) {
							if (led) ird.command = TRANSLATE_SEND_RS232;
						}
						else {
							if (led) {
								ird.command = TRANSLATE_SEND;
								ird.command |= trans[i+2].setup;
							}
							ird.target_mask = (word)trans[i+2].target_mask;
						}
						set_commanddata (-1,(IRDATA *)&ird,bus,i+1);
					}
				}
			}
			cmd_num++;
		}
		if (trans[i].type >= F_MACRO) {
			for (j=0;j < trans[i].type - F_MACRO && trans[i+j+1].type == F_SEND;j++) {
				snd_cnt++;
				if (res = DBFindRemoteCommandEx (trans[i+j+1].remote,trans[i+j+1].command,(IRDATA *)&ird,cal,toggle)) {
					if (res == ERR_REMOTENOTFOUND) strcpy (err_remote,trans[i+j+1].remote);
					if (res == ERR_COMMANDNOTFOUND) strcpy (err_command,trans[i+j+1].command);
					return (res);
				}
				if (ird.command == HOST_SEND_RS232) {
					if (led) ird.command = TRANSLATE_SEND_RS232;
				}
				else {
					if (led) {
						ird.command = TRANSLATE_SEND;
						ird.command |= trans[i+j+1].setup;
					}
					ird.target_mask = (word)trans[i+j+1].target_mask;
				}
				ird.address = trans[i+j+1].wait_timeout / 16;
				if (j) set_commanddata (-1,(IRDATA *)&ird,bus,i+j+1);
				else set_commanddata (cmd_num,(IRDATA *)&ird,bus,i+j+1);
			}
			cmd_num++;
		}
		if (trans[i].type >= F_TOGGLE && trans[i].type < (F_TOGGLE + 6*4)) {
			for (j=0;j < ((trans[i].type - F_TOGGLE) % 6) && trans[i+j+1].type == F_SEND;j++) {
				if (res = DBFindRemoteCommandEx (trans[i+j+1].remote,trans[i+j+1].command,(IRDATA *)&ird,cal,toggle)) {
					if (res == ERR_REMOTENOTFOUND) strcpy (err_remote,trans[i+j+1].remote);
					if (res == ERR_COMMANDNOTFOUND) strcpy (err_command,trans[i+j+1].command);
					return (res);
				}
				if (ird.command == HOST_SEND_RS232) {
					if (led) ird.command = TRANSLATE_SEND_RS232;
				}
				else {
					if (led) {
						ird.command = TRANSLATE_SEND;
						ird.command |= trans[i+j+1].setup;
					}
					ird.target_mask = (word)trans[i+j+1].target_mask;
				}
				ird.address = trans[i+j+1].wait_timeout / 16;
				if (j) set_commanddata (-1,(IRDATA *)&ird,bus,i+j+1);
				else set_commanddata (cmd_num,(IRDATA *)&ird,bus,i+j+1);
			}
			cmd_num++;
		}
	}

	val = 0;
	adr = CONTENT_LEN;
	while (adr < pnt->end_pnt) val += flash_data[adr++];
	pnt->checksum = val;


	sprintf (st,"Flash Translation Size: %d Words (%d KBytes).\n",pnt->end_pnt,(pnt->end_pnt) / 512); 
	log_print (st,LOG_DEBUG);

	finfo->statuslen = sizeof (IRDB_FLASHINFO);
	finfo->statustype = STATUS_IRDBFLASH;
	sprintf (finfo->memsize,"%d Words (%d KBytes)",pnt->end_pnt,(pnt->end_pnt)/512);
	sprintf (finfo->remotes,"%d",rcv_cnt);
	sprintf (finfo->commands,"%d",snd_cnt);
	if (IRDevices[bus].fw_capabilities & FN_FLASH128) {
		sprintf (finfo->flashsize,"128K");
	}
	else {
		sprintf (finfo->flashsize,"64K");
	}

	flashwordsize = 128;

	adr = 0;
	while (adr < pnt->end_pnt) {
		if ((adr + flashwordsize /2) < pnt->end_pnt) TransferFlashdataEx (bus,flash_data + adr,adr,flashwordsize,0,iradr);
		else TransferFlashdataEx (bus,flash_data + adr,adr,flashwordsize,1,iradr);
		adr += flashwordsize / 2;
	}
	return (0);
}



void set_commanddata (int pos,IRDATA *irpnt,int bus,int tra)
{
	IRDATA ird;
	HASH_ENTRY *hash_table;
	FLASH_ENTRY_ORG *fentry;
	FLASH_CONTENT_OLD *content;

	content = (FLASH_CONTENT_OLD *)flash_data;
	hash_table = (HASH_ENTRY *)(flash_data + CONTENT_LEN);
	fentry = (FLASH_ENTRY_ORG *)(flash_data + hash_table[pos].adr);

	if (irpnt->command == HOST_SEND_RS232 || irpnt->command == TRANSLATE_SEND_RS232) {
	}
	else {
		if (irpnt->mode == TIMECOUNT_18) irpnt -> len = (sizeof (IRDATA_18) - CODE_LEN_18) + ((IRDATA_18 *)irpnt) -> ir_length;
		
		else if ((irpnt->mode & SPECIAL_IR_MODE) == PULSE_200) irpnt -> len = (sizeof (IRDATA_SINGLE) - CODE_LEN_SINGLE) + irpnt -> ir_length;

		else if (irpnt -> mode & RAW_DATA) irpnt -> len = (sizeof (IRDATA) - (CODE_LEN + (RAW_EXTRA))) + irpnt -> ir_length;

		else {
			irpnt -> len = (sizeof (IRDATA) - CODE_LEN) + irpnt -> ir_length;
			if (irpnt->mode & RC6_DATA) irpnt->len++;
		}
		
		TransferToTimelen18 (irpnt,&ird,bus);
		memcpy (irpnt,&ird,sizeof (IRDATA));

//		irpnt -> command = HOST_SEND;

		if (time_len != TIME_LEN) {
			if (irpnt->mode & RAW_DATA) {
				if (irpnt->ir_length > OLD_LENRAW) return;
			}
			else {
				if (irpnt->time_cnt > 6) return;
				ConvertToIRTRANS3 (irpnt);
			}
		}
	}

	if ((IRDevices[bus].fw_capabilities2 & FN2_STATEIO_SBUS) || 
		(IRDevices[bus].fw_capabilities2 & FN2_STATEIO_ANALOG) ||
		(IRDevices[bus].fw_capabilities2 & FN2_STATEIO_IRIN)) {

		if (trans[tra].io_input & 128) irpnt->checksumme = ((trans[tra].io_input & 15) + 1) * 16 | (trans[tra].io_value & 1);
	}

	if (pos >= 0) fentry->flash_adr = content->end_pnt;

	memcpy ((void *)(flash_data + content->end_pnt),(void *)irpnt,irpnt->len);
	content->end_pnt += (irpnt->len + 1) / 2;
}


void set_entry (char entry[],int pos,FLASH_CONTENT_OLD *content,byte type,byte remote,byte group,byte shift,byte setup,word sourcemask,word acc_timeout,word acc_repeat,char *version,byte nmflag,char *remname,char *comname,byte rcv_len)
{
	HASH_ENTRY *hash_table;
	FLASH_ENTRY_ORG *fentry;
	
	hash_table = (HASH_ENTRY *)(flash_data + CONTENT_LEN);

	if (!pos) content->data_pnt = CONTENT_LEN + content->dir_cnt * (sizeof (HASH_ENTRY) / 2);

	if (rcv_len) entry[rcv_len] = 0;

	hash_table[pos].hashcode = get_hashcode (entry,(byte)strlen (entry));
	hash_table[pos].adr = content->data_pnt;

	fentry = (FLASH_ENTRY_ORG *)(flash_data + content->data_pnt);
	fentry->type = type;
	fentry->flash_adr = 0;
	if (group) fentry->group = group - 1;
	else fentry->group = 0;
	fentry->remote = remote;
	fentry->source_mask = sourcemask;
	fentry->accelerator_timeout = (byte)acc_timeout;
	fentry->accelerator_repeat = (byte)acc_repeat;
	fentry->trans_setup = setup;
	fentry->len = (byte)strlen (entry);

	if (strcmp (version,"5.04.05") >= 0) {
		byte stlen = 0;
		char nmstr[256];
		if (nmflag) {
			sprintf (nmstr,"%s,%s\r\n",remname,comname);
			stlen = (byte)strlen (nmstr);
		}
		fentry->cdata[0] = shift;
		fentry->cdata[1] = stlen;
		strcpy (fentry->cdata+2,entry);
		strcpy (fentry->cdata+2+fentry->len,nmstr);

		content->end_pnt = content->data_pnt = (13 + fentry->len + stlen + 1) / 2 + hash_table[pos].adr;
	}
	else if (strcmp (version,"4.04.27") >= 0) {
		fentry->cdata[0] = shift;
		strcpy (fentry->cdata+1,entry);
		content->end_pnt = content->data_pnt = (12 + fentry->len + 1) / 2 + hash_table[pos].adr;
	}
	else {
		strcpy (fentry->cdata,entry);
		content->end_pnt = content->data_pnt = (11 + fentry->len + 1) / 2 + hash_table[pos].adr;
	}
	
}



/*
void read_flashdata (byte *pnt,word adr,word cnt)
{
	set_flashadr (adr);
	read_nextflashdata (pnt,cnt);
}

int last_adr;

void set_flashadr (word adr)
{
	last_adr = adr;	
}

void read_nextflashdata (byte *pnt,word cnt)
{
	memcpy (pnt,flash_data + last_adr,cnt);
	last_adr += cnt / 2;
}
*/


word get_hashcode (byte data[],byte len)
{
	byte i;
	word h = 0;

	for (i=0;i < len;i++) h += (data[i] & 7) << ((i * 2) & 15);

	return (h);
}


/*crc-16 standard root*/
#define POLYNOMIAL 0x8005


#pragma pack(1)

typedef union {
   uint32_t Whole;
   struct 
   {
      byte Data;
      word Remainder;
      byte Head;
   } Part;
} CRC_BUFFER;


void PutCRC(byte b,CRC_BUFFER *crcb)
{    
   byte i;
   crcb->Part.Data = b;
   for (i=0; i<8; i++)
   {
       crcb->Whole = crcb->Whole << 1;
       if (crcb->Part.Head & 0x01)
          crcb->Part.Remainder ^= POLYNOMIAL;
  }
}


word CRC (byte *Data, int Length,word init)
{
   CRC_BUFFER CRC_buffer;

   CRC_buffer.Part.Remainder = init;
   while (Length--  >  0)
      PutCRC(*Data++,&CRC_buffer);
   PutCRC(0,&CRC_buffer);
   PutCRC(0,&CRC_buffer);
   return CRC_buffer.Part.Remainder;
} 
