%include aj_target.i
%include aj_msg.i

%ignore AJ_DbgLevel;
%ignore dbgALL;

%{
#include "aj_debug.h"
%}

%include aj_debug.h