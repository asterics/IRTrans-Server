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
#include <winuser.h>
#include <io.h>
#include <direct.h>
#include <sys/types.h>
#include <time.h>
#include <sys/timeb.h>
#include <process.h>

#define MSG_NOSIGNAL	0

#endif


#ifdef WINCE

#include <winsock2.h>
#include <windows.h>
#include <winuser.h>
#include <time.h>

#define MSG_NOSIGNAL	0
extern int errno;

#endif


#include <stdio.h>
#include <stdlib.h>

#ifdef LINUX
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <netdb.h>
#include <stdint.h>
#include <sys/utsname.h>

typedef int DWORD;

#define closesocket		close
#define _getpid			getpid

extern int hCom;
#endif


#include "remote.h"
#include "dbstruct.h"
#include "network.h"
#include "errcode.h"
#include "fileio.h"
#include "lowlevel.h"
#include "server.h"
#include "global.h"
#include "webserver.h"
#include "flash.h"
#include "xap.h"
#include "ccf.h"

#ifdef LINUX
SOCKET local_socket;
#define LIRCD			"/dev/lircd"
#define PERMISSIONS		0666
#endif

#ifdef WIN32

#include "winusbio.h"
#include "winio.h"

#endif

int		InitServerSocket (SOCKET *,SOCKET *, SOCKET *, SOCKET*);
int		ConfigSocket (SOCKET *sock,unsigned short port);
void	ExecuteNetCommand (SOCKET sockfd);
void	DoExecuteNetCommand (int client,NETWORKCOMMAND *com,STATUSBUFFER *stat);
void	PutNetworkStatus (int res,char msg[],STATUSBUFFER *stat);
void	PutDeviceStatus (NETWORKMODE *mode);
void	PutDeviceStatusEx (void *pnt,byte command,byte offset,int ver);
int		CheckIdx (char ip[],int idx,NETWORKSTATUS *stat);
int		GetNetworkClient (SOCKET sockfd);
int		ReadIRDatabase (void);
int		ExecuteReceivedCommand (byte command[],int len,int bus);
SOCKET	register_remote_client (SOCKET fd,int mode);
void	clear_existing_socket (SOCKET fd);
void	process_lirc_command (SOCKET fd);
void	process_udp_command (char *data,int len,struct sockaddr_in *send_adr);
void	lirc_list_command (SOCKET fd,char rem[],char key[],char msg[]);
void	lirc_send_success (SOCKET fd,char msg[]);
void	lirc_send_error (SOCKET fd,char msg[],char err[]);
void	CloseIRSocket (int client);
void	ResultStoreTiming (IRDATA *ird,NETWORKTIMING *stat);
int		GetHotcode (char rem[],char com[],byte data[]);
void	ReformatHotcode (byte data[],int len);
void	display_usage (void);
void	compress_lcdbuffer (LCDCOMMAND *lcd,char *buf,int bus);
int		count_difference (LCDCOMMAND *lcd);
void	compress_char (char ln[],char src,char tar);
int		OpenIRTransLANSocket ();
int		OpenIRTransBroadcastSockets (void);
void	GetBusList (REMOTEBUFFER *buf,int offset);
void	SendUDPAck (char stat[],struct sockaddr_in *target,int port);
void	DoExecuteASCIICommand (byte command[],SOCKET sockfd,int client,int asciimode);
void	processArgs (int argc,char *argv[]);
void	SetLearnstatus (int client);
int		ConvertLongcode (byte cmd[],int len,byte target[]);



#ifdef WIN32

int build_event_table (HANDLE events[],byte event_type[],byte event_num[],OVERLAPPED OvCom[],int *ser_event);
int get_selected_event (int eventnum,HANDLE events[],byte event_type[],byte event_num[],SOCKET *sockfd);
int AnalyzeUDPString (char *st,int *netcom,char *remote,char *command,char *ccf,int *netmask,int *bus,int *led,int *port,int *macro_wait);
void IRTControlHandler (DWORD request);
void IRTransService(int argc, char** argv);

WSAEVENT SockEvent;
WSAEVENT LircEvent;
WSAEVENT UDPEvent;
WSAEVENT WebEvent;
WSAEVENT xAPEvent;
WSAEVENT IrtLanEvent;

SERVICE_STATUS ServiceStatusIRT; 
SERVICE_STATUS_HANDLE hStatusIRT; 

#endif


int comioflag;
char virt_comnames[50][100];
char virt_comfiles[50][100];
byte xbmc_mode;
byte IRDataBaseRead;
byte lcd_init = 1;
STATUS_BUFFER_N remote_statusex[MAX_IR_DEVICES];
byte status_changed = 1;
time_t status_cache_timeout = 86400;
time_t last_status_read;
byte last_adress,resend_flag;
byte ascii_initial[4];
byte statusmode_short = 1;

SOCKET server_socket;
SOCKET lirc_socket;
SOCKET udp_socket;
SOCKET web_socket;
SOCKET udp_relay_socket;


extern SOCKET irtlan_socket;
extern SOCKET irtlan_outbound;
extern unsigned long XBMC_last_ping;

char udp_relay_host[100];
int udp_relay_port = 0;
char udp_relay_format[255];
int irtrans_udp_port = IRTRANS_PORT;

unsigned short webport = 0;
unsigned int seq_client = 10;
unsigned int seq_call;
unsigned int netmask[32];
unsigned int netip[32];
byte netcount = 0;

NETWORKCLIENT sockinfo[CLIENT_COUNT];


extern IRREMOTE *rem_pnt;
extern int rem_cnt;
extern IRCOMMAND *cmd_pnt;
extern int cmd_cnt;
extern char irtrans_usage[];

unsigned int mode_flag = 0;
char logfile[256];
char hexfile[256];
char baudrate[10];
char pidfile[256];
char irdb_path[256];
FILE *hexfp;
byte hexflag;
char irserver_version[20];
char irtrans_version[100];
char err_remote[81];
char err_command[21];
FILE *logfp;
byte time_len;
byte new_lcd_flag;
int display_bus;
int protocol_version;
int device_wait = 5;
char init_message[4096];
char serverdir[255];
char *startparm;
int device_id = -1;
int eeprom_id = -1;



//	System:
//	LOCALE für Fehlermeldungen
//	Timeout bevor Repeat beginnt
//	Fehlermeldungen mit Parametern erweitern.
//	Neuer Parameter für remotes Verzeichnis


//	Diverses:
//	Export / Import

//	Adresse für LCD übergeben



//	1.0.0	Erstes Final Release								MM		20.04.03
//	2.0.0	Neues Network Format (Rückgabe)						MM		25.04.03
//	2.0.1	Fehlermeldung bei nicht definiertem Hotcode überm.	MM		25.04.03
//	2.0.2	Erkennung von globalen Wiederholungscodes			MM		26.04.03
//	2.0.3	Erweiterungen des Loggings							MM		01.05.03
//	2.0.4	Erweiterungen des Loggings							MM		02.05.03
//	2.1.0	Neues Network Command "Reload" (Girder Client)		MM		06.05.03
//	2.1.1	Kein Bus Scan beim Start des Systems				MM		07.05.03
//	2.2.0	LCD Support											MM		11.05.03
//	2.2.1	Ausblenden von Togglebits auf dem Server			MM		15.05.03
//	2.2.2	LIRC Socket abschaltbar								MM		19.05.03
//	2.3.1	LCD Support erweitert								MM		09.06.03
//	2.3.2	LCD Komprimierung implementiert						MM		10.06.03
//	2.3.3	Lernen jetzt auch mit alten Timings möglich			MM		02.07.03
//	2.3.4	Byteordererkennung und Korrektur (PowerPC)			MM		05.07.03
//	2.3.5	Fixed LCD Display init Bug							MM		17.07.03
//	2.3.6	Changed Receive Code								MM		24.07.03	
//	2.3.7	Last Command Korrektur bei nicht bekannten Befehlen	MM		24.07.03	
//	2.3.8	B&O Anpassungen										MM		02.08.03
//	2.3.9	B&O Anpassungen										MM		04.08.03
//	2.3.10	B&O Carrier											MM		07.08.03
//	2.3.11	Bug nach Client Connect unter LINUX beseitigt		MM		11.09.03
//	2.3.12	Command Nummer für MyHTPC Client ergänzt			MM		15.09.03
//	2.3.13	Anpassung an neue FW Versionen						MM		17.09.03
//	2.3.14	Bug in learnok behoben								MM		23.09.03
//	2.3.15	Small corrections									MM		24.09.03
//	2.3.16	Check for duplicate Commands						MM		24.09.03
//	2.3.17	Send all instances of duplicate Commands			MM		27.09.03
//	2.3.18	Support for temperature sensors						MM		28.09.03
//	2.3.19	Adresse des Empfängers wird an Client übergeben		MM		12.10.03
//	2.3.20	Send Routing										MM		12.10.03
//	2.3.21	Globales Toggle Bit supported						MM		17.10.03
//	2.3.22	Set ID supported									MM		03.11.03
//	2.3.23	UDP Support											FB		08.11.03
//	2.3.24	PowerOn Command wird gespeichert					MM		09.11.03
//	2.3.25	Fehler bei Short Repeat beseitigt					MM		11.11.03
//	2.3.26	Serial Number + no_reset als Default				MM		12.11.03
//	2.3.27	Bugfixes											MM		21.12.03
//	3.0.0	Webserver integriert								MM		24.12.03
//	3.0.1	Bugfix Device Status								MM		05.01.04
//	3.0.2	Neue Option -reset_eeprom							MM		06.01.04
//	3.1.0	Protocol Version im Netzwerk						MM		07.01.04
//	3.1.1	Liste der FBs + Commands							MM		17.01.04
//	3.1.2	Translator Support erweitert						MM		21.01.04
//	3.1.3	Altes Commandformat wird parallel unterstützt		MM		03.02.04
//	3.1.4	Translator erweitert								MM		08.02.04
//	3.1.5	Code Korrektur von RCMM Codes						MM		01.03.04
//	3.2.0	Support for CCF Codes								MM		05.03.04
//	3.2.2	Parallele Unterstützung von altem & neuem Format	MM		17.03.04
//	3.2.3	Fehler bei Timingneuerkennung beseitigt				MM		01.04.04
//	3.2.4	Pronto Codes: Remotes bei Repeat Codes ergänzt		MM		02.04.04
//	3.2.5	CCF Bugfix											MM		03.04.04
//	3.2.6	Test ob Command vorhanden							MM		09.04.04
//	3.2.7	RAW HF Codes werden richtig gesendet				MM		12.04.04
//	3.2.8	Korrektur von LIRC Send Command						MM		13.04.04
//	3.2.9	Returnstruct von Testcommand verkürzt				MM		25.04.04
//	3.2.10	Empfang langer Pakete korrigiert					MM		28.04.04
//	3.2.11	RAW CCF Bugfix										MM		29.04.04
//	3.2.12	CCF Support für RC5 & RC6							MM		30.04.04
//	3.2.13	Lernen von Codes wenn nur Timing da ist				MM		30.04.04
//	3.2.14	Erweiterung des RAW Supports						MM		01.05.04
//	3.2.15	Variable Flashwordsize								MM		01.05.04
//	3.2.16	LONGSEND Support									MM		02.05.04
//	3.2.17	Server Shutdown via Client							MM		07.05.04
//	3.2.18	Translator Support: Gruppen							MM		10.05.04
//	3.2.19	RC6A												MM		11.05.04
//	3.2.20	Mixed fixes											MM		13.05.04
//	3.2.21	Korrektur Receive kurzer RAW Codes					MM		15.05.04
//	3.2.22	Senderauswahl via Adress							MM		17.05.04
//	3.2.23	Capabilities in Statusabfrage						MM		02.06.04
//	3.2.24	Flash Translator via SBUS							MM		03.06.04
//	3.2.25	Neuer Debug Mode -codedump							MM		19.06.04
//	3.2.26	Crash bei großen Hexdumps beseitigt					MM		20.06.04
//			Sendmask für Translatorbefehle
//	3.2.27	CCF RAW Bugs beseitigt								MM		21.06.04
//	3.2.28	Old Commandbuffer Bug fixed							MM		11.07.04
//	3.2.29	Berechtigung über IP Adressen						MM		03.08.04
//	3.3.0	Macro Support										MM		09.08.04
//	3.3.1	Support für Macro Wait Translator					MM		16.08.04
//	3.3.2	Bug behoben: Multisend an Clients					MM		21.08.04
//	3.3.3	Support der -learned_only Funtion					MM		24.08.04
//	3.3.4	CCF Präzision verbessert							MM		24.08.04
//	3.3.5	Neue usage Meldung beim Start						MM		25.08.04
//	3.3.6	UDP Relay											MM		04.09.04
//	3.3.7	Keine Netmask auf Local Socket						MM		04.09.04
//	3.3.8	CCF Präzision weiter verbessert						MM		04.09.04
//	4.1.1	Support für TIME_LEN = 8							MM		11.09.04
//	4.1.2	Fehler bei Togglecodes behoben						MM		12.09.04
//	4.1.3	Commandlist Togglecodes								MM		15.09.04
//	4.1.4	Fehler in Swap_Irdata beseitigt						MM		19.09.04
//	4.1.5	Uneed Version										MM		21.09.04
//	4.2.1	CCF RAW Codes mit Repeat							MM		26.09.04
//			CCF Rundung von Timingwerten optimiert				MM		26.09.04
//	4.2.2	CCF Mode 7000 Codes									MM		27.09.04
//	4.3.1	Support for extended IR Carrier						MM		29.09.04
//	4.3.2	64 Bit Support for AMD64 on LINUX					MM		05.10.04
//	5.1.1	Multiple Device support								MM		10.10.04
//	5.1.2	Multiple Device extensions							MM		16.10.04
//	5.1.3	Multiple Switch devices								MM		17.10.04
//	5.1.4	Extended Carriermode für mehrere Devices			MM		21.10.04
//	5.1.5	Support für Reset Display Option					MM		22.10.04
//	5.1.6	Mediacenter Support									MM		24.10.04
//	5.1.7	Mediacenter Remote beschleunigt						MM		25.10.04
//	5.1.8	Fix für Translator Flash Problem					MM		28.10.04
//	5.1.9	Erweiterung Multibus Send							MM		31.10.04
//	5.1.10	V5 für LINUX										MM		02.11.04
//	5.1.11	LCD Init bei Display Reset							MM		05.11.04
//	5.1.12	CCF Tolerance erweitert								MM		08.11.04
//	5.1.13	Capability Fix für alte Translator					MM		08.11.04
//	5.1.14	MCE Remote Erweiterung								MM		15.11.04
//	5.1.15	LINUX USB I/O										MM		16.11.04
//	5.1.16	Diag Message removed								MM		04.12.04
//	5.1.17	Mediacenter IR support Standard						MM		07.12.04
//	5.1.18	Support für neue Statusflags (Receive Timeout)		MM		10.12.04
//	5.1.19	Receive while LCD Update							MM		12.12.04
//	5.1.20	Receive while LCD Update LINUX						MM		13.12.04
//	5.1.21	Set Status Webserver korrigiert						MM		13.12.04
//	5.1.22	Charset Konvertierung								MM		14.12.04
//	5.1.23	Numeric Codes für B&O angepaßt						MM		14.12.04
//	5.1.24	RC6 Send Fix										MM		15.12.04
//	5.1.25	Charset Konvertierung								MM		15.12.04
//	5.1.26	RC6 + RAW Repeat									MM		15.12.04
//	5.1.27	Tastenentprellung									MM		17.12.04
//	5.1.28	Get Device Command									MM		19.12.04
//	5.2.1	MCE Steuerung konfigurierbar						MM		20.12.04
//	5.2.2	I/O für langsame Systeme weiter verbessert			MM		24.12.04
//	5.2.3	Realtime Clock Support								MM		27.12.04
//	5.2.4	Realtime Clock on Shutdown							MM		28.12.04
//	5.2.5	Realtime Clock set on Shutdown						MM		29.12.04
//	5.2.6	Genauere Zeit für die Realtime Clock				MM		31.12.04
//	5.2.7	Support für einstellbaren Learn Timeout				MM		02.01.05
//	5.2.8	Device Status über den Bus							MM		03.01.05
//	5.2.9	Erweiterte Keyboard Steuerung						MM		06.01.05
//	5.2.10	Korrektur Mediacenter Funktionen					MM		17.01.05
//	5.2.11	Support für Character Sets							MM		18.01.05
//	5.2.12	MCE Volume Fix										MM		19.01.05
//	5.2.13	MCE Volume Fix										MM		19.01.05
//	5.2.14	Special Character Support							MM		20.01.05
//	5.2.15	Support für Uneed Mega32 Modul						MM		20.01.05
//			Leeren Translator flashen							MM		20.01.05
//			0 Chars auf LCD Display								MM		20.01.05
//	5.2.16	Neues LCD Protokoll									MM		21.01.05
//	5.2.17	VFD Level Support									MM		22.01.05
//	5.2.18	LCD Mode fix										MM		24.01.05
//	5.2.19	Korrektur Init EEPROM (Extended Mode 2)				MM		27.01.05
//	5.2.20	Korrektur Resend über mehrere Busse					MM		27.01.05
//	5.2.21	Fix for Send with very old devices					MM		01.02.05
//	5.2.22	Update für Multikeys Translator						MM		04.02.05
//	5.2.23	MCE Commands via Events								MM		08.02.05
//	5.2.24	Support für 1MHz Carrier							MM		16.02.05
//	5.2.25	Support für LCD Module V 2.0						MM		20.02.05
//	5.2.26	Support für UNEED V2								MM		22.02.05
//	5.2.27	Displaytyp am Bus einstellbar						MM		22.02.05
//	5.2.28	CH+ / CH- wieder über Keyboard an Mediacenter		MM		24.02.05
//	5.3.01	Events beliebig konfigurierbar						MM		27.02.05
//	5.3.02	Event Steuerung für externe Apps erweitert			MM		02.03.05
//	5.3.03	Support für RC5/RC6 Notoggle Flag					MM		08.03.05
//	5.3.04	Bug für Sendkey ohne Window behoben					MM		09.03.05
//	5.3.05	Support für Suspend / Resume						MM		24.03.05
//	5.3.06	Reihenfolge beim Exit geändert						MM		24.03.05
//	5.3.07	Zeichenumsetzung erweitert (FR)						MM		29.03.05
//	5.3.08	Sommerzeit Fix										MM		02.04.05
//	5.3.09	Fix für alte LCDs									MM		06.04.05
//	5.3.10	Fix für alte Uneed LCDs								MM		08.04.05
//	5.3.11	Neues Networkcommand DELETE_COMMAND					MM		11.04.05
//			Nach Delete keine Leerzeichen mehr am Fileende		MM		11.04.05
//	5.3.12	Kleine Erweiterungen für schnellen SBUS				MM		17.04.05
//	5.3.13	Erweiterungen für SBUS Flash Translator (Fast SBUS)	MM		19.04.05
//	5.3.14	COMMAND_EMPTY für PHP								MM		01.05.05
//	5.3.15	Mehrer IRTrans + DisplayIRTrans						MM		02.05.05
//	5.3.16	Timeout für Status Cache							MM		04.05.05
//	5.3.17	Statusproblem LINUX mit großen Buffern				MM		05.05.05
//	5.3.18	Update für Translator Flash via SBUS (Classic)		MM		09.05.05
//	5.3.19	Neue Option -hexfile								MM		09.05.05
//	5.3.20	Serielles IF Handling geändert						MM		11.05.05
//	5.3.21	xAP Support											MM		16.05.05
//	5.3.22	xAP Pronto Support									MM		17.05.05
//	5.3.23	Längenangabe [RCV-LEN] im .rem File					MM		18.05.05
//	5.3.24	IRDA Support										MM		20.05.05
//	5.3.25	Wait nach Setstatus verlängert						MM		23.05.05
//	5.3.26	Support für PowerOff Code							MM		23.05.05
//	5.3.27	RCV-LEN korrigiert									MM		23.05.05
//	5.3.28	Support für PowerOn + PowerOff						MM		24.05.05
//	5.3.29	Send Result Sockethandling geändert					MM		27.05.05
//	5.3.30	Dummydevice für Tests ohne IRTrans Hardware			MM		02.06.05
//	5.3.31	IRDA RAW Modus										MM		05.06.05
//	5.3.32	IRDA Data Modus										MM		07.06.05
//	5.3.33	Errorstatus für GetRemoteCommands					MM		15.06.05
//	5.3.34	Set Brightness Command								MM		17.06.05
//	5.3.35	Instant Receive Timeout vergrößert					MM		17.06.05
//	5.3.36	LIRC LIST Command extended							MM		29.06.05
//	5.3.38	LIRC LIST Command corrected							MM		29.06.05
//	5.4.01	Support for IRTrans LAN module						MM		18.07.05
//	5.4.02	Fix for ReadSerialStringEx							MM		22.07.05
//	5.4.03	Get Status für LAN Modul							MM		25.07.05
//	5.4.04	LINUX USB Paths extended							MM		08.08.05
//	5.4.05	Autoscan new USB paths								MM		09.08.05
//	5.4.06	New Function Keys									MM		09.08.05
//	5.4.07	Support für IRDB Devices							MM		15.08.05
//	5.4.08	RS232 Baudrate										MM		18.08.05
//	5.4.09	Support für Binäre IR Actions						MM		20.08.05
//	5.4.10	Support für IRDB & IRTrans LAN						MM		22.08.05
//	5.4.11	LINUX Device Scan erweitert							MM		22.08.05
//	5.4.12	LINUX Support für IRTrans LAN						MM		22.08.05
//	5.4.13	Clock Display Switch								MM		07.09.05
//	5.5.01	Support für MCE Keyboard							MM		09.09.05
//	5.5.02	MCE Keyboard Support über Scancodes					MM		15.09.05
//	5.5.03	MCE Keyboard Maus Support							MM		16.09.05
//	5.5.04	Fixed MCE Bug										MM		18.09.05
//	5.5.05	Weitere MCE Erweiterungen							MM		18.09.05
//	5.5.06	Medialon Hexdump									MM		20.09.05
//	5.5.07	Support für Frontview Treiber						MM		02.10.05
//	5.5.08	Timestamp im Logfile								MM		12.10.05
//	5.5.09	Erweiterung Ethernet Learn mode						MM		13.10.05
//	5.5.10	-deviceinfo für LAN Versionen						MM		17.10.05
//	5.5.11	Autofind LAN devices on startup						MM		18.10.05
//	5.5.12	Warnung vor dem Überschreiben von Translations		MM		01.11.05
//	5.5.14	Support für Combobox Translation / IRDB				MM		02.11.05
//	5.5.15	Erweiterung auf max. 256 Busse						MM		04.11.05
//	5.5.16	Auflistung der IRTrans Busse						MM		05.11.05
//	5.5.17	Send CCF											MM		09.11.05
//	5.5.18	LANIO Sleep geändert								MM		10.11.05
//	5.5.19	Send CCF lang										MM		11.11.05
//	5.5.20	Fehler in Learn Next IR korrigiert					MM		11.11.05
//	5.5.21	LiveTV Fehler korrigiert							MM		11.11.05
//	5.5.22	Support für Send RAW aus IRDB						MM		13.11.05
//	5.5.23	-no_lirc Option erweitert							MM		19.11.05
//	5.5.24	-ip_relay Option									MM		23.11.05
//	5.5.25	Learn Direct										MM		26.11.05
//	5.5.26	Repeat von Befehlen erweitert						MM		27.11.05
//	5.5.27	Pause beim Repeat von Befehlen						MM		28.11.05
//	5.5.28	Fehler bei illegalen Timings in .rem Files			MM		02.12.05
//	5.5.29	LAN + Busmodule										MM		04.12.05
//	5.5.30	UDP Sendstring										MM		10.12.05
//	5.5.31	UDP Learn erweitert									MM		14.12.05
//	5.5.32	UDP ACK												MM		17.12.05
//	5.5.33	UDP Learn Ready ACK									MM		18.12.05
//	5.5.34	TESTCOMMANDEX										MM		19.12.05
//	5.5.35	CCF Carrier > 400 korrigiert						MM		26.12.05
//	5.5.36	CCF Handling erweitert								MM		26.12.05
//	5.5.37	UDP ACK Port wählbar								MM		27.12.05
//	5.5.38	UDP ACK Port wählbar für Ping						MM		27.12.05
//	5.5.39	UDP Format angepasst								MM		27.12.05
//	5.5.40	UDP Send CCF										MM		29.12.05
//	5.5.41	-daemon Mode / xAP Port unter LINUX					MM		02.01.06
//	5.5.42	xAP Heartbeat korrigiert							MM		12.01.06
//	5.5.43	ms Timestamps (Windows)								MM		22.01.06
//	5.5.44	Macrobefehle korrigiert								MM		23.01.06
//	5.5.45	Macropause korrigiert								MM		24.01.06
//	5.5.46	Fehler bei Repeatcodes korrigiert					MM		26.01.06
//	5.5.47	Erweiterte Statuscodelänge							MM		29.01.06
//	5.5.48	SetstatusEx											MM		30.01.06
//	5.5.49	Setatatus Timeout									MM		31.01.06
//	5.5.50	Send Log erweitert									MM		08.02.06
//	5.5.51	Support für > 16 USB Devices						MM		09.02.06
//	5.5.52	Clear NETWORKRCV Buffer vor jedem Push				MM		10.02.06
//	5.5.53	Bus in Address bei recv_command richtig gefüllt		MM		13.03.06
//	5.5.54	Delete Remote										MM		16.03.06
//	5.6.01	Support for Long IR Codes							MM		24.04.06
//	5.6.02	Longsend korrigiert									MM		26.04.06
//	5.6.03	Sort distinct device Nodes							MM		28.04.06
//	5.6.04	Bereiche für LINUX USB Devices						MM		01.05.06
//	5.6.05	Fix für LINUX USB Devices							MM		02.05.06
//	5.6.06	Include Names für LAN Translator					MM		05.05.06
//	5.6.07	Support für Multi LEDs								MM		11.05.06
//	5.6.08	Differenz Count Displayversorgung = 16				MM		15.05.06
//	5.6.09	Pulse/Pause = 0->1 (CCF Codes)						MM		24.05.06
//	5.6.10	Versionsunterscheidung Translator FW				MM		24.05.06
//	5.6.11	Timingkorrektur je nach Carrier FREQ				MM		29.05.06
//	5.6.12	Support für Toggle Codes in IRDB					MM		30.05.06
//  5.6.13  Auch in /etc/irserver/remotes suchen				SvS		02.06.06
//	5.6.14	RCV-LEN für Translator								MM		13.06.06
//	5.6.15	Support für Hostbroadcast							MM		28.06.06
//	5.6.16	Set Relais											MM		04.07.06
//	5.6.17	Relaisauswahl IRDB									MM		07.07.06
//	5.6.18	Neues DB Format										MM		12.07.06
//	5.6.19	Sonderzeichen in der apps.cfg						MM		14.07.06
//	5.6.20	Div. lcdproc Korrekturen							MM		23.07.06
//	5.6.21	Unterschiedliche Displaytypen						MM		26.07.06
//	5.6.22	Bug beim Status mehrerer LAN Module behoben			MM		27.07.06
//	5.6.23	Webserver defaultmäßig aus							MM		09.08.06
//	5.6.24	-pidfile Option										MM		10.08.06
//	5.6.25	-wait Option										MM		01.09.06
//	5.6.26	2s Wait als default									MM		01.09.06
//	5.6.27	Path Variable für IR Datenbank						MM		05.09.06
//	5.6.28	Status message bei IRDB Flash						MM		09.09.06
//	5.6.29	Client ID in logs									MM		09.09.06
//	5.6.30	Client ID in logs									MM		11.09.06
//	5.6.31	Read Analog Inputs									MM		03.10.06
//	5.6.32	Read Analog Inputs V2								MM		14.10.06
//	5.6.33	Init Server für LINUX korrigiert					MM		20.11.06
//	5.6.34	Mac Adresse für Set Status gefiltert				MM		12.12.06
//	5.6.35	RS232 Send für LAN Controller XL					MM		19.12.06
//	5.7.01	ASCII Commands										MM		24.12.06
//	5.7.02	Flash User HTML Pages								MM		26.12.06
//	5.7.03	IP + MAC in Device Node String						MM		03.01.07
//	5.7.04	IP + MAC in Device Node String korrektur			MM		03.01.07
//	5.7.05	No Toggle bei RC5/6 Relay							MM		07.01.07
//	5.7.06	ASCII Send / Receive								MM		08.01.07
//	5.7.07	Support für mehr als 8 Busse beim Device Status		MM		12.01.07
//	5.7.08	IRDB EX Format										MM		24.01.07
//	5.7.09	UDP Port Option										MM		24.01.07
//	5.7.10	ASCII Getremotes / Getcommands						MM		26.01.07
//	5.7.11	FT2232 Support										MM		22.02.07
//	5.7.12	Debug Log extension									MM		23.02.07
//	5.7.13	Debug Log extension									MM		23.02.07
//	5.7.14	Handshake Wait LINUX								MM		23.02.07
//	5.7.15	Handshake Wait LINUX								MM		23.02.07
//	5.7.16	Error bei Berechnung Action LEN IRDBEX				MM		01.03.07
//	5.7.17	SEQ Nr. für USB Devices								MM		03.03.07
//	5.7.18	Timeout Values changed								MM		05.03.07
//	5.7.19	Timeout Values changed								MM		08.03.07
//	5.7.20	send_forward Option									MM		15.03.07
//	5.7.21	USB COM Port support für Vista x64					MM		30.03.07
//	5.7.22	LINUX mehrere Codes in einem Read					MM		16.04.07
//	5.7.23	LINUX USB Read Timeout								MM		17.04.07
//	5.8.01	Long Code Calibration								MM		23.04.07
//	5.8.02	RCMM Adjustments									MM		02.05.07
//	5.8.03	RCMM Adjustments									MM		02.05.07
//	5.8.04	RCMM Adjustments									MM		03.05.07
//	5.8.05	SendCCF Mods										MM		09.05.07
//	5.8.06	LAN Autofind mit mehreren LAN Interfaces			MM		29.05.07
//	5.8.07	Select LAN Interface to use							MM		31.05.07
//	5.8.08	Multiple device support LINUX						MM		01.06.07
//	5.8.09	Startup Delay										MM		07.06.07
//	5.8.10	Device DB Off error									MM		11.06.07
//	5.8.11	MCE Handling erweitert (C. Tergusek)				MM		13.06.07
//	5.8.12	CCF Bugfix											MM		13.06.07
//	5.8.13	New Flash Transfer Status code						MM		14.06.07
//	5.8.14	Fix für TCP/IP ASCII Commands						MM		14.06.07
//	5.8.15	Flash Transfer status								MM		19.06.07
//	5.8.16	Liste von LAN Modulen ohne Init						MM		25.06.07
//	5.8.17	Korrektur für Timing bei genau 256 Befehlen in IRDB	MM		06.07.07
//	5.8.18	Commandlist mit > 200 Commands						MM		06.07.07
//	5.9.01	x64 Port											MM		03.08.07
//	5.9.02	Toggle Command Support für Translator				MM		07.08.07
//	5.9.03	Remote Open Statuscode								MM		08.08.07
//	5.9.04	BSD license											MM		17.08.07
//	5.9.05	Umlaute am Ende von Strings (DBReadString)			MM		04.09.07
//	5.9.06	Support für Relaissteuerung via Broadcast			MM		21.09.07
//	5.9.07	Fix für Suche nach WLAN Interfaces (LINUX/N800)		MM		16.10.07
//	5.9.08	Reset Windows timeouts (Screen Saver/Standby)		MM		12.11.07
//	5.9.09	Output LAN IP Address on Startup					MM		16.11.07
//	5.9.10	Statusmessage for IRDB Upload						MM		17.11.07
//	5.9.11	Support für Shift Gruppen (Translator)				MM		26.11.07
//	5.9.12	Pfadangabe für IRDB Pfad (-remotes <pfad>)			MM		27.11.07
//	5.9.13	Fix für MCE DVD Subtitle							MM		16.12.07
//	5.9.14	Support für Framelength in .rem File				MM		20.12.07
//	5.9.15	Erweiterte IRDB Struktur							MM		28.12.07
//	5.10.01	Erweiterte IRDB Struktur							MM		30.12.07
//	5.10.02	SMS Keypad handling MCE								MM		08.01.08
//	5.10.03	MCE	Screensaver handling							MM		08.01.08
//	5.10.04	Virtual Keycodes für +/-/*//						MM		15.01.08
//	5.10.05	Initialisierung Err Status mit Space				MM		15.01.08
//	5.10.06	Load IRDB from file									MM		22.01.08
//	5.10.07	Support for VK_MENU									MM		04.03.08
//	5.10.08	Support for modifier keys with function keys		MM		07.03.08
//	5.10.09	Support für Windows NT								MM		13.03.08
//	5.10.10	-debug_timing Option								MM		03.04.08
//	5.10.11	RCMM Korrekturen									MM		06.04.08
//	5.10.12	Close falscher USB Devices							MM		11.04.08
//	5.10.13	Extended capabilities								MM		25.04.08
//	5.10.14	Sequence Mode für Resend Restart-sicher				MM		26.04.08
//	5.11.01	Neue ADC IRDB Struktur (1wire I/O)					MM		16.05.08
//	5.11.02	Korrektur Long Code Calibrate mit Max.Len			MM		25.05.08
//	5.11.03	-debug_timing für RAW Commands						MM		28.05.08
//	5.11.04	Always RAW Receive für LAN Module					MM		31.05.08
//	5.11.05	Support für Pulse 200 Modus							MM		09.06.08
//	5.11.06	RCV-LEN und RCV-START in der IRDB					MM		17.06.08
//	5.11.07	Serno ausblenden wenn leer							MM		01.07.08
//	5.11.08	Extended Getversion									MM		09.07.08
//	5.11.09	Fix für IRTIMING Translator/CCF						MM		24.07.08
//	5.11.10	Fix für send CCF aus IRDB							MM		25.07.08
//	5.12.01	IRServer as a service								MM		31.07.08
//	5.12.02	Loginfo für Clients									MM		31.07.08
//	5.12.03	Korrektur Timing Compare RC5/RC6					MM		04.08.08
//	5.12.04	Support für Sonavis IR Keyboard						MM		08.08.08
//	5.12.05	Store von Timings mit 8 Paaren						MM		11.08.08
//	5.12.06	Store CCF IR Commands via Client Interface			MM		19.08.08
//	5.12.07	Lernen von Timings in Datei mit Commands ohne Tim.	MM		20.08.08
//	5.12.08	Pause Key in apps.cfg								MM		26.08.08
//	5.12.09	Resend logging										MM		12.09.08
//	5.12.10	Mausbeschleunigung in hid.c							MM		22.09.08
//	5.12.11	RECS80												MM		25.09.08
//	5.12.12	-no_init_lan unter Windows korrigiert				MM		16.10.08
//	5.20.01	New extended Device Status							MM		02.11.08
//	5.20.02	Framelength bei Debug Timing						MM		27.11.08
//	5.20.03	Framelength bei Debug Timing (Korrektur)			MM		10.12.08
//	6.00.00	First Beta Release new learning						MM		10.12.08
//	6.00.01	Learnstatus Anzeige ohne Calibration Info			MM		11.12.08
//	6.00.02	Send RAW Mode mit [FREQ-MEAS]						MM		11.12.08
//	6.00.03	Spezieller RCMM Support								MM		12.12.08
//	6.01.00	Beliebige Togglebits								MM		20.12.08
//	6.01.01	RC6 CCF Codes ohne START_BIT / REPEAT_START			MM		27.12.08
//	6.01.02	RC5/RC6 NO_TOGGLE Handling beim Lernen				MM		27.12.08
//	6.01.03	Learn Command Status								MM		27.12.08
//	6.01.04	7 Byte Learncommands								MM		03.01.09
//	6.01.05	Fix Togglehandling									MM		10.01.09
//	6.01.06	Set Soft Device_ID									MM		18.01.09
//	6.01.07	Komma / Punkt für apps.cfg							MM		20.01.09
//	6.01.08	Get Device Status Timeout LAN						MM		02.02.09
//	6.01.09	Clear WakeOnLAN MAC									MM		04.02.09
//	6.01.10	Init RS232 Devices									MM		05.02.09
//	6.01.11	UDP Send: LED Select 1-8							MM		09.02.09
//	6.01.12	Debug Info Get Device Status						MM		10.02.09
//	6.01.13	Waittime Get Device Status							MM		10.02.09
//	6.01.14	RCMM Toggle / Duty Cycle via .rem File				MM		11.02.09
//	6.01.15	ACK Handshake für LAN Module						MM		12.02.09
//	6.01.16	Alle Togglebits in einer Remote mit gleicher NUM	MM		17.02.09
//	6.01.17	State I/O IR Translator								MM		01.03.09
//	6.01.18	RS232 Output										MM		08.03.09
//	6.01.19	Fix crash bei Resend								MM		08.03.09
//	6.01.20	RS232 Output										MM		11.03.09
//	6.01.21	Fix für Learnstat bei RAW Codes						MM		19.03.09
//	6.01.22	RAW Codes mit zu großen Längen						MM		20.03.09
//	6.02.01	Release Version RS232 out							MM		20.03.09
//	6.02.02	Binary Line Mode Mediacontroller					MM		21.03.09
//	6.02.03	\r / \n als RS232 Characters						MM		23.03.09
//	6.02.04	Wait Timeout RS232									MM		29.03.09
//	6.02.05	LED Select 9-16										MM		06.04.09
//	6.02.06	State input im .rem File							MM		18.04.09
//	6.02.07	Err Message bei fehlenden Remotes/Commands Transl.	MM		24.04.09
//	6.02.08	Wartezeit bei Read Device Status alte Versionen		MM		10.05.09
//	6.02.09	Additional USB Device nodes	LINUX					MM		10.05.09
//	6.02.10	Wartezeit bei Read Device Status alte Vers. LINUX	MM		10.05.09
//	6.02.11	Ext LED Select Ausgabe für LED > 8					MM		19.05.09
//	6.02.12	Überlauf in StorIRTiming bei langen Parametern		MM		20.05.09
//	6.02.14	Port Reset bei Reconnect							MM		08.06.09
//	6.02.15 WriteStatus Timeout added							MM		12.06.09
//	6.02.16 Virtual COM Port handling							MM		13.06.09
//	6.02.17 SEQ Numbers für Virtual COM Ports					MM		15.06.09
//	6.02.18 Redirect RS232 Inputs in Datei						MM		15.06.09
//	6.02.19 Select LED Mask										MM		18.06.09
//	6.02.20 Fix für Überlauf bei RS232 Learn					MM		11.07.09
//	6.02.21 Find devices under High IR Load						MM		13.07.09
//	6.02.22 Send RS232 to all bus devices						MM		13.07.09
//	6.02.23 LAN Send ACK für Resend								MM		19.07.09
//	6.02.24 Check von Version für IR Repeat über Frame Length	MM		28.07.09
//	6.02.25 IRDB Definition List LINUX							MM		05.10.09
//	6.02.26 Baudrate 57600/115200 für RS232 Geräte				MM		06.10.09
//	6.02.30 Deinit Server connection für 2. Interface			MM		09.10.09
//	6.02.31 Autofind all LAN Devices							MM		11.10.09
//	6.02.32 Prüfung ob IR Ausgang gültig ist					MM		30.10.09
//	6.02.33 Prüfung ob RS232 Ausgang gültig ist					MM		03.11.09
//	6.02.34 Set Modus Debug	/ Fix für verkürzte PowerOn Codes	MM		25.11.09
//	6.02.35 Fix ReadData LEN für Input von LAN Device			MM		04.12.09
//	6.02.36 Overlapped Handling für Serio geändert				MM		07.12.09
//	6.02.37 Neuer Getversion Modus								MM		27.12.09
//	6.02.40 IRTrans WiFi Support								MM		14.01.10
//	6.02.41 WLAN Parameter per WLAN setzen						MM		21.01.10
//	6.02.42 COMMAND_LOGLEVEL									MM		24.01.10
//	6.02.43 WLAN Parameter per WLAN setzen (LINUX)				MM		05.02.10
//	6.02.44 ACK Handshake LAN Devices							MM		15.02.10
//	6.02.45 SizeOf LIRC Domain Socket							MM		18.02.10
//	6.02.46 TCP COM für LAN Devices								MM		22.02.10
//	6.02.50 TCP Reconnect für LAN Devices						MM		23.02.10
//	6.02.51 SO_KEEPALIVE für LAN Devices						MM		25.02.10
//	6.02.52 Active Keep Alive für LAN Devices					MM		09.03.10
//	6.02.53 dummy Device										MM		20.03.10
//	6.02.54 LINK Commands in .rem Files							MM		20.03.10
//	6.02.55 Receive linked Commands in .rem Files				MM		21.03.10
//	6.02.56 New Link via GUI									MM		28.03.10
//	6.02.57 Send Mask für USB Geräte							MM		07.04.10
//	6.02.58 Send Macro für LAN Geräte							MM		24.04.10
//	6.02.59 Fix für LEARN LONG ohne Parameter					MM		04.05.10
//	6.02.60 UDP Send fixed										MM		22.05.10
//	6.02.61 TCP ASCII Send LED Select							MM		22.05.10
//	6.02.62 TCP ASCII Send Error bei ACK Fehler					MM		26.05.10
//	6.02.63 TCP ASCII Agetdevices								MM		27.05.10
//	6.02.64 RS232 I/O für LAN I/O / XL							MM		08.06.10
//	6.02.65 TCP ASCII Command Asndmask							MM		11.06.10
//	6.02.66 OneWire + Analog I/O								MM		15.06.10
//	6.02.67 GetStatus ohne SBUS via TCP							MM		15.06.10
//	6.02.68 Fix für Send Macro commands							MM		25.06.10
//	6.02.69 UDP ASCII Sendmacro									MM		28.06.10
//	6.02.70 Statusport = source									MM		30.06.10
//	6.02.71 IPs für Geräte, die mit "0" beginnen (Oktal)		MM		09.07.10
//	6.02.72 Busauswahl für WLAN und IP Modes					MM		10.07.10
//	6.02.73 SendMask für Multistream fixed						MM		24.07.10
//	6.02.74 SendMask für Multistream fixed						MM		24.07.10
//	6.02.75 Devicestatus Multistream TCP						MM		26.07.10
//	6.02.76 Send Macro mit unterschiedlichen Pausen				MM		28.07.10
//	6.02.78 Deviceliste beim Serverstart neu formatiert			MM		01.08.10
//	6.02.79 Get Status für FW 3.03.00							MM		04.08.10
//	6.02.80 TCP ASCII Sendmacro									MM		04.08.10
//	6.02.81 Send Power über .rem File							MM		09.08.10
//	6.02.82 Hex Zeichen in R232 Strings über \x..				MM		11.08.10
//	6.02.83 Send ASCII Mode mit Framelength						MM		26.08.10
//	6.02.84 CCF Hexcodes NEC Mode								MM		27.08.10
//	6.02.85 Duty Cycle 1:6 in .rem File							MM		14.09.10
//	6.02.86 Access control for IP without Mask					MM		24.09.10
//	6.02.87 OneWire Temp mit Resolution							MM		28.09.10
//	6.02.88 MODE_NO_RECEIVE für GUI Client						MM		23.10.10
//	6.03.01 Find IR Devices mit allen Extended Modes			MM		28.10.10
//	6.03.02 Send Flash Status bei MODE_NO_RECEIVE				MM		28.10.10
//	6.03.03 [RL][RP] fixed										MM		09.11.10
//	6.03.04 Repeat IR Codes über [RL] auch mit Repeat Codes		MM		09.11.10
//	6.03.05 Fix für lange RCV Pakete							MM		09.11.10
//	6.03.06 Minor CCF Fix										MM		09.11.10
//	6.03.07 Fix bei fehlendem Mode NO_RCV (else fehlte)			MM		14.11.10
//	6.03.08 Fix für linked Commands in IRDB						MM		05.11.10
//	6.03.09 IRDB + Translator									MM		17.02.11
//	6.03.10 Framelen beim Senden übergeben						MM		18.05.11
//	6.03.11 Wrong Bus beim Senden prüfen						MM		18.05.11
//	6.03.12 Framelen für Macros korrigiert						MM		18.05.11
//	6.03.13 Framelen für Codes mit Offset korrigiert			MM		26.05.11
//	6.03.14 Send Codes mit Repeat Offset						MM		27.05.11
//	6.03.15 Learn Codes mit Repeat Offset						MM		07.06.11
//	6.03.16 RCV Codes mit Repeat Offset							MM		08.06.11
//	6.03.17 Device Status extended								MM		09.06.11
//	6.03.18 Device Status extended								MM		10.06.11
//	6.03.19 SendMacro mit LED Select							MM		17.06.11
//	6.03.20 Resend Timing USB Geräte							MM		18.06.11
//	6.08.01 Extended Sort Support								MM		20.06.11
//	6.08.02 Support für IRDATA Lang								MM		20.06.11
//	6.08.03 Support für IRDATA Lang								MM		21.06.11
//	6.08.04 LED Select per Send Command in Translator Macro		MM		22.06.11
//	6.08.05 Read Analog Inputs für USB							MM		24.06.11
//	6.08.06 USB	Analog Level Support							MM		27.06.11
//	6.08.07 ReadAnalogInputsEx bei alter Firmware				MM		28.06.11
//	6.08.08 Translator Status									MM		28.06.11
//	6.08.10 Release Version										MM		01.07.11
//	6.08.11 Export Hexcodes										MM		03.07.11
//	6.08.12 Autofind LAN Devices über mehrere Interfaces		MM		05.07.11
//	6.08.14 Fix Export Hexcodes									MM		06.07.11
//	6.08.15 Fix MODE_NO_RECEIVE									MM		09.07.11
//	6.08.16 XBMC Plugin											MM		11.07.11
//	6.08.17 XBMC Plugin	LINUX									MM		12.07.11
//	6.08.18 XBMC Plugin											MM		13.07.11
//	6.08.19 Delay beim ASCII TCP Send							MM		19.07.11
//	6.08.20 Delay beim ASCII TCP Send abhängig von FW Version	MM		19.07.11
//	6.08.21 Neue Flashentry Header Translator					MM		20.07.11
//	6.08.22	RS232 Relay IR Codes erkennen						MM		25.07.11
//	6.08.23	RS232 Relay IR Codes: Set Mode						MM		29.07.11
//	6.08.24 Import Hexcodes										MM		03.08.11
//	6.08.25 Additional log entry for LINUX Select call			MM		15.08.11
//	6.08.26 USB Analog level mit Relais Steuerung				MM		17.08.11
//	6.08.27 Modified GetLearnStatus call						MM		25.08.11
//	6.08.28 IRTrans USB Reconnect (LINUX)						MM		24.09.11
//	6.08.29 IRDB Receive Error fix (Load IRDB)					MM		14.10.11
//	6.08.30 UDP ASCII Error Messages							MM		14.11.11
//	6.08.31 Bugfix IRDB Dialog mit Toggle Befehlen >200 CMDs	MM		19.11.11
//	6.08.32 Debug Output modified								MM		23.11.11
//	6.08.33 Bus Check bei SendRS232								MM		24.11.11
//	6.08.34 DeviceStatus EX3 mit LAN Version					MM		27.11.11
//	6.08.35 DeviceStatus EX3 mit LAN Version					MM		27.11.11
//	6.08.36 Empty .rem Files / Rem File Error Messages			MM		05.12.11
//	6.08.37 irserver Start über .ini Files						MM		06.12.11
//	6.08.38 Check IR Codelength mit Bus == 255					MM		17.01.12
//	6.08.39 Learn Status Debug Output							MM		05.02.12
//	6.08.40 Long Codes mit "Xl" Option							MM		09.02.12
//	6.08.41 XBMC Actions erweitert								MM		26.02.12
//	6.08.42 State Input erweitert								MM		22.04.12
//	6.08.43 State Input Analog in								MM		25.04.12
//	6.08.44 Fix Flash single ports Multistream					MM		25.04.12
//	6.08.46 Fix RS232 Commands Translator						MM		01.05.12
//	6.09.01 Alearn TCP Command									MM		02.05.12
//	6.09.02 Asndhex TCP Command									MM		02.05.12
//	6.09.03 Getversion incl. Status Memory						MM		03.05.12
//	6.09.04 Getversion Short incl. -longstatus Flag				MM		08.05.12

