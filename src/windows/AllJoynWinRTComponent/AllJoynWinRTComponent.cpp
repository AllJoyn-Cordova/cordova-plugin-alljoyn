#include "pch.h"

#include "AllJoynWinRTComponent.h"
#include "aj_init.h"
#include "aj_util.h"
#include "aj_target_util.h"
#include "aj_helper.h"
#include "aj_msg.h"
#include "aj_connect.h"
#include "aj_debug.h"
#include <ppltasks.h>
#include "aj_msg_priv.h"

using namespace concurrency;

using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Foundation;


#define AJ_BUS_ID_FLAG   0x00  /**< Identifies that a message belongs to the set of builtin bus object messages */
#define AJ_APP_ID_FLAG   0x01  /**< Identifies that a message belongs to the set of objects implemented by the application */
#define AJ_PRX_ID_FLAG   0x02  /**< Identifies that a message belongs to the set of objects implemented by remote peers */


#define AJ_STRUCT_CLOSE          ')'
#define AJ_DICT_ENTRY_CLOSE      '}'


#define AJ_SCALAR    0x10
#define AJ_CONTAINER 0x20
#define AJ_STRING    0x40
#define AJ_VARIANT   0x80


/**
* Characterizes the various argument types
*/
static const uint8_t TypeFlags[] = {
	0x08 | AJ_CONTAINER,  /* AJ_ARG_STRUCT            '('  */
	0,                    /*                          ')'  */
	0x04 | AJ_CONTAINER,  /* AJ_ARG_ARRAY             'a'  */
	0x04 | AJ_SCALAR,     /* AJ_ARG_BOOLEAN           'b'  */
	0,
	0x08 | AJ_SCALAR,     /* AJ_ARG_DOUBLE            'd'  */
	0,
	0,
	0x01 | AJ_STRING,     /* AJ_ARG_SIGNATURE         'g'  */
	0x04 | AJ_SCALAR,     /* AJ_ARG_HANDLE            'h'  */
	0x04 | AJ_SCALAR,     /* AJ_ARG_INT32             'i'  */
	0,
	0,
	0,
	0,
	0x02 | AJ_SCALAR,     /* AJ_ARG_INT16             'n'  */
	0x04 | AJ_STRING,     /* AJ_ARG_OBJ_PATH          'o'  */
	0,
	0x02 | AJ_SCALAR,     /* AJ_ARG_UINT16            'q'  */
	0,
	0x04 | AJ_STRING,     /* AJ_ARG_STRING            's'  */
	0x08 | AJ_SCALAR,     /* AJ_ARG_UINT64            't'  */
	0x04 | AJ_SCALAR,     /* AJ_ARG_UINT32            'u'  */
	0x01 | AJ_VARIANT,    /* AJ_ARG_VARIANT           'v'  */
	0,
	0x08 | AJ_SCALAR,     /* AJ_ARG_INT64             'x'  */
	0x01 | AJ_SCALAR,     /* AJ_ARG_BYTE              'y'  */
	0,
	0x08 | AJ_CONTAINER,  /* AJ_ARG_DICT_ENTRY        '{'  */
	0,
	0                     /*                          '}'  */
};


/**
* This macro makes sure that the signature contains valid characters
* in the TypeFlags array. If the index passed is below ascii 'a'
* or above ascii '}' and not ascii '(' or ')' then the signature is invalid.
* Below is the macro broken into smaller chunks:
*
* ((t) == '(' || (t) == ')') ? (t) - '('       --> If the value is ) or (, get the value in TypeFlags
* :
* (((t) < 'a' || (t) > '}') ? '}' + 2 - 'a'    --> The value is too high or too low, return TypeFlags[30] (0)
* :
* (t) + 2 - 'a'                                --> The value is valid, get the value in TypeFlags
*/
#define TYPE_FLAG(t) TypeFlags[((t) == '(' || (t) == ')') ? (t) - '(' : (((t) < 'a' || (t) > '}') ? '}' + 2 - 'a' : (t) + 2 - 'a') ]

/*
* For scalar types returns the size of the type
*/
#define SizeOfType(typeId) (TYPE_FLAG(typeId) & 0xF)


static ::AJ_Object* _s_cachedLocalObjects = NULL;
static ::AJ_Object* _s_cachedProxyObjects = NULL;
static const Array<AllJoynWinRTComponent::AJ_Object^>^ s_cachedLocalObjects;
static const Array<AllJoynWinRTComponent::AJ_Object^>^ s_cachedProxyObjects;
static ::AJ_SessionOpts* _s_cachedSessionOpts = NULL;


AllJoynWinRTComponent::AllJoyn::AllJoyn()
{
}


AllJoynWinRTComponent::AllJoyn::~AllJoyn()
{
}


void AllJoynWinRTComponent::AllJoyn::AJ_Initialize()
{
	::AJ_Initialize();
}


void AllJoynWinRTComponent::AllJoyn::ReleaseObjects(::AJ_Object* _objects, const Array<AJ_Object^>^ objects)
{
	if (_objects == NULL)
	{
		return;
	}

	int nObjects = objects->Length;

	for (int j = 0; j < nObjects; j++)
	{
		if (_objects[j].path)
		{
			// Free path
			SAFE_DEL_ARRAY(_objects[j].path);

			// Free interfaces
			int nInterfaces = objects[j]->interfaces->Size;

			for (int k = 0; k < nInterfaces; k++)
			{
				if (_objects[j].interfaces[k])
				{
					int nEntries = objects[j]->interfaces->GetAt(k)->Size;

					for (int m = 0; m < nEntries; m++)
					{
						if (_objects[j].interfaces[k][m])
						{
							delete[] _objects[j].interfaces[k][m];
						}
					}

					delete[] _objects[j].interfaces[k];
				}
			}

			delete[] _objects[j].interfaces;
		}
	}

	SAFE_DEL_ARRAY(_objects);
}


void AllJoynWinRTComponent::AllJoyn::AJ_ReleaseObjects()
{
	ReleaseObjects(_s_cachedLocalObjects, s_cachedLocalObjects);
	ReleaseObjects(_s_cachedProxyObjects, s_cachedProxyObjects);
}


