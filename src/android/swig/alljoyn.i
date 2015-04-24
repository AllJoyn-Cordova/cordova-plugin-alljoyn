%module alljoyn

%include "typemaps.i"

%{
#define SWIG_FILE_WITH_INIT
#include <alljoyn.h>
#include <stdio.h>
#include <stdlib.h>

static char buffer[64];
static uint8_t u8;
static uint16_t u16;
static uint32_t u32;
static uint64_t u64;
static double d;
%}

%apply unsigned short {uint8_t}
%apply unsigned int {uint16_t}
%apply unsigned long {uint32_t}
%apply unsigned long {uint64_t}
%rename(_AJ_Message) AJ_Message;

%inline %{

const char* getV_bool(uint32_t* v_data) 
{
	sprintf(buffer, "%u", *v_data);
	return buffer;
}

const char* getV_byte(uint8_t* v_data) 
{
	sprintf(buffer, "%u", *v_data);
	return buffer;
}

const char* getV_uint16(uint16_t* v_data) 
{
	sprintf(buffer, "%u", *v_data);
	return buffer;
}

const char* getV_uint32(uint32_t* v_data) 
{
	sprintf(buffer, "%u", *v_data);
	return buffer;
}

const char* getV_uint64(uint64_t* v_data) 
{
	sprintf(buffer, "%lu", *v_data);
	return buffer;
}

const char* getV_int16(int16_t* v_data) 
{
	sprintf(buffer, "%d", *v_data);
	return buffer;
}

const char* getV_int32(int32_t* v_data) 
{
	sprintf(buffer, "%d", *v_data);
	return buffer;
}

const char* getV_int64(int64_t* v_data) 
{
	sprintf(buffer, "%ld", *v_data);
	return buffer;
}

const char* getV_double(double* v_data) 
{
	sprintf(buffer, "%f", *v_data);
	return buffer;
}

uint32_t* setV_bool(const char* val)
{
	u32 = atoi(val);
	return &u32;
}

uint8_t* setV_byte(const char* val) 
{
	u8 = atoi(val);
	return &u8;
}

uint16_t* setV_uint16(const char* val) 
{
	u16 = atoi(val);
	return &u16;
}

uint32_t* setV_uint32(const char* val) 
{
	u32 = atoi(val);
	return &u32;
}

uint64_t* setV_uint64(const char* val) 
{
	u64 = atol(val);
	return &u64;
}

int16_t* setV_int16(const char* val) 
{
	u16 = atoi(val);
	return (int16_t*)&u16;
}

int32_t* setV_int32(const char* val) 
{
	u32 = atoi(val);
	return (int32_t*)&u32;
}

int64_t* setV_int64(const char* val) 
{
	u64 = atol(val);
	return (int64_t*)&u64;
}

double* setV_double(const char* val) 
{
	d = atof(val);
	return &d;	
}

long getMsgPointer(AJ_Message* msg) 
{
	return (long)(msg);
}

%}

%include aj_target.i
%include aj_debug.i
%include aj_version.i
%include aj_status.i
%include aj_init.i
%include aj_util.i
%include aj_bus.i
%include aj_msg.i
%include aj_introspect.i
%include aj_std.i
%include aj_connect.i
%include aj_about.i
%include aj_helper.i
