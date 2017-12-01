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
#include <sys/timeb.h>


HMODULE kdll;

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

#define CONTROL		1
#define	SHIFT		2
#define NO_KEYUP	4
#define ALT			8
#define LWINKEY		16


#define MULTIKEY_TIMEOUT 750


#define SCAN_SHIFT		1
#define SCAN_CTRL		2
#define SCAN_ALT		4
#define SCAN_WIN		8

#define SC_ALTGR		1
#define SC_SHIFT_R		2
#define SC_CTRL_R		4
#define SC_WIN_R		8
#define SC_ALT			16
#define SC_SHIFT_L		32
#define SC_CTRL_L		64

void	SendXBMC (APPCOMMAND *appcom);

#ifdef MEDIACENTER

extern DEVICEINFO IRDevices[MAX_IR_DEVICES];
extern APP app_pnt[30];
extern int app_cnt;

void	SendMediaCenterAction (int app,int com);
void	IRTransSendInput (int key,int flags);
void	SendKey (APP *app,APPCOMMAND *appcom,byte flag);
void	ConvertLcase (char *pnt,int len);
void	SendMediacenterEvent (int eventcode);
void	SendAppcommand (APP *app,APPCOMMAND *appcom);
void	SendWMChar (APP *app,APPCOMMAND *appcom);
void	HandleKeyboardScancodes (char scan[]);
void	HandleDirectScancodes (char scan[]);
void	HandleMouse (char mov[]);
void	HandleShortcut (char shortcut[]);
void	StartXBMC (APP *app);

extern byte xbmc_mode;
extern int xbmc_remote;
extern byte xbmc_init;

unsigned int GetFineTime (void);



void PostWindowsMessage (int rem,int com,char name[])
{
	int i,j;
	APPCOMMAND *appcom;

	for (i=0;i < app_cnt;i++) if (app_pnt[i].remnum == rem) {
#ifdef WIN32
		if (app_pnt[i].type == TYPE_KEYBOARD) {
			HandleKeyboardScancodes (name);
		}
		else if (app_pnt[i].type == TYPE_SCANCODE) {
			HandleDirectScancodes (name);
		}
		else if (app_pnt[i].type == TYPE_MOUSE) {
			HandleMouse (name);
		}
		else if (app_pnt[i].type == TYPE_SHORTCUT) {
			HandleShortcut (name);
		}
		else
#endif
			for (j=0;j < app_pnt[i].com_cnt;j++) if (app_pnt[i].com[j].comnum == com) {
				appcom = &(app_pnt[i].com[j]);
#ifdef WIN32
				if (!xbmc_init || rem != xbmc_remote) {
					if (appcom->type[0] == TYPE_MCE) SendMediaCenterAction (i,j);
					if (appcom->type[0] == TYPE_KEY) SendKey (app_pnt + i,appcom,0);
					if (appcom->type[0] == TYPE_KEYF) SendKey (app_pnt + i,appcom,1);
					if (appcom->type[0] == TYPE_APPCOM) SendAppcommand (app_pnt + i,appcom);
					if (appcom->type[0] == TYPE_CHR) SendWMChar (app_pnt + i,appcom);
				}
#endif
				if (appcom->type[0] == TYPE_XBMC) StartXBMC (app_pnt + i);
				if (appcom->type[0] == TYPE_STR || appcom->type[0] == TYPE_XBMC_BUTTON || 
					appcom->type[0] == TYPE_XBMC_ACTION || appcom->type[0] == TYPE_XBMC_ACTION_BUILTIN) SendXBMC (appcom);
		}
	}
}

#ifdef WIN32

void HandleShortcut (char shortcut[])
{
	ShellExecute(NULL,"open",shortcut,NULL,NULL,SW_SHOWNORMAL ); //execute shortcut in remotes folder
}