::AJ_Object* AllJoynWinRTComponent::AllJoyn::RegisterObjects(const Array<AJ_Object^>^ objects)
{
	::AJ_Object* _objects = NULL;

	if (objects->Length != 0)
	{
		_objects = new ::AJ_Object[objects->Length];
		ZeroMemory(_objects, sizeof(_objects));

		for (int i = 0; i < objects->Length; i++)
		{
			ZeroMemory(&_objects[i], sizeof(_objects[i]));

			if (objects[i])
			{
				// Copy path
				PLSTODYNMBS(objects[i]->path, _path);
				_objects[i].path = _path;

				// Copy interface
				int nInterfaces = objects[i]->interfaces->Size;
				::AJ_InterfaceDescription* _interfaces = new ::AJ_InterfaceDescription[nInterfaces];
				ZeroMemory(_interfaces, sizeof(_interfaces));
				char*** interfaces = new char**[nInterfaces];
				ZeroMemory(interfaces, sizeof(interfaces));
				for (int j = 0; j < nInterfaces; j++)
				{
					_interfaces[j] = NULL;
					if (objects[i]->interfaces->GetAt(j))
					{
						int nEntries = objects[i]->interfaces->GetAt(j)->Size;
						interfaces[j] = new char*[nEntries + 1];
						ZeroMemory(interfaces[j], sizeof(interfaces[j]));
						for (int k = 0; k < nEntries; k++)
						{
							char* entry = NULL;
							if (objects[i]->interfaces->GetAt(j)->GetAt(k))
							{
								PLSTODYNMBS(objects[i]->interfaces->GetAt(j)->GetAt(k), _entry);
								entry = _entry;
							}
							interfaces[j][k] = entry;
						}
						_interfaces[j] = interfaces[j];
						_interfaces[nEntries] = NULL;
					}
				}
				_objects[i].interfaces = _interfaces;

				// Copy flag
				_objects[i].flags = objects[i]->flags;
			}
		}
	}

	return _objects;
}


void AllJoynWinRTComponent::AllJoyn::AJ_PrintXML(const Array<AJ_Object^>^ objects)
{
	::AJ_Object* _objects = AllJoynWinRTComponent::AllJoyn::RegisterObjects(objects);
#if _DEBUG
	::AJ_PrintXML(_objects);
#endif
	ReleaseObjects(_objects, objects);
}


void AllJoynWinRTComponent::AllJoyn::AJ_RegisterObjects(const Array<AJ_Object^>^ localObjects, const Array<AJ_Object^>^ proxyObjects)
{
	// Free the old objects first
	if (_s_cachedLocalObjects)
	{
		ReleaseObjects(_s_cachedLocalObjects, s_cachedLocalObjects);
		ReleaseObjects(_s_cachedProxyObjects, s_cachedProxyObjects);
	}

	// Cache the objects
	_s_cachedLocalObjects = AllJoynWinRTComponent::AllJoyn::RegisterObjects(localObjects);
	_s_cachedProxyObjects = AllJoynWinRTComponent::AllJoyn::RegisterObjects(proxyObjects);
	s_cachedLocalObjects = localObjects;
	s_cachedProxyObjects = proxyObjects;

	// Register the objects
	::AJ_RegisterObjects(_s_cachedLocalObjects, _s_cachedProxyObjects);
}


IAsyncOperation<AllJoynWinRTComponent::AJ_Session>^ AllJoynWinRTComponent::AllJoyn::AJ_StartClient
(
	AllJoynWinRTComponent::AJ_BusAttachment^ bus,
	String^ daemonName,
	uint32_t timeout,
	uint8_t connected,
	String^ name,
	uint16_t port,
	AllJoynWinRTComponent::AJ_SessionOpts^ opts)
{
	return create_async([bus, daemonName, timeout, connected, name, port, opts]() -> AllJoynWinRTComponent::AJ_Session
	{
		::AJ_BusAttachment* _bus = new ::AJ_BusAttachment();
		::AJ_SessionOpts* _opts = NULL;
		PLSTOMBS(daemonName, mbsDaemonName);
		char* _daemonName = (daemonName == nullptr ? NULL : mbsDaemonName);
		PLSTOMBS(name, mbsName);
		char* _name = (name == nullptr ? NULL : mbsName);

		if (opts)
		{
			SAFE_DEL(_s_cachedSessionOpts);
			_opts = new ::AJ_SessionOpts();
			ZeroMemory(_opts, sizeof(_opts));

			STRUCT_COPY(opts, isMultipoint);
			STRUCT_COPY(opts, proximity);
			STRUCT_COPY(opts, traffic);
			STRUCT_COPY(opts, transports);

			_s_cachedSessionOpts = _opts;
		}

		uint32_t _sessionId;
		::AJ_Status _status = ::AJ_StartClient(_bus, _daemonName, timeout, connected, _name, port, &_sessionId, _opts);
		bus->_bus = _bus;
		AJ_Session retObj;
		retObj.sessionId = _sessionId;
		retObj.status = static_cast<uint8_t>(_status);

		return retObj;
	});
}


IAsyncOperation<AllJoynWinRTComponent::AJ_Session>^ AllJoynWinRTComponent::AllJoyn::AJ_StartClientByName
(
AllJoynWinRTComponent::AJ_BusAttachment^ bus,
String^ daemonName,
uint32_t timeout,
uint8_t connected,
String^ name,
uint16_t port,
AllJoynWinRTComponent::AJ_SessionOpts^ opts)
{
	return create_async([bus, daemonName, timeout, connected, name, port, opts]() -> AllJoynWinRTComponent::AJ_Session
	{
		::AJ_BusAttachment* _bus = new ::AJ_BusAttachment();
		::AJ_SessionOpts* _opts = NULL;

		PLSTOMBS(daemonName, mbsDaemonName);
		char* _daemonName = (daemonName == nullptr ? NULL : mbsDaemonName);
		PLSTOMBS(name, mbsName);
		char* _name = (name == nullptr ? NULL : mbsName);
		char _fullName[AJ_MAX_SERVICE_NAME_SIZE];

		if (opts)
		{
			SAFE_DEL(_s_cachedSessionOpts);
			_opts = new ::AJ_SessionOpts();
			ZeroMemory(_opts, sizeof(_opts));

			STRUCT_COPY(opts, isMultipoint);
			STRUCT_COPY(opts, proximity);
			STRUCT_COPY(opts, traffic);
			STRUCT_COPY(opts, transports);

			_s_cachedSessionOpts = _opts;
		}

		uint32_t _sessionId;
		::AJ_Status _status = ::AJ_StartClientByName(_bus, _daemonName, timeout, connected, _name, port, &_sessionId, _opts, _fullName);
		bus->_bus = _bus;
		AJ_Session retObj;
		retObj.sessionId = _sessionId;
		retObj.status = static_cast<uint8_t>(_status);

		if (_fullName)
		{
			MBSTOWCS(_fullName, wcsFullName);
			retObj.fullName = ref new String(wcsFullName);
		}
		else
		{
			retObj.fullName = nullptr;
		}

		return retObj;
	});
}

