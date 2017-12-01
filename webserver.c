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

#ifdef LINUX
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
extern int hCom;

#endif


#include <stdio.h>
#include <time.h>


#include "pictures.h"
#include "remote.h"
#include "network.h"
#include "errcode.h"
#include "dbstruct.h"
#include "global.h"
#include "server.h"
#include "lowlevel.h"
#include "fileio.h"
#include "webserver.h"


#ifdef WIN32
extern WSAEVENT WebEvent;
#endif



void ProcessWebRequest (SOCKET sock)
{
	int res;
	char data[2048];
	SOCKET call;
	int clilen;
	unsigned int adr;
	struct sockaddr_in cli_addr;

	clilen = sizeof (cli_addr);

	call = accept (sock,(struct sockaddr *)&cli_addr,&clilen);

#ifdef WIN32
	adr = ntohl (cli_addr.sin_addr.S_un.S_addr);
#else
	adr = ntohl (cli_addr.sin_addr.s_addr);
#endif
	if (adr != 0x7f000001) {
		for (res=0;res < netcount;res++) {
			if ((netip[res] & netmask[res]) == (adr & netmask[res])) break;
		}
		if (netcount && res == netcount) {
			sprintf (data,"Error: IP Address %s not allowed (Access rights).\n",inet_ntoa (cli_addr.sin_addr));
			log_print (data,LOG_ERROR);
			shutdown (call,2);
			closesocket (call);
			return;
		}
	}

#ifdef WIN32
	res = 0;
	WSAEventSelect (call,WebEvent,0);
	ioctlsocket (call,FIONBIO,&res);
#endif

	res = recv (call,data,2048,MSG_NOSIGNAL);

	data[res] = 0;

	ParseRequest (data,call);

}


void ParseRequest (char* data,SOCKET sock)
{
	char mem[65536];
	int len;

	if (!strncmp (data,"GET ",4)) {
		len = GetHtmlPage (mem,data + 4);
		send (sock,mem,len,0);
		closesocket (sock);
	}
}


int GetHtmlPage (char *mem,char *page)
{
	int i = 0;
	char content[65536];
	char parm[2050];
	char value[2050];
	char lang[10];

	while (page[i] > ' ') i++;
	page[i] = 0;

	*parm = 0;
	for (i=0;page[i] && page[i] != '?';i++);
	if (page[i] == '?') {
		page[i++] = 0;
		page[i + 2048] = 0;
		strcpy (parm,page + i);
	}
	
	GetHtmlParameter (parm,"lang",value);
	value[8] = 0;
	strcpy (lang,value);

	if (*lang == 0) strcpy (lang,"EN");

	printf ("%s\n",page);

	if (!strcmp (page,"/") || !strcmp (page,"/index.htm") || !strcmp (page,"/index.html")) {
		GetIndexPage (content,lang);
		GenerateHtmlHeader (mem,content);
		return ((int)strlen (mem));
	}
	if (!strcmp (page,"/DeviceList.htm")) {
		GetDevicesPage (content,lang,NULL);
		GenerateHtmlHeader (mem,content);
		return ((int)strlen (mem));
	}
	if (!strcmp (page,"/DeviceConfig.htm")) {
		GetHtmlParameter (parm,"id",value);
		i = atoi (value);
		if (i < 0 || i > 15) i = 0;
		GetDeviceConfigPage (content,lang,i);
		GenerateHtmlHeader (mem,content);
		return ((int)strlen (mem));
	}
	if (!strcmp (page,"/SetConfig.htm")) {
		GetDevicesPage (content,lang,parm);
		GenerateHtmlHeader (mem,content);
		return ((int)strlen (mem));
	}

	if (!strcmp (page,"/Send.htm")) {
		GetSendPage (content,lang,parm);
		GenerateHtmlHeader (mem,content);
		return ((int)strlen (mem));
	}

	if (!strcmp (page,"/IRTransLogo.gif")) return (GenerateImageHeader (mem,irtrans_logo,"gif",sizeof (irtrans_logo)));
	if (!strcmp (page,"/OKButton.gif")) return (GenerateImageHeader (mem,ok_button,"gif",sizeof (ok_button)));
	if (!strcmp (page,"/GoButton.gif")) return (GenerateImageHeader (mem,go_button,"gif",sizeof (go_button)));
	if (!strcmp (page,"/English.jpg")) return (GenerateImageHeader (mem,england_logo,"jpeg",sizeof (england_logo)));
	if (!strcmp (page,"/Deutsch.jpg")) return (GenerateImageHeader (mem,deutschland_logo,"jpeg",sizeof (deutschland_logo)));
	if (!strcmp (page,"/BackButton.gif")) return (GenerateImageHeader (mem,back_button,"gif",sizeof (back_button)));

	GenerateErrorPage (mem,404);
	return ((int)strlen (mem));
}



