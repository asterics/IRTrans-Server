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

#include "remote.h"
#include "global.h"
#include "network.h"
#include "lowlevel.h"
#include "dbstruct.h"
#include "fileio.h"

#endif


void KeyboardInput (char code[]);
void EnterKeyboardInput (byte reset);
void MouseInput (char code[]);


#define		S_MOD_CTRL		1
#define		S_MOD_SHIFT		2
#define		S_MOD_ALT		4

static byte mod_flags;
static byte last_mods;
static byte keys[10];
static byte flags[10];
static byte keynum;


void HandleHID (int rem_num,int com_num,char rname[],char xcode[])
{
	word relc;
	char st[200],rem[100],name[30],code[200];
	int start_pos,cnum,rnum;

	sprintf (st,"HID: %s\n",xcode);
	log_print (st,LOG_DEBUG);

	strcpy (code,xcode);

	if (!strcmp (rname,"kb") || !strcmp (rname,"kx") || !strcmp (rname,"rm")) {
		if (strlen (code) > 11) {														// Keyboard / Remote

			strcpy (st,code+10);
			st[0] = 'C';
			while (strlen (st) >= 10) {

				start_pos = DBFindCommandName (st,rem,name,0,&rnum,&cnum,&relc,com_num);
				if (start_pos && rnum == rem_num) {
					KeyboardInput (name);
				}

				strcpy (code,st);
				strcpy (st,code+8);
				st[0] = 'C';
			}
		}
		else EnterKeyboardInput (1);

		EnterKeyboardInput (0);
	}
	else if (!strcmp (rname,"power")) EnterKeyboardInput (0);
	else if (!strcmp (rname,"pt") || !strcmp (rname,"px")) {									// Maus
		if (strlen (code) > 11) MouseInput (code + 11);
		else MouseInput ("000");
	}

}


#define MOUSE_SPEED		1.2
#define MOUSE_ACCEL		1.02
#define ACCEL_MAX		20
#define ACCEL_TIMEOUT	2
#define ACCEL_INC		0.5
#define ACCEL_START		1
#define ACCEL_START_INC	0.1

void MouseInput (char code[])
{
	static byte last_m1,last_m2;
	static float accel_x,accel_y;
	static unsigned long accel_timer;
	

	char st[100];
	byte m1,m2;
	int p = 0;
	INPUT InpInfo[20];
	char x = 0,y = 0;
	unsigned long tv;
	struct _timeb tb;

	m1 = code[2] - '0';
	m2 = code[1] - '0';
	
	_ftime (&tb);

	tv = tb.time * 10 + tb.millitm / 100;
	
	if (tv > accel_timer) accel_x = accel_y = 0;
	
	if (strlen (code) > 4) {

		memset (st,0,10);
		memcpy (st,code+3,8);
		x = (char)strtoul (st,NULL,2);

		memset (st,0,10);
		memcpy (st,code+11,8);
		y = (char)strtoul (st,NULL,2);
		
	}


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
		InpInfo[p].mi.dx = (long)((x + (long)(x * MOUSE_ACCEL * accel_x)) * MOUSE_SPEED);
		InpInfo[p].mi.dy = (long)((y + (long)(y * MOUSE_ACCEL * accel_y)) * MOUSE_SPEED);
		InpInfo[p].mi.dwFlags = MOUSEEVENTF_MOVE; 

		if (x && accel_x < ACCEL_START) {
			accel_x += (float)ACCEL_START_INC;
		}
		else {
			if (x && accel_x < ACCEL_MAX) accel_x += ACCEL_INC;
		}

		if (y && accel_y < ACCEL_START) {
			accel_y += (float)ACCEL_START_INC;
		}
		else {
			if (y && accel_y < ACCEL_MAX) accel_y += ACCEL_INC;
		}
		p++;
		
		sprintf (st,"HID Mouse: %d [%f],%d [%f]\n",x,accel_x,y,accel_y);
		log_print (st,LOG_DEBUG);
	}

	if (p) {
		SystemParametersInfo (0x1027,FALSE,NULL,0); //turn off screensaver
		SetThreadExecutionState (ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED); //reset idle timers
		SendInput (p,InpInfo,sizeof (INPUT));
	}

	last_m1 = m1;
	last_m2 = m2;
	accel_timer = tv + ACCEL_TIMEOUT;
}


void KeyboardInput (char code[])
{
	int key,flg = 0;

	if (!strcmp (code,"ctrl")) mod_flags |= S_MOD_CTRL;
	else if (!strcmp (code,"shift")) mod_flags |= S_MOD_SHIFT;
	else if (!strcmp (code,"alt")) mod_flags |= S_MOD_ALT;
	else if (!strcmp (code,"altgr")) mod_flags |= S_MOD_ALT | S_MOD_CTRL;
	else {
		if (code[1] == 0) {
			if (*code >= 'a' && *code <= 'z') *code -= ('a' - 'A');
			key = *code;
		}
		else if (*code == 'c') key = atoi (code + 1);
		else if (*code == 'e') {
			key = atoi (code + 1);
			flg = KEYEVENTF_EXTENDEDKEY;
		}
			
		else return;

		keys[keynum] = key;
		flags[keynum++] = flg;
	}
}