IAsyncOperation<AllJoynWinRTComponent::AJ_Status>^ AllJoynWinRTComponent::AllJoyn::AJ_StartService
(
AllJoynWinRTComponent::AJ_BusAttachment^ bus,
String^ daemonName,
uint32_t timeout,
uint8_t connected,
uint16_t port,
String^ name,
uint32_t flags,
AllJoynWinRTComponent::AJ_SessionOpts^ opts)
{
	return create_async([bus, daemonName, timeout, connected, port, name, flags, opts]() -> AllJoynWinRTComponent::AJ_Status
	{
		::AJ_BusAttachment* _bus = new ::AJ_BusAttachment();
		::AJ_SessionOpts* _opts = NULL;

		PLSTOMBS(daemonName, _daemonName);
		PLSTOMBS(name, _name);

		if (opts)
		{
			SAFE_DEL(_s_cachedSessionOpts);
			_opts = new ::AJ_SessionOpts();
			ZeroMemory(_opts, sizeof(_opts));

			STRUCT_COPY(opts, isMultipoint);
			STRUCT_COPY(opts, proximity);
			STRUCT_COPY(opts, traffic);
			STRUCT_COPY(opts, transports);

			_s_cachedSessionOpts = _opts;
		}

		::AJ_Status _status = ::AJ_StartService(_bus, _daemonName, timeout, connected, port, _name, flags, _opts);
		bus->_bus = _bus;

		return (static_cast<AJ_Status>(_status));
	});
}

AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_MarshalMethodCall(AJ_BusAttachment^ bus, AJ_Message^ msg, uint32_t msgId, String^ destination, AJ_SessionId sessionId, uint8_t flags, uint32_t timeout)
{
	char* _destination = new char[MAX_STR_LENGTH];
	wcstombs(_destination, destination->Data(), MAX_STR_LENGTH);

	::AJ_Status _status = ::AJ_MarshalMethodCall(bus->_bus, &msg->_msg, msgId, _destination, sessionId, flags, timeout);

	return (static_cast<AJ_Status>(_status));
}


AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_MarshalArg(AJ_Message^ msg, String^ signature, String^ arg)
{
	::AJ_Status _status = ::AJ_Status::AJ_ERR_INVALID;

	PLSTOMBS(signature, _signature);

	uint8_t u8;
	uint16_t u16;
	uint32_t u32;
	uint64_t u64;
	void* val = NULL;
	uint8_t typeId = (uint8_t)_signature[0];

	if (AJ_IsScalarType(typeId))
	{
		if (SizeOfType(typeId) == 8)
		{
			u64 = _wtoi64(arg->Data());
			val = &u64;
		}
		else if (SizeOfType(typeId) == 4)
		{
			if (!wcscmp(arg->Data(), L"true") || !wcscmp(arg->Data(), L"TRUE"))
			{
				u32 = 1;
			}
			else if (!wcscmp(arg->Data(), L"false") || !wcscmp(arg->Data(), L"FALSE"))
			{
				u32 = 0;
			}
			else
			{
				u32 = _wtoi(arg->Data());
			}

			val = &u32;
		}
		else if (SizeOfType(typeId) == 2)
		{
			u16 = _wtoi(arg->Data());
			val = &u16;
		}
		else
		{
			u8 = _wtoi(arg->Data());
			val = &u8;
		}
	}
	else
	{
		PLSTOMBS(arg, str);
		val = &str;
	}

	if (val)
	{
		::AJ_Arg _arg;
		_arg.typeId = typeId;
		_arg.flags = 0;
		_arg.len = 0;
		_arg.val.v_data = (void*)val;
		_arg.sigPtr = NULL;
		_arg.container = NULL;
		_status = ::AJ_MarshalArg(&msg->_msg, &_arg);

		if (_status != AJ_OK)
		{
			AJ_ErrPrintf(("AJ_MarshalArgs(): status=%s\n", AJ_StatusText(_status)));
		}
	}
	else
	{
		_status = ::AJ_Status::AJ_ERR_INVALID;
	}

	return (static_cast<AJ_Status>(_status));
}


AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_DeliverMsg(AJ_Message^ msg)
{
	::AJ_Status _status = ::AJ_DeliverMsg(&msg->_msg);

	return (static_cast<AJ_Status>(_status));
}


AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_CloseMsg(AJ_Message^ msg)
{
	::AJ_Status _status = ::AJ_CloseMsg(&msg->_msg);

	return (static_cast<AJ_Status>(_status));
}


IAsyncOperation<AllJoynWinRTComponent::AJ_Status>^ AllJoynWinRTComponent::AllJoyn::AJ_UnmarshalMsg(AJ_BusAttachment^ bus, AJ_Message^ msg, uint32_t timeout)
{
	return create_async([bus, msg, timeout]() -> AllJoynWinRTComponent::AJ_Status
	{
		::AJ_Status _status = ::AJ_UnmarshalMsg(bus->_bus, &msg->_msg, timeout);

		return (static_cast<AJ_Status>(_status));
	});
}


AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_UnmarshalArg(AJ_Message^ msg, AJ_Arg^ arg)
{
	::AJ_Status _status = ::AJ_UnmarshalArg(&msg->_msg, &arg->_arg);

	if (_status == ::AJ_Status::AJ_OK)
	{
		AllJoynWinRTComponent::_AJ_Arg _val;
		_val.v_byte = *arg->_arg.val.v_byte;
		_val.v_int16 = *arg->_arg.val.v_int16;
		_val.v_uint16 = *arg->_arg.val.v_uint16;
		_val.v_bool = *arg->_arg.val.v_bool;
		_val.v_uint32 = *arg->_arg.val.v_uint32;
		_val.v_int32 = *arg->_arg.val.v_int32;
		_val.v_int64 = *arg->_arg.val.v_int64;
		_val.v_uint64 = *arg->_arg.val.v_uint64;
		_val.v_double = *arg->_arg.val.v_double;
		MBSTOWCS(arg->_arg.val.v_string, v_string);
		_val.v_string = ref new String(v_string);
		MBSTOWCS(arg->_arg.val.v_objPath, v_objPath);
		_val.v_objPath = ref new String(v_objPath);
		MBSTOWCS(arg->_arg.val.v_signature, v_signature);
		_val.v_signature = ref new String(v_signature);
		arg->val = _val;
	}

	return (static_cast<AJ_Status>(_status));
}


AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_BusHandleBusMessage(AJ_Message^ msg)
{
	::AJ_Status _status = ::AJ_BusHandleBusMessage(&msg->_msg);

	return (static_cast<AJ_Status>(_status));
}


AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_BusFindAdvertisedName(AJ_BusAttachment^ bus, String^ namePrefix, uint8_t op)
{
	PLSTOMBS(namePrefix, _namePrefix);
	::AJ_Status _status = ::AJ_BusFindAdvertisedName(bus->_bus, _namePrefix, op);

	return (static_cast<AJ_Status>(_status));
}


AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_FindBusAndConnect(AJ_BusAttachment^ bus, String^ serviceName, uint32_t timeout)
{
	SAFE_DEL(bus->_bus);
	::AJ_BusAttachment* _bus = new ::AJ_BusAttachment();
	PLSTOMBS(serviceName, _serviceName);
	::AJ_Status _status = ::AJ_FindBusAndConnect(_bus, _serviceName, timeout);
	bus->_bus = _bus;

	return (static_cast<AJ_Status>(_status));
}


AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_BusSetSignalRule(AJ_BusAttachment^ bus, String^ ruleString, uint8_t rule)
{
	PLSTOMBS(ruleString, _ruleString);
	::AJ_Status _status = ::AJ_BusSetSignalRule(bus->_bus, _ruleString, rule);

	return (static_cast<AJ_Status>(_status));
}


void AllJoynWinRTComponent::AllJoyn::AJ_Disconnect(AJ_BusAttachment^ bus)
{
	::AJ_Disconnect(bus->_bus);
}


AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_BusJoinSession(AJ_BusAttachment^ bus, String^ sessionHost, uint16_t port, AJ_SessionOpts^ opts)
{
	PLSTOMBS(sessionHost, _sessionHost);
	::AJ_SessionOpts* _opts = NULL;

	if (opts)
	{
		SAFE_DEL(_s_cachedSessionOpts);
		_opts = new ::AJ_SessionOpts();
		ZeroMemory(_opts, sizeof(_opts));

		STRUCT_COPY(opts, isMultipoint);
		STRUCT_COPY(opts, proximity);
		STRUCT_COPY(opts, traffic);
		STRUCT_COPY(opts, transports);

		_s_cachedSessionOpts = _opts;
	}

	::AJ_Status _status = ::AJ_BusJoinSession(bus->_bus, _sessionHost, port, _opts);

	return (static_cast<AJ_Status>(_status));
}


AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_BusLeaveSession(AJ_BusAttachment^ bus, uint32_t sessionId)
{
	::AJ_Status _status = ::AJ_BusLeaveSession(bus->_bus, sessionId);

	return (static_cast<AJ_Status>(_status));
}


AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_MarshalSignal(AJ_BusAttachment^ bus, AJ_Message^ msg, uint32_t msgId, String^ destination, AJ_SessionId sessionId, uint8_t flags, uint32_t ttl)
{
	char* _destination = new char[MAX_STR_LENGTH];
	wcstombs(_destination, destination->Data(), MAX_STR_LENGTH);

	::AJ_Status _status = ::AJ_MarshalSignal(bus->_bus, &msg->_msg, msgId, _destination, sessionId, flags, ttl);

	return (static_cast<AJ_Status>(_status));
}


// Pointer to Javascript function
AllJoynWinRTComponent::AJ_AuthPwdFunc^ pwdCallback;

void AllJoynWinRTComponent::AllJoyn::AJ_BusSetPasswordCallback(AJ_BusAttachment^ bus, AJ_AuthPwdFunc^ pwdCallback)
{
	::pwdCallback = pwdCallback;
	::AJ_BusSetPasswordCallback(bus->_bus, AllJoynWinRTComponent::AllJoyn::PasswordCallback);
}


// Pointer to Javascript function
AllJoynWinRTComponent::AJ_PeerAuthenticateCallback^ authCallback;
char _peerBusName[MAX_STR_LENGTH];

AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_BusAuthenticatePeer(AJ_BusAttachment^ bus, String^ peerBusName, AJ_PeerAuthenticateCallback^ pwdCallback)
{
	::authCallback = pwdCallback;
	wcstombs(_peerBusName, peerBusName->Data(), MAX_STR_LENGTH);
	AJ_Status status = static_cast<AJ_Status>(::AJ_BusAuthenticatePeer(bus->_bus, _peerBusName, AllJoynWinRTComponent::AllJoyn::AuthCallback, NULL));
	
	return status;
}


uint32_t AllJoynWinRTComponent::AllJoyn::PasswordCallback(uint8_t* buffer, uint32_t bufLen)
{
	String^ password = pwdCallback();
	PLSTOMBS(password, _password);
	int strLen = strlen(_password);
	memcpy(buffer, _password, strlen(_password));
	buffer[strLen] = '\0';

	return password->Length();
}


void AllJoynWinRTComponent::AllJoyn::AuthCallback(const void* context, ::AJ_Status status)
{
	authCallback(status);
}


AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_BusReplyAcceptSession(AllJoynWinRTComponent::AJ_Message^ msg, uint32_t accept)
{
	return static_cast<AllJoynWinRTComponent::AJ_Status>(::AJ_BusReplyAcceptSession(&msg->_msg, accept));
}


AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_MarshalReplyMsg(AllJoynWinRTComponent::AJ_Message^ methodCall, AllJoynWinRTComponent::AJ_Message^ reply)
{
	::AJ_Status status = ::AJ_MarshalReplyMsg(&methodCall->_msg, &reply->_msg);

	return static_cast<AllJoynWinRTComponent::AJ_Status>(status);
}

AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_MarshalErrorMsg(AllJoynWinRTComponent::AJ_Message^ methodCall, AllJoynWinRTComponent::AJ_Message^ reply, String^ error)
{
	PLSTOMBS(error, _error);
	::AJ_Status status = ::AJ_MarshalErrorMsg(&methodCall->_msg, &reply->_msg, _error);

	return static_cast<AllJoynWinRTComponent::AJ_Status>(status);
}

AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_MarshalArg(AllJoynWinRTComponent::AJ_Message^ msg, AllJoynWinRTComponent::AJ_Arg^ arg)
{
	return static_cast<AllJoynWinRTComponent::AJ_Status>(::AJ_MarshalArg(&msg->_msg, &arg->_arg));
}


void AllJoynWinRTComponent::AllJoyn::AJ_InitArg(AllJoynWinRTComponent::AJ_Arg^ arg, uint8_t typeId, uint8_t flags, Object^ val, size_t len)
{
	if (typeId == AJ_ARG_STRING)
	{
		String^ valStr = (String^)val;
		PLSTOMBS(valStr, _val);
		::AJ_InitArg(&arg->_arg, typeId, flags, _val, len);
	}
	else
	{
		Array<uint8_t>^ valArray = (Array<uint8_t>^)val;
		uint8_t* _val = valArray->Data;
		::AJ_InitArg(&arg->_arg, typeId, flags, _val, len);
	}
}


AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_MarshalPropertyArgs(AllJoynWinRTComponent::AJ_Message^ msg, uint32_t propId)
{
	return static_cast<AllJoynWinRTComponent::AJ_Status>(::AJ_MarshalPropertyArgs(&msg->_msg, propId));
}


// Pointer to Javascript function
AllJoynWinRTComponent::AJ_BusPropGetCallback^ busPropGetCallback;
AllJoynWinRTComponent::AJ_Message^ getMsg;

AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_BusPropGet(AJ_Message^ msg, AJ_BusPropGetCallback^ busPropGetCallback)
{
	::busPropGetCallback = busPropGetCallback;
	::AJ_Status status = ::AJ_BusPropGet(&msg->_msg, AllJoynWinRTComponent::AllJoyn::BusPropGetCallback, NULL);
	::getMsg = msg;
	return static_cast<AJ_Status>(status);
}