#define VERSION	"6.09.04"

// Autofind LAN Devices mehrere LAN Interfaces						OK
// Virtual COM Port Send: -virtual_comio							OK
// Sortierung LAN Devices nach MAC Adresse							OK
// irclient upload IRDB Multistream									OK
// IR Code Wiederholung mit Repeat Code								OK
// [RC][RP]: Pause über IR Code Length kalibrieren
// [RC][RP]: Wahlweise [FL]
// [RC][RP]: Pause über Firmware
// CCF Code Eingeben (GUI Client): 0000 006D 0022 0002 0155 ...		OK


// Links / Alternate names für .rem Dateien incl. IRDB
// Anzeige von Capabilities im GUI Client
// Auswahl B&O Codes in den Devicesettings LAN ausgegraut !!!		OK
// remotes Ordner unter Windows in Application Data verschieben
// TCP ASCII / UDP ASCII:
// - Analog mit Optionen
// - RS232 mit Optionen

// RS232 Erweiterung:
// Translator State I/O über Checksumme / RS232 über Command		OK
// SendSerialBlock für AUX RS232 über Bus							OK
// SendSerialBlock für AUX RS232 über IR IN (Soft UART)				OK
// Send RS232 über SEND IR (.rem File)								OK
// Send RS232 aus IRDB												OK
// Send RS232 aus Translator										OK
// Alle RS232 Optionen auch für Geräte mit normalem RS232 AUX		OK
// Add RS232 Command via GUI Client									OK
// RS232 Parameter in Send RS232 request							OK
// Send RS232 direkt über TCP										OK
// Send RS232 direkt über TCP ASCII									OK
// Send RS232 direkt über UDP										OK
// RS232 Parameter EEPROM (Extended Mode EX[6] wie AUX Parameter)	OK
// AUX RS232 über 2. Dialog konfigurieren							OK
// RS232 Parameter für LAN XL										OK
// IR Out als RS232 out (LAN)										OK
// IRDB -> AUX RS232 XL												OK

// Getstatus für V 5.11 Mediacontroller								OK


// Alle GUI Optionen in den neuen GUI Client						OK
// Learn Status (Overflow) neuer GUI Client							OK
// Chkline Mediacontroller neuer GUI Client							OK
// frmHTML neuer GUI Client											OK
// Befehl Senden neuer GUI Client: Füllen der Comboboxen (IRDB)		OK
// Analog Status direkt ohne Analog = Crash (neuer GUI Client)		OK
// Resize Translator Dialog (neuer GUI Client)						OK
// Neuer GUI Client: Status Input Translator
// Neuer GUI Client: GetRemoteKey in Translator.frm

// Befehl Senden alter GUI Client: Füllen der Comboboxen (IRDB)		OK



#if defined LINUX || defined _CONSOLE

main (int argc,char *argv[])
{
	int res;
	char *pnt;
	char st[255];
#ifdef WIN32
    SERVICE_TABLE_ENTRY ServiceTable[2];
#endif


#ifdef WIN32
	pnt = strrchr(argv[0], '\\');
#endif
#ifdef LINUX
	pnt = strrchr(argv[0], '/');
#endif
	if (pnt != NULL) {
		*pnt = 0;
		strcpy (serverdir,argv[0]);
	}


#if defined (WIN32) && !defined (_M_X64)
	{
		int vers;
		vers = GetOSInfo ();
		if (vers >= 510 && (vers & 1)) {		// Running on x64 (WOW64)
			if (serverdir[0]) chdir (serverdir);
			if (_execv (".\\irserver64.exe",argv)) {
				fprintf (stderr,"Error executing the 64Bit IRServer\n");
				exit (-1);
			}

		}
	}
#endif
#if defined (LINUX) && !defined (X64)
	{
		struct utsname u;
		
		uname (&u);
		if (!strcmp (u.machine,"x86_64")) {		// Running on x64
			argv[0] = "irserver64";
			if (serverdir[0]) chdir (serverdir);
			if (execv ("./irserver64",argv)) {
				fprintf (stderr,"Error executing the 64Bit IRServer\n");
				exit (-1);
			}
		}
	}
#endif

	new_lcd_flag = 0;
	mode_flag |= NO_RESET;
	mode_flag |= LOG_FATAL;
	mode_flag |= NO_WEB;
	strcpy (irserver_version,VERSION);

	if (argc == 2 && !strcmp (argv[1],"-version")) {
#if defined (WIN64) || defined (X64)
		printf ("IRServer64 Version %s\nMinimun IRTrans FW Version %s\n",VERSION,MINIMUM_SW_VERSION);
#else
		printf ("IRServer Version %s\nMinimun IRTrans FW Version %s\n",VERSION,MINIMUM_SW_VERSION);
#endif
		exit (0);
		}

	if (argc == 2 && !strcmp (argv[1],"-deviceinfo")) {
		mode_flag = (mode_flag & ~LOG_MASK) | 3;
#ifdef WIN32
		strcpy (st,"usb;com1;com2;com3;com4;lan");
#else
		strcpy (st,"usb;/dev/ttyS0;/dev/ttyS1;/dev/ttyS2;/dev/ttyS3;lan");
#endif
		return (get_devices (st,1));
		}
	
	if (serverdir[0]) chdir (serverdir);

	argc++;

	processArgs (argc,argv);

	if (mode_flag & PARAMETER_FILE) processArgs (0,0);

#ifdef WIN32
	if (mode_flag & DAEMON_MODE) {
	    ServiceTable[0].lpServiceName = "IRTrans Service";
		ServiceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)IRTransService;

	    ServiceTable[1].lpServiceName = NULL;
		ServiceTable[1].lpServiceProc = NULL;

	    StartServiceCtrlDispatcher(ServiceTable);  
		return (0);
	}
#endif


	res = InitServer (startparm);

	if (res) {
		GetError (res,st);
		log_print (st,LOG_FATAL);
		exit (res);
	}

	if (!(mode_flag & DAEMON_MODE)) {
		if (device_id != -1) {  // Bus !!!
			res = SetTransceiverIDEx (0,(byte)device_id);
			exit (res);
		}

		if (eeprom_id != -1) {	// Bus !!!
			res = SetTransceiverModusEx (0,159,0xffff,0,"",0,0xf,0,0,0,0,0);
			exit (res);
		}
	}

	res = RunServer ();

	if (res) {
		GetError (res,st);
		log_print (st,LOG_FATAL);
		exit (res);
	}
	return (0);
}


void processArgs (int argc,char *argv[])
{
	FILE *fp = NULL;
	char *pnt;
	int res,i,adr,comcount,filecount;
	char *str[40];
	static char ln[50][256];

	filecount = comcount = 0;
	if (argv == NULL) {
		fp = fopen ("irserver.ini","r");
		if (fp == NULL) {
			log_print ("Unable to open parameter file irserver.ini\n",LOG_FATAL);
			return;
		}
		
		str[0] = 0;
		argv = str;
		argc = 1;
		while (argc < 40) {
			str[argc] = ln[argc];
			if (fgets (argv[argc],2048,fp) == NULL) break;
			else {
				while (argv[argc][strlen (argv[argc]) - 1] == 10 || argv[argc][strlen (argv[argc]) - 1] == 13) argv[argc][strlen (argv[argc]) - 1] = 0;
				argc++;
			}
		}
		argc++;
	}

	for (;--argc > 2;argv++) {											// Process all Command Line Arguments
		if (!strcmp (argv[1],"-no_reconnect")) {
			mode_flag |= NO_RECONNECT;
			continue;
		}
		if (!strcmp (argv[1],"-no_init_lan")) {
			mode_flag |= NO_INIT_LAN;
			continue;
		}
		if (!strcmp (argv[1],"-start_clock")) {
			mode_flag |= CLOCK_STARTUP;
			continue;
		}
		if (!strcmp (argv[1],"-send_forward")) {
			mode_flag |= SEND_FORWARD;
			continue;
		}
		if (!strcmp (argv[1],"-send_forwardall")) {
			mode_flag |= SEND_FORWARDALL;
			continue;
		}
		if (!strcmp (argv[1],"-ip_relay")) {
			mode_flag |= IP_RELAY;
			continue;
		}
		if (!strcmp (argv[1],"-no_clock")) {
			mode_flag |= NO_CLOCK;
			continue;
		}
		if (!strcmp (argv[1],"-no_lirc")) {
			mode_flag |= NO_LIRC;
			continue;
		}
		if (!strcmp (argv[1],"-tcp")) {
			mode_flag |= ETHERNET_TCP;
			continue;
		}
		if (!strcmp (argv[1],"-tcp_reconnect")) {
			mode_flag |= ETHERNET_TCP | TCP_RECONNECT;
			continue;
		}
		if (!strcmp (argv[1],"-debug_code")) {
			mode_flag |= DEBUG_CODE;
			continue;
		}
		if (!strcmp (argv[1],"-debug_timing")) {
			mode_flag |= DEBUG_TIMING;
			continue;
		}
		if (!strcmp (argv[1],"-hexdump")) {
			mode_flag |= HEXDUMP;
			continue;
		}
		if (!strcmp (argv[1],"-timestamp")) {
			mode_flag |= TIMESTAMP;
			continue;
		}
		if (!strcmp (argv[1],"-medialon")) {
			mode_flag |= MEDIALON;
			continue;
		}
		if (!strcmp (argv[1],"-codedump")) {
			mode_flag |= CODEDUMP;
			continue;
		}
		if (!strcmp (argv[1],"-longstatus")) {
			statusmode_short = 0;
			continue;
		}
		if (!strcmp (argv[1],"-xap")) {
			mode_flag |= XAP;
			continue;
		}
		if (!strcmp (argv[1],"-learned_only")) {
			mode_flag |= LEARNED_ONLY;
			continue;
		}
		if (!strcmp (argv[1],"-virtual_comport")) {
			argc--;
			argv++;
			strcpy (virt_comnames[comcount++],argv[1]);
			continue;
		}
		if (!strcmp (argv[1],"-virtual_comio")) {
			argc--;
			argv++;
			strcpy (virt_comnames[comcount++],argv[1]);
			comioflag = 1;
			continue;
		}
		if (!strcmp (argv[1],"-comport_file")) {
			argc--;
			argv++;
			strcpy (virt_comfiles[filecount++],argv[1]);
			continue;
		}
		if (!strcmp (argv[1],"-logfile")) {
			argc--;
			argv++;
			strcpy (logfile,argv[1]);
			continue;
		}
		if (!strcmp (argv[1],"-remotes")) {
			argc--;
			argv++;
			strcpy (irdb_path,argv[1]);
			continue;
		}
		if (!strcmp (argv[1],"-baudrate")) {
			argc--;
			argv++;
			strncpy (baudrate,argv[1],10);
			continue;
		}
		if (!strcmp (argv[1],"-hexfile")) {
			argc--;
			argv++;
			strcpy (hexfile,argv[1]);
			continue;
		}
		if (!strcmp (argv[1],"-xbmc")) {
			xbmc_mode = 1;
			continue;
		}
		if (!strcmp (argv[1],"-pidfile")) {
			argc--;
			argv++;
			strcpy (pidfile,argv[1]);
			continue;
		}
		if (!strcmp (argv[1],"-udp_relay")) {
			argc--;
			argv++;
			strcpy (udp_relay_host,argv[1]);
			argc--;
			argv++;
			udp_relay_port = atoi (argv[1]);
			argc--;
			argv++;
			strcpy (udp_relay_format,argv[1]);
			continue;
		}
		if (!strcmp (argv[1],"-loglevel")) {
			argc--;
			argv++;
			mode_flag = (mode_flag & ~LOG_MASK) | (atoi (argv[1]) & LOG_MASK);
			continue;
		}
		if (!strcmp (argv[1],"-set_id")) {
			argc--;
			argv++;
			device_id = atoi (argv[1]) & 0xf;
			continue;
		}
		if (!strcmp (argv[1],"-delay")) {
			argc--;
			argv++;
			msSleep (atoi (argv[1]));
			continue;
		}
		if (!strcmp (argv[1],"-wait")) {
			argc--;
			argv++;
			device_wait = atoi (argv[1]);
			continue;
		}
		if (!strcmp (argv[1],"-stat_timeout")) {
			argc--;
			argv++;
			status_cache_timeout = atoi (argv[1]);
			continue;
		}
		if (!strcmp (argv[1],"-lcd")) {
			argc--;
			argv++;
			new_lcd_flag = atoi (argv[1]);
			continue;
		}

		if (!strcmp (argv[1],"-netmask")) {
			argc--;
			argv++;
			if (netcount < 32) {
				netip[netcount] = ntohl (inet_addr (strtok (argv[1],"/")));
				pnt = strtok (NULL,"/");
				if (pnt) {
					adr = 0;
					res = atoi (pnt);
					for (i=0;i < res;i++) adr |= 1 << (31-i);
					netmask[netcount++] = adr;
				}
				else netmask[netcount++] = 0xffffffff;
			}
			continue;
		}
		if (!strcmp (argv[1],"-reset_eeprom")) {
			argc--;
			argv++;
			eeprom_id = atoi (argv[1]) & 0xf;
			continue;
		}
		if (!strcmp (argv[1],"-udp_port")) {
			argc--;
			argv++;
			irtrans_udp_port = atoi (argv[1]);
			continue;
		}
		if (!strcmp (argv[1],"-http_port")) {
			argc--;
			argv++;
			mode_flag &= ~NO_WEB;
			webport = atoi (argv[1]);
			continue;
		}
		if (!strcmp (argv[1],"-no_web")) {
			mode_flag |= NO_WEB;
			continue;
		}
		if (!strcmp (argv[1],"-parameter_file")) {
			mode_flag |= PARAMETER_FILE;
			continue;
		}
		if (!strcmp (argv[1],"-read_eeprom")) {
			mode_flag |= READ_EEPROM;
			continue;
		}
		if (!strcmp (argv[1],"-daemon")) {
			if (*logfile == 0) strcpy (logfile,"irserver.log");
			mode_flag |= DAEMON_MODE;
			continue;
		}
#ifdef WIN32
		if (!strcmp (argv[1],"-service")) {
			if (*logfile == 0) strcpy (logfile,"irserver.log");
			mode_flag |= DAEMON_MODE | PARAMETER_FILE;
			if (device_wait == 5) device_wait = 180;
			continue;
		}
#endif
		fprintf (stderr,"Unknown option %s\n",argv[1]);
		display_usage ();
	}


	if (argc > 1 && !strcmp (argv[1],"-parameter_file")) mode_flag |= PARAMETER_FILE;
#ifdef WIN32
	if (argc > 1 && !strcmp (argv[1],"-service")) {
		mode_flag |= PARAMETER_FILE | DAEMON_MODE;
		if (*logfile == 0) strcpy (logfile,"irserver.log");
		if (device_wait == 5) device_wait = 180;
	}
#endif

	if (!(mode_flag & PARAMETER_FILE) || fp) {
		if (argc == 1 || argv[1][0] == '-') display_usage ();
		startparm = argv[1];
	}

	if (fp) fclose (fp);
}



