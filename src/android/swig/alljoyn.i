%module alljoyn

%include "typemaps.i"

%{
#define SWIG_FILE_WITH_INIT
#include <alljoyn.h>
#include <stdio.h>
#include <stdlib.h>

static char buffer[64];
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

void setV_bool(uint32_t* v_data, const char* val)
{
	*v_data = atoi(val);
}

void setV_byte(uint8_t* v_data, const char* val) 
{
	*v_data = atoi(val);
}

void setV_uint16(uint16_t* v_data, const char* val) 
{
	*v_data = atoi(val);
}

void setV_uint32(uint32_t* v_data, const char* val) 
{
	*v_data = atoi(val);
}

void setV_uint64(uint64_t* v_data, const char* val) 
{
	*v_data = atol(val);
}

void setV_int16(int16_t* v_data, const char* val) 
{
	*v_data = atoi(val);
}

void setV_int32(int32_t* v_data, const char* val) 
{
	*v_data = atoi(val);
}

void setV_int64(int64_t* v_data, const char* val) 
{
	*v_data = atol(val);
}

void setV_double(double* v_data, const char* val) 
{
	*v_data = atof(val);
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