void GetHtmlParameter (char *pnt,char *name,char *value)
{
	char parm[2050];
	int i = 0,j;
	*value = 0;
	strcpy (parm,pnt);
	while (parm[i]) {
		j = i;
		while (parm[i] && parm[i] != '=' && parm[i] != '?' && parm[i] != '&') i++;
		if (!parm[i]) return;
		if (parm[i] == '=') {
			parm[i++] = 0;
			if (!strcmp (name,parm+j)) {
				j = i;
				while (parm[i] && parm[i] != '?' && parm[i] != '&') i++;
				parm[i] = 0;
				strcpy (value,parm+j);
			}
			while (parm[i] && parm[i] != '?' && parm[i] != '&') i++;
		}
		if (parm[i] == '?' || parm[i] == '&') i++;
	}
}


void GetSendPage (char *mem,char *lang,char *parm)
{
	int res,mask,setres = 0;
	int cmd_num;
	char err[1000];
	char value[1000];
	char ln[1000];
	char remote[100],command[100];
	STATUSBUFFER stat;
	NETWORKSTATUS *ns;

	strcpy (mem,"<HTML><HEAD><TITLE>IRTrans Send Command</TITLE></HEAD><BODY bgcolor=\"#C0C0C0\">");
	strcat (mem,"<table width=\"80%\"><tr><td align = \"center\">");
	strcat (mem,"<IMG src=\"IRTransLogo.gif\" align = \"center\">");
	strcat (mem,"</td><td width=\"40\">&nbsp</td><td align = \"center\"><font face=\"Verdana\" size=\"4\">IRTrans Send Command</font></td><td>");
	sprintf (ln,"<a href=\"index.htm?lang=%s\">",lang);
	strcat (mem,ln);
	strcat (mem,"<IMG src=\"BackButton.gif\" align = \"center\" border=\"0\"></a></td></tr></table><hr><br>");

	if (parm) {
		mask = 0;
		GetHtmlParameter (parm,"remote",remote);
		GetHtmlParameter (parm,"command",command);
		GetHtmlParameter (parm,"sendmask",value);
		sscanf (value,"%x",&mask);
		if (*remote && *command) {
			res = DBFindRemoteCommand (remote,command,&cmd_num,NULL);
			if (res) {
				ns = (NETWORKSTATUS *)&stat;
				PutNetworkStatus (res,NULL,&stat);
				strcpy (err,ns->message);
				if (res == ERR_REMOTENOTFOUND) sprintf (ns->message,err,remote);
				if (res == ERR_COMMANDNOTFOUND) sprintf (ns->message,err,command);
				strcat (mem,"<p align=\"center\"><font face=\"Verdana\" size=\"4\">");
				strcat (mem,ns->message);
				strcat (mem,"</p></font>");
				}
			else {
				if (mask) mask = ((word)mask & 0xffff) | 0x10000;
				SendIR (cmd_num,mask,COMMAND_SEND,NULL);
			}
			resend_flag = 0;
			return;
		}

	}
}