#ifdef WIN32

void IRTransService(int argc, char** argv) 
{
	int res;
	char st[255];

	ServiceStatusIRT.dwServiceType        = SERVICE_WIN32; 
	ServiceStatusIRT.dwCurrentState       = SERVICE_START_PENDING; 
	ServiceStatusIRT.dwControlsAccepted   = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	ServiceStatusIRT.dwWin32ExitCode      = 0; 
	ServiceStatusIRT.dwServiceSpecificExitCode = 0; 
	ServiceStatusIRT.dwCheckPoint         = 0; 
	ServiceStatusIRT.dwWaitHint           = 0; 

	hStatusIRT = RegisterServiceCtrlHandler ("IRTrans Service",(LPHANDLER_FUNCTION)IRTControlHandler);

	if (hStatusIRT == (SERVICE_STATUS_HANDLE)0) return;

	chdir (serverdir);
	res = InitServer (startparm);

	if (res) {
		GetError (res,st);
		log_print (st,LOG_FATAL);

		ServiceStatusIRT.dwCurrentState       = SERVICE_STOPPED; 
		ServiceStatusIRT.dwWin32ExitCode      = res; 
		SetServiceStatus(hStatusIRT, &ServiceStatusIRT); 
		WSACleanup ();

		return;
	}

	ServiceStatusIRT.dwCurrentState = SERVICE_RUNNING; 
	SetServiceStatus (hStatusIRT, &ServiceStatusIRT);

	res = RunServer ();

	if (res) {
		GetError (res,st);
		log_print (st,LOG_FATAL);
	}

	ServiceStatusIRT.dwCurrentState       = SERVICE_STOPPED; 
	ServiceStatusIRT.dwWin32ExitCode      = res; 
	SetServiceStatus(hStatusIRT, &ServiceStatusIRT); 
	WSACleanup ();
}





void IRTControlHandler (DWORD request) 
{ 
    switch(request) 
    { 
        case SERVICE_CONTROL_STOP: 
			log_print ("IRTrans Service stopped\n",LOG_FATAL);

            ServiceStatusIRT.dwWin32ExitCode = 0; 
            ServiceStatusIRT.dwCurrentState  = SERVICE_STOPPED; 
            SetServiceStatus (hStatusIRT, &ServiceStatusIRT);
			WSACleanup ();
			return; 
 
        case SERVICE_CONTROL_SHUTDOWN: 
			log_print ("IRTrans Service stopped\n",LOG_FATAL);

            ServiceStatusIRT.dwWin32ExitCode = 0; 
            ServiceStatusIRT.dwCurrentState  = SERVICE_STOPPED; 
            SetServiceStatus (hStatusIRT, &ServiceStatusIRT);
			WSACleanup ();
            return; 
        
        default:
            break;
    } 
 
    SetServiceStatus (hStatusIRT,  &ServiceStatusIRT);
 
    return; 
} 
#endif



void display_usage (void)
{
	fprintf (stderr,"%s",irtrans_usage);
	exit (1);
}

#endif

int InitServer (char dev[])
{
	int res,i;
	char msg[256],msg2[256];
	time_t ti;

#ifdef LINUX
	FILE *fp;
#endif

	if (*logfile) {
		logfp = fopen (logfile,"w");
		if (!logfp) {
			fprintf (stderr,"Unable to open Logfile %s\n",logfile);
			exit (-1);
		}
	}

	for (i=0;i < CLIENT_COUNT;i++) sockinfo[i].fd = -1;

	res = InitServerSocket (&server_socket,&lirc_socket,&udp_socket,&web_socket);
	if (res) return (res);

	log_print ("Init Server Socket done\n",LOG_DEBUG);


#ifdef WIN32
	SockEvent = WSACreateEvent ();
	WSAEventSelect (server_socket,SockEvent,FD_ACCEPT);

	UDPEvent = WSACreateEvent ();
	WSAEventSelect (udp_socket, UDPEvent,FD_READ);

	xAPEvent = WSACreateEvent ();
	WSAEventSelect (xAP_rcv, xAPEvent,FD_READ);

	IrtLanEvent = WSACreateEvent ();
	WSAEventSelect (irtlan_socket, IrtLanEvent,FD_READ);

	if (mode_flag & NO_LIRC) LircEvent = NULL;
	else {
		LircEvent = WSACreateEvent ();
		WSAEventSelect (lirc_socket,LircEvent,FD_ACCEPT);
	}

	if (mode_flag & NO_WEB) WebEvent = NULL;
	else {
		WebEvent = WSACreateEvent ();
		WSAEventSelect (web_socket,WebEvent,FD_ACCEPT);
	}
#endif

#if defined (WIN64) || defined (X64)
	sprintf (msg,"IRServer64 Version %s\n",VERSION);
#else
	sprintf (msg,"IRServer Version %s\n",VERSION);
#endif
	log_print (msg,LOG_FATAL);
	strcpy (init_message,msg);

	log_print ("Init Events done\n",LOG_DEBUG);
//	InitInput ();
	ti = time (0) + device_wait;

	do {
		res = InitCommunicationEx (dev);
		log_print ("Init communication ...\n",LOG_DEBUG);
		if (res && device_wait) msSleep (2000);
	} while ((res || !device_cnt) && device_wait && time (0) < ti && strcmp (dev,"dummy"));
		
	if (res) return (res);
	
	res = OpenVirtualComPorts ();
	if (res) return (res);
	res = OpenComFiles ();
	if (res) return (res);

	if (*hexfile) {
		hexfp = fopen (hexfile,"w");
		if (!hexfp) {
			fprintf (stderr,"Unable to open Hexfile %s\n",hexfile);
			exit (-1);
		}
	}

#ifdef LINUX
	if (mode_flag & DAEMON_MODE) {
		if (fork () == 0) {
			fclose (stdin);
			fclose (stdout);
			fclose (stderr);
			stderr = NULL;
			setsid ();
			fp = fopen ("/tmp/.irserver.pid","w");
			if (fp) {
				fprintf (fp,"%d\n",getpid ());
				fclose (fp);
			}
		}
		else exit (0);
	}
#endif


	if (!device_cnt && strcmp (dev,"dummy")) {
		sprintf (msg,"No IRTrans Devices found.\nAborting ...\n\n");
		log_print (msg,LOG_FATAL);
		exit (-1);
	}
	for (i=0;i < device_cnt;i++) {
		msg2[0] = 0;
		if (IRDevices[i].io.socket != 0) strcpy (msg2,"[TCP]");
		if (IRDevices[i].io.if_type == IF_LAN) sprintf (msg,"[%2d]: %-10s: IR VER: %-8s       ETH VER: %-8s   SN: %u\n                     MAC:%02x-%02x-%02x-%02x-%02x-%02x  IP Addr: %s %s\n",i,
			IRDevices[i].name,IRDevices[i].version,IRDevices[i].lan_version,IRDevices[i].fw_serno,
			IRDevices[i].mac_adr[0],
			IRDevices[i].mac_adr[1],
			IRDevices[i].mac_adr[2],
			IRDevices[i].mac_adr[3],
			IRDevices[i].mac_adr[4],
			IRDevices[i].mac_adr[5],
			inet_ntoa (IRDevices[i].io.IPAddr[0].sin_addr),msg2);
		else if (IRDevices[i].fw_serno) sprintf (msg,"[%2d]: %-20s %-12s SN: %u\n",i,IRDevices[i].name,IRDevices[i].version,IRDevices[i].fw_serno);
		else sprintf (msg,"[%2d]: %-20s %-12s\n",i,IRDevices[i].name,IRDevices[i].version);
		if (IRDevices[i].io.if_type == IF_USB && !strcmp (IRDevices[i].name,"IRTrans WiFi")) {
			sprintf (msg2,"      %-20s %-12sMAC: %02X-%02X-%02X-%02X-%02X-%02X\n","",IRDevices[i].lan_version,IRDevices[i].mac_adr[0],
					 IRDevices[i].mac_adr[1],IRDevices[i].mac_adr[2],IRDevices[i].mac_adr[3],IRDevices[i].mac_adr[4],IRDevices[i].mac_adr[5]);
			strcat (msg,msg2);
		}
		log_print (msg,LOG_FATAL);
		if (strlen (init_message) + strlen (msg) < 4096) strcat (init_message,msg);
	}

	if (udp_relay_port) {
		sprintf (msg,"Relaying to UDP %s:%d using Format %s\n",udp_relay_host,udp_relay_port,udp_relay_format);
		log_print (msg,LOG_INFO);
	}

	res = ReadIRDatabase ();
	if (res) return (res);

	InitConversionTables ();

	if (pidfile[0]) {
		FILE *pf;
		pf = fopen (pidfile,"w");
		if (pf == NULL) {
			sprintf (msg,"Cannot create PID file %s\n",pidfile);
			log_print (msg,LOG_ERROR);
		}
		else {
			fprintf (pf,"%d\n",_getpid ());
			fclose (pf);
		}
	}
	return (0);
}



#define LAN_TCP_IDLE	1800


int RunServer ()
{
	char err[300];
	int res,wait,evnt,i,j,bus;
	SOCKET sockfd;
	DWORD cnt;
	byte rdcom[1024],rcvdata[1024],dummy;
	time_t lasttimesync = 0;
	struct sockaddr_in send_adr;
	IRDATA_LAN_SHORT *ir;


#ifdef LINUX
	fd_set events;
	int maxfd,ionum;
	int socktype;
	struct timeval tv;
	byte usbflag;
#endif

#ifdef WIN32
	int numevents;
	int waittime;
	int ser_event;
	HANDLE events[CLIENT_COUNT + MAX_IR_DEVICES + 4];
	OVERLAPPED OvCom[MAX_IR_DEVICES * 2];
	byte event_type[CLIENT_COUNT + MAX_IR_DEVICES + 4];
	byte event_num[CLIENT_COUNT + MAX_IR_DEVICES + 4];
#endif

	if (mode_flag & READ_EEPROM) {
		ReadFlashdataEx (0,0);
		ReadFlashdataEx (0,128);
		ReadFlashdataEx (0,256);
		ReadFlashdataEx (0,384);
		ReadFlashdataEx (0,512);
		ReadFlashdataEx (0,768);
		ReadFlashdataEx (0,896);
		ReadFlashdataEx (0,1024);
		ReadFlashdataEx (0,1152);
		exit (0);
	}

	if (webport) {
		sprintf (rdcom,"IRTrans Webserver started on Port %d.\n",webport);
		if (webport) log_print (rdcom,LOG_FATAL);
	}

	if (mode_flag & CLOCK_STARTUP) LCDTimeCommand (LCD_DISPLAYTIME);
	else LCDTimeCommand (LCD_SETTIME);
	LCDBrightness (5);
	lasttimesync = time (0);
#ifdef LINUX
	if (new_lcd_flag) {
		SetLCDProcCharsV (rdcom);

		AdvancedLCD (LCD_DATA | LCD_DEFINECHAR,rdcom,rdcom[0] * 9 + 1);
		msSleep (250);
	}
#endif

#ifdef WIN32

	waittime = INFINITE;
	for (i=0;i < device_cnt;i++) if ((!(mode_flag & NO_RECONNECT) && IRDevices[i].io.if_type == IF_USB) || mode_flag & XAP || (mode_flag & DAEMON_MODE) || (IRDevices[i].io.if_type == IF_LAN && IRDevices[i].io.socket != 0)) waittime = 120000;
	if (xbmc_mode) waittime = 40000;

	if (!(mode_flag & DAEMON_MODE)) ServiceStatusIRT.dwCurrentState = SERVICE_RUNNING;

	while (ServiceStatusIRT.dwCurrentState == SERVICE_RUNNING) {
		for (i=0;i < device_cnt;i++) {
			for (j=0;j < IRDevices[i].io.receive_buffer_cnt;j++) {
				if (IRDevices[i].io.receive_cnt[j] > 2) ExecuteReceivedCommand (IRDevices[i].io.receive_buffer[j],IRDevices[i].io.receive_cnt[j],i);
				IRDevices[i].io.receive_cnt[j] = 0;
			}
			IRDevices[i].io.receive_buffer_cnt = 0;
		}
		for (i=0;i < device_cnt;i++) while (GetAvailableDataEx (IRDevices+i)) {
			cnt = ReadInstantReceive (&IRDevices[i],rdcom,1000);
			if (cnt > 2) ExecuteReceivedCommand (rdcom,cnt,i);
		}
		numevents = build_event_table (events,event_type,event_num,OvCom,&ser_event);

		wait = WaitForMultipleObjects (numevents,events,FALSE,waittime);
		if ((time (0) - lasttimesync) > 3600) {
			LCDTimeCommand (LCD_SETTIME);
			lasttimesync = time (0);
		}

		if ((time (0) - xAP_last_hbeat) > XAP_HBEAT) xAP_SendHeartbeat ();
		if ((time (0) - XBMC_last_ping) > 30) XBMC_SendPing ();
 
		if (wait == WAIT_FAILED) {
			sprintf (rdcom,"WAIT ERROR: %x\n",GetLastError ());
			log_print (rdcom,LOG_ERROR);
			msSleep (500);
		}
		else if (wait == WAIT_TIMEOUT) {
			dummy = 1;
			for (i=0;i < device_cnt;i++) {
				if (IRDevices[i].io.if_type == IF_USB) WriteUSBStringEx (IRDevices + i,&dummy,1);
				if (IRDevices[i].io.if_type == IF_LAN && IRDevices[i].io.socket != 0 && (time (0) - IRDevices[i].io.last_time) > LAN_TCP_IDLE) {
					IRDATA ird;
					memset (&ird,0,sizeof (IRDATA));
					IRTransLanSend (IRDevices + i,&ird);
				}
			}
		}
		else {
			evnt = get_selected_event (wait,events,event_type,event_num,&sockfd);
			switch (evnt) {
				case SELECT_TRANS:
					cnt = ReadInstantReceive (&IRDevices[event_num[wait]],rdcom,1000);
					

					/*
					printf ("CNT: %d\n",cnt);
					for (i=0;i < cnt;i++) {
						printf ("%d  %x  %c\n",i,rdcom[i],rdcom[i]);
					}*/
					
					/*
					showDebugTiming ((IRDATA *)rdcom);
					printf ("TCNT: %d\n",cnt);
					printf ("STR: %s\n",rdcom);
					printf ("CNT: %d %s\n",cnt,rdcom);

					for (i=0;i < cnt;i += 4) {
						printf ("%d - %d\n%d - %d\n",rdcom[i],rdcom[i+1],rdcom[i+2],rdcom[i+3]);
					}
					*/
					if (IRDevices[event_num[wait]].io.inst_receive_mode & 4) {
						int offset;

						if (mode_flag & DEBUG_TIMING) showDebugTiming ((IRDATA *)rdcom);

						if (rdcom[8] == TIMECOUNT_18) offset = 83;
						else offset = 43;

						rdcom[offset] = ((rdcom[8] & (RC5_DATA | RC6_DATA | RAW_DATA)) << 2) | rdcom[3];
						ExecuteReceivedCommand (rdcom + offset,cnt - offset,event_num[wait]);
					}
					else if (rdcom[0] == 255) {												// Special Mode
						if (rdcom[1] == RESULT_RCV_RS232) {								// RS232 Data
							if (IRDevices[event_num[wait]].virtual_comport != INVALID_HANDLE_VALUE) {
								res = WritePort (&IRDevices[event_num[wait]],rdcom+3,rdcom[2]);
								/*
								printf ("RES: %d\n",rdcom[2]);
								for (i=0;i < rdcom[2];i++) {
									printf ("%d  %x  %c\n",i,rdcom[i+3],rdcom[i+3]);
								}*/
							}
						}
					}
					else if (cnt > 2) ExecuteReceivedCommand (rdcom,cnt,event_num[wait]);
					break;
				case SELECT_RS232:
					{
					RS232_DATA rs232;
					cnt = ReadSerialStringComio (&IRDevices[event_num[wait]],rdcom,1000,10);
					/*
					printf ("CNT: %d\n",cnt);
					for (i=0;i < cnt;i++) {
						printf ("%d  %x  %c\n",i,rdcom[i],rdcom[i]);
					}*/
					memset (&rs232,0,sizeof (RS232_DATA));
			
					rs232.command = HOST_SEND_RS232;
					rs232.len = (byte)cnt + 5;
					rs232.parameter = 0;
					memcpy (rs232.data,rdcom,cnt);
					if (!IRDevices[event_num[wait]].comio_init) {
						if (cnt == 1 && (rdcom[0] == 18 || rdcom[0] == 146)) rs232.address = 255;
						else rs232.address = 250;
						IRDevices[event_num[wait]].comio_init = 1;
					}
					else rs232.address = 250;
				
					res = WriteTransceiverEx (&IRDevices[event_num[wait]],(IRDATA *)&rs232);
					break;
					}
				case SELECT_IRTLAN:
				case SELECT_LAN_TCP:
					ir = (IRDATA_LAN_SHORT *)rcvdata;
					i = sizeof (send_adr);
					if (evnt == SELECT_LAN_TCP) res = recvfrom(sockfd,rcvdata,1024,MSG_NOSIGNAL,(struct sockaddr *)&send_adr,&i);
					else res = recvfrom(irtlan_socket,rcvdata,1024,MSG_NOSIGNAL,(struct sockaddr *)&send_adr,&i);

					if (res > 3 && (ir->netcommand == 's' || ir->netcommand == 'S' || ir->netcommand == 'l' || ir->netcommand == 'L' || ir->netcommand == 'p' || ir->netcommand == 'P')) {
						process_udp_command ((char *)rcvdata,res,&send_adr);
						break;
					}

					if (res <= 0 || res != ir->ir_data.len + 1 || (ir->netcommand != RESULT_IR_BROADCAST_LED && ir->netcommand != RESULT_IR_BROADCAST && ir->netcommand != RESULT_IR_HOSTBROADCAST && ir->netcommand != RESULT_RCV_RS232)) break;
					
					if (ir->netcommand == RESULT_IR_BROADCAST_LED) ir->ir_data.address = 0;
					if (ir->netcommand != RESULT_RCV_RS232 && ir->ir_data.command != SBUS_REPEAT) break;

					for (i=0;i < device_cnt;i++) if (IRDevices[i].io.if_type == IF_LAN && IRDevices[i].io.IPAddr[0].sin_addr.s_addr == send_adr.sin_addr.s_addr) {
						if (IRDevices[i].io.socket && evnt != SELECT_LAN_TCP) break;
						bus = i;
						goto ip_found;
					}
					GetError (ERR_DEVICEUNKNOWN,err);
					sprintf (rdcom,err,inet_ntoa (send_adr.sin_addr));
//					log_print (rdcom,LOG_ERROR);
					break;

ip_found:			if (ir->netcommand == RESULT_RCV_RS232) {
						RS232_RECEIVE *rs232;
						rs232 = (RS232_RECEIVE *)&ir->ir_data;
						if (rs232->seq_number != IRDevices[bus].comport_seq_number) {
							sprintf (err,"RS232 Data received from Bus %d [%d-%d] LEN:%d\n",bus,rs232->error_code,rs232->seq_number,rs232->data_len);
							log_print (err,LOG_DEBUG);
							IRDevices[bus].comport_seq_number = rs232->seq_number;

							if (IRDevices[bus].virtual_comport != INVALID_HANDLE_VALUE) WritePort (&IRDevices[bus],rs232->data,rs232->data_len);
							if (IRDevices[bus].comport_file != NULL) {
								fwrite (rs232->data,1,rs232->data_len,IRDevices[bus].comport_file);
								fflush (IRDevices[bus].comport_file);
							}
						}
					}
					else {
						if (mode_flag & IP_RELAY) {
							ir->ir_data.command = HOST_SEND;
							ir->ir_data.target_mask = 0xffff;
							for (res=0;res<device_cnt;res++) if (IRDevices[res].io.if_type != IF_LAN) WriteTransceiverEx (&IRDevices[res],&ir->ir_data);
						}

						if (mode_flag & DEBUG_TIMING) showDebugTiming (&ir->ir_data);

						rdcom[0] = ir->ir_data.address;
						if (!(ir->ir_data.mode & SPECIAL_IR_FLAG)) ir->ir_data.address |= ((ir->ir_data.mode & (RC5_DATA | RC6_DATA | RAW_DATA)) << 2);

						
						if (ir->ir_data.mode == TIMECOUNT_18) {
							memcpy (rdcom+1,((IRDATA_18 *)&ir->ir_data)->data,ir->ir_data.ir_length);
						}
						else if ((ir->ir_data.mode & SPECIAL_IR_MODE) == PULSE_200) {
							memcpy (rdcom+1,((IRDATA_SINGLE *)&ir->ir_data)->data,ir->ir_data.ir_length);
						}
						else {
							if (ir->ir_data.mode & RAW_DATA) {
								memcpy (rdcom+1,((IRRAW *)(&ir->ir_data))->data,ir->ir_data.ir_length);
							}
							else {
								memcpy (rdcom+1,ir->ir_data.data,ir->ir_data.ir_length);
							}
						}
						rdcom[ir->ir_data.ir_length+1] = 0;

						ExecuteReceivedCommand (rdcom,ir->ir_data.ir_length+2,i);
					}
					break;
				case SELECT_LIRC:
				case SELECT_LOCAL:
				case SELECT_SERVER:
					register_remote_client (sockfd,evnt);
					break;
				case SELECT_WEB:
					ProcessWebRequest (sockfd);
					break;
				case SELECT_UDP:
//					process_udp_command (sockfd);
					break;
				case SELECT_XAP:
					xAP_EventReceived ();
					break;
				case COMMAND_LIRC:
				case COMMAND_LOCAL:
					process_lirc_command (sockfd);
					break;
				case COMMAND_SERVER:
				case COMMAND_REOPEN:
					ExecuteNetCommand (sockfd);
					break;
			}
		}
	}
#endif

#ifdef LINUX
	while (1) {

		maxfd = build_select_table (&events);

		tv.tv_sec = 10;
		tv.tv_usec = 0;

		usbflag = 0;
		for (i=0;i < device_cnt;i++) if (!(mode_flag & NO_RECONNECT) && IRDevices[i].io.if_type == IF_USB) usbflag = 1;

		if ((usbflag && !(mode_flag & NO_RECONNECT)) || mode_flag & XAP || xbmc_mode) wait = select (maxfd,&events,NULL,NULL,&tv);
		else wait = select (maxfd,&events,NULL,NULL,NULL);

		sprintf (rdcom,"Select Return: %x\n",wait);
		log_print (rdcom,LOG_DEBUG);
		

		if (wait == -1) {
			sprintf (rdcom,"Select Error: %d\n",errno);
			log_print (rdcom,LOG_FATAL);
			exit (-1);
		}

		if ((time (0) - lasttimesync) > 3600) {
			LCDTimeCommand (LCD_SETTIME);
			lasttimesync = time (0);
		}

		if ((time (0) - xAP_last_hbeat) > XAP_HBEAT) xAP_SendHeartbeat ();
		if ((time (0) - XBMC_last_ping) > 30) XBMC_SendPing ();

		if (!wait) {
			dummy = 1;
			for (i=0;i < device_cnt;i++) if (IRDevices[i].io.if_type == IF_USB) WriteIRStringEx (IRDevices + i,&dummy,1);
		}
		while (wait > 0) {
			evnt = get_selected_fd (&events,&sockfd,&ionum);
			switch (evnt) {
				case SELECT_TRANS:
					cnt = ReadInstantReceive (IRDevices + ionum,rdcom,1000);
					if (!cnt) {
						if (IRDevices[ionum].io.if_type == IF_USB && !(mode_flag & NO_RECONNECT)) {
							log_print ("IRTrans USB Connection lost.\n",LOG_FATAL);
							close (IRDevices[ionum].io.comport);
							res = 1;
							while (res) {
								res = OpenSerialPortEx (IRDevices[ionum].device_node,&(IRDevices[ionum].io.comport),0);
								if (res) sleep (10);
							}
							log_print ("IRTrans USB Reconnected.\n",LOG_FATAL);
						}
						else {
							log_print ("IRTrans Connection lost. Aborting ...\n",LOG_FATAL);
							exit (-1);
						}
					}
					if (IRDevices[ionum].io.inst_receive_mode & 4) {
						int offset;

						if (rdcom[8] == TIMECOUNT_18) offset = 83;
						else offset = 43;

						rdcom[offset] = ((rdcom[8] & (RC5_DATA | RC6_DATA | RAW_DATA)) << 2) | rdcom[3];
						ExecuteReceivedCommand (rdcom + offset,cnt - offset,ionum);
					}
					else if (cnt > 2) ExecuteReceivedCommand (rdcom,cnt,ionum);
					break;
				case SELECT_IRTLAN:
					ir = (IRDATA_LAN_SHORT *)rcvdata;
					i = sizeof (send_adr);
					res = recvfrom(irtlan_socket,rcvdata,1024,MSG_NOSIGNAL,(struct sockaddr *)&send_adr,&i);
					if (res > 5 && ir->netcommand == 's' || ir->netcommand == 'S' || ir->netcommand == 'l' || ir->netcommand == 'L') {
						process_udp_command ((char *)&ir,res,&send_adr);
						break;
					}

					if (res <= 0 || res != ir->ir_data.len + 1 || (ir->netcommand != RESULT_IR_BROADCAST_LED && ir->netcommand != RESULT_IR_BROADCAST && ir->netcommand != RESULT_IR_HOSTBROADCAST && ir->netcommand != RESULT_RCV_RS232)) break;
					
					if (ir->netcommand == RESULT_IR_BROADCAST_LED) ir->ir_data.address = 0;
					if (ir->netcommand != RESULT_RCV_RS232 && ir->ir_data.command != SBUS_REPEAT) break;
					
					for (i=0;i < device_cnt;i++) if (IRDevices[i].io.if_type == IF_LAN) for (j=0;j < 16 && IRDevices[i].io.IPAddr[j].sin_addr.s_addr;j++) {
						if (IRDevices[i].io.IPAddr[j].sin_addr.s_addr == send_adr.sin_addr.s_addr) {
							bus = i;
							goto ip_found;
						}
					}
					GetError (ERR_DEVICEUNKNOWN,err);
					sprintf (rdcom,err,inet_ntoa (send_adr.sin_addr));
//					log_print (rdcom,LOG_ERROR);
					break;

ip_found:			if (ir->netcommand == RESULT_RCV_RS232) {
						RS232_RECEIVE *rs232;
						rs232 = (RS232_RECEIVE *)&ir->ir_data;
						if (rs232->seq_number != IRDevices[bus].comport_seq_number) {
							sprintf (err,"RS232 Data received from Bus %d [%d-%d] LEN:%d: %s\n",bus,rs232->error_code,rs232->seq_number,rs232->data_len,rs232->data);
							log_print (err,LOG_DEBUG);
							IRDevices[bus].comport_seq_number = rs232->seq_number;
							if (IRDevices[bus].virtual_comport != INVALID_HANDLE_VALUE) WritePort (&IRDevices[bus],rs232->data,rs232->data_len);
						}
					}
					else {
						if (mode_flag & IP_RELAY) {
							ir->ir_data.command = HOST_SEND;
							ir->ir_data.target_mask = 0xffff;
							for (res=0;res<device_cnt;res++) if (IRDevices[res].io.if_type != IF_LAN) WriteTransceiverEx (&IRDevices[res],&ir->ir_data);
						}

						if (mode_flag & DEBUG_TIMING) showDebugTiming (&ir->ir_data);
						
						rdcom[0] = ir->ir_data.address;
						if (!(ir->ir_data.mode & SPECIAL_IR_FLAG)) ir->ir_data.address |= ((ir->ir_data.mode & (RC5_DATA | RC6_DATA | RAW_DATA)) << 2);
						
						if (ir->ir_data.mode == TIMECOUNT_18) {
							memcpy (rdcom+1,((IRDATA_18 *)&ir->ir_data)->data,ir->ir_data.ir_length);
						}
						else if ((ir->ir_data.mode & SPECIAL_IR_MODE) == PULSE_200) {
							memcpy (rdcom+1,((IRDATA_SINGLE *)&ir->ir_data)->data,ir->ir_data.ir_length);
						}
						else {
							if (ir->ir_data.mode & RAW_DATA) {
								memcpy (rdcom+1,((IRRAW *)(&ir->ir_data))->data,ir->ir_data.ir_length);
							}
							else {
								memcpy (rdcom+1,ir->ir_data.data,ir->ir_data.ir_length);
							}
						}
						rdcom[ir->ir_data.ir_length+1] = 0;
						ExecuteReceivedCommand (rdcom,ir->ir_data.ir_length+1,i);
					}
					break;
				case SELECT_LIRC:
				case SELECT_LOCAL:
				case SELECT_SERVER:
					register_remote_client (sockfd,evnt);
					break;
				case SELECT_WEB:
					ProcessWebRequest (sockfd);
					break;
				case COMMAND_LIRC:
				case COMMAND_LOCAL:
					process_lirc_command (sockfd);
					break;
				case SELECT_XAP:
					xAP_EventReceived ();
					break;
				case SELECT_UDP:
//					process_udp_command (sockfd);
					break;
				case COMMAND_SERVER:
				case COMMAND_REOPEN:
					ExecuteNetCommand (sockfd);
					break;
			}
			for (i=0;i < device_cnt;i++) {
				for (j=0;j < IRDevices[i].io.receive_buffer_cnt;j++) {
					if (IRDevices[i].io.receive_cnt[j] > 2) ExecuteReceivedCommand (IRDevices[i].io.receive_buffer[j],IRDevices[i].io.receive_cnt[j],i);
					IRDevices[i].io.receive_cnt[j] = 0;
				}
				IRDevices[i].io.receive_buffer_cnt = 0;
			}
			wait--;
		}
	}

#endif
	
	return (0);
}


#ifdef WIN32

