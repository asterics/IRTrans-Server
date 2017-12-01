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

extern	NETWORKCLIENT sockinfo[CLIENT_COUNT];

int		DBReadCommandFile (char remote[]);
int		DBStoreRemote (FILE *fp,char newremote[]);
int		DBStoreTimings (FILE *fp,char remote[]);
int		DBStoreCommands (FILE *fp,char remote[]);
int		DBStoreCCFCode (char cd[]);
void	DBShowStatus (void);
void    ReadRoutingTable (void);
void	ReadSwitches (void);
void	ReadAppConfig (void);
void	WriteSwitches (void);
int		FindSwitch (word id,word num,char *rem,char *com,word *mode);
int		StoreSwitch (word id,word num,char *rem,char *com,word mode);
int		DBStoreRooms (FILE *fp);
void	DBStoreRouting (FILE *fp,char section[],ROUTING **pnt,int *cnt);
void	FreeDatabaseMemory (void);
void	ReadCalibrateData (byte *pnt,byte *pntcal);

FILE	*DBOpenFile (char remote[],char mode[]);
char	*DBReadString (char ln[],FILE *fp,int *fpos);
char	*DBFindSection (FILE *fp,char section[],char data[],char end[],int *fpos);
int		StoreIRTiming (IRTIMING *irp,char data[],int toggle);
void	ConvertLcase (char *pnt,int len);
int		GetFunctionCode (byte type,char *com);
int		GetKeyCode (char *com);
void	ConvertIRDARAW (char data[]);


int		FindLineSection (char ln[],char section[]);
void	StorePulseTiming (IRTIMING *irp,char cmd[],char data[]);
void	StoreSingleTimingPulse (IRTIMING *irp,char cmd[],char data[]);
void	StoreSingleTimingPause (IRTIMING *irp,char cmd[],char data[]);

FILE	*ASCIIOpenRemote (char name[],NETWORKCLIENT *client);
void	ASCIITimingSample (FILE *fp,NETWORKCLIENT *client);
int		ASCIIStoreCommand (FILE *fp,IRDATA *ird,char name[],int timing,int seq_number);
int		ASCIIStoreCCF (int client,char name[],char ccf[]);
int		ASCIIStoreRS232 (int client,char name[],char rs232[]);
int		ASCIIStoreLink (int client,char name[],char link[]);
int		FormatCCF (char *ccf);
int		ASCIIFindToggleSeq (FILE *fp,IRDATA *ird,char name[]);
int		ASCIIStoreTiming (FILE *fp,IRDATA *ird,NETWORKLEARNSTAT *stat);
int		ASCIIStoreRAW (FILE *fp,IRRAW *ird,char name[]);
void	ASCIIStoreTimingParam (FILE *fp,IRDATA *ird,int timing);
int		ASCIIFindCommand (FILE *fp,char name[],NETWORKCLIENT *client);
int		CompareTiming (IRDATA *ird,IRTIMING *irt);
int		ASCIIFindTiming (FILE *fp,IRDATA *ird);
void	GetRemoteAddressMask (int num);
void	CopyStateInfo (byte *state,int cmd);
byte	CopyToggleData (int tim,int cmd);
void	CopyRepeatOffset (byte repeat_offset,int cmd);
int		GetRepeatOffset (IRDATA *ird);

void	FillInTiming (IRDATA *ir,IRTIMING *tim);
int		DBFindCommandName (byte command[],char remote[],char name[],byte address,int *remote_num,int *command_num,word *command_num_rel,int start);
int		DBFindRemoteCommand (char remote[],char command[],int *cmd_num,int *rem_num);
int		DBFindRemoteMacro (char remote[],char command[],int cmd_array[],word pause_array[]);
int		DBFindRemoteCommandEx(char remote[],char command[],IRDATA *ir,byte cal,byte toggle);
int		DBGetIRCode (int cmd_num,IRDATA *ir,int idx,int *mac_len,int *mac_pause,int *rpt_len,byte calflag,byte toggle);
int		DBGetRepeatCode (int cmd_num,IRDATA *ir,byte calflag,byte toggle);
int		DBFindCommand (char command[],int32_t *remote);
int		DBFindRemote (char remote[]);
void	GetNumericCode (char command[],char numeric[],char rem[],char com[]);
void	GetRemoteDatabase (REMOTEBUFFER *buf,int offset);
int		GetCommandDatabase (COMMANDBUFFER *buf,char remote[],int offset);
int		DBReferenceLinks (void);

#define	RS232_IRCOMMAND -10
#define	LINK_IRCOMMAND  -20
#define	LINK_ERROR		-21

