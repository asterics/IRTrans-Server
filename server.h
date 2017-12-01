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



int		InitServer (char dev[]);
int		RunServer ();
void	SwapNetworkheader (NETWORKSTATUS *ns);
void	SwapNetworkcommand (NETWORKCOMMAND *nc);
void	SwapNetworkstatus (void *pnt);
int		GetDeviceStatus (STATUSBUFFER *buf);
int		GetHotcode (char rem[],char com[],byte data[]);
void	PutNetworkStatus (int res,char msg[],STATUSBUFFER *buf);
void	StoreTransItem (TRANSLATECOMMAND *tr);
int		StoreDbItem (IRDBCOMMAND *db);
int		FileTransData (char nm[],byte dbtype,byte filemode);
int		LoadTranslation (TRANSLATEBUFFER *tb,char nm[],word offset);
int		LoadIRDB (IRDBBUFFER *db,char nm[],word offset);
int		SetFlashdataEx (byte bus,int iradr,STATUSBUFFER *stat);
int		SetIRDBEx (byte bus,int iradr,STATUSBUFFER *stat);
unsigned int GetMsTime (void);
void	udp_relay (char rem[],char com[],int adr);
void	InitMediacenter (void);
int		GetDeviceData (int cmd_num,DATABUFFER *dat);
int		ReadIRTransDirectory (char filetype[],REMOTEBUFFER *stat,int start,byte statustype);
void	send_forward (int client,char remote[],char command[]);
void	NetworkClientMessage (char msg[],int num);
void	HandleHID (int rem_num,int com_num,char rname[],char xcode[]);


extern byte status_changed;
extern unsigned int netmask[32];
extern unsigned int netip[32];
extern byte netcount;