int build_event_table (HANDLE events[],byte event_type[],byte event_num[],OVERLAPPED OvCom[],int *ser_event)
{
	int i,num = 0;

	for (i=0;i < device_cnt;i++) {
		if (IRDevices[i].io.if_type != IF_LAN) {
			if (IRDevices[i].io.if_type == IF_USB) SetUSBEventEx (&IRDevices[i],FT_EVENT_RXCHAR);
			else {
				memset (&OvCom[i],0,sizeof (OVERLAPPED));
				OvCom[i].hEvent = IRDevices[i].io.event;
				SetCommMask (IRDevices[i].io.comport,EV_RXCHAR);
				WaitCommEvent (IRDevices[i].io.comport,ser_event,&OvCom[i]);
			}
			event_type[num] = SELECT_TRANS;
			event_num[num] = i;
			events[num++] = IRDevices[i].io.event;
		}
		else if (IRDevices[i].io.socket) {	// LAN Device mit TCP 
			event_type[num] = SELECT_LAN_TCP;
			event_num[num] = i;
			events[num++] = IRDevices[i].io.event;
		}
		if (comioflag && IRDevices[i].virtual_comport != INVALID_HANDLE_VALUE) {
			memset (&OvCom[i + MAX_IR_DEVICES],0,sizeof (OVERLAPPED));
			OvCom[i + MAX_IR_DEVICES].hEvent = IRDevices[i].com_event;
			SetCommMask (IRDevices[i].virtual_comport,EV_RXCHAR);
			WaitCommEvent (IRDevices[i].virtual_comport,ser_event,&OvCom[i + MAX_IR_DEVICES]);
			event_type[num] = SELECT_RS232;
			event_num[num] = i;
			events[num++] = IRDevices[i].com_event;
		}
	}

	event_type[num] = SELECT_SERVER;
	event_num[num] = 0;
	events[num++] = SockEvent;

	if (LircEvent) {
		event_type[num] = SELECT_LIRC;
		event_num[num] = 0;
		events[num++] = LircEvent;
	}

	event_type[num] = SELECT_UDP;
	event_num[num] = 0;
	events[num++] = UDPEvent;

	event_type[num] = SELECT_IRTLAN;
	event_num[num] = 0;
	events[num++] = IrtLanEvent;
	

	if (WebEvent) {
		event_type[num] = SELECT_WEB;
		event_num[num] = 0;
		events[num++] = WebEvent;
	}

	if (mode_flag & XAP) {
		event_type[num] = SELECT_XAP;
		event_num[num] = 0;
		events[num++] = xAPEvent;
	}

	for (i=0;i < CLIENT_COUNT;i++) if (sockinfo[i].fd != -1 && sockinfo[i].event) {
		event_type[num] = SELECT_CLIENT;
		event_num[num] = i;
		events[num++] = sockinfo[i].event;
	}

	return (num);
}

int get_selected_event (int eventnum,HANDLE events[],byte event_type[],byte event_num[],SOCKET *sockfd)
{

	int fds = 0,i = 0;

	if (event_type[eventnum] == SELECT_TRANS || event_type[eventnum] == SELECT_RS232) ResetEvent (events[eventnum]);
	else WSAResetEvent (events[eventnum]);

	if (event_type[eventnum] == SELECT_SERVER) *sockfd = server_socket;
	else if (event_type[eventnum] == SELECT_LIRC) *sockfd = lirc_socket;
	else if (event_type[eventnum] == SELECT_UDP) *sockfd = udp_socket;
	else if (event_type[eventnum] == SELECT_WEB) *sockfd = web_socket;
	else if (event_type[eventnum] == SELECT_XAP) *sockfd = xAP_rcv;
	else if (event_type[eventnum] == SELECT_IRTLAN) *sockfd = irtlan_socket;
	else if (event_type[eventnum] == SELECT_LAN_TCP) *sockfd = IRDevices[event_num[eventnum]].io.socket;

	fds = event_type[eventnum];

	if (event_type[eventnum] == SELECT_CLIENT) {
		*sockfd = sockinfo[event_num[eventnum]].fd;
		fds = sockinfo[event_num[eventnum]].type + 100;
	}

	for (i=0;i < device_cnt;i++) {
		if (IRDevices[i].io.if_type == IF_USB) SetUSBEventEx (&IRDevices[i],0);
		else if (IRDevices[i].io.comport != INVALID_HANDLE_VALUE) SetCommMask (IRDevices[i].io.comport,0);
		if (comioflag && IRDevices[i].virtual_comport != INVALID_HANDLE_VALUE) SetCommMask (IRDevices[i].virtual_comport,0);
	}

	return (fds);
}

#endif

#ifdef LINUX

int get_selected_fd (fd_set *events,SOCKET *sockfd,int *ionum)
{
	int fds = 0,i = 0;

	for (i=0;i < device_cnt && !fds;i++) if (IRDevices[i].io.if_type != IF_LAN){
		if (FD_ISSET (IRDevices[i].io.comport,events)) {
			fds = SELECT_TRANS;
			*sockfd = IRDevices[i].io.comport;
			*ionum = i;
		}
	}
	if (!fds && FD_ISSET (server_socket,events)) {
		fds = SELECT_SERVER;
		*sockfd = server_socket;
	}
	if (!(mode_flag & NO_LIRC) && !fds && FD_ISSET (lirc_socket,events)) {
		fds = SELECT_LIRC;
		*sockfd = lirc_socket;
	}
	if (!(mode_flag & NO_LIRC) && !fds && FD_ISSET (local_socket,events)) {
		fds = SELECT_LOCAL;
		*sockfd = local_socket;
	}

	if (!(mode_flag & NO_WEB) && !fds && FD_ISSET (web_socket,events)) {
		fds = SELECT_WEB;
		*sockfd = web_socket;
	}

	if (!fds && FD_ISSET (udp_socket,events)) {
		fds = SELECT_UDP;
		*sockfd = udp_socket;
	}

	if (mode_flag & XAP && !fds && FD_ISSET (xAP_rcv,events)) {
		fds = SELECT_XAP;
		*sockfd = xAP_rcv;
	}

	if (!fds && FD_ISSET (irtlan_socket,events)) {
		fds = SELECT_IRTLAN;
		*sockfd = irtlan_socket;
	}

	i = 0;
	while (!fds && i < CLIENT_COUNT) {
		if (sockinfo[i].type && FD_ISSET (sockinfo[i].fd,events)) {
			fds = sockinfo[i].type + 100;
			*sockfd = sockinfo[i].fd;
		}
		i++;
	}

	if (fds) FD_CLR (*sockfd,events);

	return (fds);
}


int build_select_table (fd_set *events)
{
	int i,max;
	
	FD_ZERO (events);

	FD_SET (server_socket,events);
	max = server_socket;

	for (i=0;i < device_cnt;i++) if (IRDevices[i].io.if_type != IF_LAN){
		FD_SET (IRDevices[i].io.comport,events);
		if (IRDevices[i].io.comport > max) max = IRDevices[i].io.comport;
	}

	if (!(mode_flag & NO_LIRC)) {
		FD_SET (lirc_socket,events);
		if (lirc_socket > max) max = lirc_socket;
		FD_SET (local_socket,events);
		if (local_socket > max) max = local_socket;
	}


	FD_SET (udp_socket,events);
	if (udp_socket > max) max = udp_socket;

	if (!(mode_flag & NO_WEB)) {
		FD_SET (web_socket,events);
		if (web_socket > max) max = web_socket;
	}

	if (mode_flag & XAP) {
		FD_SET (xAP_rcv,events);
		if (xAP_rcv > max) max = xAP_rcv;
	}

	FD_SET (irtlan_socket,events);
	if (irtlan_socket > max) max = irtlan_socket;


	for (i=0;i < CLIENT_COUNT;i++) if (sockinfo[i].fd != -1) {
		FD_SET (sockinfo[i].fd,events);
		if (sockinfo[i].fd > max) max = sockinfo[i].fd;
	}
	return (max + 1);
}

#endif




SOCKET register_remote_client (SOCKET fd,int mode)
{
	int res,num,asciimode = 0;
	unsigned int adr;
	SOCKET call;
	int clilen;
	char rdcom[1024];
	uint32_t clientid;
	struct sockaddr_in cli_addr;

	if (mode == SELECT_LIRC) sprintf (rdcom,"LIRC TCP/IP Socket connection request\n");
	if (mode == SELECT_LOCAL) sprintf (rdcom,"Local Socket connection request\n");
	if (mode == SELECT_SERVER) sprintf (rdcom,"IRTRANS TCP/IP Socket connection request\n");

	log_print (rdcom,LOG_DEBUG);
	clilen = sizeof (cli_addr);

	call = accept (fd,(struct sockaddr *)&cli_addr,&clilen);
	if (call < 0) {
		sprintf (rdcom,"Accept error %d\n",errno);
		log_print (rdcom,LOG_ERROR);
	}
	else {
#ifdef WIN32
		adr = ntohl (cli_addr.sin_addr.S_un.S_addr);
#else
		adr = ntohl (cli_addr.sin_addr.s_addr);
#endif
		if (adr != 0x7f000001 && mode != SELECT_LOCAL) {
			for (res=0;res < netcount;res++) {
				if ((netip[res] & netmask[res]) == (adr & netmask[res])) break;
			}
			if (netcount && res == netcount) {
				sprintf (rdcom,"Error: IP Address %s not allowed (Access rights).\n",inet_ntoa (cli_addr.sin_addr));
				log_print (rdcom,LOG_ERROR);
				shutdown (call,2);
				closesocket (call);
				return (0);
			}
		}

		res = 0;
		if (mode == SELECT_SERVER) {
#ifdef WIN32
			WSAEventSelect (call,SockEvent,0);
			ioctlsocket (call,FIONBIO,&res);
#endif
			res = recv (call,(char *)&clientid,4,MSG_NOSIGNAL);
			if (res != 4) {
				shutdown (call,2);
				closesocket (call);
				return (0);
			}
#ifdef LINUX
			fcntl (call,F_SETFL,O_NONBLOCK);
#endif
			if (((char *)&clientid)[0] == 'A' || ((char *)&clientid)[1] >= 'A' || !memcmp ((char *)&clientid,"****",4)) {
				memset (ascii_initial,0,4);
				if (memcmp ((char *)&clientid,"ASCI",4) && memcmp ((char *)&clientid,"****",4)) memcpy (ascii_initial,&clientid,4);
				asciimode = MODE_ASCII;
				clientid = 0;
			}
			if (!memcmp ((char *)&clientid,"ASCT",4)) {
				asciimode = MODE_ASCII_TIME;
				clientid = 0;
			}
			swap_int (&clientid);
			res = 0;
			if (clientid > 1) {
				while (res < CLIENT_COUNT) {
					if (sockinfo[res].type == SELECT_REOPEN && 
						clientid == sockinfo[res].clientid) {
						if (sockinfo[res].fd != -1) {
#ifdef WIN32
							WSACloseEvent (sockinfo[res].event);
							sockinfo[res].event = NULL;
#endif
							shutdown (sockinfo[res].fd,2);
							closesocket (sockinfo[res].fd);
						}
						sockinfo[res].fd = call;
#ifdef WIN32
						sockinfo[res].event = WSACreateEvent ();
						WSAEventSelect (call,sockinfo[res].event,FD_READ | FD_CLOSE);
#endif
						return (call);
					}
					res++;
				}
				// Send Status illegal ID
			}
			if (clientid == 1) mode = SELECT_REOPEN;
		}
		while (res < CLIENT_COUNT) {							// Leeren Eintrag suchen
			if (sockinfo[res].type == 0) {
				sockinfo[res].fd = call;
				sockinfo[res].type = mode;
				sockinfo[res].mode = asciimode;
				if (mode == SELECT_REOPEN) sockinfo[res].clientid = seq_client++;
				strcpy (sockinfo[res].ip,inet_ntoa (cli_addr.sin_addr));
#ifdef WIN32
				sockinfo[res].event = WSACreateEvent ();
				WSAEventSelect (call,sockinfo[res].event,FD_READ | FD_CLOSE);
#endif
				if (mode == SELECT_LIRC) sprintf (rdcom,"LIRC TCP/IP Client %d accepted from %s\n",res,inet_ntoa (cli_addr.sin_addr));
				if (mode == SELECT_LOCAL) sprintf (rdcom,"Local Client %d accepted on %d\n",res,call);
				if (mode == SELECT_SERVER) {
					if (asciimode >= MODE_ASCII) sprintf (rdcom,"IRTRANS TCP/IP Client %d [ASCII Mode] accepted from %s\n",res,inet_ntoa (cli_addr.sin_addr));
					else sprintf (rdcom,"IRTRANS TCP/IP Client %d accepted from %s\n",res,inet_ntoa (cli_addr.sin_addr));
				}
				if (mode == SELECT_REOPEN) sprintf (rdcom,"IRTRANS [R] TCP/IP Client %d accepted from %s\n",res,inet_ntoa (cli_addr.sin_addr));
				log_print (rdcom,LOG_INFO);
				break;
			}
			res++;
		}
		if (res == CLIENT_COUNT) {
			res = 0;
			num = -1;
			clientid = 0xffffffff;
			while (res < CLIENT_COUNT) {						// Ältesten Eintrag suchen
				if (sockinfo[res].type == SELECT_REOPEN && 
					sockinfo[res].callno < clientid && sockinfo[res].fd == -1) {
					num = res;
					clientid = sockinfo[res].callno;
				}
				res++;
			}

			res = num;
			if (res >= 0) {
				if (sockinfo[res].fp) fclose ((sockinfo[res].fp));
				sockinfo[res].fp = NULL;

				sockinfo[res].fd = call;
				sockinfo[res].type = mode;
				if (mode == SELECT_REOPEN) sockinfo[res].clientid = seq_client++;
				strcpy (sockinfo[res].ip,inet_ntoa (cli_addr.sin_addr));
	#ifdef WIN32
				sockinfo[res].event = WSACreateEvent ();
				WSAEventSelect (call,sockinfo[res].event,FD_READ | FD_CLOSE);
	#endif
				if (mode == SELECT_LIRC) sprintf (rdcom,"LIRC TCP/IP Client %d accepted from %s\n",res,inet_ntoa (cli_addr.sin_addr));
				if (mode == SELECT_LOCAL) sprintf (rdcom,"Local Client %d accepted on %d\n",res,call);
				if (mode == SELECT_SERVER) sprintf (rdcom,"IRTRANS TCP/IP Client %d accepted from %s\n",res,inet_ntoa (cli_addr.sin_addr));
				log_print (rdcom,LOG_INFO);
			}
		}

		if (res == CLIENT_COUNT) {
			sprintf (rdcom,"No more socket client (max=%d)\n",res);
			log_print (rdcom,LOG_ERROR);
			shutdown (call,2);
			closesocket (call);
		}
	}

	return (call);
}


void process_lirc_command (SOCKET fd)
{
    int res,i;
	int num;
    char com[1024],msg[1024],*pnt,*key,*rem,err[256];
	char remote[80],command[20];
	static char active_lirc_string[1024];
    char *mp;
    char *ep;

    res = recv (fd,com,1024,MSG_NOSIGNAL);
    if (res <= 0) {
         i = 0;
         while (i < CLIENT_COUNT) {
           if (sockinfo[i].fd == fd) CloseIRSocket (i);
           i++;
		}
		*active_lirc_string = 0;
         return;
    }
    com[res] = 0;
	strcat (active_lirc_string,com);
	if (com[res-1] != 13 && com[res-1] != 10) return;
	strcpy (com,active_lirc_string);
	*active_lirc_string = 0;

	sprintf (err,"LIRC Command String: %s\n",com);
	log_print (err,LOG_DEBUG);
    
	for (mp = com; *mp && (ep = strchr(mp, '\n')); mp = ep)
        {
	       *ep++ = '\0';
		    strcpy (msg,mp);
			strcat (msg, "\n");
	        pnt = strtok (mp," \t\n\r");
										// Unterstützung Start / Stop Repeat ??
		    if (!strcmp (mp,"SEND_ONCE") || !strcmp (mp,"send_once")) {
				rem = strtok (NULL," \t\n\r");
				key = strtok (NULL," \t\n\r");
				if (rem == NULL || key == NULL) {
					sprintf (err,"IR Send Error: No Remote / Command specified\n");
					log_print (err,LOG_ERROR);
					lirc_send_error (fd,msg,err);
					continue;
				}
			strcpy (remote,rem);
			ConvertLcase (remote,(int)strlen (remote));
			strcpy (command,key);
			ConvertLcase (command,(int)strlen (command));
			sprintf (err,"LIRC SEND_ONCE  Rem: %s  Key: %s\n",remote,command);
			log_print (err,LOG_DEBUG);

			if (!memcmp (command,"+++wait",7)) {
				msSleep (atol (command + 7));
			}
			else {
				res = DBFindRemoteCommand (remote,command,&num,NULL);
				if (res) {
					sprintf (err,"IR Send Error %d\n",res);
	                log_print (err,LOG_ERROR);
		            lirc_send_error (fd,msg,err);
			        continue;
				}
			SendIR (num,0x40000000,COMMAND_SEND,NULL);
         }
         resend_flag = 0;
         lirc_send_success (fd,msg);
         continue;
        }
        if (!strcmp (mp,"LIST") || !strcmp (mp,"list") || !strcmp (mp,"List")) {
         rem = strtok (NULL," \t\n\r");
         key = strtok (NULL," \t\n\r");
         lirc_list_command (fd,rem,key,msg);
         continue;
        }

        sprintf (err,"Unknown LIRC Command received: %s\n",pnt);
        log_print (err,LOG_ERROR);
        lirc_send_error (fd,msg,err);
        }
}


void lirc_list_command (SOCKET fd,char rem[],char key[],char msg[])
{
	int i,j;
	char st[1024],err[256],num[1000];
	char remote[80],command[20];

	memset (remote,0,80);
	memset (command,0,20);


	if (rem == NULL && key == NULL) {
		sprintf (err,"LIRC LIST REMOTES received.\n");
		log_print (err,LOG_DEBUG);

		sprintf (st,"BEGIN\n%sSUCCESS\nDATA\n%d\n",msg,rem_cnt);
		send (fd,st,(int)strlen (st),MSG_NOSIGNAL);
		for (i=0;i < rem_cnt;i++) {
			sprintf (st,"%s\n",rem_pnt[i].name);
			send (fd,st,(int)strlen (st),MSG_NOSIGNAL);
		}

		sprintf (st,"END\n");
		send (fd,st,(int)strlen (st),MSG_NOSIGNAL);
		return;
	}

	if (key == NULL) {
		strcpy (remote,rem);
		ConvertLcase (remote,(int)strlen (remote));
		sprintf (err,"LIRC LIST COMMANDS %s received.\n",remote);
		log_print (err,LOG_DEBUG);

		i = DBFindRemote (remote);
		if (i == -1) {
			sprintf (st,"No Remote %s found",rem);
			lirc_send_error (fd,msg,st);
			return;
		}
		sprintf (st,"BEGIN\n%sSUCCESS\nDATA\n%d\n",msg,rem_pnt[i].command_end - rem_pnt[i].command_start);
		send (fd,st,(int)strlen (st),MSG_NOSIGNAL);
		for (j=0;j < rem_pnt[i].command_end - rem_pnt[i].command_start;j++) {
			
			GetNumericCode (cmd_pnt[rem_pnt[i].command_start + j].data,num,remote,cmd_pnt[rem_pnt[i].command_start + j].name);

			sprintf (st,"%s %s\n",num,cmd_pnt[rem_pnt[i].command_start + j].name);
			send (fd,st,(int)strlen (st),MSG_NOSIGNAL);
		}

		sprintf (st,"END\n");
		send (fd,st,(int)strlen (st),MSG_NOSIGNAL);
		return;
	}
	strcpy (remote,rem);
	ConvertLcase (remote,(int)strlen (remote));
	i = DBFindRemote (remote);
	if (i == -1) {
		sprintf (st,"No Remote %s found",rem);
		lirc_send_error (fd,msg,st);
		return;
	}
	strcpy (command,key);
	ConvertLcase (command,(int)strlen (command));
	
	sprintf (err,"LIRC LIST COMMAND DETAIL %s-%s received.\n",remote,command);
	log_print (err,LOG_DEBUG);

	j = DBFindCommand (command,&i);
	if (j == -1) {
		sprintf (st,"No Remote/Command  %s/%s found",rem,key);
		lirc_send_error (fd,msg,st);
		return;
	}
	sprintf (st,"BEGIN\n%sSUCCESS\nDATA\n%d\n",msg,1);
	send (fd,st,(int)strlen (st),MSG_NOSIGNAL);

	GetNumericCode (cmd_pnt[j].data,num,remote,command);

	sprintf (st,"%s %s\n",num,command);
	send (fd,st,(int)strlen (st),MSG_NOSIGNAL);

	sprintf (st,"END\n");
	send (fd,st,(int)strlen (st),MSG_NOSIGNAL);
}


void lirc_send_success (SOCKET fd,char msg[])
{
	char st[1024];

	sprintf (st,"BEGIN\n%sSUCCESS\nEND\n",msg);
	send (fd,st,(int)strlen (st),MSG_NOSIGNAL);
}


void lirc_send_error (SOCKET fd,char msg[],char err[])
{
	char st[1024];

	sprintf (st,"BEGIN\n%sERROR\nDATA\n1\n%s\nEND\n",msg,err);
	send (fd,st,(int)strlen (st),MSG_NOSIGNAL);
}
	

// Erkannte IR Codes an Clients schicken

#ifdef WIN32
#ifndef WINCE
unsigned int GetFineTime (void)
{
	struct _timeb tb;

	_ftime (&tb);
	return (unsigned int)((tb.time & 0x7fffff) * 1000 + tb.millitm);
}
#endif
#endif

#ifdef LINUX
unsigned int GetFineTime (void)
{
	struct timeval tb;

	gettimeofday (&tb,NULL);
	return (tb.tv_sec & 0x7fffff) * 1000 + tb.tv_usec / 1000;
}
#endif

#ifdef WINCE
unsigned int GetFineTime (void)
{
	// GetLocalTime (...);
}


#endif


void send_forward (int client,char rem[],char name[])
{
	int res,i;
	NETWORKRECV nr;
	char msg[256];

	memset (&nr,0,sizeof (NETWORKRECV));
	memset (nr.data,' ',200);
	memset (nr.remote,' ',80);
	memset (nr.command,' ',20);

	nr.clientid = 0;
	nr.statuslen = sizeof (NETWORKRECV) - 224;
	nr.statustype = STATUS_RECEIVE;
	memcpy (nr.remote,rem,strlen (rem));
	memcpy (nr.command,name,strlen (name));

	SwapNetworkheader ((NETWORKSTATUS *)&nr);

	for (i=0;i < CLIENT_COUNT;i++) if (i != client) {

		if (sockinfo[i].type == SELECT_SERVER || sockinfo[i].type == SELECT_REOPEN) {
			if (sockinfo[i].mode >= MODE_ASCII) {
				sprintf (msg,"**00000 RCV_COM %s,%s,%d,%d\n",rem,name,0,0);
				sprintf (msg+2,"%05d",strlen (msg));
				msg[7] = ' ';
				res = send (sockinfo[i].fd,msg,(int)strlen (msg),MSG_NOSIGNAL);
			}
			else res = send (sockinfo[i].fd,(char *)&nr,sizeof (NETWORKRECV),MSG_NOSIGNAL);
			if (res <= 0) CloseIRSocket (i);
		}
	}
}



int ExecuteReceivedCommand (byte *command,int len,int bus)
{
	int res,i = 0;
	int start_pos = 0;
	byte longcode[512];
	static time_t l_time;
	static char l_command[21];
	static char l_remote[81];
	static int l_repeat;
	static char l_addr;
	int com_num,rem_num;
	static unsigned int ltv;
	static int rcnt;
	unsigned int tv;


	char rem[512],name[512],num[512],dat[1024],msg[1024];
	NETWORKRECV_LONG nr;

	memset (&nr,0,sizeof (NETWORKRECV_LONG));
	memset (nr.data,' ',424);
	memset (nr.remote,' ',80);
	memset (nr.command,' ',20);


	if (!(IRDevices[bus].io.inst_receive_mode & 2)) {
		tv = GetFineTime ();

		if ((!rcnt && (tv - ltv) < 200) || (tv - ltv) < 250) {
			rcnt++;
			ltv = tv;
			if (rcnt < 5) return (1);
		}
		else rcnt = 0;
		ltv = tv;
	}

	if (command[1] & LONG_CODE_FLAG) {
		*longcode = *command;
		ConvertLongcode (command + 1,len-2,longcode + 1);
		command = longcode;
	}

	start_pos = DBFindCommandName (command + 1,rem,name,*command,&rem_num,&com_num,&nr.command_num,start_pos);

	if (start_pos) {
		while (start_pos) {
			if (time (0) != l_time || strcmp (l_command,name) || strcmp (l_remote,rem)) l_repeat = 0;
			
			if ((time (0) - l_time) < 2 && !strcmp (l_command,name) && !strcmp (l_remote,rem) && l_addr != (*command & 0xf)) return (1);
			GetNumericCode (command + 1,num,rem,name);
			if (mode_flag & DEBUG_CODE) {
				sprintf (msg,"[%d.%d] %s %s\n",bus,(*command & 15),name,rem);
				log_print (msg,LOG_FATAL);
			}
			sprintf (dat,"%s %02d %s %s%c",num,l_repeat,name,rem,10);

			memset (&nr,0,sizeof (NETWORKRECV_LONG));
			memset (nr.data,' ',424);
			memset (nr.remote,' ',80);
			memset (nr.command,' ',20);
			nr.clientid = 0;
			nr.statuslen = sizeof (NETWORKRECV);
			nr.statustype = STATUS_RECEIVE;
			memcpy (nr.remote,rem,strlen (rem));
			memcpy (nr.command,name,strlen (name));
			if (command[1] >= '0') memcpy (nr.data,command+1,strlen (command+1));
			else memcpy (nr.data,num,strlen (num));
			nr.adress = (*command & 15) + bus * 16;

			SwapNetworkheader ((NETWORKSTATUS *)&nr);
			if (command[1] != 'M' && command[1] != 'K' && command[1] != 'R' &&  command[1] != 'E' && command[1] != 'P') {
				i = 0;
				while (i < CLIENT_COUNT) {
					if (sockinfo[i].type == SELECT_LIRC || sockinfo[i].type == SELECT_LOCAL) {
						res = send (sockinfo[i].fd,dat,(int)strlen (dat),MSG_NOSIGNAL);
						if (res <= 0) CloseIRSocket (i);
					}
					if (sockinfo[i].type == SELECT_SERVER || sockinfo[i].type == SELECT_REOPEN) {
						res = 1;
						if (sockinfo[i].mode == MODE_NO_RECEIVE) {
							if (nr.data[0] == 'F') {
								res = send (sockinfo[i].fd,(char *)&nr,sizeof (NETWORKRECV),MSG_NOSIGNAL);
								if (res <= 0) CloseIRSocket (i);
							}
						}
						else if (sockinfo[i].mode >= MODE_ASCII) {
							sprintf (msg,"**00000 RCV_COM %s,%s,%d,%d\n",rem,name,bus,*command & 15);
							sprintf (msg+2,"%05d",strlen (msg));
							msg[7] = ' ';
							res = send (sockinfo[i].fd,msg,(int)strlen (msg),MSG_NOSIGNAL);
						}
						else res = send (sockinfo[i].fd,(char *)&nr,sizeof (NETWORKRECV),MSG_NOSIGNAL);
						if (res <= 0) CloseIRSocket (i);
					}
					i++;
				}
				if (udp_relay_port) udp_relay (rem,name,*command & 15);

				if (mode_flag & XAP) xAP_SendIREvent (rem,name,bus,*command & 15);
			}
#ifdef MEDIACENTER
#ifdef WIN32
			if (command[1] == 'R' || command[1] == 'E' || command[1] == 'P') {
				HandleHID (rem_num,com_num,name,command + 1);
			}
			else 
#endif
				PostWindowsMessage (rem_num,com_num,name);
#endif
			l_repeat++;
			l_time = time (0);
			strcpy (l_command,name);
			strcpy (l_remote,rem);
			l_addr = *command & 0xf;
			start_pos = DBFindCommandName (command + 1,rem,name,*command,&rem_num,&com_num,&nr.command_num,start_pos);
		}
		return (0);
	}
	else {
		if (!(((*command & 0xf0) >> 2) & RAW_DATA) && command[1] >= '0' && !(mode_flag & LEARNED_ONLY)) { 
			memset (&nr,0,sizeof (NETWORKRECV_LONG));
			memset (nr.data,' ',424);
			memset (nr.remote,' ',80);
			memset (nr.command,' ',20);
			nr.clientid = 0;
			nr.statuslen = sizeof (NETWORKRECV);
			nr.statustype = STATUS_RECEIVE;
			memcpy (nr.data,command+1,strlen (command+1));
			nr.adress = (*command & 15) + bus * 16;
			nr.command_num = 0;

			SwapNetworkheader ((NETWORKSTATUS *)&nr);
			i = 0;
			while (i < CLIENT_COUNT) {
				if (sockinfo[i].type == SELECT_SERVER || sockinfo[i].type == SELECT_REOPEN) {
					res = 1;
					if (sockinfo[i].mode == MODE_NO_RECEIVE) {
						if (nr.data[0] == 'F') {
							res = send (sockinfo[i].fd,(char *)&nr,sizeof (NETWORKRECV),MSG_NOSIGNAL);
							if (res <= 0) CloseIRSocket (i);
						}
					}
					else if (sockinfo[i].mode >= MODE_ASCII) {
						sprintf (msg,"**00000 RCV_COD %s,%d,%d\n",command+1,bus,*command & 15);
						sprintf (msg+2,"%05d",strlen (msg));
						msg[7] = ' ';
						res = send (sockinfo[i].fd,msg,(int)strlen (msg),MSG_NOSIGNAL);
					}
					else res = send (sockinfo[i].fd,(char *)&nr,sizeof (NETWORKRECV),MSG_NOSIGNAL);
					if (res <= 0) CloseIRSocket (i);
				}
				i++;
			}
			if (mode_flag & DEBUG_CODE) {
				sprintf (msg,"[%d.%d]: LEN: %d  %s\n",bus,(*command & 15),strlen(command+1),command+1);
				log_print (msg,LOG_FATAL);
			}
		}
	}
	return (1);
}

int ConvertLongcode (byte cmd[],int len,byte target[])
{
	int i,irlen = 0;
	
	if (cmd[0] & LONG_CODE_LEN) {
		len--;
		irlen++;
	}
	irlen += len * 2;

	for (i=0;i < len;i++) {
		target[i * 2] = (cmd[i] & 0x7) + '0';
		target[i * 2 + 1] = ((cmd[i] & 0x70) >> 4) + '0';
	}
	if (cmd[0] & LONG_CODE_LEN) target[i * 2] = (cmd[i] & 0x7) + '0';
	
	target[irlen] = 0;

	return (irlen);
}

void udp_relay (char rem[],char com[],int adr)
{
	int i,p;
	char frm[255],dat[255];
	char adrst[10];
	char *parm[3];

	parm[0] = parm[1] = parm[2] = 0;
	sprintf (adrst,"%02d",adr);
	strcpy (frm,udp_relay_format);

	p = 0;
	for (i=0;frm[i];i++) {
		if (frm[i] == '%') {
			if (p == 3) frm[i] = ' ';
			else {
				i++;
				if (frm[i] == 'r') {
					parm[p++] = rem;
					frm[i] = 's';
				}
				else if (frm[i] == 'c') {
					parm[p++] = com;
					frm[i] = 's';
				}
				else if (frm[i] == 'a') {
					parm[p++] = adrst;
					frm[i] = 's';
				}
				else frm[i-1] = ' ';
			}
		}
	}

	sprintf (dat,frm,parm[0],parm[1],parm[2]);
	i = send (udp_relay_socket,dat,(int)strlen (dat),0);
}


void CloseIRSocket (int client)
{
#ifdef WIN32
	WSACloseEvent (sockinfo[client].event);
	sockinfo[client].event = NULL;
#endif
	if (sockinfo[client].fd) {
		shutdown (sockinfo[client].fd,2);
		closesocket (sockinfo[client].fd);
	}
	sockinfo[client].fd = -1;
	if (sockinfo[client].type == SELECT_REOPEN) return;

	sockinfo[client].type = 0;
	sockinfo[client].callno = 0;
	sockinfo[client].mode = 0;
	sockinfo[client].msg_mode = 0;

	if (sockinfo[client].fp) fclose ((sockinfo[client].fp));
	sockinfo[client].fp = NULL;

}