void GetDeviceConfigPage (char *mem,char *lang,int id)
{
	int res,i;
	char ln[1000];
	STATUSBUFFER buf;
	NETWORKMODE *status;


	strcpy (mem,"<HTML><HEAD><TITLE>IRTrans Device Status</TITLE></HEAD><BODY bgcolor=\"#C0C0C0\">");
	strcat (mem,"<table width=\"80%\"><tr><td align = \"center\">");
	strcat (mem,"<IMG src=\"IRTransLogo.gif\" align = \"center\">");
	strcat (mem,"</td><td width=\"40\">&nbsp</td><td align = \"center\"><font face=\"Verdana\" size=\"4\">IRTrans Device Configuration</font></td><td>");
	sprintf (ln,"<a href=\"DeviceList.htm?lang=%s\">",lang);
	strcat (mem,ln);
	strcat (mem,"<IMG src=\"BackButton.gif\" align = \"center\" border=\"0\"></a></td></tr></table><hr><br>");

	res = GetDeviceStatus (&buf);
	if (res) {
	}
	else {
		status = (NETWORKMODE *)&buf;

		
		strcat (mem,"<font face=\"Verdana\" size=\"3\"><table><tr><td width=\"20\">&nbsp;</td><td>");
		sprintf (ln,"Configuring IRTrans Device ID %d:</td></tr>",id);
		if (!strcmp (lang,"DE")) sprintf (ln,"IRTrans mit ID %d wird konfiguriert:</td></tr>",id);
		strcat (mem,ln);
		strcat (mem,"</table>");
		strcat (mem,"<font face=\"Verdana\" size=\"3\"><form method=\"GET\" action=\"SetConfig.htm\"><table><tr><td width=\"20\">&nbsp;</td>");
		strcat (mem,"<td>Target Mask</td></tr><tr><td width=\"20\">&nbsp;</td><td>");
		sprintf (ln,"<input type=\"hidden\" value=\"%d\" name=\"id\">",id);
		strcat (mem,ln);
		for (i=0; i<=15; i++) {
			if (status->stat[id].send_mask & (1 << i)) sprintf (ln,"<input type=\"checkbox\" name=\"TM%02d\" value=\"%d\" checked>%d",i,1<<i,i);
			else sprintf (ln,"<input type=\"checkbox\" name=\"TM%02d\" value=\"%d\">%d",i,1<<i,i);
			strcat (mem,ln);
		}
		strcat (mem,"</td></tr><tr><td>&nbsp;</td></tr>");

		strcat (mem,"<tr><td width=\"20\">&nbsp;</td><td><table width=\"100%\"><tr><td>");
		sprintf (ln,"<input type=\"checkbox\" name=\"DEVMODE_SEND\" value=\"%d\" %s>IR Send",DEVMODE_SEND,(status->stat[id].device_mode & DEVMODE_SEND ? "checked":""));
		if (!strcmp (lang,"DE")) sprintf (ln,"<input type=\"checkbox\" name=\"SEND\" value=\"%d\" %s>IR Senden",DEVMODE_SEND,(status->stat[id].device_mode & DEVMODE_SEND ? "checked":""));
		strcat (mem,ln);
		strcat (mem,"</td>");
		strcat (mem,"<td>");
		sprintf (ln,"<input type=\"checkbox\" name=\"DEVMODE_IR\" value=\"%d\" %s>IR Receive",DEVMODE_IR,(status->stat[id].device_mode & DEVMODE_IR ? "checked":""));
		if (!strcmp (lang,"DE")) sprintf (ln,"<input type=\"checkbox\" name=\"RECEIVE\" value=\"%d\" %s>IR Empfangen",DEVMODE_IR,(status->stat[id].device_mode & DEVMODE_IR ? "checked":""));
		strcat (mem,ln);
		strcat (mem,"</td></tr>");

		strcat (mem,"<tr><td>");
		sprintf (ln,"<input type=\"checkbox\" name=\"DEVMODE_REPEAT\" value=\"%d\" %s>Repeat Mode",DEVMODE_REPEAT,(status->stat[id].device_mode & DEVMODE_REPEAT ? "checked":""));
		if (!strcmp (lang,"DE")) sprintf (ln,"<input type=\"checkbox\" name=\"REPEAT\" value=\"%d\" %s>Repeat Modus",DEVMODE_REPEAT,(status->stat[id].device_mode & DEVMODE_REPEAT ? "checked":""));
		strcat (mem,ln);
		strcat (mem,"</td>");
		strcat (mem,"<td>");
		sprintf (ln,"<input type=\"checkbox\" name=\"DEVMODE_SBUS\" value=\"%d\" %s>SBUS Active",DEVMODE_SBUS,(status->stat[id].device_mode & DEVMODE_SBUS ? "checked":""));
		if (!strcmp (lang,"DE")) sprintf (ln,"<input type=\"checkbox\" name=\"SBUS\" value=\"%d\" %s>SBUS aktivieren",DEVMODE_SBUS,(status->stat[id].device_mode & DEVMODE_SBUS ? "checked":""));
		strcat (mem,ln);
		strcat (mem,"</td></tr>");

/*		strcat (mem,"<tr><td>");
		sprintf (ln,"<input type=\"checkbox\" name=\"DEVMODE_IRCODE\" value=\"%d\" %s>Remote Control IR",DEVMODE_IRCODE,(status->stat[id].device_mode & DEVMODE_IRCODE ? "checked":""));
		if (!strcmp (lang,"DE")) sprintf (ln,"<input type=\"checkbox\" name=\"RCIR\" value=\"%d\" %s>PC &uumlber IR steuern",DEVMODE_IRCODE,(status->stat[id].device_mode & DEVMODE_IRCODE ? "checked":""));
		strcat (mem,ln);
		strcat (mem,"</td>");*/
		strcat (mem,"<td>");
		sprintf (ln,"<input type=\"checkbox\" name=\"DEVMODE_SBUSCODE\" value=\"%d\" %s>Remote Control SBUS",DEVMODE_SBUSCODE,(status->stat[id].device_mode & DEVMODE_SBUSCODE ? "checked":""));
		if (!strcmp (lang,"DE")) sprintf (ln,"<input type=\"checkbox\" name=\"RCSBUS\" value=\"%d\" %s>PC &uumlber SBUS steuern",DEVMODE_SBUSCODE,(status->stat[id].device_mode & DEVMODE_SBUSCODE ? "checked":""));
		strcat (mem,ln);
		strcat (mem,"</td></tr>");

		sprintf (ln,"<input type=\"checkbox\" name=\"DEVMODE_RAW\" value=\"%d\" %s>RAW Mode",DEVMODE_RAW,(status->stat[id].device_mode & DEVMODE_RAW ? "checked":""));
		if (!strcmp (lang,"DE")) sprintf (ln,"<input type=\"checkbox\" name=\"RAW\" value=\"%d\" %s>RAW Modus",DEVMODE_RAW,(status->stat[id].device_mode & DEVMODE_RAW ? "checked":""));
		strcat (mem,ln);
		strcat (mem,"</td></tr>");

		strcat (mem,"<tr><td>");
		sprintf (ln,"Hotremote&nbsp;&nbsp;<input type=\"text\" name=\"Remote\" value=\"%s\" size=\"20\">&nbsp;",status->stat[id].remote);
		if (!strcmp (lang,"DE")) sprintf (ln,"PowerOn FB&nbsp;&nbsp;<input type=\"text\" name=\"Remote\" value=\"%s\" size=\"20\">&nbsp;",status->stat[id].remote);
		strcat (mem,ln);
		strcat (mem,"</td>");
		strcat (mem,"<td>");
		sprintf (ln,"Hotcommand&nbsp;&nbsp;<input type=\"text\" name=\"Command\" value=\"%s\" size=\"20\">",status->stat[id].command);
		if (!strcmp (lang,"DE")) sprintf (ln,"PowerOn Befehl&nbsp;&nbsp;<input type=\"text\" name=\"Command\" value=\"%s\" size=\"20\">",status->stat[id].command);
		strcat (mem,ln);
		strcat (mem,"</td></tr>");
		strcat (mem,"<tr><td>&nbsp");
		strcat (mem,"</td></tr>");
		strcat (mem,"<tr><td>&nbsp");
		strcat (mem,"</td></tr>");

		strcat (mem,"<tr><td align=\"center\">");
//		strcat (mem,"<input type=\"submit\" value=\"Werte setzen\" name=\"SET\">");
		strcat (mem,"</td>");
		strcat (mem,"<td align=\"center\">");
//		strcat (mem,"<input type=\"reset\">");
		strcat (mem,"</td></tr>");

		strcat (mem,"</table></td></tr>");

		strcat (mem,"</table></form>");
	}


	strcat (mem,"<br><br><br><br>");
	strcat (mem,"<p align=\"center\"><font face=\"Verdana\" size=\"3\">");
	sprintf (ln,"IRServer %s (c) 2003 Marcus M&uumlller</font></p>",irserver_version);
	strcat (mem,ln);
	strcat (mem,"</BODY></HTML>");
}