void EnterKeyboardInput (byte reset)
{
	int p,i;
	char st[100];
	INPUT InpInfo[20];
	HWINSTA hwinsta;
	HDESK hdesk;

	memset (InpInfo,0,sizeof (InpInfo));

	if (reset) {
		mod_flags = 0;
		last_mods = S_MOD_ALT | S_MOD_CTRL | S_MOD_SHIFT;
		keynum = 0;
	}

	if (mode_flag & DAEMON_MODE) {
		if (mod_flags == (S_MOD_ALT | S_MOD_CTRL) && keynum == 1 && keys[0] == VK_DELETE) {

			p = 0;
			InpInfo[p].type = INPUT_KEYBOARD;
			InpInfo[p].ki.wVk = VK_LSHIFT;
			InpInfo[p].ki.dwFlags = KEYEVENTF_KEYUP;
			p++;
			InpInfo[p].type = INPUT_KEYBOARD;
			InpInfo[p].ki.wVk = VK_LCONTROL;
			InpInfo[p].ki.dwFlags = KEYEVENTF_KEYUP;
			p++;
			InpInfo[p].type = INPUT_KEYBOARD;
			InpInfo[p].ki.wVk = VK_MENU;
			InpInfo[p].ki.dwFlags = KEYEVENTF_KEYUP;
			p++;

			keynum = 0;
			last_mods = 0;
			mod_flags = 0;
			SendInput (p,InpInfo,sizeof (INPUT));

			hwinsta = OpenWindowStation("winsta0", FALSE,
									  WINSTA_ACCESSCLIPBOARD   |
									  WINSTA_ACCESSGLOBALATOMS |
									  WINSTA_CREATEDESKTOP     |
									  WINSTA_ENUMDESKTOPS      |
									  WINSTA_ENUMERATE         |
									  WINSTA_EXITWINDOWS       |
									  WINSTA_READATTRIBUTES    |
									  WINSTA_READSCREEN        |
									  WINSTA_WRITEATTRIBUTES);
			if (hwinsta == NULL) return;

			if (!SetProcessWindowStation(hwinsta)) return;

			hdesk = OpenDesktop("Winlogon", 0, FALSE,
								DESKTOP_CREATEMENU |
								DESKTOP_CREATEWINDOW |
								DESKTOP_ENUMERATE    |
								DESKTOP_HOOKCONTROL  |
								DESKTOP_JOURNALPLAYBACK |
								DESKTOP_JOURNALRECORD |
								DESKTOP_READOBJECTS |
								DESKTOP_SWITCHDESKTOP |
								DESKTOP_WRITEOBJECTS);
			if (hdesk == NULL) return;

			if (!SetThreadDesktop(hdesk)) return;

			PostMessage(HWND_BROADCAST,WM_HOTKEY,0,MAKELPARAM(MOD_ALT|MOD_CONTROL,VK_DELETE));

			return;
		}

		hdesk = OpenInputDesktop(0, FALSE,
							DESKTOP_CREATEMENU |
							DESKTOP_CREATEWINDOW |
							DESKTOP_ENUMERATE    |
							DESKTOP_HOOKCONTROL  |
							DESKTOP_JOURNALPLAYBACK |
							DESKTOP_JOURNALRECORD |
							DESKTOP_READOBJECTS |
							DESKTOP_SWITCHDESKTOP |
							DESKTOP_WRITEOBJECTS);
		if (hdesk == NULL) return;

		SetThreadDesktop(hdesk);
	}

	p = 0;
	if (mod_flags & S_MOD_SHIFT && !(last_mods & S_MOD_SHIFT)) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = VK_LSHIFT;
		p++;
	}
	if (mod_flags & S_MOD_CTRL && !(last_mods & S_MOD_CTRL)) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = VK_LCONTROL;
		p++;
	}
	if (mod_flags & S_MOD_ALT && !(last_mods & S_MOD_ALT)) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = VK_MENU;
		p++;
	}

	if (!(mod_flags & S_MOD_SHIFT) && last_mods & S_MOD_SHIFT) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = VK_LSHIFT;
		InpInfo[p].ki.dwFlags = KEYEVENTF_KEYUP;
		p++;
	}
	if (!(mod_flags & S_MOD_CTRL) && last_mods & S_MOD_CTRL) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = VK_LCONTROL;
		InpInfo[p].ki.dwFlags = KEYEVENTF_KEYUP;
		p++;
	}
	if (!(mod_flags & S_MOD_ALT) && last_mods & S_MOD_ALT) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk = VK_MENU;
		InpInfo[p].ki.dwFlags = KEYEVENTF_KEYUP;
		p++;
	}


	for (i=0;i < keynum;i++) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.wVk  = keys[i];
		InpInfo[p].ki.dwFlags = flags[i];
		p++;
	}

	for (i=0;i < keynum;i++) {
		InpInfo[p].type = INPUT_KEYBOARD;
		InpInfo[p].ki.dwFlags = KEYEVENTF_KEYUP | flags[i];
		InpInfo[p].ki.wVk = keys[i];
		p++;
	}

	sprintf (st,"HID Input Count: %d\n",keynum);
	log_print (st,LOG_DEBUG);

	keynum = 0;

	SystemParametersInfo (0x1027,FALSE,NULL,0); //turn off screensaver
	SetThreadExecutionState (ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED); //reset idle timers
	SendInput (p,InpInfo,sizeof (INPUT));

	last_mods = mod_flags;
	mod_flags = 0;
	
	if (mode_flag & DAEMON_MODE) {
		CloseDesktop (hdesk);
	}
}