#ifdef LINUX
#ifdef DBOX

int ReadIRDatabase (void)
{

        int res,fl;
        char st[1024],*pnt,msg[256];
        FILE *fd;
        char *home = getenv("HOME");
 
        if (home) snprintf(st, sizeof(st), "%s/.irtrans/remotes", home);
        
		if (IRDataBaseRead) FreeDatabaseMemory ();


		else {
			if (irdb_path[0]) {
				if (chdir (irdb_path)) return (ERR_NODATABASE);			
			}
			else if (chdir ("./remotes") && !(home && chdir (st) == 0) && 
				     chdir ("/etc/irserver/remotes") && chdir ("/usr/local/share/irtrans/remotes") && 
					 chdir ("/usr/share/irtrans/remotes")) return (ERR_NODATABASE);
		}

        ReadRoutingTable ();
		
        ReadSwitches ();
		
        fd = popen ("ls","r");

        pnt = fgets (st,1000,fd);
		
        while (pnt) {

         if (pnt[strlen (pnt) - 1] == 10) pnt[strlen (pnt) - 1] = 0;
         fl = strlen (pnt) - 4;
         if (fl >= 1 && !strcmp (pnt + fl,".rem")) {
           res = DBReadCommandFile (pnt);
           if (res) {
                sprintf (msg,"Error %d reading DB-File %s\n",res,pnt);
                log_print (msg,LOG_ERROR);
           }
         }
         pnt = fgets (st,1000,fd);
        }

        pclose (fd);

		res = DBReferenceLinks ();

        DBShowStatus ();

		ReadAppConfig ();

        IRDataBaseRead = 1;
        return (0);
}

#else   // Keine DBOX, normales LINUX

int ReadIRDatabase (void)
{

        int fd,i,len,pos,res,fl;
        long off;
        char st[2048],msg[256];
        struct dirent *di;
        char *home = getenv("HOME");
		char *rdir = getenv("IRTRANS_REMOTES");

		if (irdb_path[0]) {
			if (chdir (irdb_path)) {
				sprintf (msg,"Error opening remote database %s\n",irdb_path);
				log_print (msg,LOG_FATAL);
				return (ERR_NODATABASE);			
			}
		}
		else if (rdir) {
			if (chdir (rdir)) {
				sprintf (msg,"Error opening remote database %s\n",rdir);
				log_print (msg,LOG_FATAL);

				return (ERR_NODATABASE);
			}
		}

		else {
			if (home)
				snprintf(st, sizeof(st), "%s/.irtrans/remotes", home);
        
			if (IRDataBaseRead) FreeDatabaseMemory ();

			else if (chdir ("./remotes")
			&& !(home && chdir (st) == 0)
			&& chdir ("/etc/irserver/remotes")
			&& chdir ("/usr/local/share/irtrans/remotes")
			&& chdir ("/usr/share/irtrans/remotes")) return (ERR_NODATABASE);
		}

		sprintf (msg,"Chdir to DB OK\n");
		log_print (msg,LOG_DEBUG);

        ReadRoutingTable ();

		sprintf (msg,"Read routing OK\n");
		log_print (msg,LOG_DEBUG);

        ReadSwitches ();

  		sprintf (msg,"Read Switches OK\n");
		log_print (msg,LOG_DEBUG);

	    fd = open (".",0);

		sprintf (msg,"Open DIR: %d\n",fd);
		log_print (msg,LOG_DEBUG);

        do {
         len = getdirentries (fd,st,2048,&off);

		sprintf (msg,"Get Dirent: %d\n",len);
		log_print (msg,LOG_DEBUG);

         pos = 0;
         while (pos < len) {
           di = (struct dirent *)&st[pos];
           fl = strlen (di -> d_name) - 4;
           if (fl >= 1 && !strcmp (di->d_name + fl,".rem")) {
                res = DBReadCommandFile (di->d_name);
                if (res) {
                      sprintf (msg,"Error %d reading DB-File %s\n",res,di->d_name);
                      log_print (msg,LOG_ERROR);
                }
           }
           pos += di -> d_reclen;
         }
        } while (len);

        close (fd);

		res = DBReferenceLinks ();

        DBShowStatus ();
		ReadAppConfig ();

        IRDataBaseRead = 1;
        return (0);
}

#endif

#endif



#ifdef WIN32

#ifdef WINCE
int ReadIRDatabase (void)
{
	return (0);
}

#else
int ReadIRDatabase (void)
{
	int res;
    struct _finddata_t c_file;
#ifdef _M_X64
    intptr_t hFile;
#else
    int hFile;
#endif
	char msg[256];
	char *rdir = getenv("IRTRANS_REMOTES");

	if (irdb_path[0]) {
		if (chdir (irdb_path)) {
			sprintf (msg,"Error opening remote database %s\n",irdb_path);
			log_print (msg,LOG_FATAL);
			return (ERR_NODATABASE);			
		}
	}
	else if (rdir) {
		if (_chdir (rdir)) {
			sprintf (msg,"Error opening remote database %s\n",rdir);
			log_print (msg,LOG_FATAL);

			return (ERR_NODATABASE);
		}
	}

	else {
		if (IRDataBaseRead) FreeDatabaseMemory ();
		else if (_chdir ("remotes")) return (ERR_NODATABASE);
	}

    ReadRoutingTable ();
	ReadSwitches ();

	if((hFile = _findfirst( "*.rem", &c_file )) != -1L) {
		do {
			res = DBReadCommandFile (c_file.name);
			if (res) {
				sprintf (msg,"Error %d reading DB-File %s\n",res,c_file.name);
				log_print (msg,LOG_ERROR);
			}
		} while( _findnext( hFile, &c_file ) == 0);
		_findclose( hFile );
	}

	res = DBReferenceLinks ();

	DBShowStatus ();
	ReadAppConfig ();

	IRDataBaseRead = 1;
	return (0);
}


#endif
#endif


void ExecuteNetCommand (SOCKET sockfd)
{
	int client,errcnt;
	int res,sz,len;
	char err[2048];
	char c;
	char *pnt;
	char buffer[sizeof (CCFLEARNCOMMAND)];
	char buffer1[sizeof (CCFLEARNCOMMAND)];

	STATUSBUFFER stat;
	NETWORKCOMMAND *com;

	client = GetNetworkClient (sockfd);
	if (client == -1) {
		// Error not found
		return;
	}

	sockinfo[client].callno = seq_call++;
	
	com = (NETWORKCOMMAND *)buffer;

	while (1) {
		if (sockinfo[client].restlen) {
			memcpy (buffer,sockinfo[client].restdata,sockinfo[client].restlen + 1);
			
			sz = sockinfo[client].restread - sockinfo[client].restlen;

			res = recv (sockfd,buffer + sockinfo[client].restlen + 1,sz,MSG_NOSIGNAL);
			if (res < 0) {
#ifdef WIN32
				if (WSAGetLastError () == WSAEWOULDBLOCK) return;
#else
				if (errno == EAGAIN) return;
#endif
			}

			sockinfo[client].restlen = 0;
		}

		else {
			res = recv (sockfd,buffer,1,MSG_NOSIGNAL);


			if (res != 1 || ((*buffer < 1 || *buffer > COMMAND_STATUSEX3_SHORT) && !*ascii_initial) && (sockinfo[client].mode < MODE_ASCII || sockinfo[client].mode >= MODE_NO_RECEIVE)) {
				if (res < 0) {
		#ifdef WIN32
					if (WSAGetLastError () == WSAEWOULDBLOCK) return;
		#else
					if (errno == EAGAIN) return;
		#endif
				}

				if (res <= 0) CloseIRSocket (client);
				if (res == 0) {
					sprintf (err,"Client [%d] disconnect\n",client);
					log_print (err,LOG_INFO);
				}
				else {
					if (res == 1) sprintf (err,"Illegal Network command [%d]\n",*buffer);
					if (res < 0)  sprintf (err,"Network connection [%d] closed\n",client);
					log_print (err,LOG_ERROR);
				}
				return;
			}

			if (sockinfo[client].mode >= MODE_ASCII && sockinfo[client].mode < MODE_NO_RECEIVE) {
				while (res == 1 && (*buffer == 13 || *buffer == 10)) res = recv (sockfd,buffer,1,MSG_NOSIGNAL);
				if (res <= 0) return;
			}

			if (*buffer == COMMAND_ASCII || (sockinfo[client].mode >= MODE_ASCII && sockinfo[client].mode < MODE_NO_RECEIVE)) {
				if (*ascii_initial) {
					buffer[4] = buffer[0];
					memcpy (buffer,ascii_initial,4);
					memset (ascii_initial,0,4);
					len = 5;
				}
				else len = 1;
				res = recv (sockfd,&c,1,MSG_NOSIGNAL);
				while (res == 1 && c != '\n') {
					buffer[len++] = c;
					res = recv (sockfd,&c,1,MSG_NOSIGNAL);
					if (res != 1) {
						msSleep (1);
						res = recv (sockfd,&c,1,MSG_NOSIGNAL);
					}
				}
				if (c == '\n') buffer[len++] = c;
				buffer[len] = 0;
				if (buffer[len-1] != 13 && buffer[len-1] != 10) {
					sprintf (err,"Non terminated ASCII string: %d\n",buffer[len-1]);
					log_print (err,LOG_ERROR);
					return;
				}

				while (buffer[len-1] == 13 || buffer[len-1] == 10) {
					buffer[len-1] = 0;
					len--;
				}

				pnt = buffer;
				if (*buffer == COMMAND_ASCII) pnt++;

				sprintf (err,"ASCII Command: %s\n",pnt);
				log_print (err,LOG_INFO);

				DoExecuteASCIICommand ((byte *)pnt,sockfd,client,sockinfo[client].mode);
				return;
			}

			if (*buffer == COMMAND_LCD || *buffer == COMMAND_LCDINIT) sz = sizeof (LCDCOMMAND) - 1;
			else if (*buffer == COMMAND_STORETRANS) sz = sizeof (TRANSLATECOMMAND) - 1;
			else if (*buffer == COMMAND_SENDCCF) sz = sizeof (CCFCOMMAND) - 1;
			else if (*buffer == COMMAND_SENDCCFLONG) sz = sizeof (LONGCCFCOMMAND) - 1;
			else if (*buffer == COMMAND_RS232_SEND) sz = sizeof (SERCOMMAND) - 1;
			else if (*buffer == COMMAND_STOREIRDB)  sz = sizeof (IRDBHEADER) - 1;
			else if (*buffer == COMMAND_SENDCCFSTR) sz = sizeof (CCFSTRINGCOMMAND) - 1;
			else if (*buffer == COMMAND_SENDCCFSTRS) sz = sizeof (CCFSTRINGCOMMAND_SHORT) - 1;
			else if (*buffer == COMMAND_SETSTATEX) sz = sizeof (MODUSCOMMAND) - 1;
			else if (*buffer == COMMAND_SETSTATEX2) sz = sizeof (MODUSCOMMAND_EX) - 1;
			else if (*buffer == COMMAND_SETSTATEX3) sz = sizeof (MODUSCOMMAND_EX3) - 1;
			else if (*buffer == COMMAND_STORETIMER) sz = sizeof (TIMERCOMMAND) - 1;
			else if (*buffer == COMMAND_LEARNCCF || *buffer == COMMAND_LEARNRS232 || *buffer == COMMAND_LEARNLINK || *buffer == COMMAND_SET_IRCODE) sz = sizeof (CCFLEARNCOMMAND) - 1;
			else if (*buffer == COMMAND_STOREWLAN) sz = sizeof (WLANCONFIGCOMMAND) - 1;
			else if (*buffer == COMMAND_WRITE_SYSPARAM) sz = sizeof (SYSPARAMCOMMAND) - 1;
			else if (*buffer == COMMAND_SENDMACRO) sz = sizeof (MACRO_NETCOMMAND) - 1;
			else if (*buffer == COMMAND_SET_ANALOGLEVEL) sz = sizeof (ANALOG_CONFIG_COMMAND) - 1;
			else sz = sizeof (NETWORKCOMMAND) - 1;

			res = recv (sockfd,buffer + 1,sz,MSG_NOSIGNAL);
			sprintf (err,"Netcommand Size: %d/%d\n",sz,res);
			log_print (err,LOG_DEBUG);

		}

		if (*buffer == COMMAND_STOREIRDB && res == (sizeof (IRDBCOMMAND) - 1)) sz = (sizeof (IRDBCOMMAND) - 1);
		 
		if (res == sz - 4) {														// Altes Commandformat
			sz = res;
			memcpy (buffer1,buffer,res);
			memcpy (buffer,buffer1,8);
			memcpy (buffer+12,buffer1+8,res-8);
		}
		
		else if (res > 12 && (com->protocol_version / 100) != (PROTOCOL_VERSION / 100)) {
			sprintf (err,"ExecuteNetCommand: Illegal Protocol Version %d.%d (should be %d.%d)\n",
					 com->protocol_version/100,com->protocol_version%100,PROTOCOL_VERSION/100,PROTOCOL_VERSION%100);
			log_print (err,LOG_FATAL);
			return;
		}
		protocol_version = com->protocol_version;

		while (res != sz) {
			msSleep (50);
			len = recv (sockfd,buffer + 1 + res,sz - res,MSG_NOSIGNAL);
			if (len <= 0) {
				CloseIRSocket (client);
/*			if (res) {
				memcpy (sockinfo[client].restdata,buffer,res + 1);
				sockinfo[client].restlen = res;
				sockinfo[client].restread = sz;
			}*/
				return;
			}
			res += len;
		}

		SwapNetworkcommand (com);
		DoExecuteNetCommand (client,com,&stat);

		stat.clientid = sockinfo[client].clientid;
		len = stat.statuslen;
		SwapNetworkstatus (&stat);

		errcnt = sz = 0;
		while (sz < len && errcnt < 20) {
			res = send (sockfd,((char *)&stat) + sz,len - sz,MSG_NOSIGNAL);
			sprintf (err,"Send Status %d - %d [%d]\n",res,len,stat.statustype);
			log_print (err,LOG_DEBUG);
			if (res > 0) sz += res;
			if (res == -1) {
				msSleep (100);
				errcnt++;
			}
		}

		if (res <= 0) {
			CloseIRSocket (client);
			sprintf (err,"IP Connection lost\n");
			log_print (err,LOG_ERROR);
		}
	}

}



