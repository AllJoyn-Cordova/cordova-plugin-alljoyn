%include aj_target.i
%include aj_status.i
%include aj_bus.i
%include aj_msg.i

%{
#include "aj_introspect.h"
%}

uint32_t AJ_DESCRIPTION_ID(uint32_t,uint32_t,uint32_t,uint32_t);
uint32_t AJ_DESC_ID_FROM_MSG_ID(uint32_t, uint32_t);
uint32_t AJ_DESC_ID_FROM_PROP_ID(uint32_t);
uint32_t AJ_DESC_ID_FROM_OBJ_INDEX(uint32_t);
uint32_t AJ_DESC_ID_FROM_INTERFACE_INDEX(uint32_t, uint32_t);
uint32_t AJ_BUS_MESSAGE_ID(uint32_t, uint32_t, uint32_t);
uint32_t AJ_APP_MESSAGE_ID(uint32_t, uint32_t, uint32_t);
uint32_t AJ_PRX_MESSAGE_ID(uint32_t, uint32_t, uint32_t);
uint32_t AJ_BUS_PROPERTY_ID(uint32_t, uint32_t, uint32_t);
uint32_t AJ_APP_PROPERTY_ID(uint32_t, uint32_t, uint32_t);
uint32_t AJ_PRX_PROPERTY_ID(uint32_t, uint32_t, uint32_t);
uint32_t AJ_REPLY_ID(uint32_t);

%include aj_introspect.h