void HandleMouse (char mov[])
{
	static byte last_m1,last_m2;

	byte m1,m2;
	INPUT InpInfo[20];
	int x,y,p = 0;

	m1 = mov[9] - '0';
	m2 = mov[8] - '0';
	x = atoi (mov);
	y = atoi (mov + 4);

	p = 0;
	memset (InpInfo,0,sizeof (InpInfo));

	if (last_m1 != m1) {
		InpInfo[p].type = INPUT_MOUSE;
		if (m1) InpInfo[p].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
		else InpInfo[p].mi.dwFlags = MOUSEEVENTF_LEFTUP;
		p++;
	}
	if (last_m2 != m2) {
		InpInfo[p].type = INPUT_MOUSE;
		if (m1) InpInfo[p].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
		else InpInfo[p].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
		p++;
	}

	if (x || y) {
		InpInfo[p].type = INPUT_MOUSE;
		InpInfo[p].mi.dx = x;
		InpInfo[p].mi.dy = y;
		InpInfo[p].mi.dwFlags = MOUSEEVENTF_MOVE; 
		p++;
	}

	//------------------------------------------modified------------------------------------------
	if (p) 
	{
		SystemParametersInfo (0x1027,FALSE,NULL,0); //turn off screensaver
		SetThreadExecutionState (ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED); //reset idle timers
		SendInput (p,InpInfo,sizeof (INPUT));
	}

	last_m1 = m1;
	last_m2 = m2;
}

void HandleDirectScancodes (char scan[])
{
	static byte last_scan;
	static byte last_flags;
	static unsigned int last_time;

	HKL layout;
	INPUT InpInfo[20];
	int p,key;

	byte chr;
	byte scancode;
	byte flags;

	chr = scan[0];
	if (chr >= 'a' && chr <= 'f') chr = chr - 'a' + 10;
	else chr -= '0';

	scancode = chr;

	chr = scan[1];
	if (chr >= 'a' && chr <= 'f') chr = chr - 'a' + 10;
	else chr -= '0';
	scancode = scancode * 16 + chr;

	p = 2;
	flags = 0;
	key = 1;

	while (p <= 8) {
		if (scan[p] == '1') flags |= key;
		p++;
		key <<= 1;
	}

	if (flags & SC_ALTGR) flags |= SC_ALT | SC_CTRL_L;

	if (last_scan == scancode && last_flags != flags && (GetFineTime () - last_time) < 250) return;

	layout = GetKeyboardLayout  (0);

	if (scancode == VK_APPS) key = VK_APPS;
	else key = MapVirtualKeyEx (scancode,1,layout);


	memset (InpInfo,0,sizeof (InpInfo));

	p = 0;
	if (flags & SC_SHIFT_L) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = VK_LSHIFT;
		p++;
	}
	if (flags & SC_SHIFT_R) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = VK_RSHIFT;
		p++;
	}
	if (flags & SC_CTRL_L) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = VK_LCONTROL;
		p++;
	}
	if (flags & SC_CTRL_R) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = VK_RCONTROL;
		p++;
	}
	if (flags & SC_ALT) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = VK_MENU;
		p++;
	}
	if (flags & SC_WIN_R) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = VK_LWIN;
		p++;
	}

	if (key) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = key;
		p++;

		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.dwFlags = KEYEVENTF_KEYUP;
		InpInfo[p].ki.wVk = key;
		p++;
	}

	if (flags & SC_WIN_R) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = VK_LWIN;
		InpInfo[p].ki.dwFlags = KEYEVENTF_KEYUP;
		p++;
	}
	if (flags & SC_ALT) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = VK_MENU;
		InpInfo[p].ki.dwFlags = KEYEVENTF_KEYUP;
		p++;
	}
	if (flags & SC_CTRL_R) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = VK_RCONTROL;
		InpInfo[p].ki.dwFlags = KEYEVENTF_KEYUP;
		p++;
	}
	if (flags & SC_CTRL_L) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = VK_LCONTROL;
		InpInfo[p].ki.dwFlags = KEYEVENTF_KEYUP;
		p++;
	}
	if (flags & SC_SHIFT_R) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = VK_RSHIFT;
		InpInfo[p].ki.dwFlags = KEYEVENTF_KEYUP;
		p++;
	}
	if (flags & SC_SHIFT_L) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = VK_LSHIFT;
		InpInfo[p].ki.dwFlags = KEYEVENTF_KEYUP;
		p++;
	}

	SystemParametersInfo (0x1027,FALSE,NULL,0); //turn off screensaver
	SetThreadExecutionState (ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED); //reset idle timers
	SendInput (p,InpInfo,sizeof (INPUT));

	last_scan = scancode;
	last_flags = flags;
	last_time = GetFineTime ();
}


