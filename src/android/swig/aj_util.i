%include aj_target.i
%include aj_status.i

%{
#include "aj_util.h"
%}

%apply char *OUTPUT {char* str};
char* AJ_GetLine(char* str, size_t num, void* fp);
%clear char* str;
%apply char *OUTPUT {char* buf};
char* AJ_GetCmdLine(char* buf, size_t num);
%clear char* buf;

%ignore AJ_SuspendWifi(uint32_t msec);

%include aj_util.h