void GetDevicesPage (char *mem,char *lang,char *parm)
{
	int res,cnt,i,id,setres = 0;
	word tm;
	byte md;
	char err[1000];
	char nm[100];
	char value[1000];
	char ln[1000];
	char remote[100],command[100];
	STATUSBUFFER buf;
	NETWORKMODE *status;

	strcpy (mem,"<HTML><HEAD><TITLE>IRTrans Device Status</TITLE></HEAD><BODY bgcolor=\"#C0C0C0\">");
	strcat (mem,"<table width=\"80%\"><tr><td align = \"center\">");
	strcat (mem,"<IMG src=\"IRTransLogo.gif\" align = \"center\">");
	strcat (mem,"</td><td width=\"40\">&nbsp</td><td align = \"center\"><font face=\"Verdana\" size=\"4\">IRTrans Device Status</font></td><td>");
	sprintf (ln,"<a href=\"index.htm?lang=%s\">",lang);
	strcat (mem,ln);
	strcat (mem,"<IMG src=\"BackButton.gif\" align = \"center\" border=\"0\"></a></td></tr></table><hr><br>");

	status = (NETWORKMODE *)&buf;
	res = GetDeviceStatus (&buf);

	if (res) {
	}
	else {
		if (parm) {
			GetHtmlParameter (parm,"id",value);
			id = atoi (value);
			if (*value == 0 || id < 0 || id > 15) {
				setres = -1;
				strcpy (err,"Can not set status: Illegal Device ID");
				if (!strcmp (lang,"DE")) strcpy (err,"Status kann nicht gesetzt werden: Illegale Device ID");
			}
			else {
				tm = 0;
				for (i=0;i<=15;i++) {
					sprintf (nm,"TM%02d",i);
					GetHtmlParameter (parm,nm,value);
					tm += atoi (value);
				}
				md = 0;
				GetHtmlParameter (parm,"DEVMODE_SEND",value);
				md += atoi (value);
				GetHtmlParameter (parm,"DEVMODE_IR",value);
				md += atoi (value);
				GetHtmlParameter (parm,"DEVMODE_SBUS",value);
				md += atoi (value);
				GetHtmlParameter (parm,"DEVMODE_IRCODE",value);
				md += atoi (value);
				GetHtmlParameter (parm,"DEVMODE_SBUSCODE",value);
				md += atoi (value);
				GetHtmlParameter (parm,"DEVMODE_RAW",value);
				md += atoi (value);
				GetHtmlParameter (parm,"DEVMODE_RAWFAST",value);
				md += atoi (value);
				GetHtmlParameter (parm,"DEVMODE_REPEAT",value);
				md += atoi (value);
				GetHtmlParameter (parm,"Remote",remote);
				GetHtmlParameter (parm,"Command",command);

				res = GetHotcode (remote,command,value);
				if (res == -1) {
					GetError (ERR_HOTCODE,value);
					sprintf (err,value,remote,command);
					setres = -1;
				}
				else {
					StoreSwitch ((word)id,0,remote,command,1);
					WriteSwitches ();
					res = SetTransceiverModusEx (0,md,tm,(byte)id,value,res,0xf,4,0,0,0,0);							// !! Bus
					if (res) {
						strcpy (err,"Could not set status");
						if (!strcmp (lang,"DE")) strcpy (err,"Status konnte nicht gesetzt werden.");
						setres = -1;
					}
					else {
						strcpy (err,"Devicestatus was updated.");
						if (!strcmp (lang,"DE")) strcpy (err,"Devicestatus wurde gesetzt.");
						setres = 1;
					}

				}
				status_changed = 1;

			}
		
		}

		status = (NETWORKMODE *)&buf;
		cnt = 0;
		for (i=0;i < 16;i++) {
			if (status->stat[i].version[0]) cnt++;
			status->stat[i].version[8] = 0;
		}
		if (setres) {
			strcat (mem,"<br><font face=\"Verdana\" size=\"4\" color=\"red\"><p align=\"center\">");
			strcat (mem,err);
			strcat (mem,"</p></font><br>");
		}
		strcat (mem,"<font face=\"Verdana\" size=\"3\"><table><tr><td width=\"20\">&nbsp;</td><td>");

		sprintf (ln,"%d IRTrans Devices found:</td></tr>",cnt);
		if (!strcmp (lang,"DE")) sprintf (ln,"%d IRTrans gefunden:</td></tr>",cnt);
		strcat (mem,ln);
		
		strcat (mem,"<font face=\"Verdana\" size=\"3\"><table>");

		for (i=0;i < 16;i++) if (status->stat[i].version[0]) {
			GetIRTransType (status->stat[i].version[0],nm);
			sprintf (ln,"<tr><td width=\"20\">&nbsp;</td><td>%d&nbsp;&nbsp;&nbsp;&nbsp;</td><td>IRTrans %s&nbsp;&nbsp;&nbsp;&nbsp;</td><td>%s</td>",i,nm,status->stat[i].version);
			strcat (mem,ln);
			sprintf (ln,"<td><a href=\"DeviceConfig.htm?lang=%s&id=%d\"><img border=\"0\" src=\"GoButton.gif\"></a></td></tr>",lang,i);
			strcat (mem,ln);
		}

		strcat (mem,"</table>");
	}


	strcat (mem,"<br><br><br><br>");
	strcat (mem,"<p align=\"center\"><font face=\"Verdana\" size=\"3\">");
	sprintf (ln,"IRServer %s (c) 2003 Marcus M&uumlller</font></p>",irserver_version);
	strcat (mem,ln);
	strcat (mem,"</BODY></HTML>");
}

