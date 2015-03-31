%include aj_status.i
%include aj_bus.i

%{
#include "aj_helper.h"
%}

// This is the mapping for AJ_StartClient's sessionId

%apply long *OUTPUT { uint32_t *sessionId };
%apply char *OUTPUT { char *serviceName };

%include aj_helper.h
%clear uint32_t *sessionId;
%clear char *serviceName;