void HandleKeyboardScancodes (char scan[])
{
	static byte last_scan;
	static byte last_flags;
	static unsigned int last_time;

	HKL layout;
	INPUT InpInfo[10];
	int p,key;

	byte chr;
	byte scancode;
	byte flags;

	chr = scan[0];
	if (chr >= 'a' && chr <= 'f') chr = chr - 'a' + 10;
	else chr -= '0';

	scancode = chr;

	chr = scan[1];
	if (chr >= 'a' && chr <= 'f') chr = chr - 'a' + 10;
	else chr -= '0';
	scancode = scancode * 16 + chr;

	flags = scan[2] - '0';

	if (last_scan == scancode && last_flags != flags && (GetFineTime () - last_time) < 250) return;

	layout = GetKeyboardLayout  (0);

	if (scancode == VK_APPS) key = VK_APPS;
	else key = MapVirtualKeyEx (scancode,1,layout);


	memset (InpInfo,0,sizeof (InpInfo));

	p = 0;
	if (flags & SCAN_CTRL) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = VK_CONTROL;
		p++;
	}
	if (flags & SCAN_SHIFT) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = VK_SHIFT;
		p++;
	}
	if (flags & SCAN_ALT) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = VK_MENU;
		p++;
	}
	if (flags & SCAN_WIN) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = VK_LWIN;
		p++;
	}

	if (key) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = key;
		p++;

		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.dwFlags = KEYEVENTF_KEYUP;
		InpInfo[p].ki.wVk = key;
		p++;
	}

	if (flags & SCAN_WIN) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.dwFlags = KEYEVENTF_KEYUP;
		InpInfo[p].ki.wVk = VK_LWIN;
		p++;
	}
	if (flags & SCAN_ALT) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.dwFlags = KEYEVENTF_KEYUP;
		InpInfo[p].ki.wVk = VK_MENU;
		p++;
	}
	if (flags & SCAN_SHIFT) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = VK_SHIFT;
		InpInfo[p].ki.dwFlags = KEYEVENTF_KEYUP;
		p++;
	}
	if (flags & SCAN_CTRL) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = VK_CONTROL;
		InpInfo[p].ki.dwFlags = KEYEVENTF_KEYUP;
		p++;
	}
	SystemParametersInfo (0x1027,FALSE,NULL,0); //turn off screensaver
	SetThreadExecutionState (ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED); //reset idle timers
	SendInput (p,InpInfo,sizeof (INPUT));

	last_scan = scancode;
	last_flags = flags;
	last_time = GetFineTime ();
}


void SendWMChar (APP *app,APPCOMMAND *appcom)
{
	HWND win;

	win = FindWindow (app->classname,NULL);
	if (!win) return;

	PostMessage (win,WM_KEYDOWN,(WPARAM)appcom->function.function[0],(LPARAM)0);
}

void SendAppcommand (APP *app,APPCOMMAND *appcom)
{
	HWND win;

	win = FindWindow (app->classname,NULL);
	if (!win) return;

	PostMessage (win,WM_APPCOMMAND,(WPARAM)1,(LPARAM)(appcom->function.function[0] << 16));
}

void SendKey (APP *app,APPCOMMAND *appcom,byte flag)
{
	HWND win;
	DWORD thr;
	byte mf = 0;
	struct _timeb tb;
	unsigned int tv;

	static byte cindex;
	static APPCOMMAND *lastcom;
	static unsigned int lasttime;

	if (app && app->classname[0]) {
		win = FindWindow (app->classname,NULL);
		if (!win) return;
		if (flag) {
			thr = GetWindowThreadProcessId (win,NULL);
			AttachThreadInput (GetCurrentThreadId (),thr,TRUE);

			SetFocus (win);

			AttachThreadInput (GetCurrentThreadId (),thr,FALSE);
		}
	}

	_ftime (&tb);
	tv = (unsigned int)((tb.time & 0x7fffff) * 1000 + tb.millitm);

	if (appcom != lastcom) cindex = 0;
	else {
		if ((tv - lasttime) < MULTIKEY_TIMEOUT) mf = ++cindex;
		else cindex = 0;
	}

	if (appcom->type[cindex] != TYPE_KEY) {
		if (cindex == 1) mf = 0;
		cindex = 0;
	}

	if (mf != 0)
	{
		IRTransSendInput (VK_DELETE,0);
	}

	IRTransSendInput (appcom->function.function[cindex] & 0xff,(appcom->function.function[cindex] & 0xff00) >> 8);
	
	lastcom = appcom;
	lasttime = tv;
}