::AJ_Status AllJoynWinRTComponent::AllJoyn::BusPropGetCallback(::AJ_Message* replyMsg, uint32_t propId, void* context)
{
	AllJoynWinRTComponent::AJ_Status status = ::busPropGetCallback(::getMsg, propId);

	return static_cast<::AJ_Status>(status);
}


// Pointer to Javascript function
AllJoynWinRTComponent::AJ_BusPropSetCallback^ busPropSetCallback;
AllJoynWinRTComponent::AJ_Message^ setMsg;

AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_BusPropSet(AJ_Message^ msg, AJ_BusPropSetCallback^ busPropSetCallback)
{
	::busPropSetCallback = busPropSetCallback;
	::AJ_Status status = ::AJ_BusPropSet(&msg->_msg, AllJoynWinRTComponent::AllJoyn::BusPropSetCallback, NULL);
	::setMsg = msg;
	return static_cast<AJ_Status>(status);
}


::AJ_Status AllJoynWinRTComponent::AllJoyn::BusPropSetCallback(::AJ_Message* replyMsg, uint32_t propId, void* context)
{
	AllJoynWinRTComponent::AJ_Status status = ::busPropSetCallback(::setMsg, propId);

	return static_cast<::AJ_Status>(status);
}

AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_BusBindSessionPort(AJ_BusAttachment^ bus, uint16_t port, AJ_SessionOpts^ opts, uint8_t flags)
{
	::AJ_SessionOpts* _opts = NULL;

	if (opts)
	{
		SAFE_DEL(_s_cachedSessionOpts);
		_opts = new ::AJ_SessionOpts();
		ZeroMemory(_opts, sizeof(_opts));

		STRUCT_COPY(opts, isMultipoint);
		STRUCT_COPY(opts, proximity);
		STRUCT_COPY(opts, traffic);
		STRUCT_COPY(opts, transports);

		_s_cachedSessionOpts = _opts;
	}

	::AJ_Status _status = ::AJ_BusBindSessionPort(bus->_bus, port, _opts, flags);

	return (static_cast<AJ_Status>(_status));
}

AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_BusUnbindSession(AJ_BusAttachment^ bus, uint16_t port)
{
	::AJ_Status _status = ::AJ_BusUnbindSession(bus->_bus, port);

	return (static_cast<AJ_Status>(_status));
}

AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_BusRequestName(AJ_BusAttachment^ bus, String^ name, uint32_t flags)
{
	PLSTOMBS(name, _name);
	::AJ_Status _status = ::AJ_BusRequestName(bus->_bus, _name, flags);

	return (static_cast<AJ_Status>(_status));
}

AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_BusReleaseName(AJ_BusAttachment^ bus, String^ name)
{
	PLSTOMBS(name, _name);
	::AJ_Status _status = ::AJ_BusReleaseName(bus->_bus, _name);

	return (static_cast<AJ_Status>(_status));
}

AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_BusAdvertiseName(AJ_BusAttachment^ bus, String^ name, uint16_t transportMask, uint8_t op, uint8_t flags)
{
	PLSTOMBS(name, _name);
	::AJ_Status _status = ::AJ_BusAdvertiseName(bus->_bus, _name, transportMask, op, flags);

	return (static_cast<AJ_Status>(_status));
}


AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_SetProxyObjectPath(const Array<AJ_Object^>^ proxyObjects, uint32_t msgId, String^ objPath)
{
	PLSTOMBS(objPath, _objPath);
	::AJ_Status status = ::AJ_SetProxyObjectPath(_s_cachedProxyObjects, msgId, _objPath);

	return static_cast<AJ_Status>(status);
}


AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_MarshalContainer(AJ_Message^ msg, AJ_Arg^ arg, uint8_t typeId)
{
	return static_cast<AJ_Status>(::AJ_MarshalContainer(&msg->_msg, &arg->_arg, typeId));
}


AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_MarshalCloseContainer(AJ_Message^ msg, AJ_Arg^ arg)
{
	return static_cast<AJ_Status>(::AJ_MarshalCloseContainer(&msg->_msg, &arg->_arg));
}


AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_UnmarshalContainer(AJ_Message^ msg, AJ_Arg^ arg, uint8_t typeId)
{
	return static_cast<AJ_Status>(::AJ_UnmarshalContainer(&msg->_msg, &arg->_arg, typeId));
}


AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_UnmarshalCloseContainer(AJ_Message^ msg, AJ_Arg^ arg)
{
	return static_cast<AJ_Status>(::AJ_UnmarshalCloseContainer(&msg->_msg, &arg->_arg));
}


