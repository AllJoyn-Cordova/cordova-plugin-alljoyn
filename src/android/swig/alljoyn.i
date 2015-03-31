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