void SendMediaCenterAction (int app,int com)
{
	int res;
	HWND mcewin;
	char *sysdir,prog[256];

	mcewin = FindWindow (app_pnt[app].classname,NULL);
	if (mcewin == NULL) {
		if (xbmc_mode && app_pnt[app].remnum == xbmc_remote) return;													// Wenn XBMC aktiv kein MCE starten

		if (app_pnt[app].com[com].type[0] == TYPE_RUN || app_pnt[app].com[com].type[1] == TYPE_RUN) {
			sysdir = getenv ("SystemRoot");
			if (sysdir) {
				sprintf (prog,"%s\\ehome\\ehshell.exe",sysdir);
				if (app_pnt[app].com[com].function.function[0] == 40) strcat (prog," /homepage:VideoCollection.xml /pushstartpage:true");	// Video
				if (app_pnt[app].com[com].function.function[0] == 35) strcat (prog," /homepage:Audio.Home.xml /pushstartpage:true");	// Music
				if (app_pnt[app].com[com].function.function[0] == 39) strcat (prog," /homepage:VideoHome.xml /pushstartpage:true");	// TV
				if (app_pnt[app].com[com].function.function[0] == 45) strcat (prog," /homepage:Radio.xml /pushstartpage:true");	// Radio
				if (app_pnt[app].com[com].function.function[0] == 36) strcat (prog," /homepage:Photos.xml /pushstartpage:true");	// Pictures
				if (app_pnt[app].com[com].function.function[0] == 37) strcat (prog," /homepage:VideoRecordedPrograms.xml /pushstartpage:true");	// RecTV
				res = WinExec (prog,SW_SHOWMAXIMIZED);
			}
		}
		else if (app_pnt[app].com[com].function.function[0] == 42) IRTransSendInput (VK_RETURN,ALT | LWINKEY);
	}
	else {
		switch (app_pnt[app].com[com].function.function[0]) {
		case 12:										// Clear
			IRTransSendInput (VK_ESCAPE,0);
			return;
		case 17:										// Mute
//			IRTransSendInput (119,0);
			SendMediacenterEvent (APPCOMMAND_VOLUME_MUTE);
			return;
		case 18:										// Vol-
//			IRTransSendInput (120,0);
			SendMediacenterEvent (APPCOMMAND_VOLUME_DOWN);
			return;
		case 19:										// Vol+
//			IRTransSendInput (121,0);
			SendMediacenterEvent (APPCOMMAND_VOLUME_UP);
			return;
		case 20:										// Play
//			IRTransSendInput (80,SHIFT | CONTROL);
			SendMediacenterEvent (APPCOMMAND_MEDIA_PLAY);
			return;
		case 21:										// Stop
//			IRTransSendInput (83,SHIFT | CONTROL);
			SendMediacenterEvent (APPCOMMAND_MEDIA_STOP);
			return;
		case 22:										// Next
//			IRTransSendInput (70,CONTROL);
			SendMediacenterEvent (APPCOMMAND_MEDIA_NEXTTRACK);
			return;
		case 23:										// Prev
//			IRTransSendInput (66,CONTROL);
			SendMediacenterEvent (APPCOMMAND_MEDIA_PREVIOUSTRACK);
			return;
		case 24:										// REC
//			IRTransSendInput (82,CONTROL);
			SendMediacenterEvent (APPCOMMAND_MEDIA_RECORD);
			return;
		case 25:										// Pause
//			IRTransSendInput (80,CONTROL);
			SendMediacenterEvent (APPCOMMAND_MEDIA_PAUSE);
			return;
		case 26:										// REW
//			IRTransSendInput (66,SHIFT | CONTROL);
			SendMediacenterEvent (APPCOMMAND_MEDIA_REWIND);
			return;
		case 27:										// FWD
//			IRTransSendInput (70,SHIFT | CONTROL);
			SendMediacenterEvent (APPCOMMAND_MEDIA_FAST_FORWARD);
			return;
		case 28:										// Ch+
			IRTransSendInput (187,CONTROL);
//			SendMediacenterEvent (APPCOMMAND_MEDIA_CHANNEL_UP);
			return;
		case 29:										// Ch-
			IRTransSendInput (189,CONTROL);
//			SendMediacenterEvent (APPCOMMAND_MEDIA_CHANNEL_DOWN);
			return;
		case 31:										// DVDMenu
			IRTransSendInput (77,SHIFT | CONTROL);
			return;
		case 32:										// DVDAudio
			IRTransSendInput (65,SHIFT | CONTROL);
			return;
		case 33:										// DVDSubtitle
			IRTransSendInput (85, CONTROL);
			return;
		case 34:										// EPG
			IRTransSendInput (71,CONTROL);
			return;
		case 35:										// Music
			IRTransSendInput (77,CONTROL);
			return;
		case 36:										// Pictures
			IRTransSendInput (73,CONTROL);
			return;
		case 37:										// RecTV
			IRTransSendInput (79,CONTROL);
			return;
		case 38:										// TV
			IRTransSendInput (84,CONTROL | SHIFT);
			return;
		case 39:										// LiveTV
			IRTransSendInput (84,CONTROL);
			return;
		case 40:										// Video
			IRTransSendInput (69,CONTROL);
			return;
		case 41:										// Info
			IRTransSendInput (68,CONTROL);
			return;
		case 42:										// Ehome
			IRTransSendInput (VK_RETURN,ALT | LWINKEY);
			return;
		case 43:										// Messenger
			IRTransSendInput (78,CONTROL);
			return;
		case 44:										// Teletext
			IRTransSendInput (68,CONTROL);
			return;
		case 45:										// Radio
			IRTransSendInput (65,CONTROL);
			return;
		case 46:										// Back
			SendMediacenterEvent (APPCOMMAND_BROWSER_BACKWARD);
			return;
		}
	}
}