::AJ_Status AllJoynWinRTComponent::AllJoyn::UnmarshalArgs(::AJ_Message* msg, const char** sig, Vector<Object^>^ args)
{
	::AJ_Arg structArg;
	::AJ_Arg arg;
	::AJ_Status status = ::AJ_OK;

	while (**sig)
	{
		uint8_t typeId = (uint8_t)*((*sig)++);
		uint8_t nextTypeId = (uint8_t)*(*sig);

		if (!AJ_IsBasicType(typeId))
		{
			if ((typeId == AJ_ARG_STRUCT) || (typeId == AJ_ARG_DICT_ENTRY))
			{
				status = ::AJ_UnmarshalContainer(msg, &structArg, typeId);

				if (status != ::AJ_OK)
				{
					break;
				}

				status = UnmarshalArgs(msg, sig, args);

				if (status == ::AJ_OK)
				{
					uint8_t lastNestedTypeId = (uint8_t)*((*sig) - 1);

					if ((lastNestedTypeId == AJ_STRUCT_CLOSE) || (lastNestedTypeId == AJ_DICT_ENTRY_CLOSE))
					{
						status = ::AJ_UnmarshalCloseContainer(msg, &structArg);

						if (status != ::AJ_OK)
						{
							break;
						}
					}
					else
					{
						status = AJ_ERR_MARSHAL;
						break;
					}

					continue;
				}
				else
				{
					break;
				}
			}

			if ((typeId == AJ_ARG_ARRAY) && AJ_IsBasicType(nextTypeId))
			{
				if (!AJ_IsScalarType(nextTypeId)) // "as"
				{
					::AJ_Arg arrayArg;
					status = ::AJ_UnmarshalContainer(msg, &arrayArg, AJ_ARG_ARRAY);
					std::vector<char*> vArgs;

					do
					{
						char* str;
						status = ::AJ_UnmarshalArgs(msg, "s", &str);

						if (status != ::AJ_OK)
						{
							break;
						}

						vArgs.push_back(str);
					} 
					while (status == ::AJ_OK);

					Array<String^>^ vals = ref new Array<String^>(vArgs.size());
					for (int k = 0; k < vals->Length; k++)
					{
						MBSTOPLS(vArgs[k], str);
						vals[k] = str;
					}

					args->Append(vals);
					status = ::AJ_UnmarshalCloseContainer(msg, &arrayArg);
				}
				else
				{
					status = ::AJ_UnmarshalArg(msg, &arg);

					if (status != ::AJ_OK)
					{
						break;
					}

					const void* ptr = arg.val.v_data;
					size_t lenInBytes = arg.len;

					switch (SizeOfType(nextTypeId))
					{
					case 1:
						args->Append(ref new Array<uint8_t>((uint8_t*)(ptr), lenInBytes));
						break;

					case 2:
						if (nextTypeId == 'n')
						{
							args->Append(ref new Array<int16_t>((int16_t*)(ptr), lenInBytes >> 1));
						}
						else
						{
							args->Append(ref new Array<uint16_t>((uint16_t*)(ptr), lenInBytes >> 1));
						}
						break;

					case 4:
						if (nextTypeId == 'i')
						{
							args->Append(ref new Array<int32_t>((int32_t*)(ptr), lenInBytes >> 2));
						}
						else
						{
							args->Append(ref new Array<uint32_t>((uint32_t*)(ptr), lenInBytes >> 2));
						}
						break;

					case 8:
						if (nextTypeId == 'd')
						{
							args->Append(ref new Array<double>((double*)(ptr), lenInBytes >> 3));
						}
						else if (nextTypeId == 'x')
						{
							args->Append(ref new Array<int64_t>((int64_t*)(ptr), lenInBytes >> 3));
						}
						else
						{
							args->Append(ref new Array<uint64_t>((uint64_t*)(ptr), lenInBytes >> 3));
						}
						break;
					}
				}

				(*sig)++;
				continue;
			}

			if ((typeId == AJ_STRUCT_CLOSE) || (typeId == AJ_DICT_ENTRY_CLOSE))
			{
				break;
			}

			if (typeId == AJ_ARG_VARIANT)
			{
				const char* inSig;
				status = AJ_UnmarshalVariant(msg, &inSig);
				MBSTOPLS(inSig, inSigRT);
				args->Append(inSigRT);
				status = UnmarshalArgs(msg, &inSig, args);

				if (status != ::AJ_OK)
				{
					break;
				}

				continue;
			}

			if ((typeId == AJ_ARG_ARRAY) && !AJ_IsBasicType(nextTypeId))
			{
				::AJ_Arg arrayArg;
				char subSig[MAX_STR_LENGTH];
				char closeContainer = (nextTypeId == '(') ? ')' : '}';
				memcpy(subSig, *sig, strlen(*sig));
				subSig[strchr(subSig, closeContainer) - subSig + 1] = '\0';
				status = ::AJ_UnmarshalContainer(msg, &arrayArg, AJ_ARG_ARRAY);
				Vector<Object^>^ vArgs = ref new Vector<Object^>();

				do
				{
					const char* inSig = subSig;
					Vector<Object^>^ inArgs = ref new Vector<Object^>();
					status = UnmarshalArgs(msg, &inSig, inArgs);
					int len = inArgs->Size;

					if (len != 0)
					{
						vArgs->Append(inArgs);
					}
				} 
				while (status == ::AJ_OK);

				int len = vArgs->Size;
				args->Append(vArgs);
				status = ::AJ_UnmarshalCloseContainer(msg, &arrayArg);

				if (status != ::AJ_OK)
				{
					break;
				}

				*sig += strlen(subSig);
				continue;
			}

			AJ_ErrPrintf(("AJ_UnmarshalArgs(): AJ_ERR_UNEXPECTED\n"));
			status = AJ_ERR_UNEXPECTED;
			break;
		}
		else // scalar and string
		{
			status = ::AJ_UnmarshalArg(msg, &arg);

			if (status != ::AJ_OK)
			{
				break;
			}

			if (arg.typeId != typeId) 
			{
				AJ_ErrPrintf(("AJ_UnmarshalArgs(): AJ_ERR_UNMARSHAL\n"));
				status = ::AJ_ERR_UNMARSHAL;
				break;
			}

			if (AJ_IsScalarType(typeId)) 
			{
				switch (SizeOfType(typeId)) 
				{
				case 1:
					args->Append((uint8_t)(*arg.val.v_byte));
					break;

				case 2:
					if (typeId == 'n')
					{
						args->Append((int16_t)(*arg.val.v_uint16));
					}
					else
					{
						args->Append((uint16_t)(*arg.val.v_uint16));
					}
					break;

				case 4:
					if (typeId == 'i')
					{
						args->Append((int32_t)(*arg.val.v_uint32));
					}
					else
					{
						args->Append((uint32_t)(*arg.val.v_uint32));
					}
					break;

				case 8:
					if (typeId == 'd')
					{
						double d = *((double*)(arg.val.v_uint64));
						args->Append(d);
					}
					else if (typeId == 'x')
					{
						args->Append((int64_t)(*arg.val.v_uint64));
					}
					else
					{
						args->Append((uint64_t)(*arg.val.v_uint64));
					}
					break;
				}
			}
			else 
			{
				MBSTOPLS(arg.val.v_string, val);
				args->Append(val);
			}
		}
	}

	return status;
}


void AllJoynWinRTComponent::AllJoyn::AJ_UnmarshalArgsWithDelegate(AJ_Message^ msg, String^ signature, AJ_UnmarshalArgsDelegate^ func)
{
	Vector<Object^>^ args = ref new Vector<Object^>();
	PLSTOMBS(signature, _signature);
	const char* sig = _signature;
	::AJ_Status _status = UnmarshalArgs(&msg->_msg, &sig, args);
	func(static_cast<AJ_Status>(_status), args);
}


Array<Object^>^ AllJoynWinRTComponent::AllJoyn::AJ_UnmarshalArgs(AJ_Message^ msg, String^ signature)
{
	PLSTOMBS(signature, _signature);
	const char* sig = _signature;
	Vector<Object^>^ args = ref new Vector<Object^>();
	::AJ_Status _status = UnmarshalArgs(&msg->_msg, &sig, args);
	Array<Object^>^ returnArray = ref new Array<Object^>(2);
	returnArray[0] = static_cast<AJ_Status>(_status);
	returnArray[1] = args;
	return returnArray;
}


std::vector<char*> AllJoynWinRTComponent::AllJoyn::GetArrayArgs(String^ strVal)
{
	PLSTOMBS(strVal, _strVal);
	std::vector<char*> vArgs;
	char* token = strtok(_strVal, ",");

	while (token)
	{
		vArgs.push_back(token);
		token = strtok(NULL, ",");
	}

	return vArgs;
}


std::vector<Object^> AllJoynWinRTComponent::AllJoyn::GetArrayArgsAsString(String^ strVal)
{
	PLSTOMBS(strVal, _strVal);
	std::vector<Object^> vArgs;
	char* token = strtok(_strVal, ",");

	while (token)
	{
		MBSTOPLS(token, str);
		vArgs.push_back(str);
		token = strtok(NULL, ",");
	}

	return vArgs;
}


