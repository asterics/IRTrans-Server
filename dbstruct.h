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



#pragma pack(1)


#define COMMAND_MODE_FN		1
#define COMMAND_MODE_KEY	2

typedef struct {
	char name[80];
	word target_mask;
} ROUTING;

typedef struct {
	char name[80];
	byte addr;
} ROOMS;

typedef struct {
	char name[80];
	int number;
	word target_mask;
	word source_mask;
	int timing_start;
	int timing_end;
	int command_start;
	int command_end;
	int toggle_pos;
	byte transmitter;
	byte rcv_len;
	byte rcv_start;
} IRREMOTE;


typedef struct {
	int remote;
	byte ir_length;
	byte transmit_freq;
	byte mode;
	word pause_len[TIME_LEN_SINGLE];
	word pulse_len[TIME_LEN_SINGLE];
	byte time_cnt;
	byte ir_repeat;
	byte repeat_pause;
	byte carrier_measured;
	word link_count;
	word flash_adr;
	byte toggle_pos[4];
	byte toggle_val[4][2];
	byte toggle_num;
	byte timecount_mode;
	byte repeat_offset;
} IRTIMING;


typedef struct {
	char name[20];
	int command_link;
	int remote_link;
	int remote;
	int timing;
	int command_length;
	int pause;
	byte toggle_seq;
	byte toggle_pos;
	byte mode;
	word ir_length;
	byte data[CODE_LEN * 2 + 200];
	word ir_length_cal;
	byte data_cal[CODE_LEN * 2 + 200];
	word ir_length_offset;
	byte data_offset[CODE_LEN * 2 + 100];
	byte offset_val;
} IRCOMMAND;

typedef struct {
	char name[20];
	int command_link;
	int remote_link;
	int remote;
	int timing;
	int command_length;
	int pause;
	byte toggle_seq;
	byte toggle_pos;
	byte mode;
	byte ir_length;
	int macro_num;
	int macro_len;
} IRMACRO;



typedef struct {
	char mac_remote[80];
	char mac_command[20];
	int pause;
} MACROCOMMAND;

typedef struct {
	word id;
	word num;
	word mode;
	char remote[80];
	char command[20];
} SWITCH;

typedef union {
	int function[8];
	char name[32];
} APPFUNCTION;

typedef struct {
	int comnum;
	byte type[8];
	APPFUNCTION function;
} APPCOMMAND;

typedef struct {
	char name[20];
	char classname[50];
	char appname[100];
	char remote[80];
	int remnum;
	byte type;
	byte com_cnt;
	byte active;
	byte align;
	APPCOMMAND com[50];
} APP;

#pragma pack(8)

