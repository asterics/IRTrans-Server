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
#include <time.h>
#include <sys/timeb.h>
#endif

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
#endif

word	CalcFramelength (IRDATA *ird);
int		ReadHTMLDirectory (void);
int		SetTransceiverIDEx (int bus,byte id);
int		GetBusInfo (STATUS_BUFFER *sb);
int		GetBusInfoEx (STATUS_BUFFER_N *sb,int bus);
int		GetBusInfoExShort (STATUS_BUFFER_N *sb,int bus);
void	CopyBusInfoShort (STATUS_BUFFER_N *sb,int bus);
int		GetBusInfoDetail (STATUS_BUFFER_N *sb,int bus);
int		SetTransceiverModusEx (int bus,byte mode,word send_mask,byte addr,char *hotcode,int hotlen,byte extended_mode,byte extended_mode2,byte extended_mode_ex[],byte *mac,byte rs232_mode[],STATUS_MEMORY *status_input);
int		SetTransceiverModusEx2 (int bus,byte addr,char *hotcode,int hotlen);
int		TransferFlashdataEx (int bus,word data[],int adr,int len,byte active,int iradr);
int		ReadFlashdataEx (int bus,int adr);
int		SendIR (int cmd_num,int address,byte netcommand,word *framelen);
int		SendIRMacro (int cmd_num[],int address,word macro_pause[],word *framelen);
int		DoSendIR (IRDATA *ir_data,IRDATA *ir_rep,int rpt_len,int rpt_pause,int bus,byte netcommand);
int		SendIRDataEx (IRDATA *ir_data,int address);
int		SendLCD (IRRAW *ir_data,int address);
int		AdvancedLCD (byte mode,byte data[],int len);
void	LCDBrightness (int val);
int		ResendIREx (int bus,IRDATA *ir_data);
byte	Convert2OldCarrier (byte carrier);
int		ResetTransceiverEx (int bus);
int		SetPowerLED (int bus,byte mode,byte val);
int		SetRelaisEx (int bus,byte val,byte rel);
int		ReadAnalogInputs (int bus,byte mask,ANALOG_INPUTS *inputs);
int		ReadAnalogInputsEx (int bus,word mask,byte mode,byte id[],ANALOG_INPUTS_EX *inputs);
//int		ReadAnalogInputs (int bus,byte mask,ANALOG_DATA *inputs);
int		SetAnalogConfig (ANALOG_CONFIG_COMMAND *acc);
int		SendSerialBlock (int bus,byte data[],byte len,byte param);
int		StoreTimerEntry (int bus,TIMERCOMMAND *tim);
int		TransferToTimelen18 (IRDATA *src,IRDATA *snd,int bus);
byte	CheckLEDValid (byte adr,int bus);
int		TestIRLength (IRDATA *ir,byte bus);
int		TestStatusAvailable (IRDATA *ir,int bus,byte netcommand);

int		ReadIR (byte data[]);
int		LearnIREx (IRDATA *ir_data,word addr,word timeout,word ir_timeout,byte carrier,byte modes);
int		LearnNextIREx (IRDATA *ir_data,word addr,word timeout,word ir_timeout,byte carrier,byte modes);
int		LearnRawIREx (IRRAW *ir_data,word addr,word timeout,word ir_timeout,byte carrier);
int		LearnRawIRRepeatEx (IRRAW *ir_data,word addr,word timeout,word ir_timeout,byte carrier);
int		LearnRepeatIREx (IRDATA *ir_data,word addr,word timeout,word ir_timeout,byte carrier,byte modes);
void	ResetComLines (void);
void	CorrectIRTimings (IRDATA *ir_data);
void	CorrectIRTimingsRAW (IRRAW *ir_data);


void	PrintPulseData (IRDATA *ir_data);
void	PrintCommand (IRDATA *ir_data);
void	PrintRawData (IRRAW *ir_data);

int		WriteTransceiverCommand (byte pnt);
int		WriteTransceiver (IRDATA *src,byte usb_mode);
byte	get_checksumme (IRDATA *ir);
void	ConvertToIRTRANS3 (IRDATA *ird);
void	ConvertToIRTRANS4 (IRDATA3 *ird);

int		GetTransceiverVersion (char version [],unsigned int *cap,unsigned int *serno,char mac_adr[],byte usbmode);
int		ResetTransceiver (void);
int		InitCommunication (char device[],char version[]);
int		InitCommunicationEx (char devicesel[]);
void	InitConversionTables (void);
void	ConvertLCDCharset (byte *pnt);
void	LCDTimeCommand (byte mode);
void	SetSpecialChars (byte dat[]);
void	SetLCDProcCharsV (byte dat[]);

void	FlushUSB (void);
void	FlushCom (void);
void	msSleep (int time);
int		ReadIRString (byte pnt[],int len,word timeout,byte usb_mode);
void	WriteIRString (byte pnt[],int len,byte usb_mode);
void	GetError (int res,char st[]);
void	log_print (char msg[],int level);
void	Hexdump_File (IRDATA *ird);
void	Hexdump_Medialon (IRDATA *ird);