void GetIRTransType (char ver,char typ[])
{

	*typ = 0;
	switch (ver) {
	case 'C':
		strcpy (typ,"Temp");
		return;
	case 'U':
		strcpy (typ,"USB");
		return;
	case 'V':
		strcpy (typ,"USB B&O");
		return;
	case 'S':
		strcpy (typ,"RS232/Bus");
		return;
	case 'T':
		strcpy (typ,"RS232/Bus B&O");
		return;
	case 'X':
		strcpy (typ,"IR Translator");
		return;
	}

}

void GetIndexPage (char *mem,char *lang)
{
	char nm[100];
	char ln[1000];
	char intfc[20];

	strcpy (mem,"<HTML><HEAD><TITLE>IRTrans Server</TITLE></HEAD><BODY bgcolor=\"#C0C0C0\">");
	strcat (mem,"<table><tr><td align = \"center\">");
	strcat (mem,"<IMG src=\"IRTransLogo.gif\" align = center>");
	strcat (mem,"</td><td width=\"30\">&nbsp</td><td><table><tr><td>");
	*nm = 0;
	gethostname (nm,100);
	*intfc = 0;
	if (*irtrans_version == 'U' || *irtrans_version == 'V') strcpy (intfc,"USB");
	if (*irtrans_version == 'S' || *irtrans_version == 'T') strcpy (intfc,"RS232");
	sprintf (ln,"<font face=\"Verdana\" size=\"4\">IRTrans Server on <font face=\"Courier\" color=\"blue\">%s</font></font></td></tr>",nm);
	if (!strcmp (lang,"DE")) sprintf (ln,"<font face=\"Verdana\" size=\"4\">IRTrans Server auf <font face=\"Courier\" color=\"blue\">%s</font></font></td></tr>",nm);
	strcat (mem,ln);
	strcat (mem,"<tr><td>&nbsp;</td></tr>");
//	sprintf (ln,"<tr><td><font face=\"Verdana\" size=\"4\">IRTrans <font face=\"Courier\" color=\"blue\">%s %s SerNo.%u</font> connected.",intfc,irtrans_version,serno);
//t	if (!strcmp (lang,"DE")) sprintf (ln,"<tr><td><font face=\"Verdana\" size=\"4\">IRTrans <font face=\"Courier\" color=\"blue\">%s %s SerNr.%u</font> angeschlossen.",intfc,irtrans_version,serno);
	strcat (mem,ln);
	if (!strcmp (lang,"DE")) strcat (mem,"</td></tr></table></td><td width=\"20\">&nbsp;</td><td align=\"center\"><a href=\"index.htm?lang=EN\"><img border=\"0\" src=\"English.jpg\"><br><font face=\"Verdana\" size=\"2\">English</font></a></td></tr></table>");
	else strcat (mem,"</td></tr></table></td><td width=\"20\">&nbsp;</td><td align=\"center\"><a href=\"index.htm?lang=DE\"><img border=\"0\" src=\"Deutsch.jpg\"><br><font face=\"Verdana\" size=\"2\">Deutsch</font></a></td></tr></table>");
	strcat (mem,"<br><hr><br><table>");
	strcat (mem,"<tr><td width=\"20\">&nbsp</td>");
	if (!strcmp (lang,"DE")) strcat (mem,"<td height=\"40\"><font face=\"Verdana\" size=\"4\">IRTrans Devices am Bus zeigen</font></td>");
	else strcat (mem,"<td height=\"40\"><font face=\"Verdana\" size=\"4\">Show IRTrans Devices connected to the Bus</font></td>");
	sprintf (ln,"<td width=\"10\">&nbsp;</td><td><a href=\"DeviceList.htm?lang=%s\"><img border=\"0\" src=\"GoButton.gif\"></a></td></tr>",lang);
	strcat (mem,ln);
	strcat (mem,"<tr><td width=\"20\">&nbsp</td>");
/*	if (!strcmp (lang,"DE")) strcat (mem,"<td height=\"40\"><font face=\"Verdana\" size=\"4\">IR Befehle lernen</font></td>");
	else strcat (mem,"<td height=\"40\"><font face=\"Verdana\" size=\"4\">Learn IR Commands</font></td>");
	sprintf (ln,"<td width=\"10\">&nbsp;</td><td><a href=\"Learn.htm?lang=%s\"><img border=\"0\" src=\"GoButton.gif\"></a></td></tr>",lang);
	strcat (mem,ln);
	strcat (mem,"<tr><td width=\"20\">&nbsp</td>");
	if (!strcmp (lang,"DE")) strcat (mem,"<td height=\"40\"><font face=\"Verdana\" size=\"4\">IR Befehle senden</font></td>");
	else strcat (mem,"<td height=\"40\"><font face=\"Verdana\" size=\"4\">Send IR Commands</font></td>");
	sprintf (ln,"<td width=\"10\">&nbsp;</td><td><a href=\"Send.htm?lang=%s\"><img border=\"0\" src=\"GoButton.gif\"></a></td></tr>",lang);
	strcat (mem,ln);*/
	strcat (mem,"</table><br><br><br><br>");
	strcat (mem,"<p align=\"center\"><font face=\"Verdana\" size=\"3\">");
	sprintf (ln,"IRServer %s (c) 2003 Marcus M&uumlller</font></p>",irserver_version);
	strcat (mem,ln);
	strcat (mem,"</BODY></HTML>");
}


