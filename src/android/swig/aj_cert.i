%include aj_guid.i
%include aj_crypto_ecc.i
%include aj_crypto_sha2.i

%{
#include "aj_cert.h"
%}

%apply void *OUTPUT {AJ_Certificate *certificate};
%apply char *OUTPUT {uint8_t *b8};
%include aj_cert.h