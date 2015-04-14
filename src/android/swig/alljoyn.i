%module alljoyn

%include "typemaps.i"

%{
#define SWIG_FILE_WITH_INIT
#include <alljoyn.h>
%}

%apply unsigned short {uint8_t}
%apply unsigned int {uint16_t}
%apply unsigned long {uint32_t}
%rename(_AJ_Message) AJ_Message;

%inline %{

uint32_t getV_bool(uint32_t* val) 
{
	return *val;
}

uint8_t getV_byte(uint8_t* val) 
{
	return *val;
}

uint16_t getV_uint16(uint16_t* val) 
{
	return *val;
}

uint32_t getV_uint32(uint32_t* val) 
{
	return *val;
}

uint64_t getV_uint64(uint64_t* val) 
{
	return *val;
}

int16_t getV_int16(int16_t* val) 
{
	return *val;
}

int32_t getV_int32(int32_t* val) 
{
	return *val;
}

int64_t getV_int64(int64_t* val) 
{
	return *val;
}

double getV_double(double* val) 
{
	return *val;
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