::AJ_Status AllJoynWinRTComponent::AllJoyn::MarshalArgs(::AJ_Message* msg, const char** sig, std::vector<Object^>* args)
{
	::AJ_Arg structArg;
	::AJ_Arg arg;
	::AJ_Status status = AJ_OK;
	int j = 0;
	int k = 0;
	char* vSig = NULL;

	while (**sig)
	{
		uint8_t typeId = (uint8_t)*((*sig)++);
		uint8_t nextTypeId = (uint8_t)*(*sig);

		if (!AJ_IsBasicType(typeId))
		{
			if ((typeId == AJ_ARG_STRUCT) || (typeId == AJ_ARG_DICT_ENTRY))
			{
				status = ::AJ_MarshalContainer(msg, &structArg, typeId);

				if (status != AJ_OK)
				{
					break;
				}

				status = MarshalArgs(msg, sig, args);

				if (status == AJ_OK)
				{
					uint8_t lastNestedTypeId = (uint8_t)*((*sig) - 1);

					if ((lastNestedTypeId == AJ_STRUCT_CLOSE) || (lastNestedTypeId == AJ_DICT_ENTRY_CLOSE))
					{
						status = ::AJ_MarshalCloseContainer(msg, &structArg);

						if (status != AJ_OK)
						{
							break;
						}
					}
					else
					{
						status = AJ_ERR_MARSHAL;
						break;
					}

					continue;
				}
				else
				{
					break;
				}
			}

			if ((typeId == AJ_ARG_ARRAY) && AJ_IsBasicType(nextTypeId))
			{
				if (!AJ_IsScalarType(nextTypeId)) // "as"
				{
					::AJ_Arg arrayArg;
					status = ::AJ_MarshalContainer(msg, &arrayArg, AJ_ARG_ARRAY);

					if (status != AJ_OK)
					{
						break;
					}

					String^ strVal = (String^)(*args).front();
					(*args).erase((*args).begin());
					std::vector<char*> vArgs = GetArrayArgs(strVal);
					size_t len = vArgs.size();

					for (int k = 0; k < len; k++)
					{
						const void* val = vArgs[k];
						::AJ_InitArg(&arg, nextTypeId, 0, val, 0);
						status = ::AJ_MarshalArg(msg, &arg);
					}

					status = ::AJ_MarshalCloseContainer(msg, &arrayArg);
				}
				else
				{
					void* aval = NULL;
					String^ strVal = (String^)(*args).front();
					(*args).erase((*args).begin());
					std::vector<char*> vArgs = GetArrayArgs(strVal);
					size_t len = vArgs.size();

					switch (SizeOfType(nextTypeId))
					{
					case 1:
						aval = (uint8_t*)malloc(len * sizeof(uint8_t));
						for (int k = 0; k < len; k++)
						{
							*(((uint8_t*)aval) + k) = (uint8_t)(atoi(vArgs[k]));
						}
						break;

					case 2:
						aval = (uint16_t*)malloc(len * sizeof(uint16_t));
						for (int k = 0; k < len; k++)
						{
							*(((uint16_t*)aval) + k) = (uint16_t)(atoi(vArgs[k]));
						}
						break;

					case 4:
						aval = (uint32_t*)malloc(len * sizeof(uint32_t));
						for (int k = 0; k < len; k++)
						{
							*(((uint32_t*)aval) + k) = (uint32_t)(_strtoui64(vArgs[k], NULL, 10));
						}
						break;

					case 8:
						aval = (uint64_t*)malloc(len * sizeof(uint64_t));

						if (nextTypeId == 'd')
						{
							for (int k = 0; k < len; k++)
							{
								double d = atof(vArgs[k]);
								*(((uint64_t*)aval) + k) = *((uint64_t*)&d);
							}
						}
						else
						{
							for (int k = 0; k < len; k++)
							{
								*(((uint64_t*)aval) + k) = (uint64_t)(_strtoui64(vArgs[k], NULL, 10));
							}
						}

						break;
					}

					::AJ_InitArg(&arg, nextTypeId, AJ_ARRAY_FLAG, aval, len * SizeOfType(nextTypeId));
					status = ::AJ_MarshalArg(msg, &arg);
					free(aval);
				}

				(*sig)++;
				continue;
			}

			if ((typeId == AJ_STRUCT_CLOSE) || (typeId == AJ_DICT_ENTRY_CLOSE))
			{
				break;
			}

			if (typeId == AJ_ARG_VARIANT)
			{
				String^ sig = (String^)(*args).front();
				(*args).erase((*args).begin());
				PLSTOMBS(sig, _sig);
				status = ::AJ_MarshalVariant(msg, _sig);
				const char* vSig = _sig;
				status = MarshalArgs(msg, &vSig, args);

				if (status != AJ_OK)
				{
					break;
				}

				continue;
			}

			if ((typeId == AJ_ARG_ARRAY) && !AJ_IsBasicType(nextTypeId))
			{
				::AJ_Arg arrayArg;
				char subSig[MAX_STR_LENGTH];
				char closeContainer = (nextTypeId == '(') ? ')' : '}';
				memcpy(subSig, *sig, strlen(*sig));
				subSig[strchr(subSig, closeContainer) - subSig + 1] = '\0';
				status = ::AJ_MarshalContainer(msg, &arrayArg, AJ_ARG_ARRAY);

				if (status != AJ_OK)
				{
					break;
				}

				String^ strVal = (String^)(*args).front();
				std::vector<Object^> vArgs = GetArrayArgsAsString(strVal);

				while (vArgs.size())
				{
					const char* inSig = subSig;
					status = MarshalArgs(msg, &inSig, &vArgs);

					if (status != AJ_OK)
					{
						break;
					}
				}

				(*args).erase((*args).begin());
				status = ::AJ_MarshalCloseContainer(msg, &arrayArg);

				if (status != AJ_OK)
				{
					break;
				}

				*sig += strlen(subSig);
				continue;
			}

			AJ_ErrPrintf(("AJ_UnmarshalArgs(): AJ_ERR_UNEXPECTED\n"));
			status = AJ_ERR_UNEXPECTED;
			break;
		}
		else
		{
			if (AJ_IsScalarType(typeId))
			{
				String^ strVal = (String^)(*args).front();
				void* val = NULL;
				uint8_t u8 = 0;
				uint16_t u16 = 0;
				uint32_t u32 = 0;
				uint64_t u64 = 0;

				switch (SizeOfType(typeId))
				{
				case 1:
					u8 = _wtoi(strVal->Data());
					(*args).erase((*args).begin());
					val = &u8;
					break;

				case 2:
					u16 = _wtoi(strVal->Data());
					(*args).erase((*args).begin());
					val = &u16;
					break;

				case 4:
					if (!wcscmp(strVal->Data(), L"true") || !wcscmp(strVal->Data(), L"TRUE"))
					{
						u32 = 1;
					}
					else if (!wcscmp(strVal->Data(), L"false") || !wcscmp(strVal->Data(), L"FALSE"))
					{
						u32 = 0;
					}
					else
					{
						u32 = _wcstoui64(strVal->Data(), NULL, 10);
					}

					(*args).erase((*args).begin());
					val = &u32;
					break;

				case 8:
					if (typeId == 'd')
					{
						double d = _wtof(strVal->Data());
						u64 = *((uint64_t*)&d);
					}
					else
					{
						u64 = _wcstoui64(strVal->Data(), NULL, 10);
					}

					(*args).erase((*args).begin());
					val = &u64;
					break;
				}

				::AJ_InitArg(&arg, typeId, 0, val, 0);
			}
			else
			{
				String^ str = (String^)(*args).front();
				(*args).erase((*args).begin());
				PLSTOMBS(str, _str);
				void* val = _str;
				::AJ_InitArg(&arg, typeId, 0, val, 0);
			}

			status = ::AJ_MarshalArg(msg, &arg);
		}
	}

	return status;
}


