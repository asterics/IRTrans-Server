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
#include <winsock2.h>
#include <windows.h>
#include <io.h>
#include <direct.h>

#define MSG_NOSIGNAL	0
#endif

#ifdef WINCE
#include <winsock2.h>
#include <windows.h>

#define MSG_NOSIGNAL	0
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

#include "errcode.h"
#include "remote.h"
#include "network.h"
#include "dbstruct.h"
#include "fileio.h"
#include "global.h"
#include "lowlevel.h"
#include "ccf.h"
#include "flash.h"

#define dbpath "."

#ifdef WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

extern byte rcmmflag;

#ifdef LINUX


int ReadIRTransDirectory (char filetype[],REMOTEBUFFER *buf,int start,byte statustype)
{

    int fd,i,len,pos,res,fl,dlen;
    long off;
    char st[2048],msg[256];
    struct dirent *di;

	int cnt,cnt_total,nlen;
	memset (buf,0,sizeof (REMOTEBUFFER));
	buf->statustype = statustype;
	buf->statuslen = sizeof (REMOTEBUFFER);
	buf->offset = (short)start;

	if (statustype == STATUS_IRDBFILE) dlen = 5;
	else dlen = 4;

	fd = open (".",0);

	cnt = cnt_total = 0;

    do {
		len = getdirentries (fd,st,2048,&off);

		pos = 0;
		while (pos < len) {
			di = (struct dirent *)&st[pos];
			fl = strlen (di -> d_name) - dlen;
			if (fl >= 1 && !strcmp (di->d_name + fl,filetype)) {
				if (cnt_total >= start && cnt < 40) {
					nlen = strlen (di -> d_name) - dlen;
					if (nlen > 80) nlen = 80;
					memset (buf->remotes[cnt].name,' ',80);
					memcpy (buf->remotes[cnt].name,di -> d_name,nlen);
					cnt++;
				}
				cnt_total++;
			}
			pos += di -> d_reclen;
		}
	} while (len);

	buf->count_buffer = cnt;
	buf->count_total = cnt_total;
	if (cnt == 40) buf->count_remaining = cnt_total-cnt;
	else buf->count_remaining = 0;

    close (fd);
    return (0);
}

#endif


#ifdef WIN32

int ReadIRTransDirectory (char filetype[],REMOTEBUFFER *buf,int start,byte statustype)
{
    struct _finddata_t c_file;
#ifdef _M_X64
    intptr_t hFile;
#else
    int hFile;
#endif

	int cnt,cnt_total,len,dlen;
	memset (buf,0,sizeof (REMOTEBUFFER));
	buf->statustype = statustype;
	buf->statuslen = sizeof (REMOTEBUFFER);
	buf->offset = (short)start;

	if (statustype == STATUS_IRDBFILE) dlen = 5;
	else dlen = 4;

	cnt_total = cnt = 0;
	if((hFile = _findfirst( filetype, &c_file )) != -1L) {
		do {
			if (cnt_total >= start && cnt < 40) {
				len = (int)strlen (c_file.name) - dlen;
				if (len > 80) len = 80;
				memset (buf->remotes[cnt].name,' ',80);
				memcpy (buf->remotes[cnt].name,c_file.name,len);
				cnt++;
			}
			cnt_total++;
		} while( _findnext( hFile, &c_file ) == 0);
		_findclose( hFile );
	}
	buf->count_buffer = cnt;
	buf->count_total = cnt_total;
	if (cnt == 40) buf->count_remaining = cnt_total-cnt;
	else buf->count_remaining = 0;

	return (0);
}

#endif

void GetNumericCode (char command[],char numeric[],char rem[],char com[])
{
	int b,q,z,val;
	
	if (*command < '0') {
		q = 0;
		while (rem[q] && q < 4) {
			sprintf (numeric + q * 2,"%02x",rem[q]);
			q++;
		}
		b = 0;
		while (com[b] && b < 4) {
			sprintf (numeric + q * 2,"%02x",com[b]);
			b++;
			q++;
		}
		q *= 2;
		while (q < 16) numeric[q++] = '0';
		numeric[16] = 0;
		return;
	}
	val = b = q = z = 0;
	if (strlen (command) == 22 && !strncmp (command,"004011",6)) {
		q = 6;
		while (command[q]) {
			val |= (command[q] - '0') << b;
			b+= 2;
			if (b == 4) {
				val &= 0xf;
				if (val < 10) val += '0';
				else val += 'a' - 10;
				numeric[z++] = val;
				val = 0;
				b = 0;
			}
			q++;
		}
		if (b) {
			val &= 0xf;
			if (val < 10) val += '0';
			else val += 'a' - 10;
			numeric[z++] = val;
		}
		while (z < 16) numeric[z++] = '0';
		numeric[16] = 0;
		return;
	}
	if (command[q] == 'S') q++;
	while (command[q]) {
		val |= (command[q] - '0') << b;
		b++;
		if (b == 4) {
			val &= 0xf;
			if (val < 10) val += '0';
			else val += 'a' - 10;
			numeric[z++] = val;
			val = 0;
			b = 0;
		}
		q++;
	}
	if (b) {
		val &= 0xf;
		if (val < 10) val += '0';
		else val += 'a' - 10;
		numeric[z++] = val;
	}
	while (z < 16) numeric[z++] = '0';
	numeric[16] = 0;
}

FILE *ASCIIOpenRemote (char name[],NETWORKCLIENT *client)
{
	FILE *fp;
	char nm[256],st[256];

	strcpy (nm,name);
	if (strcmp (nm + strlen (nm) - 4,".rem")) strcat (nm,".rem");
	fp = DBOpenFile (nm,"r");
	memset (&client->ird,0,sizeof (IRDATA));
	client->learnstatus.received[0] = 0;
	client->learnstatus.adress = 0;
	client->learnstatus.statustype = STATUS_LEARN;
	client->learnstatus.statuslen = sizeof (NETWORKLEARNSTAT);
	memset (client->learnstatus.remote,' ',80);
	memset (client->learnstatus.received,' ',CODE_LEN);
	client->learnstatus.num_timings = 0;
	client->learnstatus.num_commands = 0;
	client->learnstatus.learnok = 0;
	client->learnstatus.carrier = 0;

	if (fp)	{
		ASCIITimingSample (fp,client);
		fclose (fp);
		fp = DBOpenFile (nm,"r+");
		memcpy (client->learnstatus.remote,name,strlen (name));
		strcpy (client->filename,nm);
		return (fp);
	}

	fp = DBOpenFile (nm,"w+");
	if (!fp) {
		sprintf (st,"Error opening remote file %s [%d]",nm,errno);
		log_print (st,LOG_ERROR);
		return (NULL);
	}

	fprintf (fp,"[REMOTE]\n");
	fprintf (fp,"  [NAME]%s\n\n",name);
	fprintf (fp,"[TIMING]\n");

	fflush (fp);
	
	memcpy (client->learnstatus.remote,name,strlen (name));
	strcpy (client->filename,nm);

	return (fp);
}


void ASCIITimingSample (FILE *fp,NETWORKCLIENT *client)
{
	char ln[2048];
	IRTIMING irt;
	int i;
	char *data;
	char *rp;
	
	
	data = DBFindSection (fp,"TIMING",NULL,NULL,NULL);
	if (!data) return;

	data = DBFindSection (fp,"0",ln,"[COMMANDS]",NULL);

	if (data) {
		StoreIRTiming (&irt,ln,-1);
		if (irt.carrier_measured) {
			client->learnstatus.carrier = irt.transmit_freq;
			client->learnstatus.learnok |= 2;
		}
		client->learnstatus.num_timings++;

		while (data && *data == '[' && data[1] != 'C') {
			data = DBReadString (ln,fp,NULL);
			if (data && *data == '[' && data[1] != 'C') client->learnstatus.num_timings++;
		}

		FillInTiming (&client->ird,&irt);
		client->ird.ir_length = 1;
	}

	rewind (fp);

	data = DBFindSection (fp,"COMMANDS",NULL,NULL,NULL);
	if (!data) return;
	
	data = DBReadString (ln,fp,NULL);
	if (!data || *data != '[') return;

	i = (int)strlen (data);
	while (i && data[i] != ']') i--;
	if (!i) return;
	i++;

    if ((rp = strstr(data, "[RAW]")))
    {
        int jj;
        rp += 5;
        sscanf(rp, "%d", &client->ird.ir_length);
		rp = data+i;
        for (jj = 0;jj < client->ird.ir_length && jj < CODE_LENRAW; jj++)
        {
            int val;
            int len;
            sscanf(rp, "%d%n", &val, &len);
            ((IRRAW *)&(client->ird))->data[jj] = val/8;
            rp += len;
        }
    }
    else
    {
        strncpy (client->ird.data,data+i,CODE_LEN);
		if (strlen (data+i) > CODE_LEN) client->ird.ir_length = CODE_LEN;
        else client->ird.ir_length = (byte)strlen (data+i);
    }


	client->learnstatus.num_commands++;

	while (data && *data == '[') {
		data = DBReadString (ln,fp,NULL);
		if (data && *data == '[') client->learnstatus.num_commands++;
	}

}

// Sucht nach existierenden Toggle - Commands
int ASCIIFindToggleSeq (FILE *fp,IRDATA *ird,char name[])
{
	
	int i,p = 0;
	int a;
	char ln[2048],*data;

	rewind (fp);
	data = DBFindSection (fp,"COMMANDS",NULL,NULL,NULL);

	if (!data) return (-1);
	while (data) {
		data = DBReadString (ln,fp,NULL);
		if (data && *data == '[') {
			data++;
			i = 0;
			while (data[i] && data[i] != '#' && data[i] != ']') i++;
			if (data[i] && !memcmp (name,data,i)) {
				if (data[i] == ']') a = 1;
				else a = atoi (data + i + 1) + 1;
				while (data[i] != 'D') i++;
				i += 2;
				if (!strcmp (ird->data,data+i)) {
					fseek (fp,0,SEEK_END);
					return (-a);
				}
				p = a;
			}
		}
	}

	fseek (fp,0,SEEK_END);

	return (p);
}