void DoExecuteNetCommand (int client,NETWORKCOMMAND *com,STATUSBUFFER *stat)
{
	int res,i,j;
	int bus;
	byte cal;
	int cmd_num;
	IRDATA ird;
	IRRAW *irw;
	FILE *fp;
	HANDLE hfile;
	char st[255],err[255];
	byte dat[255];
	NETWORKSTATUS *ns;
	LCDCOMMAND *lcd;
	TRANSLATECOMMAND *tr;
	NETWORKLEARNSTAT *lstat;
	FUNCTIONBUFFER *fb;
	ANALOGBUFFER *ab;
	ANALOGBUFFER_EX *abex;
	FUNCTIONBUFFEREX *fbex;
	NETWORKLCDSTAT *lcdb;
	IRDBCOMMAND *db;
	SERCOMMAND *ser;
	CCFSTRINGCOMMAND *ccf;
	CCFSTRINGCOMMAND_SHORT *ccfs;
	CCFLEARNCOMMAND *ccfl;
	MODUSCOMMAND_EX3 *mcom;
	WLANCONFIGCOMMAND *wcom;
	SYSPARAMCOMMAND *sysparm;
	MACRO_NETCOMMAND *macro;

	unsigned int end_time;
	static byte suspend;


	memset (stat,0,sizeof (STATUSBUFFER));
	stat ->statuslen = 8;

	sprintf (st,"Netcommand: %d [%d]\n",com->netcommand,client);
	log_print (st,LOG_DEBUG);

	switch (com->netcommand) {

	case COMMAND_NORECEIVE:
		sockinfo[client].mode = MODE_NO_RECEIVE;
		break;
	case COMMAND_EMPTY:
		break;
	case COMMAND_CLIENTLOG:
		sockinfo[client].msg_mode = 1;
		NetworkClientMessage (init_message,client);
		break;
	case COMMAND_LED:
		SetPowerLED (0,com->remote[0],com->command[0]);
		break;
	case COMMAND_DEFINECHAR:
		AdvancedLCD (LCD_DATA | LCD_DEFINECHAR,com->remote,com->remote[0] * 9 + 1);
		if (new_lcd_flag) msSleep (250);

		break;
	case COMMAND_CLEARFLASH:
		sprintf (st,"Clear Flash ADR %x\n",com->adress);
		log_print (st,LOG_DEBUG);

		memset (st,0,128);

		TransferFlashdataEx ((byte)((com->adress) >> 8),(word *)st,49152,128,0,com->adress & 0xff);
		TransferFlashdataEx ((byte)((com->adress) >> 8),(word *)st,32768,128,0,com->adress & 0xff);
		TransferFlashdataEx ((byte)((com->adress) >> 8),(word *)st,16384,128,0,com->adress & 0xff);
		res = TransferFlashdataEx ((byte)((com->adress) >> 8),(word *)st,0,128,1,com->adress & 0xff);

		if (res) PutNetworkStatus (res,NULL,stat);
		break;
	case COMMAND_MCE_CHARS:
		SetSpecialChars (dat);

		AdvancedLCD (LCD_DATA | LCD_DEFINECHAR,dat,dat[0] * 9 + 1);
		if (new_lcd_flag) msSleep (250);

		break;
	case COMMAND_LCDSTATUS:
		lcdb = (NETWORKLCDSTAT *)stat;
		memset (lcdb,0,sizeof (NETWORKLCDSTAT));
		lcdb->statustype = STATUS_LCDDATA;
		lcdb->statuslen = sizeof (NETWORKLCDSTAT);
		lcdb->numcol = 0;
		lcdb->numrows = 0;
		lcdb->clockflag = 0;

		if (new_lcd_flag == 1) {
			lcdb->clockflag = 3;
			lcdb->virtual_col = 40;
			lcdb->numcol = 20;
			lcdb->numrows = 4;
		}

		if (new_lcd_flag == 2) {
			lcdb->clockflag = 3;
			lcdb->virtual_col = 40;
			lcdb->numcol = 40;
			lcdb->numrows = 4;
		}

		if (new_lcd_flag) {
			display_bus = 0xffff;
			return;
		}


		for (i=0;i < device_cnt;i++) if (IRDevices[i].version[0] == 'D' || (IRDevices[i].fw_capabilities & FN_DISPMASK)) {
			display_bus = i;
			break;
		}

		if (IRDevices[display_bus].version[0] == 'D') {
			lcdb->numcol = 16;
			lcdb->numrows = 2;
		}

		if ((IRDevices[display_bus].fw_capabilities & FN_DISPMASK) == FN_DISP1) {
			lcdb->numcol = 16;
			lcdb->numrows = 2;
			if (IRDevices[display_bus].fw_capabilities & FN_NOSCROLL) lcdb->virtual_col = lcdb->numcol;
			else lcdb->virtual_col = 40;
		}
		if ((IRDevices[display_bus].fw_capabilities & FN_DISPMASK) == FN_DISP2) {
			lcdb->numcol = 20;
			lcdb->numrows = 4;
			if (IRDevices[display_bus].fw_capabilities & FN_NOSCROLL) lcdb->virtual_col = lcdb->numcol;
			else lcdb->virtual_col = 40;
		}
		if (IRDevices[display_bus].fw_capabilities & FN_CLOCK) lcdb->clockflag |= 1;
		if (IRDevices[display_bus].io.advanced_lcd & 1) lcdb->clockflag |= 2;
		if (IRDevices[display_bus].io.advanced_lcd & 4) lcdb->clockflag |= 4;
		break;
	case COMMAND_STORETIMER:
		if (protocol_version >= 210) bus = (com->adress >> 19) & (MAX_IR_DEVICES - 1);
		else bus = (com->adress >> 20) & (MAX_IR_DEVICES - 1);

		res = StoreTimerEntry (bus,(TIMERCOMMAND *)com);
		if (res) PutNetworkStatus (res,NULL,stat);
		break;
	case COMMAND_SHUTDOWN:
		if (strcmp (com->remote,"XXXshutdownXXX")) {
			PutNetworkStatus (ERR_SHUTDOWN,NULL,stat);
			break;
		}
		LCDBrightness (4);
		if (!(mode_flag & NO_CLOCK)) LCDTimeCommand (LCD_DISPLAYTIME);
		sprintf (st,"IRTrans Server Shutdown via Client");
		log_print (st,LOG_FATAL);
		exit (0);
		break;
	case COMMAND_BRIGHTNESS:
		LCDBrightness (com->adress);
		break;
	case COMMAND_SUSPEND:
		suspend = 1;
		if (!(mode_flag & NO_CLOCK)) LCDTimeCommand (LCD_DISPLAYTIME);
		lcd_init = 1;
		LCDBrightness (4);
		break;
	case COMMAND_RESUME:
		suspend = 0;
		lcd_init = 1;
		LCDBrightness (5);
		break;
	case COMMAND_STORETRANS:
		tr = (TRANSLATECOMMAND *)com;
		StoreTransItem (tr);
		break;
	case COMMAND_STOREIRDB:
		db = (IRDBCOMMAND *)com;
		res = StoreDbItem (db);
		if (res) {
			ns = (NETWORKSTATUS *)stat;
			PutNetworkStatus (res,NULL,stat);
			strcpy (err,ns->message);
			if (res == ERR_REMOTENOTFOUND) sprintf (ns->message,err,db->remote);
			if (res == ERR_COMMANDNOTFOUND) sprintf (ns->message,err,db->command);
		}
		break;
	case COMMAND_FLASHIRDB:
		sprintf (st,"Flash IRDB ADR %x\n",com->adress);
		log_print (st,LOG_DEBUG);
		res = SetIRDBEx ((byte)((com->adress) >> 8),(com->adress & 0xff),stat);
		if (res) PutNetworkStatus (res,NULL,stat);
		break;
	case COMMAND_SAVETRANS:
		res = FileTransData (com->remote,1,(byte)com->adress);
		if (res) PutNetworkStatus (res,NULL,stat);
		break;
	case COMMAND_SAVEIRDB:
		res = FileTransData (com->remote,2,(byte)com->adress);
		if (res) PutNetworkStatus (res,NULL,stat);
		break;
	case COMMAND_LOADIRDBFILE:
		{
		IRDBBUFFER ird;
		extern int trans_num;
		res = LoadIRDB (&ird,com->remote,0);
		if (res) PutNetworkStatus (res,NULL,stat);
		trans_num--;
		initIRDBPointer ();
		break;
		}
	case COMMAND_LOADIRDB:
		res = LoadIRDB ((IRDBBUFFER *)stat,com->remote,(word)com->adress);
		if (res) PutNetworkStatus (res,NULL,stat);
		break;
	case COMMAND_LOADTRANS:
		res = LoadTranslation ((TRANSLATEBUFFER *)stat,com->remote,(word)com->adress);
		if (res) PutNetworkStatus (res,NULL,stat);
		break;
	case COMMAND_FLASHTRANS:
		res = SetFlashdataEx ((byte)((com->adress) >> 8),(com->adress & 0xff),stat);
		if (res) {
			PutNetworkStatus (res,NULL,stat);
			ns = (NETWORKSTATUS *)stat;
			strcpy (err,ns->message);
			if (res == ERR_REMOTENOTFOUND) sprintf (ns->message,err,err_remote);
			if (res == ERR_COMMANDNOTFOUND) sprintf (ns->message,err,err_command);
		}
		else if (protocol_version < 220) {
			memset (stat,0,sizeof (STATUSBUFFER));
			stat ->statuslen = 8;
		}
		break;
	case COMMAND_WRITE_SYSPARAM:
		sysparm = (SYSPARAMCOMMAND *)com;

		bus = ((com->adress) >> 8) & 0xff;
		
		res = WriteSysparameter (bus,&sysparm->sysparm);
		if (res) PutNetworkStatus (res,NULL,stat);
		break;

	case COMMAND_STOREWLAN:
		wcom = (WLANCONFIGCOMMAND *)com;

		bus = ((com->adress) >> 8) & 0xff;

		res = SetWLANConfig (bus,&wcom->wlan);
		if (res) PutNetworkStatus (res,NULL,stat);
		break;
	case COMMAND_RS232_SEND:
		ser = (SERCOMMAND *)com;

		if (com->adress & 0x40000000) bus = 0xffff;
		else {
			if (protocol_version >= 210) bus = (com->adress >> 19) & (MAX_IR_DEVICES - 1);
			else bus = (com->adress >> 20) & (MAX_IR_DEVICES - 1);
		}
		
		res = SendSerialBlock (bus,ser->data,ser->len,ser->parameter);
		if (res) PutNetworkStatus (res,NULL,stat);
		break;
	case COMMAND_READ_ANALOG:
		ab = (ANALOGBUFFER *)stat;
	
		if (protocol_version >= 210) bus = (com->adress >> 19) & (MAX_IR_DEVICES - 1);
		else bus = (com->adress >> 20) & (MAX_IR_DEVICES - 1);
	
		memset (ab,0,sizeof (ANALOGBUFFER));
		ab->statustype = STATUS_ANALOGINPUT;
		ab->statuslen = sizeof (ANALOGBUFFER);
		res = ReadAnalogInputs (bus,com->trasmit_freq,&(ab->inputs));
		if (res) {
			ns = (NETWORKSTATUS *)stat;
			PutNetworkStatus (res,NULL,stat);
		}
		break;
	case COMMAND_SET_ANALOGLEVEL:
		res = SetAnalogConfig ((ANALOG_CONFIG_COMMAND *)com);
		if (res) PutNetworkStatus (res,NULL,stat);
		ns = (NETWORKSTATUS *)stat;
		strcpy (err,ns->message);
		if (res == ERR_REMOTENOTFOUND) sprintf (ns->message,err,err_remote);
		if (res == ERR_COMMANDNOTFOUND) sprintf (ns->message,err,err_command);
		break;
	case COMMAND_READ_ANALOG_EX:
		abex = (ANALOGBUFFER_EX *)stat;

		if (protocol_version >= 210) bus = (com->adress >> 19) & (MAX_IR_DEVICES - 1);
		else bus = (com->adress >> 20) & (MAX_IR_DEVICES - 1);
	
		memset (abex,0,sizeof (ANALOGBUFFER_EX));
		abex->statustype = STATUS_ANALOGINPUT_EX;
		abex->statuslen = sizeof (ANALOGBUFFER_EX);
		res = ReadAnalogInputsEx (bus,com->timeout,com->trasmit_freq,(byte *)com->remote,&(abex->inputs));
		if (res) {
			ns = (NETWORKSTATUS *)stat;
			PutNetworkStatus (res,NULL,stat);
		}
		break;
	case COMMAND_FUNCTIONS:
		fb = (FUNCTIONBUFFER *)stat;
		memset (fb,0,sizeof (FUNCTIONBUFFER));
		fb->statustype = STATUS_FUNCTION;
		fb->statuslen = sizeof (FUNCTIONBUFFER);
		fb->serno = IRDevices[com->adress].fw_serno;
		fb->functions = IRDevices[com->adress].fw_capabilities;
		break;
	case COMMAND_FUNCTIONEX:
		fbex = (FUNCTIONBUFFEREX *)stat;
		memset (fbex,0,sizeof (FUNCTIONBUFFEREX));
		fbex->statustype = STATUS_FUNCTIONEX;
		fbex->statuslen = sizeof (FUNCTIONBUFFEREX);
		fbex->serno = IRDevices[com->adress].fw_serno;
		fbex->functions = IRDevices[com->adress].fw_capabilities;
		memcpy (fbex->version,IRDevices[com->adress].version,8);
		break;
	case COMMAND_LONGSEND:
		if (com->protocol_version >= 210) bus = (com->adress >> 19) & (MAX_IR_DEVICES - 1);
		else bus = (com->adress >> 20) & (MAX_IR_DEVICES - 1);

		if (com->adress & 0x40000000) {
			bus = 0xffff;
			cal = (IRDevices[0].fw_capabilities & FN_CALIBRATE) != 0;
		}
		else cal = (IRDevices[bus].fw_capabilities & FN_CALIBRATE) != 0;

		end_time = GetMsTime () + com->timeout * 10;

		sprintf (st,"Longsend %s-%s for %d ms\n",com->remote,com->command,com->timeout * 10);
		log_print (st,LOG_DEBUG);
		res = DBFindRemoteCommandEx (com->remote,com->command,&ird,cal,IRDevices[bus].io.toggle_support);
		if (res) {
			if (mode_flag & SEND_FORWARDALL) send_forward (client,com->remote,com->command);
			strcpy (err_remote,com->remote);
			strcpy (err_command,com->command);
			ns = (NETWORKSTATUS *)stat;
			PutNetworkStatus (res,NULL,stat);
			strcpy (err,ns->message);
			if (res == ERR_REMOTENOTFOUND) sprintf (ns->message,err,com->remote);
			if (res == ERR_COMMANDNOTFOUND) sprintf (ns->message,err,com->command);
			log_print (ns->message,LOG_ERROR);
		}
		else {
			if ((mode_flag & SEND_FORWARDALL) || (mode_flag & SEND_FORWARD)) send_forward (client,com->remote,com->command);
			SendIRDataEx (&ird,com->adress);
		}

		resend_flag = 0;
		while (GetMsTime () < end_time) {
			if (!resend_flag) {											// 1. Resend; Command Laden
				strcat (com->command,"@");
				res = DBFindRemoteCommandEx (com->remote,com->command,&ird,cal,IRDevices[bus].io.toggle_support);
				if (res) {
					com->command[strlen (com->command) - 1] = 0;
					res = DBFindRemoteCommand (com->remote,com->command,&cmd_num,NULL);
					if (res) PutNetworkStatus (res,NULL,stat);
					else {
						bus = 0;
						if (com->adress & 0x10000) {
							ird.target_mask = (word)com->adress & 0xffff;
						}
						if (com->protocol_version >= 210) bus = (com->adress >> 19) & (MAX_IR_DEVICES - 1);
						else bus = (com->adress >> 20) & (MAX_IR_DEVICES - 1);

						if (com->adress & 0x40000000) bus = 0xffff;

						ird.address = 0;
						if (com->adress & 0x80000000) {
							ird.address |= ((com->adress >> 25) & 28) + 4;
							ird.address += ((com->adress >> 17) & 3) * 32;
						}
						else if (com->adress & 0x60000) ird.address = (byte)((com->adress >> 17) & 3);

						ResendIREx (bus,&ird);
						resend_flag = 2;
					}
				}
				else {
					SendIRDataEx (&ird,com->adress);
					resend_flag = 1;
					com->command[strlen (com->command) - 1] = 0;
				}
				continue;
			}
			if (resend_flag == 1) strcat (com->command,"@");
			res = DBFindRemoteCommandEx (com->remote,com->command,&ird,cal,IRDevices[bus].io.toggle_support);
			if (res) PutNetworkStatus (res,NULL,stat);
			else {
				if (com->adress & 0x10000) {
					ird.target_mask = (word)com->adress & 0xffff;
				}
				if (com->protocol_version >= 210) bus = (com->adress >> 19) & (MAX_IR_DEVICES - 1);
				else bus = (com->adress >> 20) & (MAX_IR_DEVICES - 1);

				if (com->adress & 0x40000000) bus = 0xffff;

				ird.address = 0;
				if (com->adress & 0x80000000) {
					ird.address |= ((com->adress >> 25) & 28) + 4;
					ird.address += ((com->adress >> 17) & 3) * 32;
				}
				else if (com->adress & 0x60000) ird.address = (byte)((com->adress >> 17) & 3);

				ResendIREx (bus,&ird);

			}
			if (resend_flag == 1) com->command[strlen (com->command) - 1] = 0;
		}
		break;
	case COMMAND_DELETEREM:
		sprintf (st,"Delete %s\n",com->remote,com->command);
		log_print (st,LOG_DEBUG);
		strcpy (st,com->remote);
		if (com->adress != 1234) return;
		if (strcmp (st + strlen (st) - 4,".rem")) strcat (st,".rem");

		if (sockinfo[client].fp) fclose ((sockinfo[client].fp));
		sockinfo[client].fp = NULL;
		memset (sockinfo[client].learnstatus.remote,' ',80);
		memset (sockinfo[client].learnstatus.received,' ',80);

#ifdef WIN32
		res = _unlink (st);
#else
		res = unlink (st);
#endif
		if (res) {
			PutNetworkStatus (ERR_OPENASCII,com->remote,stat);
			return;
		}

		ReadIRDatabase ();
		break;
	case COMMAND_DELETECOM:
		sprintf (st,"Delete %s-%s\n",com->remote,com->command);
		log_print (st,LOG_DEBUG);
		strcpy (st,com->remote);
		if (com->adress != 1234) return;
		if (strcmp (st + strlen (st) - 4,".rem")) strcat (st,".rem");
		fp = DBOpenFile (st,"r+");
		if (!fp) {
			PutNetworkStatus (ERR_OPENASCII,com->remote,stat);
			return;
		}
		if (!ASCIIFindCommand (fp,com->command,NULL)) {
			ns = (NETWORKSTATUS *)stat;
			PutNetworkStatus (ERR_COMMANDNOTFOUND,NULL,stat);
			strcpy (err,ns->message);
			sprintf (ns->message,err,com->command);
			return;
		}
		res = ftell (fp);
		fclose (fp);
#ifdef WIN32
		hfile = CreateFile (st,GENERIC_WRITE,FILE_SHARE_READ | FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
		if (hfile) {
			SetFilePointer (hfile,res,NULL,FILE_BEGIN);
			SetEndOfFile (hfile);
			CloseHandle (hfile);
		}
#endif
#ifdef LINUX
		truncate (st,res);
#endif

		ReadIRDatabase ();
		break;
	case COMMAND_SENDCCFSTR:
	case COMMAND_SENDCCFSTRS:
	case COMMAND_SENDCCF:
	case COMMAND_SENDCCFLONG:
		ccf = (CCFSTRINGCOMMAND *)com;
		ccfs = (CCFSTRINGCOMMAND_SHORT *)com;

		if (com->netcommand == COMMAND_SENDCCFSTR) i = ccf->repeatmode;
		else if (com->netcommand == COMMAND_SENDCCFSTRS) i = ccfs->repeatmode;
		else i = ccf->timeout;

		sprintf (st,"Send CCF %s [%x]\n",ccf->ccf_data,ccf->adress);
		log_print (st,LOG_DEBUG);

		res = DecodeCCF ((char *)ccf->ccf_data,&ird,i);
		if (res <= 0) {
			sprintf (err,"Illegal xAP Pronto command\n");
			log_print (err, LOG_ERROR);
			ns = (NETWORKSTATUS *)stat;
			PutNetworkStatus (ERR_CCF,NULL,stat);
			return;
		}

		ird.target_mask = 0xffff;

		if (ccf->adress & 0x10000) {
			ird.target_mask = (word)ccf->adress & 0xffff;
		}

		ird.address = 0;
		if (ccf->adress & 0x80000000) {
			ird.address |= ((ccf->adress >> 25) & 28) + 4;
			ird.address += ((ccf->adress >> 17) & 3) * 32;
		}
		else if (ccf->adress & 0x60000) ird.address = (byte)((ccf->adress >> 17) & 3);

		if (ccf->protocol_version >= 210) bus = (ccf->adress >> 19) & (MAX_IR_DEVICES - 1);
		else bus = (ccf->adress >> 20) & (MAX_IR_DEVICES - 1);

		if (ccf->adress & 0x40000000) bus = 0xffff;

		res = DoSendIR (&ird,NULL,0,0,bus,COMMAND_SEND);

		if (res) {
			ns = (NETWORKSTATUS *)stat;
			PutNetworkStatus (res,NULL,stat);
			strcpy (err,ns->message);
			if (res == ERR_REMOTENOTFOUND) sprintf (ns->message,err,com->remote);
			if (res == ERR_COMMANDNOTFOUND) sprintf (ns->message,err,com->command);
			if (res == ERR_WRONGBUS) {
				if (com->protocol_version >= 210) sprintf (ns->message,err,(com->adress >> 19) & (MAX_IR_DEVICES - 1));
				else sprintf (ns->message,err,(com->adress >> 20) & (MAX_IR_DEVICES - 1));
			}
			log_print (ns->message,LOG_ERROR);
		}

		resend_flag = 0;
		break;
	case COMMAND_SENDMACRO:
		macro = (MACRO_NETCOMMAND *)com;
		{
			char lst[20];
			int cmd_array[16];
			word pause_array[16];
			byte send_mode = 0;
			word framelen = 0;

			if (com->adress & 0x10000) {
				macro->adress |= 0x10000;
				sprintf (lst,"Mask %x",com->adress & 0xffff);
			}
			else if (((uint32_t)com->adress) & 0x80000000) sprintf (lst,"%d",((com->adress >> 17) & 3) * 8 + ((com->adress >> 27) & 0x7) + 1);
			else {
				res = (com->adress >> 17) & 3;
				if (res == 0) strcpy (lst,"Default");
				else if (res == 1) strcpy (lst,"Internal");
				else if (res == 2) strcpy (lst,"External");
				else if (res == 3) strcpy (lst,"All");
			}

			sprintf (st,"Send Macro [%d] %s - %s [%x - B:%d  LED: %s  Pause: %d]\n",client,macro->remote,macro->command,macro->adress,(macro->adress >> 19) & (MAX_IR_DEVICES - 1),lst,macro->pause);

			log_print (st,LOG_DEBUG);
			if (hexfp) {
				fprintf (hexfp,"%s-%s\n",macro->remote,macro->command);
				hexflag = 1;
			}

			if (com->protocol_version >= 220) send_mode = com->mode;

		
			for (i=0;i < 16;i++) pause_array[i] = macro->pause;

			res = DBFindRemoteMacro (macro->remote,macro->command,cmd_array,pause_array);
			for (i=0;i < 16;i++) if (pause_array[i] > 2500) pause_array[i] = 2500;


			if (!res) res = SendIRMacro (cmd_array,macro->adress,pause_array,&framelen);
			if (res) {
				ns = (NETWORKSTATUS *)stat;
				PutNetworkStatus (res,NULL,stat);
				strcpy (err,ns->message);
				if (res == ERR_REMOTENOTFOUND) sprintf (ns->message,err,macro->remote);
				if (res == ERR_COMMANDNOTFOUND) sprintf (ns->message,err,macro->command);
				if (res == ERR_WRONGBUS) {
					if (macro->protocol_version >= 210) sprintf (ns->message,err,(macro->adress >> 19) & (MAX_IR_DEVICES - 1));
					else sprintf (ns->message,err,(macro->adress >> 20) & (MAX_IR_DEVICES - 1));
				}
				log_print (ns->message,LOG_ERROR);
			}
			else if (send_mode == SEND_MODE_FRAMELEN) {
				ns = (NETWORKSTATUS *)stat;
				ns->framelen = framelen;
				ns->statustype = STATUS_FRAMELEN;
				ns->statuslen = 16;
			}

			resend_flag = 0;
			break;
		}
	case COMMAND_GET_IRCODE:
		res = DBFindRemoteCommandEx (com->remote,com->command,&ird,1,1);
		if (res) {
			ns = (NETWORKSTATUS *)stat;
			PutNetworkStatus (res,NULL,stat);
			strcpy (err,ns->message);
			if (res == ERR_REMOTENOTFOUND) sprintf (ns->message,err,com->remote);
			if (res == ERR_COMMANDNOTFOUND) sprintf (ns->message,err,com->command);
			log_print (ns->message,LOG_ERROR);
		}
		else {
			IRDATA_BUFFER *irdb;
			
			ird.target_mask = 0;

			if (!byteorder && !(ird.mode & RAW_DATA)) {
				j = 16;
				if (ird.mode == TIMECOUNT_18) j = 36;
				ird.checksumme = 1;
					
				for (i=0;i < j;i++) force_swap_word ((word *)ird.pause_len + i);
			}

			irdb = (IRDATA_BUFFER *)stat;

			memset (irdb,0,sizeof (irdb));
			irdb->statustype = STATUS_LEARNDIRECT;
			irdb->statuslen = sizeof (IRDATA_BUFFER);
			memcpy (&irdb->ird,&ird,sizeof (IRDATA));
			irdb->irlen = ird.len;
		}
		break;
	case COMMAND_SEND:
	case COMMAND_SENDMASK:
		{
			byte send_mode = 0;
			word framelen = 0;

			if (com->protocol_version >= 220) send_mode = com->mode;

			if (com->protocol_version >= 210) {
				char lst[20];
				if (com->netcommand == COMMAND_SENDMASK) sprintf (lst,"Mask %x",com->adress & 0xffff);
				else if (((uint32_t)com->adress) & 0x80000000) sprintf (lst,"%d",((com->adress >> 17) & 3) * 8 + ((com->adress >> 27) & 0x7) + 1);
				else {
					res = (com->adress >> 17) & 3;
					if (res == 0) strcpy (lst,"Default");
					else if (res == 1) strcpy (lst,"Internal");
					else if (res == 2) strcpy (lst,"External");
					else if (res == 3) strcpy (lst,"All");
				}

				sprintf (st,"Send [%d] %s - %s [%x - B:%d  M:0x%x  LED: %s]\n",client,com->remote,com->command,com->adress,(com->adress >> 19) & (MAX_IR_DEVICES - 1),com->adress & 0xffff,lst);
			}
			else sprintf (st,"Send [%d] %s - %s [%x - B:%d  M:0x%x  L:%d  Ext L: %d]\n",client,com->remote,com->command,com->adress,(com->adress >> 20) & (MAX_IR_DEVICES - 1),com->adress & 0xffff,(com->adress >> 17) & 0x3,(com->adress >> 27) & 0x7);
			log_print (st,LOG_DEBUG);
			if (hexfp) {
				fprintf (hexfp,"%s-%s\n",com->remote,com->command);
				hexflag = 1;
			}
			
			if (com->netcommand == COMMAND_SENDMASK) com->adress |= 0x10000;
			
			if (!strncmp (com->remote,"***relais_",10)) {
				int bus;
				if (protocol_version >= 210) bus = (com->adress >> 19) & (MAX_IR_DEVICES - 1);
				else bus = (com->adress >> 20) & (MAX_IR_DEVICES - 1);

				res = SetRelaisEx (bus,(byte)com->command[0],(byte)(com->remote[10] - '1'));
				break;
			}
			res = DBFindRemoteCommand (com->remote,com->command,&cmd_num,NULL);
			if (!res) {
				if ((mode_flag & SEND_FORWARDALL) || (mode_flag & SEND_FORWARD)) send_forward (client,com->remote,com->command);
				res = SendIR (cmd_num,com->adress,com->netcommand,&framelen);
			}
			if (res) {
				if (mode_flag & SEND_FORWARDALL) send_forward (client,com->remote,com->command);
				ns = (NETWORKSTATUS *)stat;
				PutNetworkStatus (res,NULL,stat);
				strcpy (err,ns->message);
				if (res == ERR_REMOTENOTFOUND) sprintf (ns->message,err,com->remote);
				if (res == ERR_COMMANDNOTFOUND) sprintf (ns->message,err,com->command);
				if (res == ERR_WRONGBUS) {
					if (com->protocol_version >= 210) sprintf (ns->message,err,(com->adress >> 19) & (MAX_IR_DEVICES - 1));
					else sprintf (ns->message,err,(com->adress >> 20) & (MAX_IR_DEVICES - 1));
				}
				log_print (ns->message,LOG_ERROR);
			}
			else if (send_mode == SEND_MODE_FRAMELEN) {
				ns = (NETWORKSTATUS *)stat;
				ns->framelen = framelen;
				ns->statustype = STATUS_FRAMELEN;
				ns->statuslen = 16;
			}

			resend_flag = 0;
			break;
		}
	case COMMAND_DEVICEDATA:
		sprintf (st,"Get Devicedata %s-%s\n",com->remote,com->command);
		log_print (st,LOG_DEBUG);
		res = DBFindRemoteCommand (com->remote,com->command,&cmd_num,NULL);
		if (!res) res = GetDeviceData (cmd_num,(DATABUFFER *)stat);
		if (res) {
			ns = (NETWORKSTATUS *)stat;
			PutNetworkStatus (res,NULL,stat);
			strcpy (err,ns->message);
			if (res == ERR_REMOTENOTFOUND) sprintf (ns->message,err,com->remote);
			if (res == ERR_COMMANDNOTFOUND) sprintf (ns->message,err,com->command);
			log_print (ns->message,LOG_ERROR);
		}
		break;
	case COMMAND_TESTCOM:
		res = DBFindRemoteCommand (com->remote,com->command,&cmd_num,NULL);
		if (res) {
			ns = (NETWORKSTATUS *)stat;
			PutNetworkStatus (ERR_TESTCOM,NULL,stat);
			ns->statuslevel = (word)com->adress;
			ns->statuslen -= 256;
		}
		break;
	case COMMAND_TESTCOMEX:
		res = DBFindRemoteCommand (com->remote,com->command,&cmd_num,NULL);
		ns = (NETWORKSTATUS *)stat;
		if (res) {
			PutNetworkStatus (ERR_TESTCOM,NULL,stat);
		}
		else {
			PutNetworkStatus (ERR_TESTCOMOK,NULL,stat);
		}
		ns->statuslevel = (word)com->adress;
		ns->statuslen -= 256;
		break;

	case COMMAND_TEMP:
		sprintf (st,"Temperature %s-%d\n",com->remote,com->adress);
		log_print (st,LOG_DEBUG);
		memset (&ird,0,sizeof (IRDATA));
		ird.ir_length = 5;
		ird.target_mask = 1 << com->adress;
		if (!strcmp (com->remote,"get") || !strcmp (com->remote,"Get") || !strcmp (com->remote,"GET")) {
			ird.mode = TEMP_DATA | TEMP_GET;
		}
		if (!strcmp (com->remote,"reset") || !strcmp (com->remote,"Reset") || !strcmp (com->remote,"RESET")) {
			ird.mode = TEMP_DATA | TEMP_RESET;
		}
		if (ird.mode == 0) {
			ns = (NETWORKSTATUS *)stat;
			PutNetworkStatus (ERR_TEMPCOMMAND,NULL,stat);
			strcpy (err,ns->message);
			sprintf (ns->message,err,com->remote);
			log_print (ns->message,LOG_ERROR);
		}
		else SendIRDataEx (&ird,com->adress);
		break;
	case COMMAND_STARTCLOCK:
		if (!(mode_flag & NO_CLOCK)) LCDTimeCommand (LCD_DISPLAYTIME);
		lcd_init = 1;
		break;
	case COMMAND_LCD:
		lcd = (LCDCOMMAND *)com;
		irw = (IRRAW *)&ird;
		memset (irw,0,sizeof (IRRAW));
		if (lcd->lcdcommand & LCD_TEXT) {
			memset (err,0,sizeof (err));
			memcpy (err,lcd->framebuffer,40);
			memcpy (err+100,lcd->framebuffer+40,40);
			sprintf (st,"LCD: %s\n%s\n",err,err+100);
			log_print (st,LOG_DEBUG);
			compress_lcdbuffer (lcd,irw->data,display_bus);

//			sprintf (st,"COMPRESS: %s\n",irw->data);
//			log_print (st,LOG_DEBUG);
		}

		if (suspend) return;
		irw->target_mask = (unsigned short)(lcd->adress & 0xffff);
		if (lcd->adress == 'L') irw->target_mask = 0xffff;
		irw->ir_length = 0;
		irw->mode = LCD_DATA | lcd->lcdcommand;
		irw->transmit_freq = lcd->timeout;
		SendLCD (irw,lcd->adress);
		resend_flag = 0;
		if (new_lcd_flag) msSleep (100);
		//printf ("<%s>\n",irw->data);
		//lcd_init = 1; // Debug für LCD Test
		break;
	case COMMAND_SETSWITCH:
		memset (&ird,0,sizeof (IRDATA));
		sprintf (st,"SWITCH: %d - %d\n",com->remote[0],com->command[0]);
		log_print (st,LOG_DEBUG);
		
		ird.target_mask = 0xffff;
		ird.ir_length = 0;
		ird.mode = SWITCH_DATA | com->command[0];
		ird.transmit_freq = com->remote[0];
		DoSendIR (&ird,NULL,0,0,0,COMMAND_SEND);						// !!!!!! Auf Multibus erweitern
		resend_flag = 0;
		break;
	case COMMAND_LCDINIT:
		lcd = (LCDCOMMAND *)com;
		irw = (IRRAW *)&ird;
		memset (irw,0,sizeof (IRRAW));
		memcpy (irw->data,lcd->framebuffer,40);
		sprintf (st,"LCDINIT: %s\n",lcd->framebuffer);
		log_print (st,LOG_DEBUG);

		irw->target_mask = (unsigned short)lcd->adress;
		if (lcd->adress == 'L') irw->target_mask = 0xffff;
		irw->ir_length = 0;
		irw->mode = LCD_DATA | LCD_INIT;
		irw->transmit_freq = lcd->timeout;
		SendLCD (irw,lcd->adress);
		resend_flag = 0;
		break;
	case COMMAND_RESEND:
		if (com->protocol_version >= 210) {
			char lst[20];
			if (((uint32_t)com->adress) & 0x80000000) sprintf (lst,"%d",((com->adress >> 17) & 3) * 8 + ((com->adress >> 27) & 0x7) + 1);
			else {
				res = (com->adress >> 17) & 3;
				if (res == 0) strcpy (lst,"Default");
				else if (res == 1) strcpy (lst,"Internal");
				else if (res == 2) strcpy (lst,"External");
				else if (res == 3) strcpy (lst,"All");
			}

			sprintf (st,"Send [%d] %s - %s [%x - B:%d  M:0x%x  LED: %s]\n",client,com->remote,com->command,com->adress,(com->adress >> 19) & (MAX_IR_DEVICES - 1),com->adress & 0xffff,lst);
		}
		else sprintf (st,"Resend [%d] %s - %s [%x - B:%d  M:0x%x  L:%d  Ext L: %d]\n",client,com->remote,com->command,com->adress,(com->adress >> 20) & (MAX_IR_DEVICES - 1),com->adress & 0xffff,(com->adress >> 17) & 0x3,(com->adress >> 27) & 0x7);
		log_print (st,LOG_DEBUG);
		if (com->protocol_version >= 210) bus = (com->adress >> 19) & (MAX_IR_DEVICES - 1);
		else bus = (com->adress >> 20) & (MAX_IR_DEVICES - 1);

		if (com->adress & 0x40000000) {
			bus = 0xffff;
			i = 0;
			cal = (IRDevices[0].fw_capabilities & FN_CALIBRATE) != 0;
		}
		else {
			cal = (IRDevices[bus].fw_capabilities & FN_CALIBRATE) != 0;
			i = bus;
		}

		if (!resend_flag) {											// 1. Resend; Command Laden
			strcat (com->command,"@");
			res = DBFindRemoteCommandEx (com->remote,com->command,&ird,cal,IRDevices[i].io.toggle_support);
			if (res) {
				com->command[strlen (com->command) - 1] = 0;
				res = DBFindRemoteCommandEx (com->remote,com->command,&ird,cal,IRDevices[i].io.toggle_support);
				if (!res) resend_flag = 2;
				if (res == ERR_ISMACRO) {
					res = DBFindRemoteCommand (com->remote,com->command,&cmd_num,NULL);
					if (!res) res = SendIR (cmd_num,com->adress,COMMAND_SEND,NULL);
					resend_flag = 0;
				}

				if (res) {
					if (mode_flag & SEND_FORWARDALL) send_forward (client,com->remote,com->command);
					PutNetworkStatus (res,NULL,stat);
				}
				else {
					if ((mode_flag & SEND_FORWARDALL) || (mode_flag & SEND_FORWARD)) send_forward (client,com->remote,com->command);
					if (com->adress & 0x10000) {
						ird.target_mask = (word)com->adress & 0xffff;
					}

					ird.address = 0;
					if (com->adress & 0x80000000) {
						ird.address |= ((com->adress >> 25) & 28) + 4;
						ird.address += ((com->adress >> 17) & 3) * 32;
					}
					else if (com->adress & 0x60000) ird.address = (byte)((com->adress >> 17) & 3);

					if (com->protocol_version >= 210) bus = (com->adress >> 19) & (MAX_IR_DEVICES - 1);
					else bus = (com->adress >> 20) & (MAX_IR_DEVICES - 1);
					if (com->adress & 0x40000000) bus = 0xffff;
					res = ResendIREx (bus,&ird);
				}
			}
			else {
				if ((mode_flag & SEND_FORWARDALL) || (mode_flag & SEND_FORWARD)) send_forward (client,com->remote,com->command);
				res = SendIRDataEx (&ird,com->adress);
				resend_flag = 1;
			}
			if (res) PutNetworkStatus (res,NULL,stat);
			break;
		}
		if (resend_flag == 1) strcat (com->command,"@");
		res = DBFindRemoteCommandEx (com->remote,com->command,&ird,cal,IRDevices[i].io.toggle_support);
		if (res) {
			if (mode_flag & SEND_FORWARDALL) send_forward (client,com->remote,com->command);
			PutNetworkStatus (res,NULL,stat);
		}
		else {
			if ((mode_flag & SEND_FORWARDALL) || (mode_flag & SEND_FORWARD)) send_forward (client,com->remote,com->command);
			if (com->adress & 0x10000) {
				ird.target_mask = (word)com->adress & 0xffff;
			}

			ird.address = 0;
			if (com->adress & 0x80000000) {
				ird.address |= ((com->adress >> 25) & 28) + 4;
				ird.address += ((com->adress >> 17) & 3) * 32;
			}
			else if (com->adress & 0x60000) ird.address = (byte)((com->adress >> 17) & 3);
			
			if (com->protocol_version >= 210) bus = (com->adress >> 19) & (MAX_IR_DEVICES - 1);
			else bus = (com->adress >> 20) & (MAX_IR_DEVICES - 1);
			if (com->adress & 0x40000000) bus = 0xffff;
			res = ResendIREx (bus,&ird);
		}
		if (res) PutNetworkStatus (res,NULL,stat);

		break;
	case COMMAND_LRNREM:
		if (sockinfo[client].fp != NULL) fclose (sockinfo[client].fp);
		sockinfo[client].fp = ASCIIOpenRemote (com->remote,&sockinfo[client]);
		if (sockinfo[client].learnstatus.num_timings > 0) sockinfo[client].learnstatus.learnok |= 1;
		if (sockinfo[client].fp == NULL) PutNetworkStatus (ERR_OPENASCII,com->remote,stat);
		break;
	case COMMAND_LRNTIM:
		if (sockinfo[client].fp == NULL) {
			PutNetworkStatus (ERR_NOFILEOPEN,NULL,stat);
			break;
		}
		res = LearnIREx (&sockinfo[client].ird,(word)com->adress,com->timeout,(word)(com->adress >> 16),com -> trasmit_freq,com->remote[0]);

		if (res && res < ERR_LEARN_LENGTH) PutNetworkStatus (res,NULL,stat);
		else {
			sockinfo[client].timing = ASCIIStoreTiming (sockinfo[client].fp,&sockinfo[client].ird,&sockinfo[client].learnstatus);
			last_adress = sockinfo[client].ird.address;
			ResultStoreTiming (&sockinfo[client].ird,(NETWORKTIMING *)stat);
			sockinfo[client].learnstatus.learnok = 1;
			if (sockinfo[client].ird.command == 0xff) {
				sockinfo[client].learnstatus.learnok |= 2;
				sockinfo[client].learnstatus.carrier = sockinfo[client].ird.transmit_freq;
			}
			else if (sockinfo[client].ird.command == 0xfe) sockinfo[client].learnstatus.learnok |= 4;
		}
		break;
	case COMMAND_LRNRAW:
		if (sockinfo[client].fp == NULL) {
			PutNetworkStatus (ERR_NOFILEOPEN,NULL,stat);
			break;
		}
		res = LearnRawIREx ((IRRAW *)&sockinfo[client].ird,(word)com->adress,com->timeout,(word)(com->adress >> 16),com -> trasmit_freq);

		if (res) PutNetworkStatus (res,NULL,stat);

		if (!res || res >= ERR_LEARN_LENGTH) {
			if (ASCIIFindCommand (sockinfo[client].fp,com->command,sockinfo + client) == 0) sockinfo[client].learnstatus.num_commands++;
			ASCIIStoreRAW (sockinfo[client].fp,(IRRAW *)&sockinfo[client].ird,com->command);
			last_adress = sockinfo[client].ird.address;
			if (sockinfo[client].ird.command == 0xff) {
				sockinfo[client].learnstatus.learnok |= 2;
				sockinfo[client].learnstatus.carrier = sockinfo[client].ird.transmit_freq;
			}
			else if (sockinfo[client].ird.command == 0xfe) sockinfo[client].learnstatus.learnok |= 4;
		}
		break;
	case COMMAND_LRNRAWRPT:
		if (sockinfo[client].fp == NULL) {
			PutNetworkStatus (ERR_NOFILEOPEN,NULL,stat);
			break;
		}
		res = LearnRawIRRepeatEx ((IRRAW *)&sockinfo[client].ird,(word)com->adress,com->timeout,(word)(com->adress >> 16),com -> trasmit_freq);

		if (res) PutNetworkStatus (res,NULL,stat);

		if (!res || res >= ERR_LEARN_LENGTH) {
			strcat (com->command,"@");
			if (ASCIIFindCommand (sockinfo[client].fp,com->command,sockinfo + client) == 0) sockinfo[client].learnstatus.num_commands++;
			ASCIIStoreRAW (sockinfo[client].fp,(IRRAW *)&sockinfo[client].ird,com->command);
			last_adress = sockinfo[client].ird.address;
			if (sockinfo[client].ird.command == 0xff) {
				sockinfo[client].learnstatus.learnok |= 2;
				sockinfo[client].learnstatus.carrier = sockinfo[client].ird.transmit_freq;
			}
			else if (sockinfo[client].ird.command == 0xfe) sockinfo[client].learnstatus.learnok |= 4;
		}
		break;

	case COMMAND_SET_IRCODE:
		ccfl = (CCFLEARNCOMMAND *)com;
		if (sockinfo[client].fp == NULL) {
			PutNetworkStatus (ERR_NOFILEOPEN,NULL,stat);
			break;
		}

		j = 0;
		dat[2] = 0;
		i = 0;
		if (ccfl->ccf_data[i] == 'h' || ccfl->ccf_data[i] == 'H') i++;
		for (;i < 5400 && ccfl->ccf_data[i];i+=2) {
			memcpy (dat,ccfl->ccf_data + i,2);
			((byte *)(&sockinfo[client].ird))[j++] = (byte)strtol (dat,0,16);
		}

		if (j != sockinfo[client].ird.len) {
			res = ERR_IRCODE_LENGTH;
			goto hex_import_error;
		}
		// Swap
		if (!(sockinfo[client].ird.mode & RAW_DATA)) {
			byte n,flg;
			n = 16;
			flg = 1;
			if (sockinfo[client].ird.mode == TIMECOUNT_18) n = 36;

			for (i=0;i < n && flg == 1;i++) {
				if (((word *)sockinfo[client].ird.pause_len)[i] & 0x8000) flg = 2;
				if (((word *)sockinfo[client].ird.pause_len)[i] && ((word *)sockinfo[client].ird.pause_len)[i] < 256) flg = 0;
			}

			if (flg || sockinfo[client].ird.checksumme == 1) for (i=0;i < n;i++) force_swap_word ((word *)sockinfo[client].ird.pause_len + i);
		}
			
		// Timecount
		if (sockinfo[client].ird.mode == TIMECOUNT_18) {
			IRDATA_18 *irp = (IRDATA_18 *)&sockinfo[client].ird;
			if (irp->time_cnt == 0 || (irp->time_cnt & TC_ACTIVE)) {
				for (i=0;i < 18;i++) if (irp->pulse_len[i] == 0 || irp->pause_len[i] == 0) break;
				irp->time_cnt = i;
			}
		}
		else {
			IRDATA *irp = (IRDATA *)&sockinfo[client].ird;
			if (irp->time_cnt == 0 || (irp->time_cnt & TC_ACTIVE)) {
				for (i=0;i < 8;i++) if (irp->pulse_len[i] == 0 || irp->pause_len[i] == 0) break;
				irp->time_cnt = i;
			}
		}

		// Prüfung auf korrekten Code
		res = CheckIRCode (&sockinfo[client].ird);

hex_import_error:
		if (res) PutNetworkStatus (res,NULL,stat);
		else {
			sockinfo[client].timing = ASCIIStoreTiming (sockinfo[client].fp,&sockinfo[client].ird,&sockinfo[client].learnstatus);
			if (ASCIIFindCommand (sockinfo[client].fp,com->command,sockinfo + client) == 0) sockinfo[client].learnstatus.num_commands++;
			ASCIIStoreCommand (sockinfo[client].fp,&sockinfo[client].ird,com->command,sockinfo[client].timing,0);

			SetLearnstatus (client);
		}
		break;
	case COMMAND_LEARNLINK:
		ccfl = (CCFLEARNCOMMAND *)com;
		if (sockinfo[client].fp == NULL) {
			PutNetworkStatus (ERR_NOFILEOPEN,NULL,stat);
			break;
		}

		res = ASCIIStoreLink (client,ccfl->command,ccfl->ccf_data);
		if (res) PutNetworkStatus (res,NULL,stat);
		break;
	case COMMAND_LEARNRS232:
		ccfl = (CCFLEARNCOMMAND *)com;
		if (sockinfo[client].fp == NULL) {
			PutNetworkStatus (ERR_NOFILEOPEN,NULL,stat);
			break;
		}

		res = ASCIIStoreRS232 (client,ccfl->command,ccfl->ccf_data);
		if (res) PutNetworkStatus (res,NULL,stat);
		break;
	case COMMAND_LEARNCCF:
		ccfl = (CCFLEARNCOMMAND *)com;
		if (sockinfo[client].fp == NULL) {
			PutNetworkStatus (ERR_NOFILEOPEN,NULL,stat);
			break;
		}

		res = ASCIIStoreCCF (client,ccfl->command,ccfl->ccf_data);
		if (res) PutNetworkStatus (res,NULL,stat);
		break;
	case COMMAND_LRNCOM:
		if (sockinfo[client].fp == NULL) {
			PutNetworkStatus (ERR_NOFILEOPEN,NULL,stat);
			break;
		}
		if (sockinfo[client].ird.ir_length == 0) {
			PutNetworkStatus (ERR_NOTIMING,NULL,stat);
			break;
		}
		res = LearnNextIREx (&sockinfo[client].ird,(word)com->adress,com->timeout,(word)(com->adress >> 16),com -> trasmit_freq,com->remote[0]);
		if (res) PutNetworkStatus (res,NULL,stat);
		else {
			if (ASCIIFindCommand (sockinfo[client].fp,com->command,sockinfo + client) == 0) sockinfo[client].learnstatus.num_commands++;
			res = ASCIIStoreCommand (sockinfo[client].fp,&sockinfo[client].ird,com->command,sockinfo[client].timing,0);
			SetLearnstatus (client);
		}
		break;

	case COMMAND_LEARNDIRECT:
		res = LearnIREx (&sockinfo[client].ird,(word)com->adress,com->timeout,(word)(com->adress >> 16),com -> trasmit_freq,0);

		if (res && res < ERR_LEARN_LENGTH) {
			ns = (NETWORKSTATUS *)stat;
			PutNetworkStatus (res,NULL,stat);
			strcpy (err,ns->message);
			if (res == ERR_WRONGBUS) {
				if (com->protocol_version >= 210) sprintf (ns->message,err,(com->adress >> 19) & (MAX_IR_DEVICES - 1));
				else sprintf (ns->message,err,(com->adress >> 20) & (MAX_IR_DEVICES - 1));
			}
			log_print (ns->message,LOG_ERROR);
		}
		else {
			IRDATA_BUFFER *irdb;
			
			irdb = (IRDATA_BUFFER *)stat;

			memset (irdb,0,sizeof (irdb));
			irdb->statustype = STATUS_LEARNDIRECT;
			irdb->statuslen = sizeof (IRDATA_BUFFER);
			memcpy (&irdb->ird,&sockinfo[client].ird,sizeof (IRDATA));
			irdb->ird.target_mask = 0xffff;
			irdb->ird.command = HOST_SEND;
			irdb->ird.checksumme = get_checksumme (&irdb->ird);
		}
		break;
	case COMMAND_LRNLONG:
		if (sockinfo[client].fp == NULL) {
			PutNetworkStatus (ERR_NOFILEOPEN,NULL,stat);
			break;
		}
		res = LearnIREx (&sockinfo[client].ird,(word)com->adress,com->timeout,(word)(com->adress >> 16),com -> trasmit_freq,com->remote[0]);

		if (res) PutNetworkStatus (res,NULL,stat);
		
		if (!res || res >= ERR_LEARN_LENGTH) {
			sockinfo[client].timing = ASCIIStoreTiming (sockinfo[client].fp,&sockinfo[client].ird,&sockinfo[client].learnstatus);
			if (ASCIIFindCommand (sockinfo[client].fp,com->command,sockinfo + client) == 0) sockinfo[client].learnstatus.num_commands++;
			ASCIIStoreCommand (sockinfo[client].fp,&sockinfo[client].ird,com->command,sockinfo[client].timing,0);
			last_adress = sockinfo[client].ird.address;
			if (sockinfo[client].ird.command == 0xff) {
				sockinfo[client].learnstatus.learnok |= 2;
				sockinfo[client].learnstatus.carrier = sockinfo[client].ird.transmit_freq;
			}
			else if (sockinfo[client].ird.command == 0xfe) sockinfo[client].learnstatus.learnok |= 4;
			SetLearnstatus (client);

		}
		break;

	case COMMAND_LRNTOG:
		if (sockinfo[client].fp == NULL) {
			PutNetworkStatus (ERR_NOFILEOPEN,NULL,stat);
			break;
		}
		res = LearnNextIREx (&sockinfo[client].ird,(word)com->adress,com->timeout,(word)(com->adress >> 16),com -> trasmit_freq,com->remote[0]);
		if (res) PutNetworkStatus (res,NULL,stat);
		else {
			res = ASCIIFindToggleSeq (sockinfo[client].fp,&sockinfo[client].ird,com->command);
			if (res < 0) PutNetworkStatus (ERR_TOGGLE_DUP,NULL,stat);
			else {
				res = ASCIIStoreCommand (sockinfo[client].fp,&sockinfo[client].ird,com->command,sockinfo[client].timing,res);
				sockinfo[client].learnstatus.num_commands++;
				if (sockinfo[client].ird.command == 0xff) {
					sockinfo[client].learnstatus.learnok |= 2;
					sockinfo[client].learnstatus.carrier = sockinfo[client].ird.transmit_freq;
					}
				else if (sockinfo[client].ird.command == 0xfe) sockinfo[client].learnstatus.learnok |= 4;
				SetLearnstatus (client);			
			}
		}
		break;

	case COMMAND_LRNRPT:
		if (sockinfo[client].fp == NULL) {
			PutNetworkStatus (ERR_NOFILEOPEN,NULL,stat);
			break;
		}
		res = LearnRepeatIREx (&sockinfo[client].ird,(word)com->adress,com->timeout,(word)(com->adress >> 16),com -> trasmit_freq,com->remote[0]);

		if (res) PutNetworkStatus (res,NULL,stat);
		if (!res || res >= ERR_LEARN_LENGTH) {
			sockinfo[client].timing = ASCIIStoreTiming (sockinfo[client].fp,&sockinfo[client].ird,&sockinfo[client].learnstatus);
			strcat (com->command,"@");
			if (ASCIIFindCommand (sockinfo[client].fp,com->command,sockinfo + client) == 0) sockinfo[client].learnstatus.num_commands++;
			ASCIIStoreCommand (sockinfo[client].fp,&sockinfo[client].ird,com->command,sockinfo[client].timing,0);
			last_adress = sockinfo[client].ird.address;
			if (sockinfo[client].ird.command == 0xff) {
				sockinfo[client].learnstatus.learnok |= 2;
				sockinfo[client].learnstatus.carrier = sockinfo[client].ird.transmit_freq;
			}
			else if (sockinfo[client].ird.command == 0xfe) sockinfo[client].learnstatus.learnok |= 4;
			SetLearnstatus (client);
		}
		break;

	case COMMAND_CLOSE:
		if (sockinfo[client].fp == NULL) {
			PutNetworkStatus (ERR_NOFILEOPEN,NULL,stat);
			break;
		}
		if (sockinfo[client].fp) fclose ((sockinfo[client].fp));
		sockinfo[client].fp = NULL;
		memset (sockinfo[client].learnstatus.remote,' ',80);
//		memset (sockinfo[client].learnstatus.received,' ',80);			// Damit Learnstat nach Close der Remote möglich ist
		ReadIRDatabase ();
		break;

	case COMMAND_RELOAD:
		if (sockinfo[client].fp) fflush ((sockinfo[client].fp));
		ReadIRDatabase ();
		break;
	case COMMAND_LOGLEVEL:
		mode_flag = (mode_flag & ~LOG_MASK) | (com->adress & LOG_MASK);
		break;
	case COMMAND_LANPARAM:
		bus = (byte)com->adress;

		sprintf (st,"Read LAN Parameter Bus: %d\n",bus);
		log_print (st,LOG_DEBUG);
		res = GetLANParamEx ((LANSTATUSBUFFER *)stat,IRDevices + bus);
		if (res) {
			ns = (NETWORKSTATUS *)stat;
			PutNetworkStatus (res,NULL,stat);
			log_print (ns->message,LOG_ERROR);
		}
		break;
	case COMMAND_WLANPARAM:
		bus = (byte)com->adress;

		sprintf (st,"Read WLAN Parameter Bus: %d\n",bus);
		log_print (st,LOG_DEBUG);
		res = GetWiFiParamEx ((WLANSTATUSBUFFER *)stat,IRDevices + bus);
		if (res) {
			ns = (NETWORKSTATUS *)stat;
			PutNetworkStatus (res,NULL,stat);
			log_print (ns->message,LOG_ERROR);
		}
		break;
	case COMMAND_READ_SYSPARAM:
		bus = (byte)com->adress;

		sprintf (st,"Read Sys Parameter Bus: %d\n",bus);
		log_print (st,LOG_DEBUG);
		res = GetSysParameterEx ((SYSPARAMBUFFER *)stat,IRDevices + bus);
		if (res) {
			ns = (NETWORKSTATUS *)stat;
			PutNetworkStatus (res,NULL,stat);
			log_print (ns->message,LOG_ERROR);
		}
		break;
	case COMMAND_SSIDLIST:
		bus = (byte)com->adress;

		sprintf (st,"Read SSID Info Bus: %d\n",bus);
		log_print (st,LOG_DEBUG);
		res = GetWiFiStatusEx ((WLANBUFFER *)stat,IRDevices + bus);
		if (res) {
			ns = (NETWORKSTATUS *)stat;
			PutNetworkStatus (res,NULL,stat);
			log_print (ns->message,LOG_ERROR);
		}
		break;
	case COMMAND_STATUS:
		if (status_changed || (time (0) - last_status_read) >= status_cache_timeout) {
			res = GetBusInfoEx (remote_statusex,0);
			if (res) {
				PutNetworkStatus (res,NULL,stat);
				break;
			}
			else {
				status_changed = 0;
				last_status_read = time (0);
			}
		}
		PutDeviceStatus ((NETWORKMODE *)stat);
		break;

	case COMMAND_STATUSEX:
	case COMMAND_STATUSEXN:
	case COMMAND_STATUSEX2:
	case COMMAND_STATUSEX3:
	case COMMAND_STATUSEX3_SHORT:
		if (com->netcommand == COMMAND_STATUSEX3_SHORT && statusmode_short) {
			if (status_changed) GetBusInfoExShort (remote_statusex,0xffff);
		}
		else {
			if (status_changed || (time (0) - last_status_read) >= status_cache_timeout) {
				res = GetBusInfoEx (remote_statusex,0xffff);
				if (res) {
					PutNetworkStatus (res,NULL,stat);
					break;
				}
				else {
					status_changed = 0;
					last_status_read = time (0);
				}
			}
		}

		PutDeviceStatusEx (stat,com->netcommand,(byte)com->adress,(com->protocol_version >= 230));
		break;

	case COMMAND_LEARNSTAT:
		lstat = (NETWORKLEARNSTAT *)stat;
		memcpy (stat,&sockinfo[client].learnstatus,sizeof (NETWORKLEARNSTAT));
		j = i = 0;
		while (sockinfo[client].learnstatus.received[i]) {
			if (sockinfo[client].learnstatus.received[i] >= '0' && sockinfo[client].learnstatus.received[i] <= 'z') lstat->received[j++] = sockinfo[client].learnstatus.received[i];
			i++;
		}
		lstat->received[j] = 0;
		if (lstat->received[0] == 0) memset (lstat->received,' ',CODE_LEN);
		if (lstat->remote[0] == 0) memset (lstat->remote,' ',80);
		stat->statustype = STATUS_LEARN;
		stat->statuslen = sizeof (NETWORKLEARNSTAT);

		memset (sockinfo[client].learnstatus.received,' ',CODE_LEN);
		break;

	case COMMAND_RESET:
		lcd_init = 1;
		res = ResetTransceiverEx ((byte)((com->adress >> 8) & (MAX_IR_DEVICES-1)));
		if (res) PutNetworkStatus (res,NULL,stat);
		break;
	case COMMAND_SETSTATEX:
	case COMMAND_SETSTATEX2:
	case COMMAND_SETSTATEX3:
		mcom = (MODUSCOMMAND_EX3 *)com;
		res = GetHotcode (mcom->hotremote,mcom->hotcommand,st);
		if (res == -1) {
			GetError (ERR_HOTCODE,st);
			sprintf (err,st,mcom->hotremote,mcom->hotcommand);
			log_print (err,LOG_ERROR);
			PutNetworkStatus (ERR_HOTCODE,err,stat);
			break;
		}
		if (strcmp (mcom->hotremote_2,"_")) res = GetHotcode (mcom->hotremote_2,mcom->hotcommand_2,st);
		if (res == -1) {
			GetError (ERR_HOTCODE,st);
			sprintf (err,st,mcom->hotremote_2,mcom->hotcommand_2);
			log_print (err,LOG_ERROR);
			PutNetworkStatus (ERR_HOTCODE,err,stat);
			break;
		}
		res = GetHotcode (mcom->hotremote,mcom->hotcommand,st);
		
		if (com->netcommand == COMMAND_SETSTATEX3) res = SetTransceiverModusEx ((byte)(mcom->adress >> 8),mcom->mode,(word)mcom->targetmask,(byte)(mcom->adress & 0xff),st,res,mcom->extmode,mcom->extmode_2,mcom->extmode_ex,mcom->wakeup_mac,mcom->rs232_mode,&mcom->status_info); 
		else if (com->netcommand == COMMAND_SETSTATEX2) res = SetTransceiverModusEx ((byte)(mcom->adress >> 8),mcom->mode,(word)mcom->targetmask,(byte)(mcom->adress & 0xff),st,res,mcom->extmode,mcom->extmode_2,mcom->extmode_ex,mcom->wakeup_mac,NULL,NULL); 
		else res = SetTransceiverModusEx ((byte)(mcom->adress >> 8),mcom->mode,(word)mcom->targetmask,(byte)(mcom->adress & 0xff),st,res,mcom->extmode,mcom->extmode_2,0,mcom->wakeup_mac,NULL,NULL);

		StoreSwitch ((word)mcom->adress,0,mcom->hotremote,mcom->hotcommand,1);
		if (res) PutNetworkStatus (res,NULL,stat);
		if (strcmp (mcom->hotremote_2,"_")) {
			res = GetHotcode (mcom->hotremote_2,mcom->hotcommand_2,st);
			StoreSwitch ((word)mcom->adress,1,mcom->hotremote_2,mcom->hotcommand_2,1);
			msSleep (500);
			res = SetTransceiverModusEx2 ((byte)(mcom->adress >> 8),(byte)(mcom->adress & 0xff),st,res);
			if (res) PutNetworkStatus (res,NULL,stat);
		}
		WriteSwitches ();
		status_changed = 1;
		msSleep (800);
		LCDTimeCommand (LCD_REFRESHDATE);
		break;
	case COMMAND_SETSTAT:
		res = GetHotcode (com->remote,com->command,st);
		if (res == -1) {
			GetError (ERR_HOTCODE,st);
			sprintf (err,st,com->remote,com->command);
			log_print (err,LOG_ERROR);
			PutNetworkStatus (ERR_HOTCODE,err,stat);
		}
		else {
			StoreSwitch ((word)com->timeout,0,com->remote,com->command,1);
			WriteSwitches ();
			res = SetTransceiverModusEx ((byte)(com->timeout >> 8),com->mode,(word)com->adress,(byte)(com->timeout & 0xff),st,res,(byte)((com->adress >> 16) & 0xff),com->trasmit_freq,0,0,0,0);
			if (res) PutNetworkStatus (res,NULL,stat);
			msSleep (800);
			status_changed = 1;
			LCDTimeCommand (LCD_REFRESHDATE);
		}
		break;
	case COMMAND_SETSTAT2:
		res = GetHotcode (com->remote,com->command,st);
		if (res == -1) {
			GetError (ERR_HOTCODE,st);
			sprintf (err,st,com->remote,com->command);
			log_print (err,LOG_ERROR);
			PutNetworkStatus (ERR_HOTCODE,err,stat);
		}
		else {
			StoreSwitch ((word)com->timeout,1,com->remote,com->command,1);
			WriteSwitches ();
			res = SetTransceiverModusEx2 ((byte)(com->timeout >> 8),(byte)(com->timeout & 0xff),st,res);
			if (res) PutNetworkStatus (res,NULL,stat);
			status_changed = 1;
		}
		break;
	case COMMAND_IRDBFILE:
#ifdef WIN32
		ReadIRTransDirectory ("*.irdb",(REMOTEBUFFER *)stat,com->adress,STATUS_IRDBFILE);
#endif
#ifdef LINUX
		ReadIRTransDirectory (".irdb",(REMOTEBUFFER *)stat,com->adress,STATUS_IRDBFILE);
#endif
		break;
	case COMMAND_TRANSFILE:
#ifdef WIN32
		ReadIRTransDirectory ("*.tra",(REMOTEBUFFER *)stat,com->adress,STATUS_TRANSLATIONFILE);
#endif
#ifdef LINUX
		ReadIRTransDirectory (".tra",(REMOTEBUFFER *)stat,com->adress,STATUS_TRANSLATIONFILE);
#endif
		break;
	case COMMAND_GETREMOTES:
		sprintf (err,"Getremotes %d\n",com->adress);
		log_print (err,LOG_DEBUG);
		GetRemoteDatabase ((REMOTEBUFFER *)stat,com->adress);
		break;
	case COMMAND_LISTBUS:
		GetBusList ((REMOTEBUFFER *)stat,com->adress);
		break;
	case COMMAND_GETCOMMANDS:
		sprintf (err,"Getcommands: %s/%d\n",com->remote,com->adress);
		log_print (err,LOG_DEBUG);
		res = GetCommandDatabase ((COMMANDBUFFER *)stat,com->remote,com->adress);
		if (res) {
			ns = (NETWORKSTATUS *)stat;
			PutNetworkStatus (ERR_REMOTENOTFOUND,NULL,stat);
			strcpy (err,ns->message);
			sprintf (ns->message,err,com->remote);
			log_print (ns->message,LOG_ERROR);
		}
		break;
	}

}


void SetLearnstatus (int client)
{
	int i,j;

	if (sockinfo[client].ird.data[0] & LONG_CODE_FLAG) strcpy (sockinfo[client].learnstatus.received,"Long Code");
	else if (sockinfo[client].ird.mode == TIMECOUNT_18) memcpy (sockinfo[client].learnstatus.received,((IRDATA_18 *)&sockinfo[client].ird)->data,sockinfo[client].ird.ir_length);
	else if ((sockinfo[client].ird.mode & SPECIAL_IR_MODE) == PULSE_200) memcpy (sockinfo[client].learnstatus.received,((IRDATA_SINGLE *)&sockinfo[client].ird)->data,sockinfo[client].ird.ir_length);
	else {
		i = j = 0;
		while (i < sockinfo[client].ird.ir_length) {
			if (!(sockinfo[client].ird.data[i] & 128)) sockinfo[client].learnstatus.received[j++] = sockinfo[client].ird.data[i];
			i++;
		}
		sockinfo[client].learnstatus.received[j++] = 0;
	}
}



// Time with 1ms resolution

unsigned int GetMsTime (void)
{
#ifdef WIN32
	return (GetTickCount ());
#endif

#ifdef LINUX
	struct timeval tv;
	gettimeofday (&tv,0);

	return (tv.tv_sec * 1000 + tv.tv_usec);
#endif
}


char shadow_buf[256];

int GetDeviceStatus (STATUSBUFFER *buf)
{
	int res;

	if (status_changed || (time (0) - last_status_read) >= status_cache_timeout) {
		res = GetBusInfoEx (remote_statusex,0);
		if (res) {
			PutNetworkStatus (res,NULL,buf);
			return (res);
		}
		else {
			status_changed = 0;
			last_status_read = time (0);
		}
	}
	PutDeviceStatus ((NETWORKMODE *)buf);
	return (0);
}




void compress_lcdbuffer (LCDCOMMAND *lcd,char *buf,int bus)
{
	int i = 0;
	int pos = 0;
	char line[100];
	char lf[2];
	int x,y;


	byte LINEFEED = 0x1f;
	byte C_SPACE = 16;
	byte C_BLOCK = 17;
	byte C_DATA = 18;

	if (bus == 0xffff) bus = 0;

	if ((IRDevices[bus].io.advanced_lcd & 2) || new_lcd_flag) {
		LINEFEED = 10;
		C_SPACE = 11;
		C_BLOCK = 12;
		C_DATA = 13;
		ConvertLCDCharset ((byte *)(lcd->framebuffer));
	}
	else if (IRDevices[bus].version[0] == 'D') ConvertLCDCharset ((byte *)(lcd->framebuffer));

	lf[0] = LINEFEED;
	lf[1] = 0;


	buf[0] = 0;
	if (count_difference (lcd) <= 16) {
		i = 0;
		buf[i++] = C_DATA;
		for (y = 0;y < lcd->hgt;y++) {
			for (x = 0;x < lcd->wid;x++) {
				if (shadow_buf[x + y * lcd->wid] != lcd->framebuffer[x + y * lcd->wid] && lcd->framebuffer[x + y * lcd->wid]) {
					buf[i++] = (x + 1) | y << 6;
					buf[i++] = lcd->framebuffer[x + y * lcd->wid];
				}
			}
		}
		buf[i++] = 0;
	}
	else for (i=0;i < lcd -> hgt;i++) {
		memset (line,0,100);
		memcpy (line,lcd->framebuffer + i * lcd->wid,lcd->wid);
		pos = (int)strlen (line) - 1;
		while (line[pos] == ' ' && pos) pos--;
		if (line[pos + 1] == ' ') line[pos + 1] = 0;
		compress_char (line,' ',C_SPACE);
//		compress_char (line,(char)255,C_BLOCK);
		strcat (line,lf);
		strcat (buf,line);
	}

	memcpy (shadow_buf,lcd->framebuffer,lcd->wid * lcd->hgt);

	if (!(IRDevices[bus].io.advanced_lcd & 2) && !new_lcd_flag && IRDevices[bus].version[0] != 'D' && IRDevices[bus].version[0] != 'F') buf[strlen(buf)-1] = 0;
}


int count_difference (LCDCOMMAND *lcd)
{
	int i = 0;
	int diff = 0;
	if (lcd_init) {
		lcd_init = 0;
		return (99);
	}
	while (i < lcd->wid * lcd->hgt) {
		if (shadow_buf[i] != lcd->framebuffer[i]) diff++;
		i++;
	}
	return (diff);
}


void compress_char (char ln[],char src,char tar)
{
	char st[100];
	char tr[100];
	int i = 0;
	int pos;

	strcpy (tr,ln);
	while (ln[i]) {
		if (ln[i] == src) {
			pos = i;
			while (ln[i] == src) i++;
			if ((i - pos) > 2) {
				strcpy (st,ln + i);
				tr[pos] = tar;
				tr[pos+1] = i - pos;
				tr[pos+2] = 0;
				strcpy (tr + pos + 2,st);
			}
		}
		i++;
	}
	strcpy (ln,tr);

}


int GetHotcode (char rem[],char com[],byte data[])
{
	IRDATA ir;
	int i = 0;

	*data = 0;

	if (*rem == 0 || *com == 0) return (0);

	if (DBFindRemoteCommandEx (rem,com,&ir,0,0)) return (-1);
	if (ir.mode & RAW_DATA) {
		memcpy (data,ir.pause_len,ir.ir_length);
		ReformatHotcode (data,ir.ir_length);
		return (ir.ir_length);
	}
	else {
		memcpy (data,ir.data,ir.ir_length);
		return (ir.ir_length);
	}
}


void ReformatHotcode (byte data[],int len)
{
	int i = 0;
	word wert;
	while (i < len) {
		if (!data[i]) {
			i++;
			wert = (data[i] << 8) + data[i+1];
			wert -= (wert >> 4) + RAW_TOLERANCE;
			data[i] = wert >> 8;
			data[i+1] = wert & 0xff;
			i += 2;
		}
		else {
			data[i] -= (data[i] >> 4) + RAW_TOLERANCE;
			i++;
		}
	}

}

void PutDeviceStatus (NETWORKMODE *mode)
{
	int i;
	mode->adress = remote_statusex[0].my_adress;
	mode->statustype = STATUS_DEVICEMODE;
	mode->statuslen = sizeof (NETWORKMODE);

	for (i=0;i < 16;i++) {
		if (i == mode->adress) mode->stat[i].features = IRDevices[0].fw_capabilities;
		else mode->stat[i].features = remote_statusex[0].stat[i].capabilities;
		mode->stat[i].extended_mode = remote_statusex[0].stat[i].extended_mode;
		mode->stat[i].extended_mode2 = remote_statusex[0].stat[i].extended_mode2;
		mode->stat[i].device_mode = remote_statusex[0].stat[i].device_mode;
		mode->stat[i].send_mask = remote_statusex[0].stat[i].send_mask;
		memcpy (mode->stat[i].version,remote_statusex[0].stat[i].version,10);
		if (mode->stat[i].version[0]) {
			FindSwitch ((word)i,0,mode->stat[i].remote,mode->stat[i].command,&mode->stat[i].switch_mode);
		}

	}
}

void PutDeviceStatusEx (void *pnt,byte command,byte offset,int ver)
{
	int i,bus;
	NETWORKMODEEX *mode;
	NETWORKMODEEXN *moden;
	NETWORKMODEEX2 *modeex;
	NETWORKMODEEX3 *modeex3;
	NETWORKMODEEX4 *modeex4;

	if (command == COMMAND_STATUSEX) {
		mode = (NETWORKMODEEX *)pnt;
		mode->statustype = STATUS_DEVICEMODEEX;
		mode->statuslen = sizeof (NETWORKMODEEX);
		mode->count = device_cnt;
		for (bus=0;bus < device_cnt;bus++) {
			mode->dev_adr[bus] = remote_statusex[bus].my_adress;
			for (i=0;i < 16;i++) {
				if (i == mode->dev_adr[bus]) mode->stat[bus][i].features = IRDevices[bus].fw_capabilities;
				else mode->stat[bus][i].features = remote_statusex[bus].stat[i].capabilities;
				mode->stat[bus][i].extended_mode = remote_statusex[bus].stat[i].extended_mode;
				mode->stat[bus][i].extended_mode2 = remote_statusex[bus].stat[i].extended_mode2;
				mode->stat[bus][i].device_mode = remote_statusex[bus].stat[i].device_mode;
				mode->stat[bus][i].send_mask = remote_statusex[bus].stat[i].send_mask;
				memcpy (mode->stat[bus][i].version,remote_statusex[bus].stat[i].version,10);
				if (mode->stat[bus][i].version[0]) {
					FindSwitch ((word)(i + bus * 16),0,mode->stat[bus][i].remote,mode->stat[bus][i].command,&mode->stat[bus][i].switch_mode);
				}

			}
		}
	}

	if (command == COMMAND_STATUSEXN) {
		moden = (NETWORKMODEEXN *)pnt;
		moden->statustype = STATUS_DEVICEMODEEXN;
		moden->statuslen = sizeof (NETWORKMODEEXN);
		moden->count = device_cnt;
		moden->offset = offset;
		for (bus=0;bus < 8;bus++) {
			moden->dev_adr[bus] = remote_statusex[offset + bus].my_adress;
			for (i=0;i < 16;i++) {
				if (i == moden->dev_adr[bus]) moden->stat[bus][i].features = IRDevices[offset + bus].fw_capabilities;
				else moden->stat[bus][i].features = remote_statusex[offset + bus].stat[i].capabilities;
				moden->stat[bus][i].extended_mode = remote_statusex[offset + bus].stat[i].extended_mode;
				moden->stat[bus][i].extended_mode2 = remote_statusex[offset + bus].stat[i].extended_mode2;
				moden->stat[bus][i].device_mode = remote_statusex[offset + bus].stat[i].device_mode;
				moden->stat[bus][i].send_mask = remote_statusex[offset + bus].stat[i].send_mask;
				moden->stat[bus][i].extended_mode3 = remote_statusex[offset + bus].stat[i].extended_mode_ex[1];
				moden->stat[bus][i].extended_mode4 = remote_statusex[offset + bus].stat[i].extended_mode_ex[2];
				memcpy (moden->stat[bus][i].version,remote_statusex[offset + bus].stat[i].version,10);
 				if (moden->stat[bus][i].version[0]) {
					FindSwitch ((word)(i + bus * 16),0,moden->stat[bus][i].remote,moden->stat[bus][i].command,&moden->stat[bus][i].switch_mode);
					if (remote_statusex[offset + bus].stat[i].wake_mac[0] || remote_statusex[offset + bus].stat[i].wake_mac[1] || remote_statusex[offset + bus].stat[i].wake_mac[2] ||
						remote_statusex[offset + bus].stat[i].wake_mac[3] || remote_statusex[offset + bus].stat[i].wake_mac[4] || remote_statusex[offset + bus].stat[i].wake_mac[5]) {

						sprintf (moden->stat[bus][i].remote2,"@@@~~~LAN~~~@@@ %s %s          ",IRDevices[offset + bus].usb_serno,IRDevices[offset + bus].device_node);
						memcpy (moden->stat[bus][i].command2,remote_statusex[offset + bus].stat[i].wake_mac,6);
					}
					else {
						if (moden->stat[bus][i].features & FN_LAN) {
							sprintf (moden->stat[bus][i].remote2,"@@@~~~lan~~~@@@ %s %s          ",IRDevices[offset + bus].usb_serno,IRDevices[offset + bus].device_node);
							memset (moden->stat[bus][i].command2,0,6);
						}
						else FindSwitch ((word)(i + bus * 16),1,moden->stat[bus][i].remote2,moden->stat[bus][i].command2,&moden->stat[bus][i].switch_mode2);
					}
				}

			}
		}
	}
	if (command == COMMAND_STATUSEX2) {
		modeex = (NETWORKMODEEX2 *)pnt;
		modeex->statustype = STATUS_DEVICEMODEEX2;
		modeex->statuslen = sizeof (NETWORKMODEEX2);
		modeex->count = device_cnt;
		modeex->offset = offset;
		for (bus=0;bus < 8;bus++) {
			modeex->dev_adr[bus] = remote_statusex[offset + bus].my_adress;
			for (i=0;i < 16;i++) {
				if (i == modeex->dev_adr[bus]) {
					modeex->stat[bus][i].features = IRDevices[offset + bus].fw_capabilities;
					modeex->stat[bus][i].features2 = IRDevices[offset + bus].fw_capabilities2;
					modeex->stat[bus][i].features3 = IRDevices[offset + bus].fw_capabilities3;
					modeex->stat[bus][i].features4 = IRDevices[offset + bus].fw_capabilities4;
				}
				else {
					modeex->stat[bus][i].features = remote_statusex[offset + bus].stat[i].capabilities;
					modeex->stat[bus][i].features2 = remote_statusex[offset + bus].stat[i].capabilities2;
					modeex->stat[bus][i].features3 = remote_statusex[offset + bus].stat[i].capabilities3;
					modeex->stat[bus][i].features4 = remote_statusex[offset + bus].stat[i].capabilities4;
				}
				modeex->stat[bus][i].extended_mode = remote_statusex[offset + bus].stat[i].extended_mode;
				modeex->stat[bus][i].extended_mode2 = remote_statusex[offset + bus].stat[i].extended_mode2;
				modeex->stat[bus][i].device_mode = remote_statusex[offset + bus].stat[i].device_mode;
				modeex->stat[bus][i].send_mask = remote_statusex[offset + bus].stat[i].send_mask;
				modeex->stat[bus][i].extended_mode_ex[0] = remote_statusex[offset + bus].stat[i].extended_mode_ex[0];
				modeex->stat[bus][i].extended_mode_ex[1] = remote_statusex[offset + bus].stat[i].extended_mode_ex[1];
				modeex->stat[bus][i].extended_mode_ex[2] = remote_statusex[offset + bus].stat[i].extended_mode_ex[2];
				modeex->stat[bus][i].extended_mode_ex[3] = remote_statusex[offset + bus].stat[i].extended_mode_ex[3];
				modeex->stat[bus][i].extended_mode_ex[4] = remote_statusex[offset + bus].stat[i].extended_mode_ex[4];
				modeex->stat[bus][i].extended_mode_ex[5] = remote_statusex[offset + bus].stat[i].extended_mode_ex[5];
				modeex->stat[bus][i].extended_mode_ex[6] = remote_statusex[offset + bus].stat[i].extended_mode_ex[6];
				modeex->stat[bus][i].extended_mode_ex[7] = remote_statusex[offset + bus].stat[i].extended_mode_ex[7];
				memcpy (modeex->stat[bus][i].version,remote_statusex[offset + bus].stat[i].version,10);
 				if (modeex->stat[bus][i].version[0]) {
					FindSwitch ((word)(i + bus * 16),0,modeex->stat[bus][i].remote,modeex->stat[bus][i].command,&modeex->stat[bus][i].switch_mode);
					if (remote_statusex[offset + bus].stat[i].wake_mac[0] || remote_statusex[offset + bus].stat[i].wake_mac[1] || remote_statusex[offset + bus].stat[i].wake_mac[2] ||
						remote_statusex[offset + bus].stat[i].wake_mac[3] || remote_statusex[offset + bus].stat[i].wake_mac[4] || remote_statusex[offset + bus].stat[i].wake_mac[5]) {

						sprintf (modeex->stat[bus][i].remote2,"@@@~~~LAN~~~@@@ %s %s          ",IRDevices[offset + bus].usb_serno,IRDevices[offset + bus].device_node);
						memcpy (modeex->stat[bus][i].command2,remote_statusex[offset + bus].stat[i].wake_mac,6);
					}
					else {
						if (modeex->stat[bus][i].features & FN_LAN) {
							sprintf (modeex->stat[bus][i].remote2,"@@@~~~lan~~~@@@ %s %s          ",IRDevices[offset + bus].usb_serno,IRDevices[offset + bus].device_node);
							memset (modeex->stat[bus][i].command2,0,6);
						}
						else FindSwitch ((word)(i + bus * 16),1,modeex->stat[bus][i].remote2,modeex->stat[bus][i].command2,&modeex->stat[bus][i].switch_mode2);
					}
				}
			}
		}
	}
	if (command == COMMAND_STATUSEX3 || command == COMMAND_STATUSEX3_SHORT) {
		if (ver) {
			modeex4 = (NETWORKMODEEX4 *)pnt;
			modeex4->statustype = STATUS_DEVICEMODEEX3;
			modeex4->statuslen = sizeof (NETWORKMODEEX4);
			modeex4->count = device_cnt;
			modeex4->offset = offset;
			for (bus=0;bus < 8;bus++) {
				modeex4->dev_adr[bus] = remote_statusex[offset + bus].my_adress;
				for (i=0;i < 16;i++) {
					if (i == modeex4->dev_adr[bus]) {
						modeex4->stat[bus][i].features = IRDevices[offset + bus].fw_capabilities;
						modeex4->stat[bus][i].features2 = IRDevices[offset + bus].fw_capabilities2;
						modeex4->stat[bus][i].features3 = IRDevices[offset + bus].fw_capabilities3;
						modeex4->stat[bus][i].features4 = IRDevices[offset + bus].fw_capabilities4;
						memset (modeex4->stat[bus][i].lanversion,' ',10);
						if (IRDevices[offset + bus].lan_version[0] >= 'A' && IRDevices[offset + bus].lan_version[0] <= 'Z')
							memcpy (modeex4->stat[bus][i].lanversion,IRDevices[offset + bus].lan_version,strlen (IRDevices[offset + bus].lan_version));
					}
					else {
						modeex4->stat[bus][i].features = remote_statusex[offset + bus].stat[i].capabilities;
						modeex4->stat[bus][i].features2 = remote_statusex[offset + bus].stat[i].capabilities2;
						modeex4->stat[bus][i].features3 = remote_statusex[offset + bus].stat[i].capabilities3;
						modeex4->stat[bus][i].features4 = remote_statusex[offset + bus].stat[i].capabilities4;
						memset (modeex4->stat[bus][i].lanversion,' ',10);
					}
					modeex4->stat[bus][i].extended_mode = remote_statusex[offset + bus].stat[i].extended_mode;
					modeex4->stat[bus][i].extended_mode2 = remote_statusex[offset + bus].stat[i].extended_mode2;
					modeex4->stat[bus][i].device_mode = remote_statusex[offset + bus].stat[i].device_mode;
					modeex4->stat[bus][i].send_mask = remote_statusex[offset + bus].stat[i].send_mask;
					memcpy (modeex4->stat[bus][i].extended_mode_ex,remote_statusex[offset + bus].stat[i].extended_mode_ex,8);
					memcpy (modeex4->stat[bus][i].extended_mode_ex2,remote_statusex[offset + bus].stat[i].extended_mode_ex2,8);
					memcpy (&modeex4->stat[bus][i].status_input,&remote_statusex[offset + bus].stat[i].status_input,sizeof (STATUS_MEMORY));
					memcpy (modeex4->stat[bus][i].rs232_mode,remote_statusex[offset + bus].stat[i].rs232_mode,16);

					memcpy (modeex4->stat[bus][i].version,remote_statusex[offset + bus].stat[i].version,10);
 					if (modeex4->stat[bus][i].version[0]) {
						FindSwitch ((word)(i + bus * 16),0,modeex4->stat[bus][i].remote,modeex4->stat[bus][i].command,&modeex4->stat[bus][i].switch_mode);
						if (remote_statusex[offset + bus].stat[i].wake_mac[0] || remote_statusex[offset + bus].stat[i].wake_mac[1] || remote_statusex[offset + bus].stat[i].wake_mac[2] ||
							remote_statusex[offset + bus].stat[i].wake_mac[3] || remote_statusex[offset + bus].stat[i].wake_mac[4] || remote_statusex[offset + bus].stat[i].wake_mac[5]) {

							sprintf (modeex4->stat[bus][i].remote2,"@@@~~~LAN~~~@@@ %s %s          ",IRDevices[offset + bus].usb_serno,IRDevices[offset + bus].device_node);
							memcpy (modeex4->stat[bus][i].command2,remote_statusex[offset + bus].stat[i].wake_mac,6);
						}
						else {
							if (modeex4->stat[bus][i].features & FN_LAN) {
								sprintf (modeex4->stat[bus][i].remote2,"@@@~~~lan~~~@@@ %s %s          ",IRDevices[offset + bus].usb_serno,IRDevices[offset + bus].device_node);
								memset (modeex4->stat[bus][i].command2,0,6);
							}
							else FindSwitch ((word)(i + bus * 16),1,modeex4->stat[bus][i].remote2,modeex4->stat[bus][i].command2,&modeex4->stat[bus][i].switch_mode2);
						}
					}
				}
			}
			return;
		}
		modeex3 = (NETWORKMODEEX3 *)pnt;
		modeex3->statustype = STATUS_DEVICEMODEEX3;
		modeex3->statuslen = sizeof (NETWORKMODEEX3);
		modeex3->count = device_cnt;
		modeex3->offset = offset;
		for (bus=0;bus < 8;bus++) {
			modeex3->dev_adr[bus] = remote_statusex[offset + bus].my_adress;
			for (i=0;i < 16;i++) {
				if (i == modeex3->dev_adr[bus]) {
					modeex3->stat[bus][i].features = IRDevices[offset + bus].fw_capabilities;
					modeex3->stat[bus][i].features2 = IRDevices[offset + bus].fw_capabilities2;
					modeex3->stat[bus][i].features3 = IRDevices[offset + bus].fw_capabilities3;
					modeex3->stat[bus][i].features4 = IRDevices[offset + bus].fw_capabilities4;
				}
				else {
					modeex3->stat[bus][i].features = remote_statusex[offset + bus].stat[i].capabilities;
					modeex3->stat[bus][i].features2 = remote_statusex[offset + bus].stat[i].capabilities2;
					modeex3->stat[bus][i].features3 = remote_statusex[offset + bus].stat[i].capabilities3;
					modeex3->stat[bus][i].features4 = remote_statusex[offset + bus].stat[i].capabilities4;
				}
				modeex3->stat[bus][i].extended_mode = remote_statusex[offset + bus].stat[i].extended_mode;
				modeex3->stat[bus][i].extended_mode2 = remote_statusex[offset + bus].stat[i].extended_mode2;
				modeex3->stat[bus][i].device_mode = remote_statusex[offset + bus].stat[i].device_mode;
				modeex3->stat[bus][i].send_mask = remote_statusex[offset + bus].stat[i].send_mask;
				memcpy (modeex3->stat[bus][i].extended_mode_ex,remote_statusex[offset + bus].stat[i].extended_mode_ex,8);
				memcpy (modeex3->stat[bus][i].extended_mode_ex2,remote_statusex[offset + bus].stat[i].extended_mode_ex2,8);
				memcpy (&modeex3->stat[bus][i].status_input,&remote_statusex[offset + bus].stat[i].status_input,sizeof (STATUS_MEMORY));
				memcpy (modeex3->stat[bus][i].rs232_mode,remote_statusex[offset + bus].stat[i].rs232_mode,16);

				memcpy (modeex3->stat[bus][i].version,remote_statusex[offset + bus].stat[i].version,10);
 				if (modeex3->stat[bus][i].version[0]) {
					FindSwitch ((word)(i + bus * 16),0,modeex3->stat[bus][i].remote,modeex3->stat[bus][i].command,&modeex3->stat[bus][i].switch_mode);
					if (remote_statusex[offset + bus].stat[i].wake_mac[0] || remote_statusex[offset + bus].stat[i].wake_mac[1] || remote_statusex[offset + bus].stat[i].wake_mac[2] ||
						remote_statusex[offset + bus].stat[i].wake_mac[3] || remote_statusex[offset + bus].stat[i].wake_mac[4] || remote_statusex[offset + bus].stat[i].wake_mac[5]) {

						sprintf (modeex3->stat[bus][i].remote2,"@@@~~~LAN~~~@@@ %s %s          ",IRDevices[offset + bus].usb_serno,IRDevices[offset + bus].device_node);
						memcpy (modeex3->stat[bus][i].command2,remote_statusex[offset + bus].stat[i].wake_mac,6);
					}
					else {
						if (modeex3->stat[bus][i].features & FN_LAN) {
							sprintf (modeex3->stat[bus][i].remote2,"@@@~~~lan~~~@@@ %s %s          ",IRDevices[offset + bus].usb_serno,IRDevices[offset + bus].device_node);
							memset (modeex3->stat[bus][i].command2,0,6);
						}
						else FindSwitch ((word)(i + bus * 16),1,modeex3->stat[bus][i].remote2,modeex3->stat[bus][i].command2,&modeex3->stat[bus][i].switch_mode2);
					}
				}
			}
		}
	}
}

void PutNetworkStatus (int res,char msg[],STATUSBUFFER *buf)
{

	NETWORKSTATUS *stat;
	char txt[256];

	stat = (NETWORKSTATUS *)buf;

	stat->statustype = STATUS_MESSAGE;
	
	if (!res) {
		stat->statuslen = 8;
		return;
	}

	stat->netstatus = res;
	stat->statuslen = sizeof (NETWORKSTATUS);

	if (res == ERR_TIMEOUT) {
		stat->statuslevel = IRTIMEOUT;
		sprintf (stat->message,"Timeout");
		return;
	}
	
	if (res == ERR_OPENASCII) {
		stat->statuslevel = FATAL;
		sprintf (stat->message,"Error opening Remote %s",msg);
		return;
	}

	if (res == ERR_NOFILEOPEN) {
		stat->statuslevel = FATAL;
		sprintf (stat->message,"No Remote open");
		return;
	}

	stat->statuslevel = IR;

	if (msg) {
		strcpy (stat->message,msg);
		return;
	}
	
	GetError (res,txt);
	sprintf (stat->message,"IR Error: %s",txt);
}

void ResultStoreTiming (IRDATA *ird,NETWORKTIMING *stat)
{
	stat->statustype = STATUS_TIMING;
	stat->statuslen = sizeof (NETWORKTIMING);

	stat->timing.mode = ird->mode;
	stat->timing.time_cnt = ird->time_cnt;
	stat->timing.ir_repeat = ird->ir_repeat;
	stat->timing.repeat_pause = ird->repeat_pause;
	memcpy (stat->timing.pause_len,ird->pause_len,12);
	memcpy (stat->timing.pulse_len,ird->pulse_len,12);
	memcpy (stat->timing.data,ird->data,CODE_LEN);
	stat->adress = ird->address;
}


void SwapNetworkheader (NETWORKSTATUS *ns)
{
	if (!byteorder) return;

	swap_int (&ns->clientid);
	swap_word (&ns->statuslen);
	swap_word (&ns->statustype);
	swap_word (&ns->adress);
}


void SwapNetworkcommand (NETWORKCOMMAND *nc)
{
	if (!byteorder) return;

	swap_int (&nc->adress);
	swap_word (&nc->timeout);
}

void SwapNetworkstatus (void *pnt)
{
	int i;
	word type;
	NETWORKSTATUS *ns;
	NETWORKTIMING *nt;
	NETWORKMODE *nm;
	REMOTEBUFFER *rb;
	COMMANDBUFFER *cb;
	FUNCTIONBUFFER *fb;

	if (!byteorder) return;

	ns = (NETWORKSTATUS *)pnt;
	type = ns->statustype;

	SwapNetworkheader ((NETWORKSTATUS *)pnt);

	if (type == STATUS_FRAMELEN) {
		swap_word (&ns->framelen);
		return;
	}
	if (type == STATUS_MESSAGE) {
		swap_word (&ns->netstatus);
		swap_word (&ns->statuslevel);
		return;
	}
	if (type == STATUS_TIMING) {
		nt = (NETWORKTIMING *)pnt;
		for (i=0;i < TIME_LEN;i++) {
			swap_word (&nt->timing.pause_len[i]); 
			swap_word (&nt->timing.pulse_len[i]);
		}
		return;
	}
	if (type == STATUS_DEVICEMODE) {
		nm = (NETWORKMODE *)pnt;
		for (i=0;i<16;i++) swap_int (&nm->stat[i].send_mask);
		return;
	}
	if (type == STATUS_REMOTELIST) {
		rb = (REMOTEBUFFER *)pnt;
		swap_word (&rb->offset);
		swap_word (&rb->count_buffer);
		swap_word (&rb->count_remaining);
		swap_word (&rb->count_total);
		for (i=0;i<rb->count_buffer;i++) {
			swap_int (&rb->remotes[i].source_mask);
			swap_int (&rb->remotes[i].target_mask);
		}
		return;
	}
	if (type == STATUS_COMMANDLIST) {
		cb = (COMMANDBUFFER *)pnt;

		swap_word (&cb->offset);
		swap_word (&cb->count_buffer);
		swap_word (&cb->count_remaining);
		swap_word (&cb->count_total);
		return;
	}
	if (type == STATUS_FUNCTION) {
		fb = (FUNCTIONBUFFER *)pnt;
		swap_int (&fb->functions);
		swap_int (&fb->serno);
		return;
	}
}

int GetNetworkClient (SOCKET sockfd)
{

	int i = 0;
	while (i < CLIENT_COUNT) {
		if ((sockinfo[i].type == SELECT_SERVER || sockinfo[i].type == SELECT_REOPEN) 
			&& sockfd == sockinfo[i].fd) return (i);
		i++;
	}
	return (-1);
}


void CloseServerSockets (SOCKET sock,SOCKET lirc, SOCKET udp,SOCKET web)
{
	if (sock) closesocket (sock);
	if (lirc) closesocket (lirc);
	if (udp) closesocket (udp);
	if (web) closesocket (web);
#ifdef LINUX
	if (local_socket) closesocket (local_socket);
#endif
	CloseIRTransLANSocket ();
	// XAP
	// Broadcast Socket (LINUX)
}


int InitServerSocket (SOCKET *sock,SOCKET *lirc, SOCKET *udp,SOCKET *web)
{
	char msg[256],hub[50];
	struct sockaddr_in serv_addr;

#ifdef LINUX
	int res,new = 1;
	struct stat s;
	struct sockaddr_un serv_addr_un;
#endif

#ifdef WIN32
	int err,res;
    WORD	wVersionRequired;
    WSADATA	wsaData;
    wVersionRequired = MAKEWORD(2,2);
    err = WSAStartup(wVersionRequired, &wsaData);
    if (err != 0) exit(1);

#endif
// ************************************************************* LIRC local Socket
#ifdef LINUX
	if (mode_flag & NO_LIRC) {
		local_socket = 0;
	}
	else {
		local_socket = socket(AF_UNIX,SOCK_STREAM,0);
		if (local_socket == -1) return (ERR_OPENSOCKET);

		res = stat (LIRCD,&s);
		if (res == -1 && errno != ENOENT) {
			close (local_socket);
			return (ERR_OPENSOCKET);
			}

		if(res != -1) {
			new = 0;
			res = unlink (LIRCD);
			if (res == -1 && errno != ENOENT) {
				close (local_socket);
				return (ERR_OPENSOCKET);
				}
			}

		serv_addr_un.sun_family = AF_UNIX;
		strcpy (serv_addr_un.sun_path,LIRCD);

		if (bind (local_socket,(struct sockaddr *)&serv_addr_un,sizeof(serv_addr_un)) == -1) {
			close(local_socket);
			return (ERR_OPENSOCKET);
			}

		if (new) chmod (LIRCD,PERMISSIONS);
		else {
			chmod(LIRCD,s.st_mode);
			chown(LIRCD,s.st_uid,s.st_gid);
		}

		listen(local_socket,3);
	}

#endif

// ************************************************************* IRTrans Socket
	*sock = socket (PF_INET,SOCK_STREAM,0);
	if (*sock < 0) return (ERR_OPENSOCKET);

	if (res = ConfigSocket (sock,TCP_PORT)) return (res);

// ************************************************************* LIRC TCP/IP Socket

	if (mode_flag & NO_LIRC) {
		*lirc = 0;
	}
	else {
		*lirc = socket (PF_INET,SOCK_STREAM,0);
		if (*lirc < 0) return (ERR_OPENSOCKET);
		if (res = ConfigSocket (lirc,LIRC_PORT)) return (res);
	}
// ************************************************************* Web Socket
	*web = socket (PF_INET,SOCK_STREAM,0);
	if (*web < 0) return (ERR_OPENSOCKET);

	if (mode_flag & NO_WEB) {
		*web = 0;
	}
	else {
		if (webport) {
			if (res = ConfigSocket (web,webport)) return (ERR_BINDWEB);
		}

		else {
			if (res = ConfigSocket (web,webport = WEB_PORT)) {
				if (ConfigSocket (web,webport = ALTERNATE_WEB)) return (ERR_BINDWEB);
			}
		}
	}

// ************************************************************* UDP Socket
	*udp = socket (PF_INET,SOCK_DGRAM,0);
	if (*udp < 0) return (ERR_OPENSOCKET);

	memset (&serv_addr,0,sizeof (serv_addr));
	serv_addr.sin_family = AF_INET;
#ifdef DBOX
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = UDP_PORT;
#else
	serv_addr.sin_addr.s_addr = htonl (INADDR_ANY);
	serv_addr.sin_port = htons (8765);
#endif

	if (bind (*udp,(struct sockaddr *)&serv_addr,sizeof (serv_addr)) < 0) return (ERR_BINDSOCKET);


// ************************************************************* UDP Socket Port 21000 für IRTrans LAN Module

	if (res = OpenIRTransLANSocket ()) return res;
	OpenIRTransBroadcastSockets ();

// ************************************************************* UDP Relay Socket
	if (udp_relay_port) {
		udp_relay_socket = socket (PF_INET,SOCK_DGRAM,0);
		if (udp_relay_socket < 0) return (ERR_OPENSOCKET);

		memset (&serv_addr,0,sizeof (serv_addr));
		serv_addr.sin_family = AF_INET;

		serv_addr.sin_addr.s_addr = *((unsigned int *)gethostbyname (udp_relay_host)->h_addr);
		serv_addr.sin_port = htons ((word)udp_relay_port);

		if (connect (udp_relay_socket,(struct sockaddr *)&serv_addr,sizeof (serv_addr)) < 0) return (ERR_BINDSOCKET);
	}


	if (mode_flag & XAP) {
		xAP_rcv_port = XAP_PORT;
		xAP_rcv = socket (PF_INET,SOCK_DGRAM,0);
		if (xAP_rcv < 0) return (ERR_OPENSOCKET);

		memset (&serv_addr,0,sizeof (serv_addr));
		serv_addr.sin_family = AF_INET;

		serv_addr.sin_addr.s_addr = htonl (INADDR_ANY);
		serv_addr.sin_port = htons ((word)xAP_rcv_port);

		hub[0] = 0;

		if (bind (xAP_rcv,(struct sockaddr *)&serv_addr,sizeof (serv_addr)) < 0) {		// Hub aktiv
			sprintf (hub,"Hub active. ");
			for (xAP_rcv_port = XAP_PORT + 1;xAP_rcv_port < XAP_PORT + 100;xAP_rcv_port++) {
				memset (&serv_addr,0,sizeof (serv_addr));
				serv_addr.sin_family = AF_INET;

				serv_addr.sin_addr.s_addr = inet_addr ("127.0.0.1");
				serv_addr.sin_port = htons ((word)xAP_rcv_port);
				res = bind (xAP_rcv,(struct sockaddr *)&serv_addr,sizeof (serv_addr));
				if (!res) break;
			}
			if (res < 0) return (ERR_OPENSOCKET);
		}
		
		xAP_send = socket (PF_INET,SOCK_DGRAM,0);
		if (xAP_send < 0) return (ERR_OPENSOCKET);

		res = 1;
		setsockopt (xAP_send,SOL_SOCKET,SO_BROADCAST,(char *)&res,sizeof (int));

		memset (&serv_addr,0,sizeof (serv_addr));
		serv_addr.sin_family = AF_INET;

		serv_addr.sin_addr.s_addr = INADDR_BROADCAST;
		serv_addr.sin_port = htons ((word)XAP_PORT);

		if (res = connect (xAP_send,(struct sockaddr *)&serv_addr,sizeof (serv_addr)) < 0) {
			printf ("RES: %d   errno: %d\n",res,errno);
			return (ERR_BINDSOCKET);
		}

		if (mode_flag & XAP) {
			sprintf (msg,"xAP Interface enabled. %sListening on port %d\n",hub,xAP_rcv_port);
			log_print (msg,LOG_INFO);
		}
		
		xAP_SendHeartbeat ();
	}

	return (0);
}



int ConfigSocket (SOCKET *sock,unsigned short port)
{
	struct sockaddr_in serv_addr;
	memset (&serv_addr,0,sizeof (serv_addr));
	serv_addr.sin_family = AF_INET;

#ifdef DBOX
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = port;
#else
	serv_addr.sin_addr.s_addr = htonl (INADDR_ANY);
	serv_addr.sin_port = htons (port);
#endif
	if (bind (*sock,(struct sockaddr *)&serv_addr,sizeof (serv_addr)) < 0) return (ERR_BINDSOCKET);
		
	listen (*sock,5);
	return (0);
}

void process_udp_command (char *data,int len,struct sockaddr_in *send_adr)
{
	int res,i;
	int cmd_num,adr;
	char err[256],txt[256];
	char remote[80],command[512],ccf[2048];
	int netcom,netmask,bus,led,port,wait;
	NETWORKCLIENT client;

	data[len] = 0;

	res = AnalyzeUDPString (data,&netcom,remote,command,ccf,&netmask,&bus,&led,&port,&wait);
	if (res) {
		log_print ("Illegal UDP Command\n", LOG_ERROR);
		return;
	}

	if (port == 0) port = ntohs (send_adr->sin_port);		// Port = Sourceport

	sprintf (txt,"UDP Command: %d %s,%s,%d,%d [%x.%d]\n", netcom,remote,command,bus,led,send_adr->sin_addr.s_addr,port);
	log_print (txt,LOG_DEBUG);
	
	adr = 0;

	if (netmask) adr |= 0x10000 | netmask;

	if (led > 3) {
		led -= 4;
        adr |= ((led & 7) << 27) | 0x80000000;
		if (led > 8) adr |= ((led / 8) & 3) << 17;
	}
	else adr |= (led & 3) << 17;
	
	if (bus == 255) adr |= 0x40000000;
	else adr |= bus << 19;
	protocol_version = 210;

	if (netcom == COMMAND_SEND || netcom == COMMAND_SENDMASK || netcom == COMMAND_SENDMACRO) {
		if (netcom == COMMAND_SENDMACRO) {
			int cmd_array[16];
			word pause_array[16];
				
			if (!netmask) adr |= 0x10000 | 0xffff;

			for (i=0;i < 16;i++) pause_array[i] = wait;

			res = DBFindRemoteMacro (remote,command,cmd_array,pause_array);
				
			for (i=0;i < 16;i++) if (pause_array[i] > 2500) pause_array[i] = 2500;
			if (!res) res = SendIRMacro (cmd_array,adr,pause_array,NULL);
		}
		if (netcom == COMMAND_SEND || netcom == COMMAND_SENDMASK) {
			res = DBFindRemoteCommand (remote,command,&cmd_num,NULL);
			if (!res) SendIR (cmd_num,adr,COMMAND_SEND,NULL);
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
			log_print (err, LOG_ERROR);
			SendUDPAck ("ERR",send_adr,port);
			return;
		}
		SendUDPAck ("OK",send_adr,port);
		return;
	}

	if (netcom == 2) {
		memset (&client,0,sizeof (client));
		client.fp = ASCIIOpenRemote (remote,&client);
		if (!client.fp) {
			sprintf (txt,"Unable to open remote %s\n",remote);
			log_print (txt,LOG_DEBUG);
			SendUDPAck ("ERR",send_adr,port);
			return;
		}
		SendUDPAck ("READY",send_adr,port);
		res = LearnIREx (&client.ird,(word)'L',30000,0,0,0);
		if (res && res < ERR_LEARN_LENGTH) {
			fclose (client.fp);
			sprintf (txt,"Learn Error\n");
			log_print (txt,LOG_DEBUG);
			SendUDPAck ("ERR",send_adr,port);
			return;
		}

		client.timing = ASCIIStoreTiming (client.fp,&client.ird,&client.learnstatus);
		ASCIIFindCommand (client.fp,command,&client);
		ASCIIStoreCommand (client.fp,&client.ird,command,client.timing,0);
		fclose (client.fp);
		ReadIRDatabase ();
		SendUDPAck ("OK",send_adr,port);
	}
	if (netcom == 3) SendUDPAck ("OK",send_adr,port);
}

void SendUDPAck (char stat[],struct sockaddr_in *target,int port)
{
	target->sin_family = AF_INET;
	target->sin_port = htons ((word)port);

	sendto (irtlan_socket,stat,(int)strlen (stat),0,(struct sockaddr *)target,sizeof (struct sockaddr_in));
}


int AnalyzeUDPString (char *st,int *netcom,char *remote,char *command,char *ccf,int *netmask,int *bus,int *led,int *port,int *macro_wait)
{
	int i,p,v;

	*netcom = 0;
	ccf[0] = remote[0] = command[0] = 0;
	*bus = *led = 0;
	*netmask = 0;
	*port = 21000;
	*macro_wait = 500;

	i = 3;

	if (!strncmp (st,"sndccfr",7) || !strncmp (st,"Sndccfr",7) || !strncmp (st,"SNDCCFR",7)) {
		i = 7;
		*netcom = 5;
	}
	else if (!strncmp (st,"sndccf",6) || !strncmp (st,"Sndccf",6) || !strncmp (st,"SNDCCF",6)) {
		i = 6;
		*netcom = 4;
	}
	else if (!strncmp (st,"sndmask",7) || !strncmp (st,"Sndmask",7) || !strncmp (st,"SNDMASK",7) || !strncmp (st,"SndMask",7)) {
		i = 7;
		*netcom = COMMAND_SENDMASK;
	}
	else if (!strncmp (st,"sndmacro",8) || !strncmp (st,"Sndmacro",8) || !strncmp (st,"SNDMACRO",8) || !strncmp (st,"SndMacro",8)) {
		i = 8;
		*netcom = COMMAND_SENDMACRO;
	}
	else if (!strncmp (st,"snd",3) || !strncmp (st,"Snd",3) || !strncmp (st,"SND",3)) *netcom = COMMAND_SEND;
	else if (!strncmp (st,"lrn",3) || !strncmp (st,"Lrn",3) || !strncmp (st,"LRN",3)) *netcom = 2;
	else if (!strncmp (st,"ping",4) || !strncmp (st,"Ping",4) || !strncmp (st,"PING",4)) {
		*netcom = 3;
		i = 4;

		while (st[i] == ' ') i++;
		p = i;
		while (st[i] && st[i] != ',') i++;
		st[i++] = 0;

		if (st[p] == 'p' || st[p] == 'P') *port = atoi (st+p+1);
		return (0);
	}
	else return ERR_UDPFORMAT;

	while (st[i] == ' ') i++;

	if (*netcom == 4 || *netcom == 5) {
		p = i;
		while (st[i] && st[i] != ',') i++;

		v = st[i];
		st[i++] = 0;
		strcpy (ccf,st+p);
	}
	else {
		p = i;
		while (st[i] && st[i] != ',') i++;

		if (!st[i]) return ERR_UDPFORMAT + 1;
		st[i++] = 0;
		strncpy (remote,st+p,80);

		p = i;
		while (st[i] && st[i] != ',') i++;

		v = st[i];
		st[i++] = 0;
		if (*netcom == COMMAND_SENDMACRO) strncpy (command,st+p,512);
		strncpy (command,st+p,20);
	}

	while (v) {
		p = i;
	
		while (st[i] && st[i] != ',' && st[i] != ' ') i++;
		v = st[i];
		st[i++] = 0;

		if (st[p] == 'l' || st[p] == 'L') {
			p++;
			if (st[p] == 'i' || st[p] == 'I') *led = 1;
			else if (st[p] == 'e' || st[p] == 'E') *led = 2;
			else if (st[p] == 'b' || st[p] == 'B') *led = 3;
			else if (st[p] == '1') {
				if (st[p+1] == '0') *led = 13;
				else if (st[p+1] == '1') *led = 14;
				else if (st[p+1] == '2') *led = 15;
				else if (st[p+1] == '3') *led = 16;
				else if (st[p+1] == '4') *led = 17;
				else if (st[p+1] == '5') *led = 18;
				else if (st[p+1] == '6') *led = 19;
				else *led = 4;
			}
			else if (st[p] == '2') *led = 5;
			else if (st[p] == '3') *led = 6;
			else if (st[p] == '4') *led = 7;
			else if (st[p] == '5') *led = 8;
			else if (st[p] == '6') *led = 9;
			else if (st[p] == '7') *led = 10;
			else if (st[p] == '8') *led = 11;
			else if (st[p] == '9') *led = 12;
			else return ERR_UDPFORMAT + 2;
		}

		else if (st[p] == 'b' || st[p] == 'B') {
			p++;
			*bus = atoi (st+p);
			if (*bus > 255) return ERR_UDPFORMAT + 3;
		}

		else if (st[p] == 'm' || st[p] == 'M') {
			p++;
			*netmask = atoi (st+p);
		}

		else if (st[p] == 'p' || st[p] == 'P') {
			p++;
			*port = atoi (st+p);
		}

		else if (st[p] == 'w' || st[p] == 'W') {
			p++;
			*macro_wait = atoi (st+p);
		}

		else return ERR_UDPFORMAT + 4;
	}

	return (0);
}