AllJoynWinRTComponent::AJ_Status AllJoynWinRTComponent::AllJoyn::AJ_MarshalArgs(AJ_Message^ msg, String^ signature, const Array<String^>^ argsRT)
{
	std::vector<Object^> args;
	PLSTOMBS(signature, _signature);
	const char* sig = _signature;

	for (int i = 0; i < argsRT->Length; i++)
	{
		args.push_back(argsRT[i]);
	}

	::AJ_Status _status = MarshalArgs(&msg->_msg, &sig, &args);

	return (static_cast<AJ_Status>(_status));
}


//////////////////////////////////////////////////////////////////////////////////////////
// Helper functions
//////////////////////////////////////////////////////////////////////////////////////////

AllJoynWinRTComponent::_AJ_Message AllJoynWinRTComponent::AJ_Message::Get()
{
	AllJoynWinRTComponent::_AJ_Message msg;

	msg.msgId = _msg.msgId;

	if (_msg.hdr)
	{
		msg.hdr.endianess = _msg.hdr->endianess;
		msg.hdr.msgType = _msg.hdr->msgType;
		msg.hdr.flags = _msg.hdr->flags;
		msg.hdr.majorVersion = _msg.hdr->majorVersion;
		msg.hdr.bodyLen = _msg.hdr->bodyLen;
		msg.hdr.serialNum = _msg.hdr->serialNum;
		msg.hdr.headerLen = _msg.hdr->headerLen;
	}

	if (_msg.iface)
	{
		MBSTOWCS(_msg.iface, iface);
		msg.iface = ref new String(iface);
	}

	if (_msg.sender)
	{
		MBSTOWCS(_msg.sender, sender);
		msg.sender = ref new String(sender);
	}

	if (_msg.destination)
	{
		MBSTOWCS(_msg.destination, destination);
		msg.destination = ref new String(destination);
	}

	if (_msg.signature)
	{
		MBSTOWCS(_msg.signature, signature);
		msg.signature = ref new String(signature);
	}

	// This was an uninitialized pointer when not set so ignore until needed
	//if (_msg.objPath)
	//{
	//	MBSTOWCS(_msg.objPath, objPath);
	//	msg.objPath = ref new String(objPath);
	//}

	if (_msg.member)
	{
		MBSTOWCS(_msg.member, member);
		msg.member = ref new String(member);
	}

	if (_msg.error)
	{
		MBSTOWCS(_msg.error, error);
		msg.error = ref new String(error);
	}

	msg.replySerial = _msg.replySerial;
	msg.sessionId = _msg.sessionId;
	msg.timestamp = _msg.timestamp;
	msg.ttl = _msg.ttl;

	return msg;
}


uint32_t AllJoynWinRTComponent::AllJoyn::AJ_Description_ID(uint32_t o, uint32_t i, uint32_t m, uint32_t a)
{
	return (((uint32_t)(o) << 24) | (((uint32_t)(i)) << 16) | (((uint32_t)(m)) << 8) | (a));
}


uint32_t AllJoynWinRTComponent::AllJoyn::AJ_Encode_Message_ID(uint32_t o, uint32_t p, uint32_t i, uint32_t m)
{
	return (((uint32_t)(o) << 24) | (((uint32_t)(p)) << 16) | (((uint32_t)(i)) << 8) | (m));
}


uint32_t AllJoynWinRTComponent::AllJoyn::AJ_Encode_Property_ID(uint32_t o, uint32_t p, uint32_t i, uint32_t m)
{
	return (((uint32_t)(o) << 24) | (((uint32_t)(p)) << 16) | (((uint32_t)(i)) << 8) | (m));
}


uint32_t AllJoynWinRTComponent::AllJoyn::AJ_Bus_Message_ID(uint32_t p, uint32_t i, uint32_t m)
{
	return (((uint32_t)(AllJoynWinRTComponent::AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(p)) << 16) | (((uint32_t)(i)) << 8) | (m));
}


uint32_t AllJoynWinRTComponent::AllJoyn::AJ_App_Message_ID(uint32_t p, uint32_t i, uint32_t m)
{
	return (((uint32_t)(AllJoynWinRTComponent::AJ_Introspect::AJ_App_ID_Flag) << 24) | (((uint32_t)(p)) << 16) | (((uint32_t)(i)) << 8) | (m));
}


uint32_t AllJoynWinRTComponent::AllJoyn::AJ_Prx_Message_ID(uint32_t p, uint32_t i, uint32_t m)
{
	return (((uint32_t)(AllJoynWinRTComponent::AJ_Introspect::AJ_Prx_ID_Flag) << 24) | (((uint32_t)(p)) << 16) | (((uint32_t)(i)) << 8) | (m));
}


uint32_t AllJoynWinRTComponent::AllJoyn::AJ_Bus_Property_ID(uint32_t p, uint32_t i, uint32_t m)
{
	return (((uint32_t)(AllJoynWinRTComponent::AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(p)) << 16) | (((uint32_t)(i)) << 8) | (m));
}


uint32_t AllJoynWinRTComponent::AllJoyn::AJ_App_Property_ID(uint32_t p, uint32_t i, uint32_t m)
{
	return (((uint32_t)(AllJoynWinRTComponent::AJ_Introspect::AJ_App_ID_Flag) << 24) | (((uint32_t)(p)) << 16) | (((uint32_t)(i)) << 8) | (m));
}


uint32_t AllJoynWinRTComponent::AllJoyn::AJ_Prx_Property_ID(uint32_t p, uint32_t i, uint32_t m)
{
	return (((uint32_t)(AllJoynWinRTComponent::AJ_Introspect::AJ_Prx_ID_Flag) << 24) | (((uint32_t)(p)) << 16) | (((uint32_t)(i)) << 8) | (m));
}


uint32_t AllJoynWinRTComponent::AllJoyn::AJ_Reply_ID(uint32_t id)
{
	return ((id) | (uint32_t)((uint32_t)(AllJoynWinRTComponent::AJ_Introspect::AJ_Rep_ID_Flag) << 24));
}