void GenerateErrorPage (char *mem,int error)
{
	char ln[1000],stat[1000],body[1000];

	if (error == 404) {
		sprintf (body,"<HTML><HEAD><TITLE>404 Not found</TITLE></HEAD><BODY>The requested page was not found on this Server</BODY></HTML>");
		sprintf (stat,"HTTP/1.1 404 Not found\r\n");
	}
	strcpy (mem,stat);
	strcat (mem,"Date: Sun, 21 Dec 2003 23:10:01 GMT\r\n");
	strcat (mem,"Server: IRTrans 2.0\r\n");
	strcat (mem,"Mime-Version: 1.0\r\n");
	sprintf (ln,"Content-Type: text/html\r\nContent-Length: %6d\r\n",strlen (body));
	strcat (mem,ln);

	strcat (mem,"Expires: Sun, 21 Dec 2003 23:10:01 GMT\r\n");
	strcat (mem,"Cache-control: no-cache\r\n");

	strcat (mem,"\r\n");
	strcat (mem,body);
}

void GenerateHtmlHeader (char *mem,char *data)
{
	int len;
	char ln[1000];

	len = (int)strlen (data);
	sprintf (mem,"HTTP/1.1 200 OK\r\n");
	strcat (mem,"Date: Sun, 21 Dec 2003 23:10:01 GMT\r\n");
	strcat (mem,"Server: IRTrans 2.0\r\n");
	strcat (mem,"Mime-Version: 1.0\r\n");
	sprintf (ln,"Content-Type: text/html\r\nContent-Length: %6d\r\n",len);
	strcat (mem,ln);

	strcat (mem,"Expires: Sun, 21 Dec 2003 23:10:01 GMT\r\n");
	strcat (mem,"Cache-control: no-cache\r\n");

	strcat (mem,"\r\n");
	strcat (mem,data);
}

int GenerateImageHeader (char *mem,char *data,char *type,int len)
{
	char ln[1000];
	int hlen;

	sprintf (mem,"HTTP/1.1 200 OK\r\n");
	strcat (mem,"Date: Sun, 21 Dec 2003 23:10:01 GMT\r\n");
	strcat (mem,"Server: IRTrans 2.0\r\n");
	strcat (mem,"Mime-Version: 1.0\r\n");
	sprintf (ln,"Content-Type: image/%s\r\nContent-Length: %6d\r\n",type,len);
	strcat (mem,ln);

	strcat (mem,"\r\n");
	hlen = (int)strlen (mem);
	memcpy (mem+hlen,data,len);
	return (hlen + len);
}