void SendMediacenterEvent (int eventcode)
{
	HWND mcewin;

	mcewin = FindWindow ("eHome Render Window",NULL);
	if (!mcewin) return;
	PostMessage (mcewin,WM_APPCOMMAND,(WPARAM)1,(LPARAM)(eventcode << 16));
}





void IRTransSendInput (int key,int flags)
{
	INPUT InpInfo[10];
	int p;

	memset (InpInfo,0,sizeof (InpInfo));
	p = 0;
	if (flags & CONTROL) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = VK_CONTROL;
		p++;
	}
	if (flags & SHIFT) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = VK_SHIFT;
		p++;
	}
	if (flags & ALT) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = VK_MENU;
		p++;
	}
	if (flags & LWINKEY) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = VK_LWIN;
		p++;
	}

	 
	InpInfo[p].type = INPUT_KEYBOARD;
	InpInfo[p].ki.wVk = key;
	p++;
	
	if (!(flags & NO_KEYUP)) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.dwFlags = KEYEVENTF_KEYUP;
		InpInfo[p].ki.wVk = key;
		p++;
	}

	if (flags & LWINKEY) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.dwFlags = KEYEVENTF_KEYUP;
		InpInfo[p].ki.wVk = VK_LWIN;
		p++;
	}
	if (flags & ALT) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.dwFlags = KEYEVENTF_KEYUP;
		InpInfo[p].ki.wVk = VK_MENU;
		p++;
	}
	if (flags & SHIFT) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = VK_SHIFT;
		InpInfo[p].ki.dwFlags = KEYEVENTF_KEYUP;
		p++;
	}
	if (flags & CONTROL) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = VK_CONTROL;
		InpInfo[p].ki.dwFlags = KEYEVENTF_KEYUP;
		p++;
	}
	SystemParametersInfo (0x1027,FALSE,NULL,0); //turn off screensaver
	SetThreadExecutionState (ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED); //reset idle timers
	SendInput (p,InpInfo,sizeof (INPUT));
}

#endif
#endif

