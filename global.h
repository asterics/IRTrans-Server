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

#include "functioncodes.h"

#define LOG_DEBUG		4
#define LOG_INFO		3
#define LOG_ERROR		2
#define LOG_FATAL		1
#define LOG_MASK		15

#define DEBUG_CODE		16
#define	HEXDUMP			32
#define OLDFORMAT		64
#define CODEDUMP		128
#define LEARNED_ONLY	256
#define	XAP				512
#define MEDIALON		1024
#define TIMESTAMP		2048
#define READHTML		4096

#define DAEMON_MODE		0x10000
#define NO_RECONNECT	0x20000
#define	NO_RESET		0x40000
#define NO_LIRC			0x80000
#define NO_WEB			0x100000
#define CLOCK_STARTUP	0x200000
#define READ_EEPROM		0x400000
#define NO_CLOCK		0x800000
#define IP_RELAY		0x1000000
#define SEND_FORWARD	0x2000000
#define SEND_FORWARDALL	0x4000000
#define NO_INIT_LAN		0x8000000
#define DEBUG_TIMING	0x10000000
#define PARAMETER_FILE	0x20000000
#define ETHERNET_TCP	0x40000000
#define TCP_RECONNECT	0x80000000



#define TYPE_MCE	1
#define TYPE_KEY	2
#define TYPE_RUN	3
#define TYPE_APPCOM	4
#define TYPE_COM	5
#define TYPE_CHR	6
#define TYPE_KEYF	7
#define TYPE_KEYBOARD	8
#define TYPE_SCANCODE	9
#define TYPE_MOUSE		10
#define TYPE_SHORTCUT	11
#define TYPE_XBMC		12
#define TYPE_STR		13
#define TYPE_XBMC_BUTTON	14
#define TYPE_XBMC_ACTION	15
#define TYPE_XBMC_ACTION_BUILTIN	16



#define SELECT_TRANS	1
#define	SELECT_SERVER	2
#define	SELECT_LIRC		3
#define SELECT_LOCAL	4
#define	SELECT_COMMAND	5
#define SELECT_REOPEN	6
#define SELECT_WEB		7
#define SELECT_UDP		8
#define SELECT_XAP		9
#define SELECT_IRTLAN	10
#define SELECT_CLIENT	11
#define SELECT_RS232	12
#define SELECT_LAN_TCP	13

#define COMMAND_SERVER	102
#define COMMAND_LIRC	103
#define COMMAND_LOCAL	104
#define COMMAND_REOPEN	106


extern unsigned int mode_flag;
extern char logfile[256];
extern FILE *logfp;
extern char irserver_version[20];
extern char irtrans_version[100];
extern byte last_adress,resend_flag;
extern unsigned short capabilities;
extern unsigned short capabilities2;
extern unsigned short capabilities3;
extern unsigned short capabilities4;
extern byte time_len;
extern byte raw_repeat;
extern char err_remote[81];
extern char err_command[21];

#ifdef LINUX
#define INVALID_HANDLE_VALUE -1
#endif


void XBMC_SendBye ();
void XBMC_SendPing ();
int InitXBMC (void);
int BuildXBMCHeader (byte *data,short type,int payload_len);
int BuildXBMCHelo (byte *data,char *name);
int BuildXBMCKey (byte *data,char *button);
int BuildXBMCAction (byte *data,byte type,char *action);



#ifdef WIN32

void	PostWindowsMessage (int rem,int com,char name[]);

/* cmd for HSHELL_APPCOMMAND and WM_APPCOMMAND */
#define APPCOMMAND_BROWSER_BACKWARD       1
#define APPCOMMAND_BROWSER_FORWARD        2
#define APPCOMMAND_BROWSER_REFRESH        3
#define APPCOMMAND_BROWSER_STOP           4
#define APPCOMMAND_BROWSER_SEARCH         5
#define APPCOMMAND_BROWSER_FAVORITES      6
#define APPCOMMAND_BROWSER_HOME           7
#define APPCOMMAND_VOLUME_MUTE            8
#define APPCOMMAND_VOLUME_DOWN            9
#define APPCOMMAND_VOLUME_UP              10
#define APPCOMMAND_MEDIA_NEXTTRACK        11
#define APPCOMMAND_MEDIA_PREVIOUSTRACK    12
#define APPCOMMAND_MEDIA_STOP             13
#define APPCOMMAND_MEDIA_PLAY_PAUSE       14
#define APPCOMMAND_LAUNCH_MAIL            15
#define APPCOMMAND_LAUNCH_MEDIA_SELECT    16
#define APPCOMMAND_LAUNCH_APP1            17
#define APPCOMMAND_LAUNCH_APP2            18
#define APPCOMMAND_BASS_DOWN              19
#define APPCOMMAND_BASS_BOOST             20
#define APPCOMMAND_BASS_UP                21
#define APPCOMMAND_TREBLE_DOWN            22
#define APPCOMMAND_TREBLE_UP              23
#define APPCOMMAND_MICROPHONE_VOLUME_MUTE 24
#define APPCOMMAND_MICROPHONE_VOLUME_DOWN 25
#define APPCOMMAND_MICROPHONE_VOLUME_UP   26
#define APPCOMMAND_HELP                   27
#define APPCOMMAND_FIND                   28
#define APPCOMMAND_NEW                    29
#define APPCOMMAND_OPEN                   30
#define APPCOMMAND_CLOSE                  31
#define APPCOMMAND_SAVE                   32
#define APPCOMMAND_PRINT                  33
#define APPCOMMAND_UNDO                   34
#define APPCOMMAND_REDO                   35
#define APPCOMMAND_COPY                   36
#define APPCOMMAND_CUT                    37
#define APPCOMMAND_PASTE                  38
#define APPCOMMAND_REPLY_TO_MAIL          39
#define APPCOMMAND_FORWARD_MAIL           40
#define APPCOMMAND_SEND_MAIL              41
#define APPCOMMAND_SPELL_CHECK            42
#define APPCOMMAND_DICTATE_OR_COMMAND_CONTROL_TOGGLE    43
#define APPCOMMAND_MIC_ON_OFF_TOGGLE      44
#define APPCOMMAND_CORRECTION_LIST        45
#define APPCOMMAND_MEDIA_PLAY             46
#define APPCOMMAND_MEDIA_PAUSE            47
#define APPCOMMAND_MEDIA_RECORD           48
#define APPCOMMAND_MEDIA_FAST_FORWARD     49
#define APPCOMMAND_MEDIA_REWIND           50
#define APPCOMMAND_MEDIA_CHANNEL_UP       51
#define APPCOMMAND_MEDIA_CHANNEL_DOWN     52

#define WM_APPCOMMAND                   0x0319

#endif