void	swap_irdata (IRDATA *src,IRDATA *tar);
void	swap_word (word *pnt);
void	force_swap_word (word *pnt);
void	swap_int (int32_t *pnt);
void	SwapWordN (word *pnt);
int		GetByteorder (void);
void	SwapStatusbuffer (STATUS_BUFFER *sb);
unsigned int GetMsTime (void);
int		get_devices (char sel[],byte testmode);
int		get_detail_deviceinfo (char serno[],char devnode[],byte if_type);
void	sort_ir_devices (char selstring[]);
void	CloseIRTransLANSocket (void);
void	CloseServerSockets (SOCKET sock,SOCKET lirc, SOCKET udp,SOCKET web);
void	showDebugTiming (IRDATA *ird);


extern byte byteorder;

#define MINIMUM_SW_VERSION "2.18.04"

#ifdef LINUX

typedef int HANDLE;
typedef int OVERLAPPED;

#endif

#ifndef FTD2XX_H

typedef void* FT_HANDLE;

#endif


#define MAX_IR_DEVICES	256


#pragma pack(8)

#define IF_RS232	0
#define IF_USB		1
#define IF_LAN		2
#define IF_SERUSB	3

typedef struct {
	byte if_type;					// 0 = RS232    1 = USB		2 = LAN
	byte time_len;
	byte raw_repeat;
	byte ext_carrier;
	byte toggle_support;
	byte inst_receive_mode;
	byte advanced_lcd;
	byte io_seq_mode;
	byte io_sequence;
	byte lan_io_sequence;
	char node[20];
	FT_HANDLE usbport;
	HANDLE comport;
	HANDLE event;
	OVERLAPPED ov_read;
	OVERLAPPED ov_write;
	SOCKET socket;
	time_t last_time;
	int tcp_reconnect;
	struct sockaddr_in IPAddr[16];
	char receive_buffer[4][256];
	int	 receive_cnt[4];
	int  receive_buffer_cnt;
} IOINFO;

typedef struct {
	char name[40];
	char usb_serno[20];
	char device_node[40];
	char cap_string[80];
	char version[20];
	char lan_version[20];
	byte mac_adr[6];
	uint32_t fw_serno;
	uint32_t fw_capabilities;
	uint32_t fw_capabilities2;
	uint32_t fw_capabilities3;
	uint32_t fw_capabilities4;
	byte extended_mode;
	byte extended_mode2;
	byte extended_mode_ex[8];
	byte extended_mode_ex2[8];
	byte ext_rs232_setting[16];
	STATUS_MEMORY status_info;
	byte my_addr;
	IOINFO io;
	byte   comport_seq_number;
	HANDLE virtual_comport;
	HANDLE com_event;
	OVERLAPPED com_ov;
	byte comio_init;
	FILE *comport_file;
} DEVICEINFO;


int		GetLANParamEx (LANSTATUSBUFFER *buf,DEVICEINFO *dev);
int		WriteSysparameter (int bus,SYS_PARAMETER *sysparm);
int		SetWLANConfig (int bus,WIFI_MODE *wbuf);
int		ReadSerialStringComio (DEVICEINFO *dev,byte pnt[],int len,word timeout);
int		WriteIRStringEx (DEVICEINFO *dev,byte pnt[],int len);
int		ReadIRStringEx (DEVICEINFO *dev,byte pnt[],int len,word timeout);
int		ReadIRStringEx_ITo (DEVICEINFO *dev,byte pnt[],int len,word timeout);
int		ReadIRStringAvailable (DEVICEINFO *dev,byte pnt[],int len,word timeout);
int		WriteTransceiverEx (DEVICEINFO *dev,IRDATA *src);
void	FlushIoEx (DEVICEINFO *dev);
int		GetTransceiverVersionEx (DEVICEINFO *dev);
int		GetWiFiStatusEx (WLANBUFFER *buf,DEVICEINFO *dev);
int		GetWiFiParamEx (WLANSTATUSBUFFER *buf,DEVICEINFO *dev);
int		GetSysParameterEx (SYSPARAMBUFFER *buf,DEVICEINFO *dev);
void	FlushComEx(HANDLE fp);
void	CancelLearnEx (DEVICEINFO *dev);
int		ReadInstantReceive (DEVICEINFO *dev,byte pnt[],int len);
int		GetAvailableDataEx (DEVICEINFO *dev);
int		IRTransLanSend (DEVICEINFO *dev,IRDATA *ird);
int		OpenVirtualComPorts (void);
int		OpenVirtualComport (char Pname[],HANDLE *port);
int		WritePort (DEVICEINFO *dev,byte pnt[],int len);
int		OpenComFiles (void);
int		rcv_status_timeout (int timeout,uint32_t ip);
int		rcv_status_timeout_tcp (DEVICEINFO *dev,int timeout);
int		open_irtrans_tcp (DEVICEINFO *dev);
int		ReadLearndataLAN (DEVICEINFO *dev,byte *ird,int timeout);
void	force_swap_irdata (IRDATA *ir);


extern	DEVICEINFO IRDevices[MAX_IR_DEVICES];
extern	int device_cnt;

extern	char hexfile[256];
extern	FILE *hexfp;
extern	byte hexflag;
extern STATUS_BUFFER_N remote_statusex[MAX_IR_DEVICES];


#define TABLE_CNT	2

extern byte DispConvTable[TABLE_CNT][256];
extern char virt_comnames[50][100];
extern char virt_comfiles[50][100];