int GetKeyCode (char *com)
{
	int key = 0;

#ifdef WIN32
	if (*com == '\\') {
		com++;
		if (!strcmp (com,"space")) return ' ';
		if (!strcmp (com,"period")) return 0xbe;
		if (!strcmp (com,"comma")) return 0xbc;
		if (!strcmp (com,"enter")) return VK_RETURN;
		if (!strcmp (com,"up")) return VK_UP;
		if (!strcmp (com,"down")) return VK_DOWN;
		if (!strcmp (com,"right")) return VK_RIGHT;
		if (!strcmp (com,"left")) return VK_LEFT;
		if (!strcmp (com,"backspace")) return VK_BACK;
		if (!strcmp (com,"end")) return VK_END;
		if (!strcmp (com,"home")) return VK_HOME;
		if (!strcmp (com,"pgup")) return VK_PRIOR;
		if (!strcmp (com,"pgdown")) return VK_NEXT;
		if (!strcmp (com,"esc")) return VK_ESCAPE;
		if (!strcmp (com,"tab")) return VK_TAB;
		if (!strcmp (com,"f1")) return VK_F1;
		if (!strcmp (com,"f2")) return VK_F2;
		if (!strcmp (com,"f3")) return VK_F3;
		if (!strcmp (com,"f4")) return VK_F4;
		if (!strcmp (com,"f5")) return VK_F5;
		if (!strcmp (com,"f6")) return VK_F6;
		if (!strcmp (com,"f7")) return VK_F7;
		if (!strcmp (com,"f8")) return VK_F8;
		if (!strcmp (com,"f9")) return VK_F9;
		if (!strcmp (com,"f10")) return VK_F10;
		if (!strcmp (com,"f11")) return VK_F11;
		if (!strcmp (com,"f12")) return VK_F12;
		if (!strcmp (com,"menu")) return VK_APPS;
		if (!strcmp (com,"pause")) return VK_PAUSE;
		while (!strncmp (com,"alt",3) || !strncmp (com,"ctrl",4) || !strncmp (com,"shift",5)) {
			if (!strncmp (com,"alt",3)) {
				key |= ALT << 8;
				com += 3;
			}
			if (!strncmp (com,"ctrl",4)) {
				key |= CONTROL << 8;
				com += 4;
			}
			if (!strncmp (com,"shift",5)) {
				key |= SHIFT << 8;
				com += 5;
			}
			if (*com == 0) return (0);
			
			if (*com == '\\') {
				com++;
				if (!strcmp (com,"space"))		return key | ' ';
				if (!strcmp (com,"period"))		return key | 0xbe;
				if (!strcmp (com,"comma"))		return key | 0xbc;
				if (!strcmp (com,"enter"))		return key | VK_RETURN;
				if (!strcmp (com,"up"))			return key | VK_UP;
				if (!strcmp (com,"down"))		return key | VK_DOWN;
				if (!strcmp (com,"right"))		return key | VK_RIGHT;
				if (!strcmp (com,"left"))		return key | VK_LEFT;
				if (!strcmp (com,"backspace"))	return key | VK_BACK;
				if (!strcmp (com,"end"))		return key | VK_END;
				if (!strcmp (com,"home"))		return key | VK_HOME;
				if (!strcmp (com,"pgup"))		return key | VK_PRIOR;
				if (!strcmp (com,"pgdown"))		return key | VK_NEXT;
				if (!strcmp (com,"esc"))		return key | VK_ESCAPE;
				if (!strcmp (com,"tab"))		return key | VK_TAB;
				if (!strcmp (com,"f1"))			return key | VK_F1;
				if (!strcmp (com,"f2"))			return key | VK_F2;
				if (!strcmp (com,"f3"))			return key | VK_F3;
				if (!strcmp (com,"f4"))			return key | VK_F4;
				if (!strcmp (com,"f5"))			return key | VK_F5;
				if (!strcmp (com,"f6"))			return key | VK_F6;
				if (!strcmp (com,"f7"))			return key | VK_F7;
				if (!strcmp (com,"f8"))			return key | VK_F8;
				if (!strcmp (com,"f9"))			return key | VK_F9;
				if (!strcmp (com,"f10"))		return key | VK_F10;
				if (!strcmp (com,"f11"))		return key | VK_F11;
				if (!strcmp (com,"f12"))		return key | VK_F12;
				if (!strcmp (com,"menu"))		return key | VK_APPS;
				if (!strcmp (com,"pause"))		return key | VK_PAUSE;
			}
			else {				
				if (*com >= 'a' && *com <= 'z') return (key | (*com - ('a' - 'A')));
				return (key | *com);
			}
		}

		return (0);
	}
	if (*com >= 'a' && *com <= 'z') return (*com - ('a' - 'A'));
	if (*com == '+') return VK_ADD;
	if (*com == '-') return VK_SUBTRACT;
	if (*com == '*') return VK_MULTIPLY;
	if (*com == '/') return VK_DIVIDE;
	
	return (*com);
#else
	return (0);
#endif
}

