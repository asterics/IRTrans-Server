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



#include "ftd2xx.h"

//typedef unsigned short word;
extern char usbcomports[20][5];
extern int usbcomcnt;


#ifdef LINUX

#define F_Open				FT_Open
#define F_OpenEx			FT_OpenEx
#define F_Write				FT_Write
#define F_ListDevices		FT_ListDevices
#define F_GetDeviceInfo		FT_GetDeviceInfo
#define F_Read				FT_Read
#define F_SetLatencyTimer	FT_SetLatencyTimer
#define F_Close				FT_Close
#define	F_GetQueueStatus	FT_GetQueueStatus
#define F_ResetDevice		FT_ResetDevice
#define F_CyclePort			FT_CyclePort	
#define F_Reload			FT_Reload
#define	F_Purge				FT_Purge
#define F_SetTimeouts		FT_SetTimeouts
#define F_SetEventNotification	FT_SetEventNotification

#endif

#ifdef WIN32
HMODULE hdll;

void GetComPorts (void);
int GetOSInfo (void);

typedef enum FT_STATUS (WINAPI *PtrToListDevices)(PVOID, PVOID, DWORD);
PtrToListDevices m_pListDevices; 
enum FT_STATUS F_ListDevices(PVOID, PVOID, DWORD);

typedef enum FT_STATUS (WINAPI *PtrToOpen)(PVOID, FT_HANDLE *); 
PtrToOpen m_pOpen; 
enum FT_STATUS F_Open(PVOID, FT_HANDLE *);

typedef enum FT_STATUS (WINAPI *PtrToGetDeviceInfo)(FT_HANDLE,FT_DEVICE *,DWORD *,char *,char *,PVOID); 
PtrToGetDeviceInfo m_pGetDeviceInfo; 
enum FT_STATUS F_GetDeviceInfo(FT_HANDLE,FT_DEVICE *,DWORD *,char *,char *,PVOID);

typedef enum FT_STATUS (WINAPI *PtrToOpenEx)(PVOID, DWORD, FT_HANDLE *); 
PtrToOpenEx m_pOpenEx; 
enum FT_STATUS F_OpenEx(PVOID, DWORD, FT_HANDLE *);

typedef enum FT_STATUS (WINAPI *PtrToRead)(FT_HANDLE, LPVOID, DWORD, LPDWORD);
PtrToRead m_pRead;
enum FT_STATUS F_Read(FT_HANDLE,LPVOID, DWORD, LPDWORD);

typedef enum FT_STATUS (WINAPI *PtrToClose)(FT_HANDLE);
PtrToClose m_pClose;
enum FT_STATUS F_Close(FT_HANDLE);

typedef enum FT_STATUS (WINAPI *PtrToGetQueueStatus)(FT_HANDLE, LPDWORD);
PtrToGetQueueStatus m_pGetQueueStatus;
enum FT_STATUS F_GetQueueStatus(FT_HANDLE,LPDWORD);

typedef enum FT_STATUS (WINAPI *PtrToWrite)(FT_HANDLE, LPVOID, DWORD, LPDWORD);
PtrToWrite m_pWrite;
enum FT_STATUS F_Write(FT_HANDLE,LPVOID, DWORD, LPDWORD);

typedef enum FT_STATUS (WINAPI *PtrToResetDevice)(FT_HANDLE);
PtrToResetDevice m_pResetDevice;
enum FT_STATUS F_ResetDevice(FT_HANDLE);

typedef enum FT_STATUS (WINAPI *PtrToCyclePort)(FT_HANDLE);
PtrToCyclePort m_pCyclePort;
enum FT_STATUS F_CyclePort(FT_HANDLE);

typedef enum FT_STATUS (WINAPI *PtrToReload)(WORD, WORD);
PtrToReload m_pReload;
enum FT_STATUS F_Reload(WORD,WORD);

typedef enum FT_STATUS (WINAPI *PtrToPurge)(FT_HANDLE, ULONG);
PtrToPurge m_pPurge;
enum FT_STATUS F_Purge(FT_HANDLE,ULONG);

typedef enum FT_STATUS (WINAPI *PtrToSetTimeouts)(FT_HANDLE, ULONG, ULONG);
PtrToSetTimeouts m_pSetTimeouts;
enum FT_STATUS F_SetTimeouts(FT_HANDLE,ULONG, ULONG);

typedef enum FT_STATUS (WINAPI *PtrToSetEvent)(FT_HANDLE, DWORD, LPVOID);
PtrToSetEvent m_pSetEvent;
enum FT_STATUS F_SetEventNotification(FT_HANDLE,DWORD,LPVOID);

typedef enum FT_STATUS (WINAPI *PtrToSetLatencyTimer)(FT_HANDLE, UCHAR);
PtrToSetLatencyTimer m_pSetLatencyTimer;
enum FT_STATUS F_SetLatencyTimer(FT_HANDLE,UCHAR);

#endif

int		LoadUSBLibrary (void);
void	cleanup_exit (void);
void	break_signal (int sig);
int		OpenUSBPort (void);
int		WriteUSBString (byte pnt[],int len);
int		ReadUSBString (byte pnt[],int len,long timeout);
void	FlushUSB (void);
void	SetUSBEvent (PVOID,DWORD);
void	SetUSBEventEx (DEVICEINFO *dev,DWORD mask);
void	FlushUSBEx (FT_HANDLE hndl);
void	WriteUSBStringEx (DEVICEINFO *dev,byte pnt[],int len);
int		ReadUSBStringEx (DEVICEINFO *dev,byte pnt[],int len,word timeout);
int		GetUSBAvailableEx (DEVICEINFO *dev);
int		ReadUSBStringEx_ITo (DEVICEINFO *dev,byte pnt[],int len,word timeout);
int		ReadUSBStringAvailable (DEVICEINFO *dev,byte pnt[],int len,word timeout);