int ASCIIFindCommand (FILE *fp,char name[],NETWORKCLIENT *client)
{
	
	HANDLE hfile;
	int pos,new,len,oldlen;
	int i;
	char ln[2048],*data;
	char com[256];

	rewind (fp);
	data = DBFindSection (fp,"COMMANDS",NULL,NULL,NULL);

	if (!data) {
		fseek (fp,0,SEEK_END);
		fprintf (fp,"\n\n[COMMANDS]\n");
		return (0);
	}
	strcpy (com,name);
	ConvertLcase (com,(int)strlen (com));
	while (data) {
		pos = ftell (fp);
		data = DBReadString (ln,fp,NULL);
		if (data && *data == '[') {
			ConvertLcase (data,(int)strlen (data));
			data++;
			i = 0;
			while (data[i] && data[i] != ']') i++;
			if (data[i] && !memcmp (com,data,i) && i == (int)strlen (com)) {
				new = ftell (fp);							// Debug Info über Update
				oldlen = new - pos;
				fseek (fp,0,SEEK_END);
				len = ftell (fp) - new;
				fseek (fp,new,SEEK_SET);
				data = malloc (len + 1);
				len = (int)fread (data,1,len + 1,fp);
				fseek (fp,pos,SEEK_SET);
				fwrite (data,1,len,fp);
				pos = ftell (fp);
				free (data);
				data = malloc (oldlen);
				memset (data,' ',oldlen);
				fwrite (data,1,oldlen,fp);
				fseek (fp,pos,SEEK_SET);
				free (data);

				if (client) {
#ifdef WIN32
					hfile = CreateFile (client->filename,GENERIC_WRITE,FILE_SHARE_READ | FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
					if (hfile) {
						SetFilePointer (hfile,pos,NULL,FILE_BEGIN);
						SetEndOfFile (hfile);
						CloseHandle (hfile);
					}
#endif
#ifdef LINUX
					truncate (client->filename,pos);
#endif
				}

				return (1);												
			}
		}
	}

	fseek (fp,0,SEEK_END);

	return (0);
}


int FormatCCF (char *ccf)
{
	char st[10000];
	int j,i;
	
	i = 0;
	while (ccf[i]) {
		if (ccf[i] >= 'a' && ccf[i] <= 'z') ccf[i] -= 'a' - 'A';
		if (ccf[i] == 13 || ccf[i] == 10 || ccf[i] == 8) ccf[i] = ' ';
		if ((ccf[i] < '0' || ccf[i] > '9') && (ccf[i] < 'A' || ccf[i] > 'Z') && ccf[i] != ' ') {
			return (ERR_CCFSYNTAX);
		}

		i++;
	}

	j = i = 0;
	while (ccf[i]) {
		while (ccf[i] == ' ') i++;
		if (ccf[i]) st[j++] = ccf[i++];
		while (ccf[i] == ' ') i++;
		if (ccf[i]) st[j++] = ccf[i++];
		while (ccf[i] == ' ') i++;
		if (ccf[i]) st[j++] = ccf[i++];
		while (ccf[i] == ' ') i++;
		if (ccf[i]) st[j++] = ccf[i++];
		st[j++] = ' ';
	}
	st[j] = 0;

	if (strlen (st) < 24 || (strlen (st) % 5) || strlen (st) > 5140) return (ERR_CCFLEN);

	st[j-1] = 0;
	strcpy (ccf,st);

	return (0);
}

int ASCIIStoreLink (int client,char name[],char link[])
{
	if (ASCIIFindCommand (sockinfo[client].fp,name,sockinfo + client) == 0) sockinfo[client].learnstatus.num_commands++;

	fprintf (sockinfo[client].fp,"  [%s][LINK]%s\n",name,link);
	last_adress = sockinfo[client].ird.address;
	
	sprintf (sockinfo[client].learnstatus.received,"Link to: %s",link);

	fflush (sockinfo[client].fp);
	return (0);
}

int ASCIIStoreRS232 (int client,char name[],char rs232[])
{
	int i,j;
	char str[1024];

	if (ASCIIFindCommand (sockinfo[client].fp,name,sockinfo + client) == 0) sockinfo[client].learnstatus.num_commands++;

	j = 0;
	for (i=0;rs232[i];i++) {
		if (rs232[i] == 10) {
			str[j++] = '\\';
			str[j++] = 'n';
		}
		else if (rs232[i] == 13) {
			str[j++] = '\\';
			str[j++] = 'r';
		}
		else str[j++] = rs232[i];
	}
	str[j] = 0;

	fprintf (sockinfo[client].fp,"  [%s][RS232]%s\n",name,str);
	last_adress = sockinfo[client].ird.address;
	
	strcpy (sockinfo[client].learnstatus.received,"RS232 String");

	fflush (sockinfo[client].fp);
	return (0);
}

int ASCIIStoreCCF (int client,char name[],char ccf[])
{
	int res;
	
	res = FormatCCF (ccf);
	if (res) return (res);

	res = DecodeCCF (ccf,&sockinfo[client].ird,START);
	if (res == -1) return (ERR_CCF);

	if (res == 2 || res == 3) {
		sockinfo[client].timing = ASCIIStoreTiming (sockinfo[client].fp,&sockinfo[client].ird,&sockinfo[client].learnstatus);
		if (ASCIIFindCommand (sockinfo[client].fp,name,sockinfo + client) == 0) sockinfo[client].learnstatus.num_commands++;
		ASCIIStoreCommand (sockinfo[client].fp,&sockinfo[client].ird,name,sockinfo[client].timing,0);
		last_adress = sockinfo[client].ird.address;
		if (sockinfo[client].ird.data[0] & LONG_CODE_FLAG) strcpy (sockinfo[client].learnstatus.received,"Long Code");
		else memcpy (sockinfo[client].learnstatus.received,sockinfo[client].ird.data,sockinfo[client].ird.ir_length);
	}
	if (res == 4 || res == 5) {
		if (ASCIIFindCommand (sockinfo[client].fp,name,sockinfo + client) == 0) sockinfo[client].learnstatus.num_commands++;
		ASCIIStoreRAW (sockinfo[client].fp,(IRRAW *)&sockinfo[client].ird,name);
		last_adress = sockinfo[client].ird.address;

	}

	return (0);
}

int ASCIIStoreCommand (FILE *fp,IRDATA *ird,char name[],int timing,int seq_number)
{
	int i,offset;

	if (seq_number) fprintf (fp,"  [%s#%02d][T]%d",name,seq_number,timing);
	else fprintf (fp,"  [%s][T]%d",name,timing);

	offset = 0;
	if (ird->mode == TIMECOUNT_18) {
		if (((IRDATA_18 *)ird) -> data[0] < '0') offset = ((IRDATA_18 *)ird) -> data[0];
	}
	else if ((ird->mode & SPECIAL_IR_MODE) == PULSE_200) {
		if (((IRDATA_SINGLE *)ird) -> data[0] < '0') offset = ((IRDATA_SINGLE *)ird) -> data[0];
	}
	else {
		if (ird -> data[0] < '0') offset = ird -> data[0];
	}

	fprintf (fp,"[D]");

	if (ird->mode == TIMECOUNT_18) {
		for (i=offset;i < ((IRDATA_18 *)ird) -> ir_length;i++) {
			fprintf (fp,"%c",((IRDATA_18 *)ird) -> data[i]);
		}
		fprintf (fp,"\n");
	}

	else if ((ird->mode & SPECIAL_IR_MODE) == PULSE_200) {
		for (i=offset;i < ((IRDATA_SINGLE *)ird) -> ir_length;i++) {
			fprintf (fp,"%c",((IRDATA_SINGLE *)ird) -> data[i]);
		}
		fprintf (fp,"\n");
	}
	else if (ird->data[offset] & LONG_CODE_FLAG || (ird->data[offset] == 'X' && ird->data[offset+1] == 'l' && ird->data[offset+2] & LONG_CODE_FLAG)) {
		if (ird -> data[offset] == 'X' && ird -> data[offset + 1] == 'l') {
			fprintf (fp,"Xl");
			offset += 2;
		}
		for (i=offset;i < ird -> ir_length-1;i++) {
			if (i > offset && (ird -> data[i] & 128)) {		// Calibration
				if (ird -> data[i] & 64) fprintf (fp,"#-%03d;",(ird -> data[i] & 63) * 8);
				else fprintf (fp,"#0%03d;",(ird -> data[i] & 63) * 8);
			}
			else fprintf (fp,"%d%d",ird -> data[i] & 7,(ird -> data[i] >> 4) & 7);
		}
		fprintf (fp,"%d",ird -> data[i] & 7);
		if (!(ird->data[offset] & LONG_CODE_LEN)) fprintf (fp,"%d",(ird -> data[i] >> 4) & 7);
		fprintf (fp,"\n");
	}
	else {
		for (i=offset;i < ird -> ir_length;i++) {
			if (i > offset && ird -> data[i] & 128) {		// Calibration
				if (ird -> data[i] & 64) fprintf (fp,"#-%03d;",(ird -> data[i] & 63) * 8);
				else fprintf (fp,"#0%03d;",(ird -> data[i] & 63) * 8);
			}
			else fprintf (fp,"%c",ird -> data[i]);
		}
		fprintf (fp,"\n");
	}
	fflush (fp);
	return (0);
}

int ASCIIStoreRAW (FILE *fp,IRRAW *ird,char name[])
{
	int i;
	char st[20];
	if (ird->command == 0xff) strcpy (st,"[FREQ-MEAS]");
	else st[0] = 0;

	if (ird->transmit_freq == 255)
		fprintf (fp,"  [%s][RAW]%d[FREQ]455%s[D]",name,ird->ir_length,st);
	else
		fprintf (fp,"  [%s][RAW]%d[FREQ]%d%s[D]",name,ird->ir_length,ird->transmit_freq,st);
	for (i=0;i < ird->ir_length;i++) fprintf (fp,"%d ",ird->data[i] * 8);
	fprintf (fp,"\n");
	fflush (fp);
	return (0);
}

int TestTimeValue (int val,int sollk,int sollg)
{
	if (val < sollk || val > sollg) return (0);
	return (1);
}


void CheckRCMMCode (IRDATA *ird)
{
	int i;
	char msg[256];

	if (rcmmflag) return;

	if (ird->time_cnt < 3) return;

	for (i=0;i<ird->time_cnt;i++) {
		if (i == (ird->data[0] - '0')) {
			if (!TestTimeValue (ird->pulse_len[i],47,60)) return;
			if (!TestTimeValue (ird->pause_len[i],33,40)) return;
		}
		else {
			if (!TestTimeValue (ird->pulse_len[i],19,26)) return;
			if (!TestTimeValue (ird->pause_len[i],31,39) && !TestTimeValue (ird->pause_len[i],52,59) &&
				!TestTimeValue (ird->pause_len[i],71,80) && !TestTimeValue (ird->pause_len[i],93,101)) return;
		}
	}
	
	sprintf (msg,"RCMM Timing found\n");
	log_print (msg,LOG_DEBUG);

	for (i=0;i<ird->time_cnt;i++) {
		if (TestTimeValue (ird->pulse_len[i],15,22)) ird->pulse_len[i] = 21;
		if (TestTimeValue (ird->pulse_len[i],48,55)) ird->pulse_len[i] = 52;
		if (TestTimeValue (ird->pause_len[i],32,40)) ird->pause_len[i] = 35;
		if (TestTimeValue (ird->pause_len[i],53,60)) ird->pause_len[i] = 55;
		if (TestTimeValue (ird->pause_len[i],72,80)) ird->pause_len[i] = 76;
		if (TestTimeValue (ird->pause_len[i],93,101)) ird->pause_len[i] = 97;
	}
	ird->repeat_pause = 25;
	ird->ir_repeat = 2;
	ird->transmit_freq = 36;
}



int ASCIIStoreTiming (FILE *fp,IRDATA *ird,NETWORKLEARNSTAT *stat)
{
	int pos;
	int fpos,size;
	char *mem;
	char st[100];

	CheckRCMMCode (ird);

	pos = ASCIIFindTiming (fp,ird);

	if (pos == -1) return (-ERR_TIMINGNOTFOUND);

	if (pos) {
		if (pos >= 100) {
			fseek (fp,0,SEEK_END);
			return (pos - 100);
		}
		else {
			rewind (fp);
			DBFindSection (fp,"TIMING",NULL,NULL,NULL);
			sprintf (st,"%d",pos-1);
			DBFindSection (fp,st,NULL,NULL,NULL);
			fpos = ftell (fp);
			fseek (fp,0,SEEK_END);
			size = 1024 + ftell (fp) - fpos;
			mem = malloc (size);
			fseek (fp,fpos,SEEK_SET);
			size = (int)fread (mem,1,size,fp);
			fseek (fp,fpos,SEEK_SET);
			ASCIIStoreTimingParam (fp,ird,pos);
			fprintf (fp,"\n");
			fwrite (mem,1,size,fp);
			free (mem);
			fflush (fp);
			stat->num_timings++;
			stat->learnok = 1;
			return (pos);
		}
	}

	rewind (fp);
	mem = DBFindSection (fp,"COMMANDS",NULL,NULL,NULL);
	if (mem) {														// Command & Timings Section da aber keine Timings
		rewind (fp);
		DBFindSection (fp,"TIMING",NULL,NULL,NULL);
		fpos = ftell (fp);
		fseek (fp,0,SEEK_END);
		size = 1024 + ftell (fp) - fpos;
		mem = malloc (size);
		fseek (fp,fpos,SEEK_SET);
		size = (int)fread (mem,1,size,fp);
		fseek (fp,fpos,SEEK_SET);
		ASCIIStoreTimingParam (fp,ird,0);
		fprintf (fp,"\n");
		fwrite (mem,1,size,fp);
		free (mem);
		fflush (fp);
		stat->num_timings++;
		stat->learnok = 1;
		return (0);
	}


	fseek (fp,0,SEEK_END);
	ASCIIStoreTimingParam (fp,ird,pos);
	fprintf (fp,"\n\n[COMMANDS]\n");
	
	stat->num_timings++;
	stat->learnok = 1;

	fflush (fp);
	return (0);
}


void ASCIIStoreTimingParam (FILE *fp,IRDATA *ird,int timing)
{
	int i,offset;
	IRDATA_SINGLE *irs;
	IRDATA_18 *ird18;

	if (ird->mode == TIMECOUNT_18) {
		ird18 = (IRDATA_18 *)ird;

		fprintf (fp,"  [%d][N]%d",timing,ird18->time_cnt);
		for (i=1;i <= ird18->time_cnt;i++) {
			fprintf (fp,"[%d]%d %d",i,ird18->pulse_len[i-1] * 8,ird18->pause_len[i-1] * 8);
		}

		if (ird18->ir_repeat & 128) fprintf (fp,"[RC]%d[FL]%d",ird18->ir_repeat & 127,ird18->repeat_pause);
		else fprintf (fp,"[RC]%d[RP]%d",ird18->ir_repeat,ird18->repeat_pause);
		offset = GetRepeatOffset (ird);
		if (offset) fprintf (fp,"[RO]%d",offset);

		if (ird18->transmit_freq == 255) 
			fprintf (fp,"[FREQ]455");
		else
			fprintf (fp,"[FREQ]%d",ird->transmit_freq);
		if (ird->command == 0xff) fprintf (fp,"[FREQ-MEAS]");
		
		if (ird18->mode & START_BIT) fprintf (fp,"[SB]");
		if (ird18->mode & REPEAT_START) fprintf (fp,"[RS]");
	}
	else if ((ird->mode & SPECIAL_IR_MODE) == PULSE_200) {
		irs = (IRDATA_SINGLE *)ird;

		fprintf (fp,"  [%d][PULSE200][PULSE]%d[N]%d",timing,irs->single_len * 8,irs->time_cnt);
		for (i=1;i <= irs->time_cnt;i++) {
			fprintf (fp,"[%d]%d",i,irs->multi_len[i-1] * 8);
		}
		if (irs->transmit_freq == 255) 
			fprintf (fp,"[FREQ]455");
		else
			fprintf (fp,"[FREQ]%d",irs->transmit_freq);
		if (ird->command == 0xff) fprintf (fp,"[FREQ-MEAS]");
		fprintf (fp,"[RC]%d[RP]%d",irs->ir_repeat,irs->repeat_pause);
		offset = GetRepeatOffset (ird);
		if (offset) fprintf (fp,"[RO]%d",offset);
	}

	else {
		fprintf (fp,"  [%d][N]%d",timing,ird->time_cnt);
		for (i=1;i <= ird->time_cnt;i++) {
			fprintf (fp,"[%d]%d %d",i,ird->pulse_len[i-1] * 8,ird->pause_len[i-1] * 8);
		}

		if (ird->ir_repeat & 128) fprintf (fp,"[RC]%d[FL]%d",ird->ir_repeat & 127,ird->repeat_pause);
		else fprintf (fp,"[RC]%d[RP]%d",ird->ir_repeat,ird->repeat_pause);
		offset = GetRepeatOffset (ird);
		if (offset) fprintf (fp,"[RO]%d",offset);

		if (ird->transmit_freq == 255) 
			fprintf (fp,"[FREQ]455");
		else
			fprintf (fp,"[FREQ]%d",ird->transmit_freq);
		if (ird->command == 0xff) fprintf (fp,"[FREQ-MEAS]");

		if ((ird->mode & SPECIAL_IR_MODE) == RECS80) fprintf (fp,"[RECS80]");
		if ((ird->mode & SPECIAL_IR_MODE) == RCMM) fprintf (fp,"[RCMM]");
		else {
			if (ird->mode & RC6_DATA) {
				fprintf (fp,"[RC6][SB][RS]");
				if ((ird->mode & START_MASK) != START_MASK && (ird->mode & NO_TOGGLE)) fprintf (fp,"[NOTOG]");
			}
			else if (ird->mode & RC5_DATA) {
				fprintf (fp,"[RC5]");
				if (ird->mode & NO_TOGGLE) fprintf (fp,"[NOTOG]");
			}
			else {
				if (ird->mode & START_BIT) fprintf (fp,"[SB]");
				if (ird->mode & REPEAT_START) fprintf (fp,"[RS]");
			}
		}
	}
}


int ASCIIFindTiming (FILE *fp,IRDATA *ird)
{
	int i,flag;
	char ln[256],*data;
	char st[255];
	IRTIMING irt;
	
	rewind (fp);
	data = DBFindSection (fp,"TIMING",NULL,NULL,NULL);

	if (!data) return (-1);

	flag = i = 0;
	while (data) {
		sprintf (st,"%d",i);
		data = DBFindSection (fp,st,ln,"[COMMANDS]",NULL);
		if (data) {
			flag++;
			StoreIRTiming (&irt,ln,-1);
			if (CompareTiming (ird,&irt)) return (i + 100);
		}
		i++;
	}


	if (!flag) fseek (fp,0,SEEK_END);
	return (flag);
}


int GetRepeatOffset (IRDATA *ird)
{
	int i,offset = 0;

	if (ird->mode == TIMECOUNT_18) offset = 40;
	else if ((ird->mode & SPECIAL_IR_MODE) == PULSE_200) offset = 10;

	if (ird->data[offset] == 'X' && ird->data[offset+1] == 'l') offset += 2;
	if (ird->data[offset] < '0') {
		for (i=1;i < ird->data[offset];) {
			if (ird->data[offset+i+1] == OFFSET_TYP_REPEAT) return (ird->data[offset+i+2]);
			i += ird->data[offset+i];
		}
	}

	return (0);
}

int CompareTiming (IRDATA *ird,IRTIMING *irt)
{
	int i,md,frq;
	IRDATA_SINGLE *irs;
	IRDATA_18 *ird18;
	// Check for Timing Adjustments

	if (GetRepeatOffset (ird) != irt->repeat_offset) return (0);

	if (ird->mode == TIMECOUNT_18) {
		ird18 = (IRDATA_18 *)ird;

		if (ird18->mode != irt->mode || ird18->time_cnt != irt->time_cnt || ird18->transmit_freq != irt->transmit_freq) return (0);

		if (abs (ird18->ir_repeat - irt->ir_repeat) > 1) return (0);

		for (i = 0;i < ird18->time_cnt;i++) if (ird18->pause_len[i] != irt->pause_len[i] || ird18->pulse_len[i] != irt->pulse_len[i]) return (0);

		return (1);
	}

	if ((ird->mode & SPECIAL_IR_MODE) == PULSE_200) {
		irs = (IRDATA_SINGLE *)ird;

		if (irs->mode != irt->mode || irs->time_cnt != irt->time_cnt || irs->transmit_freq != irt->transmit_freq) return (0);

		if (irs->single_len != irt->pulse_len[0]) return (0);

		for (i = 0;i < irs->time_cnt;i++) if (irs->multi_len[i] != irt->pause_len[i]) return (0);

		return (1);
	}


	md = ird->mode;

	if (!((md & IRDA_DATA) == IRDA_DATA) && md & (RC5_DATA | RC6_DATA)) if ((md & START_MASK) != 2)  md &= ~START_MASK;

	frq = ird->transmit_freq;
	
	if (frq == 255) frq = 241;

	if (abs (ird->ir_repeat - irt->ir_repeat) > 1) return (0);

	if (md != irt->mode || ird->time_cnt != irt->time_cnt || frq != irt->transmit_freq) return (0);

	for (i=1;i < ird->ir_length;i++) if (ird->data[i] & 128) break;

	if (i < ird->ir_length) {
		for (i = 0;i < ird->time_cnt;i++) if (ird->pause_len[i] != irt->pause_len[i] || ird->pulse_len[i] != irt->pulse_len[i]) return (0);
	}

	else {
		for (i = 0;i < ird->time_cnt;i++) {
			if (ird->pause_len[i] < (irt->pause_len[i] - IR_TOLERANCE) || ird->pause_len[i] > (irt->pause_len[i] + IR_TOLERANCE) ||
				ird->pulse_len[i] < (irt->pulse_len[i] - IR_TOLERANCE) || ird->pulse_len[i] > (irt->pulse_len[i] + IR_TOLERANCE)) return (0);
		}
	}
	return (1);
}


IRREMOTE *rem_pnt;
int rem_cnt;
IRCOMMAND *cmd_pnt;
int cmd_cnt;
int link_cnt;
int cal_cnt;
IRTIMING *tim_pnt;
int tim_cnt;
int toggle_cnt;
MACROCOMMAND *mac_pnt;
int mac_cnt;
ROUTING *recv_routing;
int recv_routing_cnt;
ROUTING *send_routing;
int send_routing_cnt;
ROOMS *rooms;
int room_cnt;
SWITCH *switches;
int switch_cnt;
int ccf_raw;
int ccf_data;
int ccf_err;
APP app_pnt[30];
int app_cnt;


void GetRemoteDatabase (REMOTEBUFFER *buf,int offset)
{
	int i;
	memset (buf,0,sizeof (REMOTEBUFFER));
	buf->statustype = STATUS_REMOTELIST;
	buf->statuslen = sizeof (REMOTEBUFFER);
	buf->offset = (short)offset;

	i = 0;
	while (i < 40 && offset < rem_cnt) {
		memset (buf->remotes[i].name,' ',80);
		memcpy (buf->remotes[i].name,rem_pnt[offset].name,strlen (rem_pnt[offset].name));
		buf->remotes[i].source_mask = rem_pnt[offset].source_mask;
		buf->remotes[i].target_mask = rem_pnt[offset].target_mask;
		i++;
		offset++;
	}

	buf->count_buffer = i;
	buf->count_total = (word)rem_cnt;
	if (i == 40) buf->count_remaining = (short)(rem_cnt - offset);
	else buf->count_remaining = 0;
}

int GetCommandDatabase (COMMANDBUFFER *buf,char remote[],int offset)
{
	static int nrem;
	int i,start,tog = 0,togb,togs;
	char remcmp[100];

	memset (buf,0,sizeof (COMMANDBUFFER));
	buf->statustype = STATUS_COMMANDLIST;
	buf->statuslen = sizeof (COMMANDBUFFER);
	buf->offset = (short)offset;

	memset (remcmp,0,100);
	strcpy (remcmp,remote);
	ConvertLcase (remcmp,(int)strlen (remcmp));

	if (!offset || remote[0]) {
		nrem = DBFindRemote (remcmp);
		if (nrem == -1) return (1);
	}

	start = rem_pnt[nrem].command_start;
	
	i = rem_pnt[nrem].command_start;
	while (i < rem_pnt[nrem].command_end) {
		if (cmd_pnt[i].toggle_seq > 1) tog++;
		i++;
	}

	togb = i = 0;
	while (i < offset) {
		if (cmd_pnt[i+start].toggle_seq > 1) togb++;
		i++;
	}

	togs = i = 0;
	while (i < 200 && offset+togb < rem_pnt[nrem].command_end-start) {
		if (cmd_pnt[offset+start+togb].toggle_seq <= 1) {
			memset (buf->commands[i],' ',20);
			memcpy (buf->commands[i],cmd_pnt[offset+start+togb].name,strlen (cmd_pnt[offset+start+togb].name));
			i++;
		}
		else togs++;
		offset++;
	}

	buf->count_buffer = i;
	buf->count_total = (word)rem_pnt[nrem].command_end - start - tog;
	if (i == 200) {
		buf->count_remaining = (short)(rem_pnt[nrem].command_end + togs - start - offset - tog);
	}

	else buf->count_remaining = 0;
	return (0);
}

void FreeDatabaseMemory (void)
{
	if (rem_pnt) free (rem_pnt);
	rem_pnt = 0;
	rem_cnt = 0;
	if (cmd_pnt) free (cmd_pnt);
	cmd_pnt = 0;
	cmd_cnt = 0;
	cal_cnt = 0;
	link_cnt = 0;
	if (tim_pnt) free (tim_pnt);
	tim_pnt = 0;
	tim_cnt = 0;
	if (mac_pnt) free (mac_pnt);
	mac_pnt = 0;
	mac_cnt = 0;
	toggle_cnt = 0;
	if (recv_routing) free (recv_routing);
	recv_routing = 0;
	recv_routing_cnt = 0;
	if (send_routing) free (send_routing);
	send_routing = 0;
	send_routing_cnt = 0;
	if (rooms) free (rooms);
	rooms = 0;
	room_cnt = 0;
	ccf_data = ccf_raw = ccf_err = 0;
	app_cnt = 0;
	toggle_cnt = 0;
	memset (app_pnt,0,sizeof (app_pnt));
}

int cmpRawData (byte *rcv,byte *mem,int len);
int getRawValue (byte *pnt,int pos,word *val);

void put_mousemovement (byte pnt[],char name[])
{
	int x,y;
	char st[10];

	memset (st,0,sizeof (st));

	memcpy (st,pnt+7,6);
	y = strtoul (st,NULL,2);
	if (y > 32) y = y - 64;


	memcpy (st,pnt+14,6);
	x = strtoul (st,NULL,2);
	if (x > 32) x = x - 64;

	sprintf (name,"%03d %03d %c%c%",x,y,pnt[20],pnt[21]);
}

int DBFindCommandName (byte *command,char remote[],char name[],byte address,int *remote_num,int *command_num,word *command_num_rel,int start)
{
	int len,i,rstart,rlen,offset;
	byte *pnt;
	word mask;
	byte mode;

	static char last_name[50];
	static byte last_address;
	mask = 1 << (address & 15);
	mode = (address & 0xf0) >> 2;
	i = start;
	*command_num = *remote_num = 0;


	if (((mode & SPECIAL_IR_MODE) == RAW_DATA)) {								// RAW Vergleich
		pnt = (byte *)command;
		while (i < cmd_cnt) {

			if (cmd_pnt[i].mode & RAW_DATA && (rem_pnt[cmd_pnt[i].remote].source_mask & mask)) {
				if (cmpRawData (pnt,cmd_pnt[i].data,cmd_pnt[i].ir_length)) {
					if (cmd_pnt[i].name[strlen (cmd_pnt[i].name) - 1] == '@') {
						if (!strncmp (last_name,cmd_pnt[i].name,strlen (cmd_pnt[i].name) - 1)) {
							strcpy (remote,rem_pnt[cmd_pnt[i].remote].name);
							strcpy (name,cmd_pnt[i].name);
							name[strlen (name) - 1] = 0;
							*command_num_rel = (word)(i - rem_pnt[cmd_pnt[i].remote].command_start);
							*command_num = i;
							*remote_num = cmd_pnt[i].remote;
							last_address = address;
							return (i + 1);
							}
					}
					else {
						strcpy (remote,rem_pnt[cmd_pnt[i].remote].name);
						strcpy (name,cmd_pnt[i].name);
						strcpy (last_name,name);
						*command_num_rel = (word)(i - rem_pnt[cmd_pnt[i].remote].command_start);
						*command_num = i;
						*remote_num = cmd_pnt[i].remote;
						last_address = address;
						return (i + 1);
					}
				}
			}
			i++;
		}
		if (!start) last_name[0] = 0;
		return (0);
	}

	if (command[0] < '0') command += command[0];
	if (command[0] == 'X' && command[1] == 'l') command += 2;
	len = (int)strlen (command);

	while (i < cmd_cnt) {
		if (!cmd_pnt[i].mode && (rem_pnt[cmd_pnt[i].remote].source_mask & mask)) {
			if (cmd_pnt[i].timing == LINK_IRCOMMAND) {
				rlen = rem_pnt[cmd_pnt[i].remote_link].rcv_len;
				rstart = rem_pnt[cmd_pnt[i].remote_link].rcv_start;
			}
			else {
				rlen = rem_pnt[cmd_pnt[i].remote].rcv_len;
				rstart = rem_pnt[cmd_pnt[i].remote].rcv_start;
			}

			if (cmd_pnt[i].data[0] == 'X' && cmd_pnt[i].data[1] == 'l') offset = 2;
			else offset = 0;

			if ((rlen == 255 && len >= cmd_pnt[i].ir_length - offset && !memcmp (command,cmd_pnt[i].data + offset,cmd_pnt[i].ir_length - offset)) ||
				(rlen && len >= rlen && rstart < rlen && !memcmp (command+rstart,cmd_pnt[i].data+rstart + offset,rlen-rstart-offset)) ||
				(len == (cmd_pnt[i].ir_length - offset) && rstart < len && !memcmp (command+rstart,cmd_pnt[i].data+rstart+offset,len-rstart-offset))) {

				if (cmd_pnt[i].name[strlen (cmd_pnt[i].name) - 1] == '@') {
					if (!strncmp (last_name,cmd_pnt[i].name,strlen (cmd_pnt[i].name) - 1)) {
						strcpy (remote,rem_pnt[cmd_pnt[i].remote].name);
						strcpy (name,cmd_pnt[i].name);
						name[strlen (name) - 1] = 0;
						*command_num_rel = (word)(i - rem_pnt[cmd_pnt[i].remote].command_start);
						*command_num = i;
						*remote_num = cmd_pnt[i].remote;
						last_address = address;
						return (i + 1);
						}
				}
				else {
					strcpy (remote,rem_pnt[cmd_pnt[i].remote].name);
					strcpy (name,cmd_pnt[i].name);
					if (*command == 'K') strcat (name,command+9);

					if (*command == 'M') put_mousemovement (command,name);

					strcpy (last_name,name);
					*command_num_rel = (word)(i - rem_pnt[cmd_pnt[i].remote].command_start);
					*command_num = i;
					*remote_num = cmd_pnt[i].remote;
					last_address = address;
					return (i + 1);
				}
			}
		}
		i++;
	}

	if (!start) last_name[0] = 0;
	return (0);
}


int cmpRawData (byte *rcv,byte *mem,int len)
{
	int pos = 0;
	word recvdata,memdata;
	getRawValue (rcv,pos,&recvdata);
	pos = getRawValue (mem,pos,&memdata);
	while (pos <= len) {
		if (recvdata < (memdata - (RAW_TOLERANCE + (memdata >> 5))) || 
			recvdata > (memdata + (RAW_TOLERANCE + (memdata >> 5)))) return (0);
		getRawValue (rcv,pos,&recvdata);
		pos = getRawValue (mem,pos,&memdata);
	}
	return (1);
}


int getRawValue (byte *pnt,int pos,word *val)
{
	*val = 0;
	if (!pnt[pos]) {
		pos++;
		*val = pnt[pos++] << 8;
	}
	*val += pnt[pos++];
	return (pos);
}


int DBFindRemoteMacro (char remote[],char command[],int cmd_array[],word pause_array[])
{
	char sep,rep;
	int i,j,num;
	int ncmd,nrem;

	char remcmp[100];
	char cmdcmp[250];
	char sndcmd[100];

	memset (remcmp,0,100);
	memset (cmdcmp,0,250);

	strcpy (remcmp,remote);
	strcpy (cmdcmp,command);
	ConvertLcase (remcmp,(int)strlen (remcmp));
	ConvertLcase (cmdcmp,(int)strlen (cmdcmp));

	nrem = DBFindRemote (remcmp);
	if (nrem == -1) return (ERR_REMOTENOTFOUND);

	i = 0;
	num = 0;
	while (cmdcmp[i]) {
		if (num == 16) return (ERR_MACRO_COUNT);
		j = i;
		while (cmdcmp[i] && cmdcmp[i] != ';' && cmdcmp[i] != '~') i++;
		sep = cmdcmp[i];
		rep = cmdcmp[j];
		if (rep == '^') j++;
		cmdcmp[i++] = 0;
		memset (sndcmd,0,100);
		strcpy (sndcmd,cmdcmp + j);
		ncmd = DBFindCommand (sndcmd,&nrem);
		if (ncmd == -1) return (ERR_COMMANDNOTFOUND);
		if (sep == '~') {
			j = i;
			while (cmdcmp[i] && cmdcmp[i] != ';') i++;
			cmdcmp[i++] = 0;
			pause_array[num] = atoi (cmdcmp + j);
		}
		if (rep == '^') ncmd |= 0x40000000;
		cmd_array[num++] = ncmd;
	}
	if (num < 16) cmd_array[num] = -1;
	return (0);
}

int DBFindRemoteCommand (char remote[],char command[],int *cmd_num,int *rem_num)
{
	int ncmd,nrem;

	char remcmp[100];
	char cmdcmp[100];

	memset (remcmp,0,100);
	memset (cmdcmp,0,100);

	strcpy (remcmp,remote);
	strcpy (cmdcmp,command);
	ConvertLcase (remcmp,(int)strlen (remcmp));
	ConvertLcase (cmdcmp,(int)strlen (cmdcmp));


	nrem = DBFindRemote (remcmp);
	if (nrem == -1) return (ERR_REMOTENOTFOUND);
	if (rem_num) *rem_num = nrem;
	ncmd = DBFindCommand (cmdcmp,&nrem);
	if (ncmd == -1) return (ERR_COMMANDNOTFOUND);
	*cmd_num = ncmd;
	if (rem_num) *rem_num = nrem;
	return (0);
}


int DBGetRepeatCode (int cmd_num,IRDATA *ir,byte calflag,byte toggle)
{
	int mac_len,mac_pause;
	int nrem,ncmd,nrep;
	char cmd[100];

	IRMACRO *m_pnt;

	m_pnt = (IRMACRO *)(cmd_pnt + cmd_num);

	if (cmd_pnt[cmd_num].mode == MACRO_DATA) return (-1);

	ncmd = cmd_num;
	nrem = cmd_pnt[ncmd].remote;

	memset (cmd,' ',20);
	memcpy (cmd,cmd_pnt[ncmd].name,20);
	cmd[strlen (cmd_pnt[ncmd].name)] = '@';
	nrep = DBFindCommand (cmd,&nrem);
	if (nrep == -1) return (ERR_COMMANDNOTFOUND);

	return (DBGetIRCode (nrep,ir,0,&mac_len,&mac_pause,0,calflag,toggle));
}

void FillInTiming (IRDATA *ir,IRTIMING *tim)
{
	int i;
	IRDATA_SINGLE *irs;
	IRDATA_18 *ird18;

	ir->mode = tim->mode;
	ir->transmit_freq = tim->transmit_freq;

	if (tim->mode == TIMECOUNT_18) {
		ird18 = (IRDATA_18 *)ir;

		for (i=0;i < TIME_LEN_18;i++) {
			ird18->pause_len[i] = tim->pause_len[i];
			ird18->pulse_len[i] = tim->pulse_len[i];
		}
		ird18->time_cnt = tim->timecount_mode;
		ird18->ir_repeat = tim->ir_repeat;
		ird18->repeat_pause = tim->repeat_pause;
	}

	else if ((tim->mode & SPECIAL_IR_MODE) == PULSE_200) {
		irs = (IRDATA_SINGLE *)ir;

		irs->single_len  = tim->pulse_len[0];
		for (i=0;i < TIME_LEN_SINGLE;i++) irs->multi_len [i] = tim->pause_len[i];

		irs->time_cnt = tim->time_cnt;
		irs->ir_repeat = tim->ir_repeat;
		irs->repeat_pause = tim->repeat_pause;
	}

	else {
		for (i=0;i < TIME_LEN;i++) {
			ir->pause_len[i] = tim->pause_len[i];
			ir->pulse_len[i] = tim->pulse_len[i];
		}
		ir->time_cnt = tim->timecount_mode;
		ir->ir_repeat = tim->ir_repeat;
		ir->repeat_pause = tim->repeat_pause;
	}
	if ((tim->timecount_mode & (TC_SEND_POWER_MASK | TC_ACTIVE)) == (TC_SEND_POWER_LOW | TC_ACTIVE)) ir->address = INTERNAL_LEDS;
	if ((tim->timecount_mode & (TC_SEND_POWER_MASK | TC_ACTIVE)) == (TC_SEND_POWER_MED | TC_ACTIVE)) ir->address = EXTERNAL_LEDS;
	if ((tim->timecount_mode & (TC_SEND_POWER_MASK | TC_ACTIVE)) == (TC_SEND_POWER_HI | TC_ACTIVE)) ir->address = EXTERNAL_LEDS | INTERNAL_LEDS;
}


int DBGetIRCode (int cmd_num,IRDATA *ir,int idx,int *mac_len,int *mac_pause,int *rpt_len,byte calflag,byte toggle)
{
	int res,i,offset;
	int nrem,ncmd,ntim,mcmd;

	IRRAW *rd;
	IRMACRO *m_pnt;

	m_pnt = (IRMACRO *)(cmd_pnt + cmd_num);

	if (cmd_pnt[cmd_num].mode == MACRO_DATA) {
		*mac_len = m_pnt->macro_len;
		res = DBFindRemoteCommand (mac_pnt[m_pnt->macro_num + idx].mac_remote,mac_pnt[m_pnt->macro_num + idx].mac_command,&mcmd,NULL);
		if (res) return (res);
		ncmd = mcmd;
		*mac_pause = mac_pnt[m_pnt->macro_num + idx].pause;
	}

	else {
		ncmd = cmd_num;
		*mac_len = 0;
		*mac_pause = 0;
	}

	nrem = cmd_pnt[ncmd].remote;

	if (cmd_pnt[ncmd].mode & RAW_DATA) {
		rd = (IRRAW *)ir;
		memset (rd,0,sizeof (IRRAW));
		rd->mode = cmd_pnt[ncmd].mode;
		rd->target_mask = rem_pnt[nrem].target_mask;
		if (rem_pnt[nrem].transmitter) rd->address = rem_pnt[nrem].transmitter;
		if (cmd_pnt[ncmd].timing > 127) {
			if (cmd_pnt[ncmd].timing > 500) rd->transmit_freq = 255;
			else rd->transmit_freq = (byte)((cmd_pnt[ncmd].timing / 4) | 128);
		}

		else rd->transmit_freq = (byte)cmd_pnt[ncmd].timing;
 		rd->ir_length = (byte)cmd_pnt[ncmd].ir_length;
		memcpy (rd->data,cmd_pnt[ncmd].data,CODE_LENRAW);
		if (rpt_len) *rpt_len = cmd_pnt[ncmd].command_length;
	}
	if (!cmd_pnt[ncmd].mode) {
		ntim = rem_pnt[nrem].timing_start + cmd_pnt[ncmd].timing;
		if (ntim >= rem_pnt[nrem].timing_end) return (ERR_TIMINGNOTFOUND); 

		memset (ir,0,sizeof (IRDATA));

		ir->target_mask = rem_pnt[nrem].target_mask;
		if (rem_pnt[nrem].transmitter) ir->address = rem_pnt[nrem].transmitter;

		FillInTiming (ir,tim_pnt + ntim);

		if ((calflag && cmd_pnt[ncmd].ir_length_cal > CODE_LEN) || cmd_pnt[ncmd].ir_length > CODE_LEN) {			// Long Codes packen
			int i,p;
			offset = i = p = 0;
			if (calflag) {
				if (cmd_pnt[ncmd].data_cal[0] == 'X' && cmd_pnt[ncmd].data_cal[1] == 'l') {
					ir->data[i++] = cmd_pnt[ncmd].data_cal[p++];
					ir->data[i++] = cmd_pnt[ncmd].data_cal[p++];
					offset += 2;
				}
				while (p < cmd_pnt[ncmd].ir_length_cal) {
					if (cmd_pnt[ncmd].data_cal[p] & 128) ir->data[i++] = cmd_pnt[ncmd].data_cal[p++];
					else {
						ir->data[i] = (cmd_pnt[ncmd].data_cal[p++] & 7);
						if (p < cmd_pnt[ncmd].ir_length_cal) ir->data[i] |= ((cmd_pnt[ncmd].data_cal[p] & 7) << 4);
						p++;
						i++;
					}
				}
				
				ir->data[offset] |= LONG_CODE_FLAG;
				if (p > cmd_pnt[ncmd].ir_length_cal) ir->data[offset] |= LONG_CODE_LEN;
				ir->ir_length = i;
			}
			else {
				if (cmd_pnt[ncmd].data[0] == 'X' && cmd_pnt[ncmd].data[1] == 'l') {
					ir->data[i++] = cmd_pnt[ncmd].data[p++];
					ir->data[i++] = cmd_pnt[ncmd].data[p++];
					offset += 2;
				}
				while (p < cmd_pnt[ncmd].ir_length) {
					if (cmd_pnt[ncmd].data[p] & 128) p++;
					else {
						ir->data[i] = (cmd_pnt[ncmd].data[p++] & 7);
						if (p < cmd_pnt[ncmd].ir_length) ir->data[i] |= ((cmd_pnt[ncmd].data[p] & 7) << 4);
						p++;
						i++;
					}
				}
				
				ir->data[offset] |= LONG_CODE_FLAG;
				if (p > cmd_pnt[ncmd].ir_length) ir->data[offset] |= LONG_CODE_LEN;
				ir->ir_length = i;
			}
		}
		else {
			if (ir->mode == TIMECOUNT_18) {
				if (toggle) {
					memcpy (((IRDATA_18 *)ir)->data,cmd_pnt[ncmd].data_offset,(byte)cmd_pnt[ncmd].ir_length_offset);
					((IRDATA_18 *)ir)->ir_length = (byte)cmd_pnt[ncmd].ir_length_offset;
				}
				else {
					memcpy (((IRDATA_18 *)ir)->data,cmd_pnt[ncmd].data,CODE_LEN_18);
					((IRDATA_18 *)ir)->ir_length = (byte)cmd_pnt[ncmd].ir_length;
				}
			}
			else if ((ir->mode & SPECIAL_IR_MODE) == PULSE_200) {
				memcpy (((IRDATA_SINGLE *)ir)->data,cmd_pnt[ncmd].data,CODE_LEN_SINGLE);
				((IRDATA_SINGLE *)ir)->ir_length = (byte)cmd_pnt[ncmd].ir_length;
			}
			else if (calflag) {
				if (toggle) {
					memcpy (ir->data,cmd_pnt[ncmd].data_offset,CODE_LEN);
					ir->ir_length = (byte)cmd_pnt[ncmd].ir_length_offset;
				}
				else {
					memcpy (ir->data,cmd_pnt[ncmd].data_cal,CODE_LEN);
					ir->ir_length = (byte)cmd_pnt[ncmd].ir_length_cal;
				}
			}
			else {
				memcpy (ir->data,cmd_pnt[ncmd].data,CODE_LEN);
				ir->ir_length = (byte)cmd_pnt[ncmd].ir_length;
			}
		}

		if (ir->mode & NO_TOGGLE_H) {
			if (ir->mode & RC5_DATA) ir->data[2] = '1';
			else if (ir->mode & RC6_DATA) {
				if (ir->ir_length == 40) ir->data[23] = '1';
				else ir->data[5] = '1';
			}
			ir->mode &= SPECIAL_IR_MODE;
		}

		if (rpt_len) {
			*rpt_len = cmd_pnt[ncmd].command_length;
			if (mac_pause && *rpt_len) *mac_pause = cmd_pnt[ncmd].pause;
		}
	}

	return (0);
}


int DBFindRemoteCommandEx(char remote[],char command[],IRDATA *ir,byte calflag,byte toggle)
{

	int nrem,ncmd,ntim;
	IRRAW *rd;
	RS232_DATA *rs232;

	char remcmp[100];
	char cmdcmp[100];

	memset (remcmp,0,100);
	memset (cmdcmp,0,100);

	strcpy (remcmp,remote);
	strcpy (cmdcmp,command);
	ConvertLcase (remcmp,(int)strlen (remcmp));
	ConvertLcase (cmdcmp,(int)strlen (cmdcmp));


	nrem = DBFindRemote (remcmp);
	if (nrem == -1) return (ERR_REMOTENOTFOUND);
	ncmd = DBFindCommand (cmdcmp,&nrem);
	if (ncmd == -1) return (ERR_COMMANDNOTFOUND);

	if (cmd_pnt[ncmd].mode == MACRO_DATA) return (ERR_ISMACRO);

	if (cmd_pnt[ncmd].timing == RS232_IRCOMMAND) {
		rs232 = (RS232_DATA *)ir;
		memset (rs232,0,sizeof (RS232_DATA));

		rs232->command = HOST_SEND_RS232;
		rs232->len = (byte)(cmd_pnt[ncmd].ir_length + 5);
		rs232->parameter = (byte)cmd_pnt[ncmd].pause;
		memcpy (rs232->data,cmd_pnt[ncmd].data,cmd_pnt[ncmd].ir_length);
		return (0);
	}

	if (cmd_pnt[ncmd].mode & RAW_DATA) {
		rd = (IRRAW *)ir;
		memset (rd,0,sizeof (IRRAW));
		rd->mode = cmd_pnt[ncmd].mode;
		rd->target_mask = rem_pnt[nrem].target_mask;
		if (rem_pnt[nrem].transmitter) rd->address = rem_pnt[nrem].transmitter;
//		if (extended_carrier) {
		if (cmd_pnt[ncmd].timing > 127) {
			if (cmd_pnt[ncmd].timing > 500) rd->transmit_freq = 255;
			else rd->transmit_freq = (byte)((cmd_pnt[ncmd].timing / 4) | 128);
		}
		else rd->transmit_freq = (byte)cmd_pnt[ncmd].timing;

		rd->ir_length = (byte)cmd_pnt[ncmd].ir_length;
		memcpy (rd->data,cmd_pnt[ncmd].data,CODE_LENRAW);
		ir -> len = (sizeof (IRDATA) - (CODE_LEN + (RAW_EXTRA))) + ir -> ir_length;
	}
	if (!cmd_pnt[ncmd].mode) {
		ntim = rem_pnt[nrem].timing_start + cmd_pnt[ncmd].timing;
		if (ntim >= rem_pnt[nrem].timing_end) return (ERR_TIMINGNOTFOUND); 

		memset (ir,0,sizeof (IRDATA));

		ir->target_mask = rem_pnt[nrem].target_mask;
		if (rem_pnt[nrem].transmitter) ir->address = rem_pnt[nrem].transmitter;

		FillInTiming (ir,tim_pnt + ntim);

		if ((calflag && cmd_pnt[ncmd].ir_length_cal > CODE_LEN) || cmd_pnt[ncmd].ir_length > CODE_LEN) {			// Long Codes packen
			int i,p;
			i = p = 0;
			if (calflag) {
				while (p < cmd_pnt[ncmd].ir_length_cal) {
					if (cmd_pnt[ncmd].data_cal[p] & 128) ir->data[i++] = cmd_pnt[ncmd].data_cal[p++];
					else {
						ir->data[i] = (cmd_pnt[ncmd].data_cal[p++] & 7);
						if (p < cmd_pnt[ncmd].ir_length_cal) ir->data[i] |= ((cmd_pnt[ncmd].data_cal[p] & 7) << 4);
						p++;
						i++;
					}
				}
				
				ir->data[0] |= LONG_CODE_FLAG;
				if (p > cmd_pnt[ncmd].ir_length_cal) ir->data[0] |= LONG_CODE_LEN;
				ir->ir_length = i;
			}
			else {
				while (p < cmd_pnt[ncmd].ir_length) {
					if (cmd_pnt[ncmd].data[p] & 128) p++;
					else {
						ir->data[i] = (cmd_pnt[ncmd].data[p++] & 7);
						if (p < cmd_pnt[ncmd].ir_length) ir->data[i] |= ((cmd_pnt[ncmd].data[p] & 7) << 4);
						p++;
						i++;
					}
				}
				
				ir->data[0] |= LONG_CODE_FLAG;
				if (p > cmd_pnt[ncmd].ir_length) ir->data[0] |= LONG_CODE_LEN;
				ir->ir_length = i;
			}
			ir -> len = (sizeof (IRDATA) - CODE_LEN) + ir -> ir_length;
		}
		else {

			if (ir->mode == TIMECOUNT_18) {
				if (toggle) {
					memcpy (((IRDATA_18 *)ir)->data,cmd_pnt[ncmd].data_offset,(byte)cmd_pnt[ncmd].ir_length_offset);
					((IRDATA_18 *)ir)->ir_length = (byte)cmd_pnt[ncmd].ir_length_offset;
				}
				else {
					memcpy (((IRDATA_18 *)ir)->data,cmd_pnt[ncmd].data,CODE_LEN_18);
					((IRDATA_18 *)ir)->ir_length = (byte)cmd_pnt[ncmd].ir_length;
				}
				ir -> len = (sizeof (IRDATA_18) - CODE_LEN_18) + ((IRDATA_18 *)ir) -> ir_length;
			}
			else if ((ir->mode & SPECIAL_IR_MODE) == PULSE_200) {
				memcpy (((IRDATA_SINGLE *)ir)->data,cmd_pnt[ncmd].data,CODE_LEN_SINGLE);
				((IRDATA_SINGLE *)ir)->ir_length = (byte)cmd_pnt[ncmd].ir_length;
			}
			else if (calflag) {
				if (toggle) {
					memcpy (ir->data,cmd_pnt[ncmd].data_offset,CODE_LEN);
					ir->ir_length = (byte)cmd_pnt[ncmd].ir_length_offset;

				}
				else {
					memcpy (ir->data,cmd_pnt[ncmd].data_cal,CODE_LEN);
					ir->ir_length = (byte)cmd_pnt[ncmd].ir_length_cal;
				}
				ir -> len = (sizeof (IRDATA) - CODE_LEN) + ir -> ir_length;
			}
			else {
				memcpy (ir->data,cmd_pnt[ncmd].data,CODE_LEN);
				ir->ir_length = (byte)cmd_pnt[ncmd].ir_length;
				ir -> len = (sizeof (IRDATA) - CODE_LEN) + ir -> ir_length;
			}

		}

		if (ir->mode & NO_TOGGLE_H) {
			if (ir->mode & RC5_DATA) ir->data[2] = '1';
			else if (ir->mode & RC6_DATA) {
				if (ir->ir_length == 40) ir->data[23] = '1';
				else ir->data[5] = '1';
			}
			ir->mode &= SPECIAL_IR_MODE;
		}

	}

	return (0);
}



int DBFindCommand (char command[],int *remote)
{
	int p,s;
	int i = rem_pnt[*remote].command_start;
	while (i < rem_pnt[*remote].command_end) {
		if (!memcmp (command,cmd_pnt[i].name,20)) {
			if (cmd_pnt[i].timing == LINK_ERROR) return (-1);
			if (cmd_pnt[i].timing == LINK_IRCOMMAND) {
				i = cmd_pnt[i].command_link;
				*remote = cmd_pnt[i].remote;
			}
			if (rem_pnt[*remote].toggle_pos) {
				s = rem_pnt[*remote].toggle_pos;
				p = i;
				while ((cmd_pnt[p].toggle_seq != s || memcmp (command,cmd_pnt[p].name,20)) && p < rem_pnt[*remote].command_end) p++;
				if (cmd_pnt[p].toggle_seq == s) {
					rem_pnt[*remote].toggle_pos++;
					return (p);
				}
				else {
					rem_pnt[*remote].toggle_pos = 2;
					return (i);
				}
			}

			else if (cmd_pnt[i].toggle_seq) {
				s = cmd_pnt[i].toggle_pos;
				p = i;
				while ((cmd_pnt[p].toggle_seq != s || memcmp (command,cmd_pnt[p].name,20)) && p < rem_pnt[*remote].command_end) p++;
				if (cmd_pnt[p].toggle_seq == s) {
					cmd_pnt[i].toggle_pos++;
					return (p);
				}
				else {
					cmd_pnt[i].toggle_pos = 2;
					return (i);
				}
			}
			return (i);
		}
		i++;
	}
	return (-1);
}


int DBFindRemote (char remote[])
{
	int i = 0;
	while (i < rem_cnt) {
		if (!memcmp (remote,rem_pnt[i].name,80)) return (i);
		i++;
	}
	return (-1);
}



int	StoreSwitch (word id,word num,char *rem,char *com,word mode)
{
	int i = 0;
	while (i < switch_cnt) {
		if (switches[i].id == id && switches[i].num == num) {
			switches[i].mode = mode;
			strcpy (switches[i].remote,rem);
			strcpy (switches[i].command,com);
			return (1);
		}
		i++;
	}
	
	switches = realloc (switches,(switch_cnt + 1) * sizeof (SWITCH));
	switches[switch_cnt].id = id;
	switches[switch_cnt].num = num;
	switches[switch_cnt].mode = mode;
	strcpy (switches[switch_cnt].remote,rem);
	strcpy (switches[switch_cnt].command,com);
	switch_cnt++;

	return (0);
}


void WriteSwitches (void)
{
	int i = 0;
	char m;
	FILE *fp;

	if (switch_cnt == 0) return;

	fp = DBOpenFile ("switches.cfg","w");

	while (i < switch_cnt) {
		if (*switches[i].remote && *switches[i].command) {
			if (switches[i].mode == 1) m = 'T';
			if (switches[i].mode == 2) m = 'S';
			if (switches[i].mode == 4) m = '0';
			if (switches[i].mode == 8) m = '1';
			fprintf (fp,"[ID]%02d.%02d	[NR]%d	[%c]	%s	%s\n",switches[i].id >> 8,switches[i].id & 0xff,switches[i].num,m,switches[i].remote,switches[i].command);
		}
		i++;
	}

	fclose (fp);

}

int FindSwitch (word id,word num,char *rem,char *com,word *mode)
{
	int i = 0;
	*mode = 0;
	*rem = 0;
	*com = 0;
	while (i < switch_cnt) {
		if (switches[i].id == id && switches[i].num == num) {
			*mode = switches[i].mode;
			strcpy (rem,switches[i].remote);
			strcpy (com,switches[i].command);
			return (1);
		}
		i++;
	}
	return (0);
}

void ReadSwitches (void)
{
	FILE *fp;
	int i,j;
	char ln[2048],*data;
	SWITCH sw;

	fp = DBOpenFile ("switches.cfg","r");

	if (!fp) return;


	switch_cnt = 0;
	switches = 0;
	data = DBReadString (ln,fp,NULL);
	while (data) {
		i = 0;

		while (ln[i] && ln[i] != ']') i++;
		i++;
		if (ln[i+2] == '.') {
			ln[i+2] = 0;
			sw.id = atoi (ln + i) * 256 + atoi (ln + i + 3);
			i += 3;
		}
		else sw.id = atoi (ln + i);

		while (ln[i] && ln[i] != ']') i++;
		i++;
		sw.num = atoi (ln + i);
		sw.mode = 0;

		while (ln[i] && ln[i] != '[') i++;
		i++;
		if (ln[i] == 'T') sw.mode = 1;
		if (ln[i] == 'S') sw.mode = 2;
		if (ln[i] == '0') sw.mode = 4;
		if (ln[i] == '1') sw.mode = 8;

		i += 2;

		while (ln[i] == ' ' || ln[i] == '\t') i++;
		j = i;
		while (ln[i] != ' ' && ln[i] != '\t') i++;
		ln[i++] = 0;
		strcpy (sw.remote,ln + j);
		while (ln[i] == ' ' || ln[i] == '\t') i++;
		strcpy (sw.command,ln + i);

		switches = realloc (switches,(switch_cnt + 1) * sizeof (SWITCH));
		switches[switch_cnt++] = sw;

		data = DBReadString (ln,fp,NULL);
	}

	
	fclose (fp);
}

void ReadRoutingTable (void)
{
	FILE *fp;

	int res;

	fp = DBOpenFile ("routing","r");

	if (!fp) return;														// No routing table found

	res = DBStoreRooms (fp);												// Read Rooms
	if (!res) {
		DBStoreRouting (fp,"SEND-ROUTING",&send_routing,&send_routing_cnt);	// Read Recv Routing
		DBStoreRouting (fp,"RECV-ROUTING",&recv_routing,&recv_routing_cnt);	// Read Send Routing
	}

	fclose (fp);
	return;

}

extern int xbmc_remote;

void ReadAppConfig (void)
{

	int i,j,p,cf,res,cnum;
	FILE *fp;
	char ln[2048],*data,lchar;


	fp = DBOpenFile ("apps.cfg","r");

	if (!fp) return;														// No APP Config found

	cf = app_cnt = 0;
	rewind (fp);

	data = ln;

	while (data) {
		data = DBReadString (ln,fp,NULL);
		if (!data) {
			fclose (fp);
			return;
		}
		if (!strncmp (data,"[APP]",5)) {
			strcpy (app_pnt[app_cnt].name,data + 5);
			app_pnt[app_cnt].com_cnt = 0;
		}
		else if (!strncmp (data,"[CLASSNAME]",11)) {
			strcpy (app_pnt[app_cnt].classname,data + 11);
		}
#ifdef WIN32
		else if (!strncmp (data,"[APPNAME]",9)) {
			strcpy (app_pnt[app_cnt].appname,data + 9);
		}
#endif
#ifdef LINUX
		else if (!strncmp (data,"[APPLINUX]",10)) {
			strcpy (app_pnt[app_cnt].appname,data + 10);
		}
#endif
		else if (!strncmp (data,"[ACTIVE]",8)) {
			app_pnt[app_cnt].active = 1;
		}
		else if (!strncmp (data,"[TYPE]",6)) {
			if (!strcmp (data+6,"MCE")) app_pnt[app_cnt].type = TYPE_MCE;
			if (!strcmp (data+6,"KEY")) app_pnt[app_cnt].type = TYPE_KEY;
			if (!strcmp (data+6,"APPCOM")) app_pnt[app_cnt].type = TYPE_APPCOM;
			if (!strcmp (data+6,"COM")) app_pnt[app_cnt].type = TYPE_COM;
			if (!strcmp (data+6,"KEYBOARD")) app_pnt[app_cnt].type = TYPE_KEYBOARD;
			if (!strcmp (data+6,"SCANCODE")) app_pnt[app_cnt].type = TYPE_SCANCODE;
			if (!strcmp (data+6,"MOUSE")) app_pnt[app_cnt].type = TYPE_MOUSE;
			if (!strcmp (data+6,"SHORTCUT")) app_pnt[app_cnt].type = TYPE_SHORTCUT;
			if (!strcmp (data+6,"XBMC")) {
				app_pnt[app_cnt].type = TYPE_XBMC;
				xbmc_remote = app_pnt[app_cnt].remnum;
			}
		}
		else if (!strncmp (data,"[REMOTE]",8)) {
			strcpy (app_pnt[app_cnt].remote,data + 8);
			ConvertLcase (app_pnt[app_cnt].remote,(int)strlen (app_pnt[app_cnt].remote));
			app_pnt[app_cnt].remnum = DBFindRemote (app_pnt[app_cnt].remote);
			if (app_pnt[app_cnt].type == TYPE_XBMC)	xbmc_remote = app_pnt[app_cnt].remnum;

		}
		else if (!strncmp (data,"[COMMANDS]",10)) {
			cf = 1;
		}
		else if (!strncmp (data,"[END-COMMANDS]",14)) {
			cf = 0;
		}
		else if (!strncmp (data,"[END-APP]",9)) {
			app_cnt++;
		}
		else if (cf) {
			i = 0;
			cnum = 0;
			while (data[i] && data[i] != ' ' && data[i] != '\t' && data[i] != '[') i++;
			if (data[i] == ' ' || data[i] == '\t') data[i++] = 0;
			ConvertLcase (data,(int)strlen (data));
			while (data[i] && data[i] != '[') i++;
			if (!data[i]) continue;
			res = DBFindRemoteCommand (app_pnt[app_cnt].remote,data,&app_pnt[app_cnt].com[app_pnt[app_cnt].com_cnt].comnum,&app_pnt[app_cnt].remnum);
			if (res) continue;
			p = i;
			while (data[p]) {
				i = p + 5;
				j = i;
				while (data[j] && data[j] != '\t' && data[j] != '[') j++;
				lchar = data[j];
				data[j] = 0;
				ConvertLcase (data+i,(int)strlen (data+i));
				if (!strncmp (data+p,"[FNC]",5)) {
					res = GetFunctionCode (app_pnt[app_cnt].type,data+i);
					if (!res) goto notfound;
					app_pnt[app_cnt].com[app_pnt[app_cnt].com_cnt].type[cnum] = app_pnt[app_cnt].type;
					app_pnt[app_cnt].com[app_pnt[app_cnt].com_cnt].function.function[cnum] = res;
					cnum++;
				}
				else if (!strncmp (data+p,"[STR]",5)) {
					app_pnt[app_cnt].com[app_pnt[app_cnt].com_cnt].type[cnum] = TYPE_STR;
					strcpy (app_pnt[app_cnt].com[app_pnt[app_cnt].com_cnt].function.name,data + i);
					cnum++;
				}
				else if (!strncmp (data+p,"[XBMC-button]",13)) {
					i = p + 13;
					app_pnt[app_cnt].com[app_pnt[app_cnt].com_cnt].type[cnum] = TYPE_XBMC_BUTTON;
					strcpy (app_pnt[app_cnt].com[app_pnt[app_cnt].com_cnt].function.name,data + i);
					cnum++;
				}
				else if (!strncmp (data+p,"[XBMC-action]",13)) {
					i = p + 13;
					app_pnt[app_cnt].com[app_pnt[app_cnt].com_cnt].type[cnum] = TYPE_XBMC_ACTION;
					strcpy (app_pnt[app_cnt].com[app_pnt[app_cnt].com_cnt].function.name,data + i);
					cnum++;
				}
				else if (!strncmp (data+p,"[XBMC-action-b]",15)) {
					i = p + 15;
					app_pnt[app_cnt].com[app_pnt[app_cnt].com_cnt].type[cnum] = TYPE_XBMC_ACTION_BUILTIN;
					strcpy (app_pnt[app_cnt].com[app_pnt[app_cnt].com_cnt].function.name,data + i);
					cnum++;
				}
				else if (!strncmp (data+p,"[KEY]",5)) {
					res = GetKeyCode (data+i);
					if (!res) goto notfound;
					app_pnt[app_cnt].com[app_pnt[app_cnt].com_cnt].type[cnum] = TYPE_KEY;
					app_pnt[app_cnt].com[app_pnt[app_cnt].com_cnt].function.function[cnum] = res;
					cnum++;
				}
				else if (!strncmp (data+p,"[KEF]",5)) {
					res = GetKeyCode (data+i);
					if (!res) goto notfound;
					app_pnt[app_cnt].com[app_pnt[app_cnt].com_cnt].type[cnum] = TYPE_KEYF;
					app_pnt[app_cnt].com[app_pnt[app_cnt].com_cnt].function.function[cnum] = res;
					cnum++;
				}
				else if (!strncmp (data+p,"[CHR]",5)) {
					res = GetKeyCode (data+i);
					if (!res) goto notfound;
					app_pnt[app_cnt].com[app_pnt[app_cnt].com_cnt].type[cnum] = TYPE_CHR;
					app_pnt[app_cnt].com[app_pnt[app_cnt].com_cnt].function.function[cnum] = res;
					cnum++;
				}
				else if (!strncmp (data+p,"[APP]",5)) {
					res = GetFunctionCode (TYPE_APPCOM,data+i);
					if (!res) goto notfound;
					app_pnt[app_cnt].com[app_pnt[app_cnt].com_cnt].type[cnum] = TYPE_APPCOM;
					app_pnt[app_cnt].com[app_pnt[app_cnt].com_cnt].function.function[cnum] = res;
					cnum++;
				}
				else if (!strncmp (data+p,"[RUN]",5)) {
					app_pnt[app_cnt].com[app_pnt[app_cnt].com_cnt].type[cnum] = TYPE_RUN;
					cnum++;
				}
notfound:
				data[j] = lchar;
				p = j;
			}
		if (cnum) app_pnt[app_cnt].com_cnt++;
		}
	}
	
}

void DBStoreRouting (FILE *fp,char section[],ROUTING **pnt,int *cnt)
{
	int i;
	char ln[2048],*data;

	*cnt = 0;
	rewind (fp);
	data = DBFindSection (fp,section,NULL,NULL,NULL);

	if (!data) return;

	while (data) {
		data = DBFindSection (fp,"REM",ln,"[END]",NULL);
		if (data) {
			i = 0;
			while (data[i] && data[i] != '[') i++;
			if (data[i] == '[') {
				data[i++] = 0;
				while (data[i] && data[i] != ']') i++;
				if (data[i] == ']') {
					i++;
					*pnt = realloc (*pnt,(*cnt + 1) * sizeof (ROUTING));
					memset (*pnt + *cnt,0,sizeof (ROUTING));
					if (strlen (data) > 80) data[80] = 0;
					ConvertLcase (data,(int)strlen (data));
					((*pnt) + *cnt)->target_mask = (word)strtol (data + i,NULL,0);
					strncpy (((*pnt) + *cnt)->name,data,80);
					(*cnt)++;
				}
			}
		}
	}
}

int DBStoreRooms (FILE *fp)
{
	int i;
	char ln[2048],*data;

	
	room_cnt = 0;
	rewind (fp);
	data = DBFindSection (fp,"ADDRESS",NULL,NULL,NULL);

	if (!data) return (-1);
	while (data) {
		data = DBFindSection (fp,"NAME",ln,NULL,NULL);
		if (data) {
			i = 0;
			while (data[i] && data[i] != '[') i++;
			if (data[i] == '[') {
				data[i++] = 0;
				while (data[i] && data[i] != ']') i++;
				if (data[i] == ']') {
					i++;
					rooms = realloc (rooms,(room_cnt + 1) * sizeof (ROOMS));
					memset (rooms + room_cnt,0,sizeof (ROOMS));
					if (strlen (data) > 80) data[80] = 0;
					ConvertLcase (data,(int)strlen (data));
					rooms[room_cnt].addr = atoi (data + i);
					strncpy (rooms[room_cnt].name,data,80);
					room_cnt++;
				}
			}
		}
	}
	return (0);
}



int DBReadCommandFile (char remote[])
{
	FILE *fp;
	char includeremote[100];
	char st[256];
	int res;

	fp = DBOpenFile (remote,"r");

	if (!fp) return (ERR_DBOPENINPUT);

	res = DBStoreRemote (fp,includeremote);
	
	if (res == -1) {
		fclose (fp);
		return (ERR_DBOPENINPUT);
	}


	if (*includeremote) {
		if (strcmp (includeremote + strlen (includeremote) - 4,".rem")) strcat (includeremote,".rem");
		fclose (fp);
		fp = DBOpenFile (includeremote,"r");

		if (!fp) return (ERR_DBOPENINCLUDE);
	}
	res = DBStoreTimings (fp,remote);
	
	res = DBStoreCommands (fp,remote);

	sprintf (st,"Remote %-20s compiled:",rem_pnt[rem_cnt].name);
	log_print (st,LOG_INFO);
	sprintf (st,"  %4d Timings - ",rem_pnt[rem_cnt].timing_end-rem_pnt[rem_cnt].timing_start);
	log_print (st,LOG_INFO);
	sprintf (st,"  %4d Commands\n",rem_pnt[rem_cnt].command_end-rem_pnt[rem_cnt].command_start);
	log_print (st,LOG_INFO);

	rem_cnt++;

	fclose (fp);
	return (0);
}


void FindDuplicateCommands (void)
{
	int i = 0,j;
	int cnt;
	char st[255];

	while (i < cmd_cnt) {
		j = i + 1;
		if (cmd_pnt[i].mode == 0 && cmd_pnt[i].name[strlen (cmd_pnt[i].name) - 1] != '@') {
			cnt = 0;
			while (j < cmd_cnt) {
				if (cmd_pnt[j].mode == 0 && cmd_pnt[j].name[strlen (cmd_pnt[j].name) - 1] != '@') {
					if (cmd_pnt[i].ir_length == cmd_pnt[j].ir_length && !memcmp (cmd_pnt[j].data,cmd_pnt[i].data,cmd_pnt[i].ir_length)) {
						if (!cnt) {
							sprintf (st,"Duplicate Commands for %s.%s: ",rem_pnt[cmd_pnt[i].remote].name,cmd_pnt[i].name);
							log_print (st,LOG_INFO);
						}
						sprintf (st,"  %s.%s",rem_pnt[cmd_pnt[j].remote].name,cmd_pnt[j].name);
						log_print (st,LOG_INFO);
						cnt++;
					}
				}
				j++;
			}
			if (cnt) {
				sprintf (st,"\n");
				log_print (st,LOG_INFO);
			}
		}
		i++;
	}
}


void DBShowStatus (void)
{
	char st[256];
	sprintf (st,"Total: %3d Remotes  - %3d Timings - %4d Commands - %4d Calib. Commands\n",rem_cnt,tim_cnt,cmd_cnt,cal_cnt);
	FindDuplicateCommands ();
	log_print (st,LOG_INFO);
	sprintf (st,"         %d Togglec. - %3d CCF Data - %3d CCF RAW  - %4d CCF Error\n",toggle_cnt,ccf_data,ccf_raw,ccf_err);
	log_print (st,LOG_INFO);
}


int DBReferenceLinks (void)
{
	char st[255];
	int i,nrem,ncmd;
	for (i=0;i < cmd_cnt;i++) if (cmd_pnt[i].timing == LINK_IRCOMMAND) {
		nrem = DBFindRemote (cmd_pnt[i].data);
		if (nrem == -1) {
			sprintf (st,"Link Error [%s.%s]: Remote %s not found\n",rem_pnt[cmd_pnt[i].remote].name,cmd_pnt[i].name,cmd_pnt[i].data);
			log_print (st,LOG_ERROR);
			cmd_pnt[i].timing = LINK_ERROR;
			continue;
		}
		ncmd = DBFindCommand (cmd_pnt[i].data_cal,&nrem);
		if (ncmd == -1) {
			sprintf (st,"Link Error [%s.%s]: Command %s not found\n",rem_pnt[cmd_pnt[i].remote].name,cmd_pnt[i].name,cmd_pnt[i].data_cal);
			log_print (st,LOG_ERROR);
			cmd_pnt[i].timing = LINK_ERROR;
			continue;
		}
		if (cmd_pnt[ncmd].timing == LINK_IRCOMMAND) {
			sprintf (st,"Link Error [%s.%s]: Multistep links not allowed [%s]\n",rem_pnt[cmd_pnt[i].remote].name,cmd_pnt[i].name,cmd_pnt[i].data_cal);
			log_print (st,LOG_ERROR);
			cmd_pnt[i].timing = LINK_ERROR;
			continue;
		}
		cmd_pnt[i].remote_link = nrem;
		cmd_pnt[i].command_link = ncmd;
		memcpy (cmd_pnt[i].data,cmd_pnt[ncmd].data,cmd_pnt[ncmd].ir_length);
		cmd_pnt[i].ir_length = cmd_pnt[ncmd].ir_length;
	}

	return (0);
}

int DBStoreCommands (FILE *fp,char remote[])
{
	int i,p,j,ccf_rpt,ccf_pause,cmd_length,n,res,maxlen,ntim;
	char ln[2048],*data,st[255];
	IRMACRO *m_pnt;

	rem_pnt[rem_cnt].command_start = cmd_cnt;
	rem_pnt[rem_cnt].command_end = cmd_cnt;
	rewind (fp);
	if (time_len == TIME_LEN) maxlen = CODE_LENRAW - 2;
	else maxlen = OLD_LENRAW - 2;
	data = DBFindSection (fp,"COMMANDS",NULL,NULL,NULL);

	if (!data) return (-1);
	while (data) {
		data = DBReadString (ln,fp,NULL);
		if (data && *data == '[') {
			ccf_rpt = ccf_pause = cmd_length = 0;
			data++;
			i = 0;
			while (data[i] && data[i] != ']') i++;
			if (data[i]) {
				cmd_pnt = realloc (cmd_pnt,(cmd_cnt + 1) * sizeof (IRCOMMAND));
				memset (cmd_pnt + cmd_cnt,0,sizeof (IRCOMMAND));
				data[i++] = 0;
				if (strlen (data) > 20) data[20] = 0;
				ConvertLcase (data,(int)strlen (data));
				if (data[strlen (data) - 3] == '#') {
					data[strlen (data) - 3] = 0;
					memcpy (cmd_pnt[cmd_cnt].name,data,strlen (data));
					j = cmd_cnt - 1;
					if (atoi (data + strlen (data) + 1) == 1) while (j >= 0) {
						if (!memcmp (cmd_pnt[j].name,cmd_pnt[cmd_cnt].name,20) && !cmd_pnt[j].toggle_seq) {
							cmd_pnt[j].toggle_seq = 1;
							cmd_pnt[j].toggle_pos = 1;
							j = 0;
						}
						j--;
					}
					cmd_pnt[cmd_cnt].toggle_seq = atoi (data + strlen (data) + 1) + 1;
				}
				else {
					cmd_pnt[cmd_cnt].toggle_seq = 0;
					memcpy (cmd_pnt[cmd_cnt].name,data,strlen (data));
				}

				cmd_pnt[cmd_cnt].toggle_pos = 0;
				cmd_pnt[cmd_cnt].remote = rem_cnt;
				if (!memcmp (data + i,"[RL]",4)) {
					i += 4;
					cmd_length = atoi (data + i);
					while (data[i] && data[i] != '[') i++;
				}
				if (!memcmp (data + i,"[RC]",4)) {
					i += 4;
					ccf_rpt = atoi (data + i);
					ccf_pause = 0;
					while (data[i] && data[i] != '[') i++;
				}
				if (!memcmp (data + i,"[RP]",4)) {
					i += 4;
					ccf_pause = atoi (data + i);
					while (data[i] && data[i] != '[') i++;
				}
				if (!memcmp (data + i,"[RAW]",5)) {
					cmd_pnt[cmd_cnt].command_length = cmd_length;
					cmd_pnt[cmd_cnt].mode = RAW_DATA;
					i += 5;
					p = i;
					while (data[i] && data[i] != '[') i++;
					data[i++] = 0;
					cmd_pnt[cmd_cnt].ir_length = atoi (data+p);
					if (cmd_pnt[cmd_cnt].ir_length > maxlen) cmd_pnt[cmd_cnt].ir_length = maxlen;
					i += 5;
					p = i;
					while (data[i] && data[i] != '[') i++;
					data[i++] = 0;
					cmd_pnt[cmd_cnt].timing = atoi (data+p);
					while (data[i] != 'D') {
						while (data[i] && data[i] != '[') i++;
						i++;
					}
					i += 2;
					p = i;
					i = 0;
					while (i < cmd_pnt[cmd_cnt].ir_length) {
						cmd_pnt[cmd_cnt].data[i] = atoi (data + p) / 8;
						while (data[p] && data[p] != ' ') p++;
						p++;
						i++;
					}
					cmd_cnt++;
				}
				else if (!memcmp (data + i,"[MACRO]",7)) {
					m_pnt = (IRMACRO *)&cmd_pnt[cmd_cnt];
					cmd_pnt[cmd_cnt].mode = MACRO_DATA;

					i += 7;
					p = i;
					while (data[i] && data[i] != '[') i++;
					data[i++] = 0;
					m_pnt->macro_len = atoi (data+p);
					m_pnt->macro_num = mac_cnt;

					mac_pnt = realloc (mac_pnt,(mac_cnt + m_pnt->macro_len) * sizeof (MACROCOMMAND));
					memset (mac_pnt[mac_cnt].mac_remote,0,m_pnt->macro_len * sizeof (MACROCOMMAND));
					for (n=0;n < m_pnt->macro_len && !strncmp (data+i,"IR]",3);n++) {
						i += 3;
						if (data[i++] != '[') break;
						p = i;
						while (data[i] && data[i] != ']') i++;
						if (data[i] != ']') break;
						data[i++] = 0;
						strcpy (mac_pnt[mac_cnt + n].mac_remote,data + p);
						if (data[i++] != '[') break;
						p = i;
						while (data[i] && data[i] != ']') i++;
						if (data[i] != ']') break;
						data[i++] = 0;
						strcpy (mac_pnt[mac_cnt + n].mac_command,data + p);
						if (data[i++] != '[') break;
						p = i;
						while (data[i] && data[i] != ']') i++;
						if (data[i] != ']') break;
						data[i++] = 0;
						mac_pnt[mac_cnt + n].pause = atoi (data + p);
						i++;

					}
					m_pnt->macro_len = n;
					mac_cnt += n;
					cmd_cnt++;

				}
				else if (!memcmp (data + i,"[CCF]",5)) {
					cmd_pnt[cmd_cnt].command_length = cmd_length;
					res = DBStoreCCFCode (data + i + 5);
					if (!res) {
						sprintf (st,"**** CCF Error: %s [Remote: %s]\n",cmd_pnt[cmd_cnt].name,remote);
						log_print (st,LOG_ERROR);
					}
					else cmd_cnt += res;
				}
				else if (!strncmp (data + i,"[LINK]",6) || !strncmp (data + i,"[link]",6)) {
					link_cnt++;
					cmd_pnt[cmd_cnt].timing = LINK_IRCOMMAND;
					cmd_pnt[cmd_cnt].remote = rem_cnt;
					i += 6;
					j = i;
					while (data[j] && data[j] != ',') j++;
					if (!data[j] || (j - i) > 80) {
						sprintf (st,"**** Link Error: %s [Remote: %s]\n",cmd_pnt[cmd_cnt].name,remote);
						log_print (st,LOG_ERROR);
					}
					else {
						data[j++] = 0;
						strcpy (cmd_pnt[cmd_cnt].data,data + i);
						strncpy (cmd_pnt[cmd_cnt].data_cal,data + j,20);
						ConvertLcase (cmd_pnt[cmd_cnt].data,(int)strlen (cmd_pnt[cmd_cnt].data));
						ConvertLcase (cmd_pnt[cmd_cnt].data_cal,(int)strlen (cmd_pnt[cmd_cnt].data_cal));
						cmd_cnt++;
					}
				}
				else if (!memcmp (data + i,"[RS232]",7)) {
					cmd_pnt[cmd_cnt].command_length = cmd_length;
					cmd_pnt[cmd_cnt].timing = RS232_IRCOMMAND;
					cmd_pnt[cmd_cnt].mode = RAW_DATA;
					cmd_pnt[cmd_cnt].pause = 0;						// Hier RS232 Parameter
					i += 7;
					j = 0;
					while (data[i] >= 32) {
						if (data[i] == '\\') {
							i++;
							if (data[i] == '\\') cmd_pnt[cmd_cnt].data[j++] = '\\';
							else if (data[i] == 'r') cmd_pnt[cmd_cnt].data[j++] = 13;
							else if (data[i] == 'n') cmd_pnt[cmd_cnt].data[j++] = 10;
							else if (data[i] >= '0' && data[i] <= '9' && data[i+1] >= '0' && data[i+1] <= '9' && data[i+2] >= '0' && data[i+2] <= '9') {
								p = data[i+3];
								data[i+3] = 0;
								cmd_pnt[cmd_cnt].data[j++] = (byte)atoi (data+i);
								data[i+3] = p;
								i += 2;
							}
							else if ((data[i] == 'x' || data[i] == 'X') && 
									 ((data[i+1] >= '0' && data[i+1] <= '9') || (data[i+1] >= 'A' && data[i+1] <= 'F') || (data[i+1] >= 'a' && data[i+1] <= 'f')) && 
									 ((data[i+2] >= '0' && data[i+2] <= '9') || (data[i+2] >= 'A' && data[i+2] <= 'F') || (data[i+2] >= 'a' && data[i+2] <= 'f'))) {
								p = data[i+3];
								data[i+3] = 0;
								cmd_pnt[cmd_cnt].data[j++] = (byte)strtol (data + i + 1,NULL,16);
								data[i+3] = p;
								i += 2;
							}
						}
						else cmd_pnt[cmd_cnt].data[j++] = data[i];
						i++;
					}
					if (j > 250) j = 250;
					cmd_pnt[cmd_cnt].ir_length = j;
					cmd_cnt++;
				}
				else {
					cmd_pnt[cmd_cnt].pause = ccf_pause;
					cmd_pnt[cmd_cnt].command_length = cmd_length;
					cmd_pnt[cmd_cnt].timing = atoi (data + i + FindLineSection (data+i,"T"));
					ntim = rem_pnt[rem_cnt].timing_start + cmd_pnt[cmd_cnt].timing;

					if (ntim >= 0 && ntim < rem_pnt[rem_cnt].timing_end) {
						
						j = FindLineSection (data+i,"STATE");

						tim_pnt[ntim].link_count++;
						strcpy (cmd_pnt[cmd_cnt].data,data + i + FindLineSection (data+i,"D"));
						ReadCalibrateData (cmd_pnt[cmd_cnt].data,cmd_pnt[cmd_cnt].data_cal);
						if (tim_pnt[rem_pnt[cmd_pnt[cmd_cnt].remote].timing_start + cmd_pnt[cmd_cnt].timing].mode == IRDA_DATA) ConvertIRDARAW (cmd_pnt[cmd_cnt].data);
						cmd_pnt[cmd_cnt].ir_length = (word)strlen (cmd_pnt[cmd_cnt].data);
						cmd_pnt[cmd_cnt].ir_length_cal = (word)strlen (cmd_pnt[cmd_cnt].data_cal);

						if (j >= 0) CopyStateInfo (data + i + j,cmd_cnt);
						CopyToggleData (ntim,cmd_cnt);
						if (tim_pnt[ntim].repeat_offset > 0) CopyRepeatOffset (tim_pnt[ntim].repeat_offset,cmd_cnt);
						
						cmd_pnt[cmd_cnt].mode = 0;
						if ((tim_pnt[cmd_pnt[cmd_cnt].timing + rem_pnt[rem_cnt].timing_start].time_cnt <= TIME_LEN_18) ||
							(tim_pnt[cmd_pnt[cmd_cnt].timing + rem_pnt[rem_cnt].timing_start].mode == PULSE_200)) cmd_cnt++;
					}
					else {
						sprintf (st,"**** Illegal Timing: %d [Remote: %s]\n",ntim - rem_pnt[rem_cnt].timing_start,remote);
						log_print (st,LOG_ERROR);
					}
				}

			}

		}
	}

	rem_pnt[rem_cnt].command_end = cmd_cnt;
	return (0);

}

void CopyRepeatOffset (byte repeat_offset,int cmd)
{
	byte old_offset = cmd_pnt[cmd].offset_val;

	if (old_offset) old_offset--;										// Gesamtoffset ist nur 1x vorhanden

	cmd_pnt[cmd].data_offset[0] = old_offset + 4;						// Gesamtoffsetlänge

	cmd_pnt[cmd].data_offset[old_offset + 1] = 3;						// Offset für Toggle
	cmd_pnt[cmd].data_offset[old_offset + 2] = OFFSET_TYP_REPEAT;		// Typ = Repeat Offset
	cmd_pnt[cmd].data_offset[old_offset + 3] = repeat_offset;			// Repeat Offset

	memcpy (cmd_pnt[cmd].data_offset + 4 + old_offset,cmd_pnt[cmd].data_cal,cmd_pnt[cmd].ir_length_cal);
	cmd_pnt[cmd].ir_length_offset = cmd_pnt[cmd].ir_length_cal + 4 + old_offset;
	cmd_pnt[cmd].offset_val = 4 + old_offset;
}

void CopyStateInfo (byte *state,int cmd)
{
	byte sval = 0;

	if (!memcmp (state,"On",2) || !memcmp (state,"ON",2) || !memcmp (state,"on",2) || *state == '1') sval = 1;
	else if (!memcmp (state,"Off",3) || !memcmp (state,"OFF",3) || !memcmp (state,"off",3) || *state == '0') sval = 0;

	cmd_pnt[cmd].data_offset[0] = 3;							// Gesamtoffsetlänge
	cmd_pnt[cmd].data_offset[1] = 2;							// Offset für State
	cmd_pnt[cmd].data_offset[2] = OFFSET_TYP_STATE_0 + sval;	// Typ = Toggle

	memcpy (cmd_pnt[cmd].data_offset + 3,cmd_pnt[cmd].data_cal,cmd_pnt[cmd].ir_length_cal);
	cmd_pnt[cmd].ir_length_offset = cmd_pnt[cmd].ir_length_cal + 3;
	cmd_pnt[cmd].offset_val = 3;
}

byte CopyToggleData (int tim,int cmd)
{
	byte num = 0;
	byte offset = 0;
	byte old_offset = cmd_pnt[cmd].offset_val;

	if (tim_pnt[tim].toggle_val[0][0]) {
		offset += 6;
		num++;
	}
	if (tim_pnt[tim].toggle_val[1][0]) {
		offset += 2;
		num++;
	}
	if (tim_pnt[tim].toggle_val[2][0]) {
		offset += 2;
		num++;
	}
	if (tim_pnt[tim].toggle_val[3][0]) {
		offset += 2;
		num++;
	}

	if (num) {
		if (old_offset) old_offset--;										// Gesamtoffset ist nur 1x vorhanden
	
		cmd_pnt[cmd].data_offset[0] = offset + old_offset;					// Gesamtoffsetlänge

		cmd_pnt[cmd].data_offset[old_offset + 1] = offset - 1;				// Offset für Toggle
		cmd_pnt[cmd].data_offset[old_offset + 2] = OFFSET_TYP_TOGGLE;		// Typ = Toggle
		cmd_pnt[cmd].data_offset[old_offset + 3] = (num << 4) | tim_pnt[tim].toggle_num;
		cmd_pnt[cmd].data_offset[old_offset + 4] = tim_pnt[tim].toggle_pos[0];
		cmd_pnt[cmd].data_offset[old_offset + 5] = ((tim_pnt[tim].toggle_val[0][1] & 15) << 4) | (tim_pnt[tim].toggle_val[0][0] & 15);
		if (tim_pnt[tim].toggle_val[1][0]) {
			cmd_pnt[cmd].data_offset[old_offset + 6] = tim_pnt[tim].toggle_pos[1];
			cmd_pnt[cmd].data_offset[old_offset + 7] = ((tim_pnt[tim].toggle_val[1][1] & 15) << 4) | (tim_pnt[tim].toggle_val[1][0] & 15);
		}
		if (tim_pnt[tim].toggle_val[2][0]) {
			cmd_pnt[cmd].data_offset[old_offset + 8] = tim_pnt[tim].toggle_pos[2];
			cmd_pnt[cmd].data_offset[old_offset + 9] = ((tim_pnt[tim].toggle_val[2][1] & 15) << 4) | (tim_pnt[tim].toggle_val[2][0] & 15);
		}
		if (tim_pnt[tim].toggle_val[3][0]) {
			cmd_pnt[cmd].data_offset[old_offset + 10] = tim_pnt[tim].toggle_pos[3];
			cmd_pnt[cmd].data_offset[old_offset + 11] = ((tim_pnt[tim].toggle_val[3][1] & 15) << 4) | (tim_pnt[tim].toggle_val[3][0] & 15);
		}
	}

	memcpy (cmd_pnt[cmd].data_offset + offset + old_offset,cmd_pnt[cmd].data_cal,cmd_pnt[cmd].ir_length_cal);
	cmd_pnt[cmd].ir_length_offset = cmd_pnt[cmd].ir_length_cal + offset + old_offset;
	cmd_pnt[cmd].offset_val = offset + old_offset;

	if (num) return (1);
	return 0;
}

void ReadCalibrateData (byte *pnt,byte *pntcal)
{
	int j = 0,i = 0,val;

	while (pnt[i]) {
		if (pnt[i] == '#') break;
		i++;
	}
	if (!pnt[i]) {
		strcpy (pntcal,pnt);
		return;
	}

	cal_cnt++;
	i = 0;
	while (pnt[i]) {
		if (pnt[i] == '#') {
			i++;
			pnt[i+4] = 0;
			val = atoi(pnt+i) / 8;
			pntcal[j] = (byte)(abs (val)) | 128;
			if (val < 0) pntcal[j] |= 64;
			j++;
			i += 5;
		}
		else pntcal[j++] = pnt[i++];
	}
	pntcal[j] = 0;

	i = j = 0;
	while (pntcal[i]) {
		if (pntcal[i] & 128) i++;
		else pnt[j++] = pntcal[i++];
	}
	pnt[j] = 0;
}

void ConvertIRDARAW (char data[])
{
	int i,j;
	char tar[255];

	i = j = 0;


	tar[j++] = '0';
	
	for (i=0;data[i] && j < CODE_LEN;i++) {
		tar[j++] = data[i];

		if (!((i+1) % 8)) {
			tar[j++] = '1';
			if (data[i+1]) tar[j++] = '0';
		}

	}
	tar[j] = 0;
	strcpy (data,tar);
}

int DBStoreCCFCode (char cd[])
{
	int res;
	IRDATA ird;
	IRRAW *irr;
	
	res = DecodeCCF (cd,&ird,START);

	if (res <= 0) {
		ccf_err++;
		return (0);
	}

	if (res & 4) {
		ccf_raw++;
		irr = (IRRAW *)&ird;

		cmd_pnt[cmd_cnt].mode = irr->mode;

		cmd_pnt[cmd_cnt].ir_length = irr->ir_length;
		cmd_pnt[cmd_cnt].timing = irr->transmit_freq;

		memcpy (cmd_pnt[cmd_cnt].data,irr->data,irr->ir_length);

		if (res & 1) {
			cmd_cnt++;

			cmd_pnt = realloc (cmd_pnt,(cmd_cnt + 1) * sizeof (IRCOMMAND));
			memset (cmd_pnt + cmd_cnt,0,sizeof (IRCOMMAND));
			cmd_pnt[cmd_cnt].toggle_seq = 0;
			memcpy (cmd_pnt[cmd_cnt].name,cmd_pnt[cmd_cnt-1].name,strlen (cmd_pnt[cmd_cnt-1].name));
			strcat (cmd_pnt[cmd_cnt].name,"@");

			DecodeCCF (cd,&ird,REPEAT);
			cmd_pnt[cmd_cnt].mode = irr->mode;

			cmd_pnt[cmd_cnt].ir_length = irr->ir_length;
			cmd_pnt[cmd_cnt].timing = irr->transmit_freq;

			memcpy (cmd_pnt[cmd_cnt].data,irr->data,irr->ir_length);
			cmd_pnt[cmd_cnt].remote = rem_cnt;
		}

		return (1);
	}

	if (res & 2) {
		ccf_data++;
		
		tim_pnt = realloc (tim_pnt,(tim_cnt + 1) * sizeof (IRTIMING));
		memset (&tim_pnt[tim_cnt],0,sizeof (IRTIMING));

		memcpy (&tim_pnt[tim_cnt].ir_length,&ird.ir_length,3);
		memcpy (&tim_pnt[tim_cnt].pause_len,&ird.pause_len,TIME_LEN * 2);
		memcpy (&tim_pnt[tim_cnt].pulse_len,&ird.pulse_len,TIME_LEN * 2);
		memcpy (&tim_pnt[tim_cnt].time_cnt,&ird.time_cnt,3);
		tim_pnt[tim_cnt].link_count++;

		cmd_pnt[cmd_cnt].mode = 0;

		cmd_pnt[cmd_cnt].ir_length = ird.ir_length;
		cmd_pnt[cmd_cnt].timing = tim_cnt - rem_pnt[rem_cnt].timing_start;
		cmd_pnt[cmd_cnt].remote = rem_cnt;

		memcpy (cmd_pnt[cmd_cnt].data,ird.data,ird.ir_length);

		memcpy (cmd_pnt[cmd_cnt].data_cal,ird.data,ird.ir_length);	// Später ggf. auch Calibrate für CCF
		memcpy (cmd_pnt[cmd_cnt].data_offset,ird.data,ird.ir_length);
		cmd_pnt[cmd_cnt].ir_length_cal = cmd_pnt[cmd_cnt].ir_length_offset = ird.ir_length;

		tim_cnt++;
		if (res & 1) {
			cmd_cnt++;

			cmd_pnt = realloc (cmd_pnt,(cmd_cnt + 1) * sizeof (IRCOMMAND));
			memset (cmd_pnt + cmd_cnt,0,sizeof (IRCOMMAND));
			cmd_pnt[cmd_cnt].toggle_seq = 0;
			memcpy (cmd_pnt[cmd_cnt].name,cmd_pnt[cmd_cnt-1].name,strlen (cmd_pnt[cmd_cnt-1].name));
			strcat (cmd_pnt[cmd_cnt].name,"@");

			DecodeCCF (cd,&ird,REPEAT);
			tim_pnt = realloc (tim_pnt,(tim_cnt + 1) * sizeof (IRTIMING));
			memset (&tim_pnt[tim_cnt],0,sizeof (IRTIMING));

			memcpy (&tim_pnt[tim_cnt].ir_length,&ird.ir_length,3);
			memcpy (&tim_pnt[tim_cnt].pause_len,&ird.pause_len,TIME_LEN * 2);
			memcpy (&tim_pnt[tim_cnt].pulse_len,&ird.pulse_len,TIME_LEN * 2);
			memcpy (&tim_pnt[tim_cnt].time_cnt,&ird.time_cnt,3);
			tim_pnt[tim_cnt].link_count++;

			cmd_pnt[cmd_cnt].mode = 0;

			cmd_pnt[cmd_cnt].ir_length = ird.ir_length;
			cmd_pnt[cmd_cnt].timing = tim_cnt - rem_pnt[rem_cnt].timing_start;
			cmd_pnt[cmd_cnt].remote = rem_cnt;

			memcpy (cmd_pnt[cmd_cnt].data,ird.data,ird.ir_length);

			memcpy (cmd_pnt[cmd_cnt].data_cal,ird.data,ird.ir_length);	// Später ggf. auch Calibrate für CCF
			memcpy (cmd_pnt[cmd_cnt].data_offset,ird.data,ird.ir_length);
			cmd_pnt[cmd_cnt].ir_length_cal = cmd_pnt[cmd_cnt].ir_length_offset = ird.ir_length;

			tim_cnt++;
		}


		rem_pnt[rem_cnt].timing_end = tim_cnt;
		return (1);
	}

	return (0);
}


int DBStoreTimings (FILE *fp,char remote[])
{
	int i,tf = 0;
	char st[100];
	char ln[256],*data;

	
	rem_pnt[rem_cnt].timing_start = tim_cnt;
	rem_pnt[rem_cnt].timing_end = tim_cnt;

	rewind (fp);
	data = DBFindSection (fp,"TIMING",NULL,NULL,NULL);

	if (!data) return (-1);

	i = 0;
	while (data) {
		sprintf (st,"%d",i);
		data = DBFindSection (fp,st,ln,"[COMMANDS]",NULL);
		if (data) {
			tim_pnt = realloc (tim_pnt,(tim_cnt + 1) * sizeof (IRTIMING));
			tf |= StoreIRTiming (tim_pnt + tim_cnt,ln,toggle_cnt);
			tim_cnt++;
		}
		i++;
	}
	
	if (tf) {
		toggle_cnt++;

		if (toggle_cnt > 8) {
			sprintf (st,"Too many toggle bits [Remote: %s]\n",remote);
			log_print (st,LOG_ERROR);
			toggle_cnt = 8;
		}
	}
		
	rem_pnt[rem_cnt].timing_end = tim_cnt;

	return (0);
}


int DBStoreRemote (FILE *fp,char newremote[])
{
	char name[100],*data,tra[100];

	newremote[0] = 0;

	rem_pnt = realloc (rem_pnt,(rem_cnt + 1) * sizeof (IRREMOTE));

	data = DBFindSection (fp,"REMOTE",NULL,NULL,NULL);

	if (!data) return (-1);

	data = DBFindSection (fp,"NAME",name,"[TIMING]",NULL);

	if (!data) return (-1);


	memset (&rem_pnt[rem_cnt],0,sizeof (IRREMOTE));
	ConvertLcase (data,(int)strlen (data));
	strncpy (rem_pnt[rem_cnt].name,data,80);
	rem_pnt[rem_cnt].number = rem_cnt;
	GetRemoteAddressMask (rem_cnt);

	data = DBFindSection (fp,"GLOBAL-TOGGLE",NULL,NULL,NULL);
	if (data) rem_pnt[rem_cnt].toggle_pos = 1;

	rewind (fp);

	data = DBFindSection (fp,"TRANSMITTER",tra,"[TIMING]",NULL);
	if (data) {
		ConvertLcase (data,(int)strlen (data));
		if (!strcmp (data,"extern") || !strcmp (data,"external")) rem_pnt[rem_cnt].transmitter = EXTERNAL_LEDS;
		if (!strcmp (data,"intern") || !strcmp (data,"internal")) rem_pnt[rem_cnt].transmitter = INTERNAL_LEDS;
		if (!strcmp (data,"both") || !strcmp (data,"beide") || !strcmp (data,"all") || !strcmp (data,"alle")) rem_pnt[rem_cnt].transmitter = INTERNAL_LEDS | EXTERNAL_LEDS;
	}

	rewind (fp);

	data = DBFindSection (fp,"RCV-LEN",tra,"[TIMING]",NULL);
	if (data) {
		if (*data == '*') rem_pnt[rem_cnt].rcv_len = 255;
		else rem_pnt[rem_cnt].rcv_len = (byte)atoi (data);
	}
	else rem_pnt[rem_cnt].rcv_len = 0;

	data = DBFindSection (fp,"RCV-START",tra,"[TIMING]",NULL);
	if (data) rem_pnt[rem_cnt].rcv_start = (byte)atoi (data);
	else rem_pnt[rem_cnt].rcv_start = 0;

	rewind (fp);

	data = DBFindSection (fp,"INCLUDE",newremote,NULL,NULL);

	return (0);
}

void GetRemoteAddressMask (int num)
{
	int i = 0;

	rem_pnt[num].target_mask = 0xffff;
	rem_pnt[num].source_mask = 0xffff;

	while (i < send_routing_cnt) {
		if (!memcmp (rem_pnt[num].name,send_routing[i].name,80)) {
			rem_pnt[num].target_mask = send_routing[i].target_mask;
		}
		i++;
	}

	i = 0;
	while (i < recv_routing_cnt) {
		if (!memcmp (rem_pnt[num].name,recv_routing[i].name,80)) {
			rem_pnt[num].source_mask = recv_routing[i].target_mask;
		}
		i++;
	}
}

/* Toggle Format im Timing:

  Beispiel:
  [0][N]5[1]168 280[2]168 440[3]416 280[4]168 608[5]168 776[TOGGLE][2][01][RC]2[RP]87[FREQ]36

  [TOGGLE][2][01]:
  [2] = Position (0 basiert)
  [01] = Toggle Werte -> In das IR Command eingesetzt

  Mehrere Bits:
  [TOGGLE][2][01][3][01]:
  [2] = Position 1. Bit (0 basiert)
  [01] = Toggle Werte 1. Bit -> In das IR Command eingesetzt
  [3] = Position 2. Bit (0 basiert)
  [01] = Toggle Werte 2. Bit -> In das IR Command eingesetzt
  
  Die Werte werden in dieser Reihenfolge belegt (1. Bit zuerst, danach das 2.):

  xx00..........
  xx10..........
  xx01..........
  xx11..........

  Mehr als 2 Bit = 4 Werte werden hier wohl nicht vorkommen.

*/
int StoreIRTiming (IRTIMING *irp,char data[],int toggle)
{
	int i = 0,p,flag = 0,rp = 0,fl = 0,t,tf = 0;
	char cm[1000],par[1000];

	memset (irp,0,sizeof (IRTIMING));
	irp -> transmit_freq = 38;
	irp -> timecount_mode = 0;
	irp -> repeat_offset = 0;

	while (data[i]) {
		if (data[i] == '[') {
			i++;
			p = 0;
			while (data[i] && data[i] != ']') cm[p++] = data[i++];
			cm[p] = 0;
			p = i+1;
			while (data[i] && data[i] != '[') i++;
			strncpy (par,data+p,i-p);
			par[i-p] = 0;

			if (!strcmp (cm,"FREQ-MEAS")) irp ->carrier_measured = 1;
			if (!strcmp (cm,"RECS80")) irp -> mode = RECS80;
			if (!strcmp (cm,"RCMM")) irp -> mode = RCMM;
			if (!strcmp (cm,"RCMM-TOGGLE")) irp -> mode = RCMM_TOGGLE_MODE;
			if (!strcmp (cm,"DUTY_CYCLE_1:2")) irp -> timecount_mode |= TC_DUTY_CYCLE_2 | TC_ACTIVE;
			if (!strcmp (cm,"DUTY_CYCLE_1:4")) irp -> timecount_mode |= TC_DUTY_CYCLE_4 | TC_ACTIVE;
			if (!strcmp (cm,"DUTY_CYCLE_1:6")) irp -> timecount_mode |= TC_DUTY_CYCLE_6 | TC_ACTIVE;
			if (!strcmp (cm,"SEND_POWER_LOW")) irp -> timecount_mode |= TC_SEND_POWER_LOW | TC_ACTIVE;
			if (!strcmp (cm,"SEND_POWER_MED")) irp -> timecount_mode |= TC_SEND_POWER_MED | TC_ACTIVE;
			if (!strcmp (cm,"SEND_POWER_HI")) irp -> timecount_mode |= TC_SEND_POWER_HI | TC_ACTIVE;
			if (!strcmp (cm,"RECS80")) irp -> mode = RECS80;
			if (!strcmp (cm,"N")) irp -> time_cnt |= atoi (par);

			if (irp -> mode == PULSE_200) {
				if (*cm >= '1' && *cm <= TIME_LEN_SINGLE + 48) StoreSingleTimingPause (irp,cm,par);
				if (!strcmp (cm,"PULSE")) StoreSingleTimingPulse (irp,"1",par);
			}
			else {
				if (*cm >= '1' && *cm <= TIME_LEN_18 + 48) StorePulseTiming (irp,cm,par);
			}

			if (!strcmp (cm,"RC")) irp -> ir_repeat = atoi (par) | fl;
			
			if (!strcmp (cm,"FL")) {
				irp -> repeat_pause = (rp = atoi (par));
				fl = 128;
				irp -> ir_repeat |= 128;
			}

			if (!strcmp (cm,"RO")) irp -> repeat_offset = atoi (par);

			if (!strcmp (cm,"TOGGLE") && toggle >= 0) {
				t = 0;
				tf = 1;
				if (toggle > 7) toggle = 7;
				irp->toggle_num = toggle;

				while (data[i] && data[i] != '[') i++;
	
				while (data[i] && data[i+1] >= '0' && data[i+1] <= '9' && t < 4) {
					i++;
					irp->toggle_pos[t] = atoi (data+i);
					while (data[i] && data[i] != '[') i++;
					i++;
					if (irp->mode & START_BIT) {
						if (data[i] == 'S') data[i] = '0';
						else data[i]++;
					}
					if (data[i] >= 'A') data[i] -= 55;
					irp->toggle_val[t][0] = data[i++];
					if (irp->mode & START_BIT) {
						if (data[i] == 'S') data[i] = '0';
						else data[i]++;
					}
					if (data[i] >= 'A') data[i] -= 55;
					irp->toggle_val[t][1] = data[i++];
					while (data[i] && data[i] != '[') i++;
					t++;
				}
			}
			
			if (!rp && !strcmp (cm,"RP")) irp -> repeat_pause = (rp = atoi (par));
			if (!strcmp (cm,"SB")) irp -> mode |= START_BIT;
			if (!strcmp (cm,"RS")) irp -> mode |= REPEAT_START;
			if (!strcmp (cm,"NOTOG")) flag = 1;
			if (!strcmp (cm,"NOTOG1")) flag = 2;
			if (!strcmp (cm,"FREQ")) {
				if (atoi (par) > 127) {
					if (atoi (par) > 500) irp -> transmit_freq = 255;
					else irp -> transmit_freq = (atoi (par) / 4) | 128;
				}
				else irp -> transmit_freq = atoi (par);
			}
			if (!strcmp (cm,"RC5")) irp -> mode |= RC5_DATA;
			if (!strcmp (cm,"RC6")) irp -> mode |= RC6_DATA;
			if (!strcmp (cm,"IRDA-RAW")) {
				irp -> mode = IRDA_DATA | START_BIT;
				irp -> transmit_freq = 0;
				irp -> repeat_pause = rp / 100;
			}
			if (!strcmp (cm,"IRDA")) {
				irp -> mode = IRDA_DATA;
				irp -> transmit_freq = 0;
				irp -> repeat_pause = rp / 100;
			}
		}
		else i++;
	}

	if (irp -> time_cnt > TIME_LEN) irp -> mode |= TIMECOUNT_18;

	if (irp -> mode == RECS80) irp->mode |= flag;
	else {
		if (!((irp ->mode & IRDA_DATA) == IRDA_DATA) && !(irp -> mode == PULSE_200) && irp -> mode & (RC5_DATA | RC6_DATA)) {
			irp -> mode &= ~START_MASK;
			if (flag) irp -> mode |= NO_TOGGLE;
			if (flag == 2) irp -> mode |= NO_TOGGLE_H;
		}
	}

	return (tf);
}

void StorePulseTiming (IRTIMING *irp,char cmd[],char data[])
{
	int i = 0;

	while (data[i] && data[i] >= '0') i++;
	
	if (!data[i]) return;
	data[i++] = 0;

	irp ->pulse_len[atoi (cmd)-1] = atoi (data) / 8;
	irp ->pause_len[atoi (cmd)-1] = atoi (data+i) / 8;
}

void StoreSingleTimingPause (IRTIMING *irp,char cmd[],char data[])
{
	irp ->pause_len[atoi (cmd)-1] = atoi (data) / 8;
}


void StoreSingleTimingPulse (IRTIMING *irp,char cmd[],char data[])
{
	irp ->pulse_len[atoi (cmd)-1] = atoi (data) / 8;
}


int FindLineSection (char ln[],char section[])
{
	unsigned int pnt,len;
	char cmp[256];

	sprintf (cmp,"[%s]",section);

	len = (int)strlen (cmp);

	pnt = 0;

	while (len + pnt < strlen (ln)) {
		if (!memcmp (ln+pnt,cmp,len)) return (pnt+len);
		pnt++;
	}

	return (-1);
}



FILE *DBOpenFile (char remote[],char mode[])
{
	char nm[255];

	sprintf (nm,"%s%c%s",dbpath,PATH_SEPARATOR,remote);

	return (fopen (nm,mode));
}


char *DBFindSection (FILE *fp,char section[],char data[],char end[],int *fpos)
{
	int len;
	static char ln[2048];
	char *pnt;
	char cmp[256];

	sprintf (cmp,"[%s]",section);
	len = (int)strlen (cmp);

	pnt = DBReadString (ln,fp,fpos);
	while (pnt) {
		if (end && !strcmp (ln,end)) return (0);
		if (!strncmp (pnt,cmp,len)) {
			if (data) strcpy (data,pnt+len);
			return (pnt+len);
		}
		pnt = DBReadString (ln,fp,fpos);
		}
	return (0);
}


char *DBReadString (char ln[],FILE *fp,int *fpos)
{
	int i;
	char *pnt;

	do {
		if (fpos) *fpos = ftell (fp);
		pnt = fgets (ln,2048,fp);
		if (!pnt) return (NULL);
		while (*pnt == ' ' || *pnt == '\t') pnt++;

		i = (int)strlen (pnt) - 1;

		while (i && pnt[i-1] && ((byte)pnt[i-1]) <= ' ') i--;

		if (((byte)pnt[i]) <= ' ') pnt[i] = 0;
	} while (*pnt == 0);

	return (pnt);
}

void ConvertLcase (char *pnt,int len)
{
	int i = 0;
	while (i < len) {
		pnt[i] = tolower (pnt[i]);
		i++;
	}
}