int GetFunctionCode (byte type,char *com)
{
	if (type == TYPE_APPCOM) {
#ifdef WIN32
		if (!strcmp (com,"appcommand_media_play")) return APPCOMMAND_MEDIA_PLAY;
		if (!strcmp (com,"appcommand_volume_mute")) return  APPCOMMAND_VOLUME_MUTE;
		if (!strcmp (com,"appcommand_volume_down")) return  APPCOMMAND_VOLUME_DOWN;
		if (!strcmp (com,"appcommand_volume_up")) return  APPCOMMAND_VOLUME_UP;
		if (!strcmp (com,"appcommand_media_stop")) return  APPCOMMAND_MEDIA_STOP;
		if (!strcmp (com,"appcommand_media_nexttrack")) return  APPCOMMAND_MEDIA_NEXTTRACK;
		if (!strcmp (com,"appcommand_media_previoustrack")) return  APPCOMMAND_MEDIA_PREVIOUSTRACK;
		if (!strcmp (com,"appcommand_media_record")) return  APPCOMMAND_MEDIA_RECORD;
		if (!strcmp (com,"appcommand_media_pause")) return  APPCOMMAND_MEDIA_PAUSE;
		if (!strcmp (com,"appcommand_media_rewind")) return  APPCOMMAND_MEDIA_REWIND;
		if (!strcmp (com,"appcommand_media_fast_forward")) return  APPCOMMAND_MEDIA_FAST_FORWARD;
		if (!strcmp (com,"appcommand_browser_backward")) return  APPCOMMAND_BROWSER_BACKWARD;
#endif
	}

	else if (type == TYPE_MCE) {
		if (!strcmp (com,"clear")) return 12;
		if (!strcmp (com,"mute")) return 17;
		if (!strcmp (com,"vol-")) return 18;
		if (!strcmp (com,"vol+")) return 19;
		if (!strcmp (com,"play")) return 20;
		if (!strcmp (com,"stop")) return 21;
		if (!strcmp (com,"next")) return 22;
		if (!strcmp (com,"prev")) return 23;
		if (!strcmp (com,"rec")) return 24;
		if (!strcmp (com,"pause")) return 25;
		if (!strcmp (com,"rew")) return 26;
		if (!strcmp (com,"fwd")) return 27;
		if (!strcmp (com,"ch+")) return 28;
		if (!strcmp (com,"ch-")) return 29;
		if (!strcmp (com,"dvdmenu")) return 31;
		if (!strcmp (com,"dvdaudio")) return 32;
		if (!strcmp (com,"dvdsubtitle")) return 33;
		if (!strcmp (com,"epg")) return 34;
		if (!strcmp (com,"music")) return 35;
		if (!strcmp (com,"pictures")) return 36;
		if (!strcmp (com,"rectv")) return 37;
		if (!strcmp (com,"tv")) return 38;
		if (!strcmp (com,"livetv")) return 39;
		if (!strcmp (com,"video")) return 40;
		if (!strcmp (com,"info")) return 41;
		if (!strcmp (com,"ehome")) return 42;
		if (!strcmp (com,"messenger")) return 43;
		if (!strcmp (com,"teletext")) return 44;
		if (!strcmp (com,"radio")) return 45;
		if (!strcmp (com,"back")) return 46;
		return 0;
	}
	else if (type == TYPE_XBMC) {
		if (!strcmp (com,"xbmc")) return 1;
		return 0;
	}
	return 0;
}

