%include aj_target.i
%include aj_status.i
%include aj_bus.i
%include aj_util.i

#ifdef SWIGPYTHON
%include python/marshal.i
#endif

%{
 #include "aj_msg.h"
%}

%include aj_msg.h

