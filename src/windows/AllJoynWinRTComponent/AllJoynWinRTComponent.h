#pragma once

#include <ppltasks.h>

#include "aj_introspect.h"

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Platform::Collections;


#ifdef AJ_PrintXML
#undef AJ_PrintXML
#endif // AJ_PrintXML


namespace AllJoynWinRTComponent
{
	/**
	* Type for an interface description - NULL terminated array of strings.
	*/
	typedef IVector<IVector<String^>^>^ AJ_InterfaceDescription;

	/**
	* Prototype for a function provided by the property store for getting ANNOUNCE and ABOUT properties
	*
	* @param reply     The message to marshal the property values into. The getter can also figure out
	*                  from the msgId in the reply message if the reply is for ANNOUNCE or ABOUT.
	*
	* @param language  The language to use to return the string properties. If this is NULL the default
	*                  language will be used.
	*
	* @return   Return AJ_OK if the properties were succesfully marshaled into the reply.
	*
	*/
	public enum class AJ_Status
	{
		AJ_OK = 0,  /**< Success status */
		AJ_ERR_NULL = 1,  /**< Unexpected NULL pointer */
		AJ_ERR_UNEXPECTED = 2,  /**< An operation was unexpected at this time */
		AJ_ERR_INVALID = 3,  /**< A value was invalid */
		AJ_ERR_IO_BUFFER = 4,  /**< An I/O buffer was invalid or in the wrong state */
		AJ_ERR_READ = 5,  /**< An error while reading data from the network */
		AJ_ERR_WRITE = 6,  /**< An error while writing data to the network */
		AJ_ERR_TIMEOUT = 7,  /**< A timeout occurred */
		AJ_ERR_MARSHAL = 8,  /**< Marshaling failed due to badly constructed message argument */
		AJ_ERR_UNMARSHAL = 9,  /**< Unmarshaling failed due to a corrupt or invalid message */
		AJ_ERR_END_OF_DATA = 10, /**< Not enough data */
		AJ_ERR_RESOURCES = 11, /**< Insufficient memory to perform the operation */
		AJ_ERR_NO_MORE = 12, /**< Attempt to unmarshal off the end of an array */
		AJ_ERR_SECURITY = 13, /**< Authentication or decryption failed */
		AJ_ERR_CONNECT = 14, /**< Network connect failed */
		AJ_ERR_UNKNOWN = 15, /**< A unknown value */
		AJ_ERR_NO_MATCH = 16, /**< Something didn't match */
		AJ_ERR_SIGNATURE = 17, /**< Signature is not what was expected */
		AJ_ERR_DISALLOWED = 18, /**< An operation was not allowed */
		AJ_ERR_FAILURE = 19, /**< A failure has occurred */
		AJ_ERR_RESTART = 20, /**< The OEM event loop must restart */
		AJ_ERR_LINK_TIMEOUT = 21, /**< The bus link is inactive too long */
		AJ_ERR_DRIVER = 22, /**< An error communicating with a lower-layer driver */
		AJ_ERR_OBJECT_PATH = 23, /**< Object path was not specified */
		AJ_ERR_BUSY = 24, /**< An operation failed and should be retried later */
		AJ_ERR_DHCP = 25, /**< A DHCP operation has failed */
		AJ_ERR_ACCESS = 26, /**< The operation specified is not allowed */
		AJ_ERR_SESSION_LOST = 27, /**< The session was lost */
		AJ_ERR_LINK_DEAD = 28, /**< The network link is now dead */
		AJ_ERR_HDR_CORRUPT = 29, /**< The message header was corrupt */
		AJ_ERR_RESTART_APP = 30, /**< The application must cleanup and restart */
		AJ_ERR_INTERRUPTED = 31, /**< An I/O operation (READ) was interrupted */
		AJ_ERR_REJECTED = 32, /**< The connection was rejected */
		AJ_ERR_RANGE = 33, /**< Value provided was out of range */
		AJ_ERR_ACCESS_ROUTING_NODE = 34, /**< Access defined by routing node */
		AJ_ERR_KEY_EXPIRED = 35, /**< The key has expired */
		AJ_ERR_SPI_NO_SPACE = 36, /**< Out of space error */
		AJ_ERR_SPI_READ = 37, /**< Read error */
		AJ_ERR_SPI_WRITE = 38, /**< Write error */
		AJ_ERR_OLD_VERSION = 39, /**< Router you connected to is old and unsupported */
		AJ_ERR_NVRAM_READ = 40, /**< Error while reading from NVRAM */
		AJ_ERR_NVRAM_WRITE = 41, /**< Error while writing to NVRAM */
		/*
		* REMINDER: Update AJ_StatusText in aj_debug.c if adding a new status code.
		*/
		AJ_STATUS_LAST = 41  /**< The last error status code */
	};

	public enum class AJ_Introspect
	{
		/**
		* Enmeration type for characterizing interface members
		*/
		AJ_Obj_Flag_Secure = 0x01,					/**< Invalid member */
		AJ_Obj_Flag_Hidden = 0x02,					/**< If set this bit indicates this is object is not announced */
		AJ_Obj_Flag_Disabled = 0x04,				/**< If set this bit indicates that method calls cannot be made to the object at this time */
		AJ_Obj_Flag_Announced = 0x08,				/**< If set this bit indicates this object is announced by ABOUT */
		AJ_Obj_Flag_Is_Proxy = 0x10,				/**< If set this bit indicates this object is a proxy object */
		AJ_Obj_Flag_Described = 0x20,				/**< If set this bit indicates this object has descriptions and is announced by ABOUT with 'org.allseen.Introspectable' interface added to the announcement */

		AJ_Obj_Flags_All_Include_Mask = 0xFF,		/**< The include filter mask for the object iterator indicating ALL objects */

		/*
		* When a message unmarshalled the message is validated by matching it against a list of object
		* tables that fully describe the message. If the message matches the unmarshal code sets the msgId
		* field in the AJ_Message struct. Rather than using a series of string comparisons, application code
		* can simply use this msgId to identify the message. There are three predefined object tables and
		* applications and services are free to add additional tables. The maximum number of table is 127
		* because the most signifant bit in the msgId is reserved to distinguish between method calls and
		* their corresponding replies.
		*
		* Of the three predefined tables the first is reserved for bus management messages. The second is
		* for objects implemented by the application. The third is for proxy (remote) objects the
		* application interacts with.
		*
		* The same message identifiers are also used by the marshalling code to populate the message header
		* with the appropriate strings for the object path, interface name, member, and signature. This
		* relieves the application developer from having to explicitly set these values in the message.
		*/
		AJ_Bus_ID_Flag = 0x00,						/**< Identifies that a message belongs to the set of builtin bus object messages */
		AJ_App_ID_Flag = 0x01,						/**< Identifies that a message belongs to the set of objects implemented by the application */
		AJ_Prx_ID_Flag = 0x02,						/**< Identifies that a message belongs to the set of objects implemented by remote peers */
	
		/*
		* This flag AJ_REP_ID_FLAG is set in the msgId filed to indentify that a message is a reply to a
		* method call. Because the object description describes the out (call) and in (reply) arguments the
		* same entry in the object table is used for both method calls and replies but since they are
		* handled differently this flags is set by the unmarshaller to indicate whether the specific
		* message is the call or reply.
		*/
		AJ_Rep_ID_Flag = 0x80,						/**< Indicates a message is a reply message */	
	};

	/**
	* Identifiers for standard methods and signals. These are the values returned by
	* AJ_IdentifyMessage() for correctly formed method and signal messages.
	*/
	public enum class AJ_Std
	{
		/*
		* Members of the /org/freedesktop/DBus interface org.freedesktop.DBus
		*/

		/**< method for hello */
		AJ_Method_Hello = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(0)) << 16) | (((uint32_t)(0)) << 8) | (0)),

		/**< signal for name owner changed */
		AJ_Signal_Name_Owner_Changed = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(0)) << 16) | (((uint32_t)(1)) << 8) | (1)),

		/**< signal for name acquired */
		AJ_Signal_Name_Acquired = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(0)) << 16) | (((uint32_t)(0)) << 8) | (2)),

		/**< signal for name lost */
		AJ_Signal_Name_Lost = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(0)) << 16) | (((uint32_t)(0)) << 8) | (3)),

		/**< signal for props changed */
		AJ_Signal_Props_Changed = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(0)) << 16) | (((uint32_t)(0)) << 8) | (4)),

		/**< method for request name */
		AJ_Method_Request_Name = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(0)) << 16) | (((uint32_t)(0)) << 8) | (5)),

		/**< method for add match */
		AJ_Method_Add_Match = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(0)) << 16) | (((uint32_t)(0)) << 8) | (6)),

		/**< method for remove match */
		AJ_Method_Remove_Match = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(0)) << 16) | (((uint32_t)(0)) << 8) | (7)),

		/**< method for release name */
		AJ_Method_Release_Name = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(0)) << 16) | (((uint32_t)(0)) << 8) | (8)),
	
		/*
		* Members of /org/alljoyn/Bus interface org.alljoyn.Bus
		*/
	
		/**< signal for session lost */
		AJ_Signal_Session_Lost = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(1)) << 16) | (((uint32_t)(0)) << 8) | (0)),
		
		/**< signal for found advertising name */
		AJ_Signal_Found_Adv_Name = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(1)) << 16) | (((uint32_t)(0)) << 8) | (1)),
		
		/**< signal for lost advertising name */
		AJ_Signal_Lost_Adv_Name = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(1)) << 16) | (((uint32_t)(0)) << 8) | (2)),
		
		/**< signal for mp session changed */
		AJ_Signal_Mp_Session_Changed = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(1)) << 16) | (((uint32_t)(0)) << 8) | (3)),
		
		/**< method for advertise name */
		AJ_Method_Advertise_Name = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(1)) << 16) | (((uint32_t)(0)) << 8) | (4)),
		
		/**< method for cancel advertise */
		AJ_Method_Cancel_Advertise = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(1)) << 16) | (((uint32_t)(0)) << 8) | (5)),
		
		/**< method for find name */
		AJ_Method_Find_Name = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(1)) << 16) | (((uint32_t)(0)) << 8) | (6)),
		
		/**< method for cancel find name */
		AJ_Method_Cancel_Find_Name = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(1)) << 16) | (((uint32_t)(0)) << 8) | (7)),
		
		/**< method for bind session port */
		AJ_Method_Bind_Session_Port = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(1)) << 16) | (((uint32_t)(0)) << 8) | (8)),
		
		/**< method for unbind session */
		AJ_Method_Unbind_Session = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(1)) << 16) | (((uint32_t)(0)) << 8) | (9)),
		
		/**< method for join session */
		AJ_Method_Join_Session = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(1)) << 16) | (((uint32_t)(0)) << 8) | (10)),
		
		/**< method for leave session */
		AJ_Method_Leave_Session = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(1)) << 16) | (((uint32_t)(0)) << 8) | (11)),
		
		/**< method for cancel sessionless */
		AJ_Method_Cancel_Sessionless = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(1)) << 16) | (((uint32_t)(0)) << 8) | (12)),
		
		/**< method for find name by specific transports */
		AJ_Method_Find_Name_By_Transport = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(1)) << 16) | (((uint32_t)(0)) << 8) | (13)),
		
		/**< method for cancel find name by specific transports */
		AJ_Method_Cancel_Find_Name_By_Transport = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(1)) << 16) | (((uint32_t)(0)) << 8) | (14)),
		
		/**< method for setting the link timeout for a session */
		AJ_Method_Set_Link_Timeout = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(1)) << 16) | (((uint32_t)(0)) << 8) | (15)),
		
		/**< method for removing a member in a session */
		AJ_Method_Remove_Session_Member = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(1)) << 16) | (((uint32_t)(0)) << 8) | (16)),
		
		/**< signal for session lost with a reason */
		AJ_Signal_Session_Lost_With_Reason = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(1)) << 16) | (((uint32_t)(0)) << 8) | (17)),
		
		/**< method for ping */
		AJ_Method_Bus_Ping = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(1)) << 16) | (((uint32_t)(0)) << 8) | (18)),
	
		/*
		* Members of /org/alljoyn/Bus/Peer interface org.alljoyn.Bus.Peer.Session
		*/

		/**< method for accept session */
		AJ_Method_Accept_Session = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(2)) << 16) | (((uint32_t)(0)) << 8) | (0)),
		
		/**< signal for session joined */
		AJ_Signal_Session_Joined = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(2)) << 16) | (((uint32_t)(0)) << 8) | (1)),

		/*
		* Members of /org/alljoyn/Bus/Peer interface org.alljoyn.Bus.Peer.Authentication
		*/

		/**< method for exchange guids */
		AJ_Method_Exchange_Guids = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(2)) << 16) | (((uint32_t)(1)) << 8) | (0)),

		/**< method for generate session key */
		AJ_Method_GEN_Session_Key = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(2)) << 16) | (((uint32_t)(1)) << 8) | (1)),

		/**< method for exchange group keys */
		AJ_Method_Exchange_Group_Keys = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(2)) << 16) | (((uint32_t)(1)) << 8) | (2)),

		/**< method for auth challenge */
		AJ_Method_Auth_Challenge = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(2)) << 16) | (((uint32_t)(1)) << 8) | (3)),

		/**< method for exchange suites*/
		AJ_Method_Exchange_Suites = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(2)) << 16) | (((uint32_t)(1)) << 8) | (4)),

		/**< method for key exchange*/
		AJ_Method_Key_Exchange = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(2)) << 16) | (((uint32_t)(1)) << 8) | (5)),

		/**< method for authenticating key exchange*/
		AJ_Method_Key_Authentication = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(2)) << 16) | (((uint32_t)(1)) << 8) | (6)),

		/*
		* Members of interface org.freedesktop.DBus.Introspectable
		*
		* Note - If you use this message id explicitly to construct a method call it will always introspect
		* the root object.
		*/

		/**< method for introspect */
		AJ_Method_Introspect = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(3)) << 16) | (((uint32_t)(0)) << 8) | (0)),

		/*
		* Members of the interface org.freedesktop.DBus.Peer
		*/

		/**< method for ping */
		AJ_Method_Ping = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(3)) << 16) | (((uint32_t)(1)) << 8) | (0)),
		
		/**< method for get machine id */
		AJ_Method_Get_Machine_Id = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(3)) << 16) | (((uint32_t)(1)) << 8) | (1)),

		/*
		* Members of the interface org.allseen.Introspectable
		*/

		/**< method for get description langauges */
		AJ_Method_Get_Description_Lang = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(3)) << 16) | (((uint32_t)(2)) << 8) | (0)),

		/**< method for introspect with descriptions */
		AJ_Method_Introspect_With_Desc = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(3)) << 16) | (((uint32_t)(2)) << 8) | (1)),

		/*
		* Members of /org/alljoyn/Daemon interface org.alljoyn.Daemon
		*/

		/**< signal for link probe request */
		AJ_Signal_Probe_Req = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(4)) << 16) | (((uint32_t)(0)) << 8) | (0)),

		/**< signal for link probe acknowledgement */
		AJ_Signal_Probe_Ack = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(4)) << 16) | (((uint32_t)(0)) << 8) | (1)),

		/*
		* Members of interface org.alljoyn.About
		*/

		AJ_Method_About_Get_Prop = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(5)) << 16) | (((uint32_t)(0)) << 8) | (0)),
		AJ_Method_About_Set_Prop = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(5)) << 16) | (((uint32_t)(0)) << 8) | (1)),
		AJ_Property_About_Version = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(5)) << 16) | (((uint32_t)(1)) << 8) | (0)),
		AJ_Method_About_Get_About_Data = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(5)) << 16) | (((uint32_t)(1)) << 8) | (1)),
		AJ_Method_About_Get_Object_Description = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(5)) << 16) | (((uint32_t)(1)) << 8) | (2)),
		AJ_Signal_About_Announce = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(5)) << 16) | (((uint32_t)(1)) << 8) | (3)),

		/*
		* Members of interface org.alljoyn.AboutIcon
		*/

		AJ_Method_About_Icon_Get_Prop = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(6)) << 16) | (((uint32_t)(0)) << 8) | (0)),
		AJ_Method_About_Icon_Set_Prop = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(6)) << 16) | (((uint32_t)(0)) << 8) | (1)),

		AJ_Property_About_Icon_Version_Prop = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(6)) << 16) | (((uint32_t)(1)) << 8) | (0)),
		AJ_Property_About_Icon_Mimetype_Prop = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(6)) << 16) | (((uint32_t)(1)) << 8) | (1)),
		AJ_Property_About_Icon_Size_Prop = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(6)) << 16) | (((uint32_t)(1)) << 8) | (2)),

		AJ_Method_About_Icon_Get_Url = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(6)) << 16) | (((uint32_t)(1)) << 8) | (3)),
		AJ_Method_About_Icon_Get_Content = ((uint32_t)(((uint32_t)AJ_Introspect::AJ_Bus_ID_Flag) << 24) | (((uint32_t)(6)) << 16) | (((uint32_t)(1)) << 8) | (4)),

		AJ_Prop_Get = 0,        /**< index for property method get */
		AJ_Prop_Set = 1,        /**< index for property method set */
		AJ_Prop_Get_All = 2,        /**< index for property method get_all */
	};


	public enum class AJ_ArgType
	{
		AJ_Arg_Invalid = '\0',				/**< AllJoyn invalid type */
		AJ_Arg_Array = 'a',					/**< AllJoyn array container type */
		AJ_Arg_Boolean = 'b',				/**< AllJoyn boolean basic type */
		AJ_Arg_Double = 'd',				/**< AllJoyn IEEE 754 double basic type */
		AJ_Arg_Signature = 'g',				/**< AllJoyn signature basic type */
		AJ_Arg_Handle = 'h',				/**< AllJoyn socket handle basic type */
		AJ_Arg_Int32 = 'i',					/**< AllJoyn 32-bit signed integer basic type */
		AJ_Arg_Int16 = 'n',					/**< AllJoyn 16-bit signed integer basic type */
		AJ_Arg_Obj_Path = 'o',				/**< AllJoyn Name of an AllJoyn object instance basic type */
		AJ_Arg_uint16 = 'q',				/**< AllJoyn 16-bit unsigned integer basic type */
		AJ_Arg_String = 's',				/**< AllJoyn UTF-8 NULL terminated string basic type */
		AJ_Arg_Uint64 = 't',				/**< AllJoyn 64-bit unsigned integer basic type */
		AJ_Arg_Uint32 = 'u',				/**< AllJoyn 32-bit unsigned integer basic type */
		AJ_Arg_Variant = 'v',				/**< AllJoyn variant container type */
		AJ_Arg_Int64 = 'x',					/**< AllJoyn 64-bit signed integer basic type */
		AJ_Arg_Byte = 'y',					/**< AllJoyn 8-bit unsigned integer basic type */
		AJ_Arg_Struct = '(',				/**< AllJoyn struct container type */
		AJ_Arg_Dict_Entry = '{',			/**< AllJoyn dictionary or map container type - an array of key-value pairs */
	};

	public enum class AJ_MsgFlag
	{
		AJ_No_Flags = 0x00,					/**< No message flags */
		AJ_Flag_No_Reply_Expected = 0x01,   /**< Not expecting a reply */
		AJ_Flag_Auto_Start = 0x02,			/**< Auto start the service */
		AJ_Flag_Allow_Remote_Msg = 0x04,    /**< Allow messeages from remote hosts */
		AJ_Flag_Sessionless = 0x10,			/**< Sessionless message */
		AJ_Flag_Global_Broadcast = 0x20,    /**< Global (bus-to-bus) broadcast */
		AJ_Flag_Compressed = 0x40,			/**< Header is compressed */
		AJ_Flag_Encrypted = 0x80,			/**< Body is encrypted */

		Alljoyn_Flag_Sessionless = 0x10		/**< Deprecated: Use AJ_FLAG_SESSIONLESS instead */
	};

	/**
	* Type for a bus attachment
	*/
	public ref struct AJ_BusAttachment sealed
	{
	internal:
		::AJ_BusAttachment* _bus;
	};

	/**
	* Type for describing session options
	*/
	public ref struct AJ_SessionOpts sealed
	{
		property uint8_t traffic;								/**< traffic type */
		property uint8_t proximity;								/**< proximity */
		property uint16_t transports;							/**< allowed transports */
		property uint32_t isMultipoint;							/**< multi-point session capable */
	};

	/**
	* Type for an AllJoyn object description
	*/
	public ref struct AJ_Object sealed
	{
		property String^ path;									/**< object path */
		property AJ_InterfaceDescription interfaces;			/**< interface descriptor */
		property uint8_t flags;                                 /**< flags for the object */
	};

	/**
	* AllJoyn Message Header
	*/
	public value struct AJ_MsgHeader
	{
		wchar_t endianess;      /**< The endianness of this message */
		uint8_t msgType;       /**< Indicates if the message is method call, signal, etc. */
		uint8_t flags;         /**< Flag bits */
		uint8_t majorVersion;  /**< Major version of this message */
		uint32_t bodyLen;      /**< Length of the body data */
		uint32_t serialNum;    /**< serial of this message */
		uint32_t headerLen;    /**< Length of the header data */
	};

	/**
	* AllJoyn Message Helper
	*/
	public value struct _AJ_Message sealed
	{
		uint32_t msgId;
		AJ_MsgHeader hdr;
		String^ objPath;
		uint32_t replySerial;
		String^ member;
		String^ error;
		String^ iface;
		String^ sender;
		String^ destination;
		String^ signature;
		uint32_t sessionId;
		uint32_t timestamp;
		uint32_t ttl;
	};

	/**
	* AllJoyn Message
	*/
	public ref struct AJ_Message sealed
	{
	public:
		_AJ_Message Get();

	internal:
		::AJ_Message _msg;
	};

	/**
	* Type for a message argument helper
	*/
	public value struct _AJ_Arg sealed
	{
		uint8_t		v_byte;        /**< byte type field value in the message */
		int16_t		v_int16;       /**< int16 type field value in the message */
		uint16_t	v_uint16;      /**< uint16 type field value in the message */
		uint32_t	v_bool;        /**< boolean type field value in the message */
		uint32_t	v_uint32;      /**< uint32 type field value in the message */
		int32_t		v_int32;       /**< int32 type field value in the message */
		int64_t		v_int64;       /**< int64 type field value in the message */
		uint64_t	v_uint64;      /**< uint64 type field value in the message */
		double		v_double;      /**< double type field value in the message */
		String^		v_string;      /**< string(char *) type field value in the message */
		String^		v_objPath;     /**< objPath(char *) type field value in the message */
		String^		v_signature;   /**< signature(char *) type field value in the message */
	};

	/**
	* Type for a message argument
	*/
	public ref struct AJ_Arg sealed
	{
		property _AJ_Arg val;

	internal:
		::AJ_Arg _arg;
	};

	/**
	* Type for AJ_StartClient() return
	*/
	public value struct AJ_Session
	{
		uint8_t status;
		uint32_t sessionId;
		String^ fullName;
	};

	/**
	* Delegate functions passed from Javascript
	*/
	public delegate String^ AJ_AuthPwdFunc();
	public delegate void AJ_PeerAuthenticateCallback(uint8_t status);
	public delegate AJ_Status AJ_BusPropGetCallback(AJ_Message^ replyMsg, uint32_t propId);
	public delegate AJ_Status AJ_BusPropSetCallback(AJ_Message^ replyMsg, uint32_t propId);
	public delegate AJ_Status AJ_UnmarshalArgsDelegate(AJ_Status status, IVector<Object^>^ args);

	/**
	* AllJoyn Windows Runtime
	*/
    public ref class AllJoyn sealed
    {
    public:
		AllJoyn();
		virtual ~AllJoyn();

		static void AJ_Initialize();
		static void AJ_PrintXML(const Array<AJ_Object^>^ localObjects);
		static void AJ_RegisterObjects(const Array<AJ_Object^>^ localObjects, const Array<AJ_Object^>^ proxyObjects);
		static IAsyncOperation<AJ_Session>^ AJ_StartClient(AJ_BusAttachment^ bus,
														   String^ daemonName,
														   uint32_t timeout,
														   uint8_t connected,
														   String^ name,
														   uint16_t port,
														   AJ_SessionOpts^ opts);
		static IAsyncOperation<AJ_Session>^ AJ_StartClientByName(AJ_BusAttachment^ bus,
															     String^ daemonName,
															     uint32_t timeout,
															     uint8_t connected,
															     String^ name,
															     uint16_t port,
															     AJ_SessionOpts^ opts);												   
		static IAsyncOperation<AJ_Status>^ AJ_StartService(AJ_BusAttachment^ bus,
															String^ daemonName,
															uint32_t timeout,
															uint8_t connected,
															uint16_t port,
															String^ name,
															uint32_t flags,
															AJ_SessionOpts^ opts);
		static void AJ_ReleaseObjects();
		static AJ_Status AJ_MarshalMethodCall(AJ_BusAttachment^ bus, AJ_Message^ msg, uint32_t msgId, String^ destination, AJ_SessionId sessionId, uint8_t flags, uint32_t timeout);
		static AJ_Status AJ_MarshalArgs(AJ_Message^ msg, String^ signature, const Array<String^>^ argsRT);
		static AJ_Status AJ_MarshalArg(AJ_Message^ msg, String^ signature, String^ args);
		static AJ_Status AJ_DeliverMsg(AJ_Message^ msg);
		static AJ_Status AJ_CloseMsg(AJ_Message^ msg);
		static IAsyncOperation<AJ_Status>^ AJ_UnmarshalMsg(AJ_BusAttachment^ bus, AJ_Message^ msg, uint32_t timeout);
		static AJ_Status AJ_UnmarshalArg(AJ_Message^ msg, AJ_Arg^ arg);
		static AJ_Status AJ_BusHandleBusMessage(AJ_Message^ msg);
		static AJ_Status AJ_BusFindAdvertisedName(AJ_BusAttachment^ bus, String^ namePrefix, uint8_t op);
		static AJ_Status AJ_FindBusAndConnect(AJ_BusAttachment^ bus, String^ serviceName, uint32_t timeout);
		static AJ_Status AJ_BusSetSignalRule(AJ_BusAttachment^ bus, String^ ruleString, uint8_t rule);
		static void AJ_Disconnect(AJ_BusAttachment^ bus);
		static AJ_Status AJ_BusJoinSession(AJ_BusAttachment^ bus, String^ sessionHost, uint16_t port, AJ_SessionOpts^ opts);
		static AJ_Status AJ_BusLeaveSession(AJ_BusAttachment^ bus, uint32_t sessionId);
		static AJ_Status AJ_MarshalSignal(AJ_BusAttachment^ bus, AJ_Message^ msg, uint32_t msgId, String^ destination, AJ_SessionId sessionId, uint8_t flags, uint32_t ttl);
		static Array<Object^>^ AJ_UnmarshalArgs(AJ_Message^ msg, String^ signature);
		static void AJ_BusSetPasswordCallback(AJ_BusAttachment^ bus, AJ_AuthPwdFunc^ pwdCallback);
		static AJ_Status AJ_BusAuthenticatePeer(AJ_BusAttachment^ bus, String^ peerBusName, AJ_PeerAuthenticateCallback^ pwdCallback);
		static AJ_Status AJ_BusReplyAcceptSession(AJ_Message^ msg, uint32_t accept);
		static AJ_Status AJ_MarshalReplyMsg(AJ_Message^ methodCall, AJ_Message^ reply);
		static AJ_Status AJ_MarshalErrorMsg(AJ_Message^ methodCall, AJ_Message^ reply, String^ error);
		static AJ_Status AJ_MarshalArg(AJ_Message^ msg, AJ_Arg^ arg);
		static void AJ_InitArg(AJ_Arg^ arg, uint8_t typeId, uint8_t flags, Object^ val, size_t len);
		static AJ_Status AJ_MarshalPropertyArgs(AJ_Message^ msg, uint32_t propId);
		static AJ_Status AJ_BusPropGet(AJ_Message^ msg, AJ_BusPropGetCallback^ callback);
		static AJ_Status AJ_BusPropSet(AJ_Message^ msg, AJ_BusPropSetCallback^ callback);
		static AJ_Status AJ_BusBindSessionPort(AJ_BusAttachment^ bus, uint16_t port, AJ_SessionOpts^ opts, uint8_t flags);
		static AJ_Status AJ_BusUnbindSession(AJ_BusAttachment^ bus, uint16_t port);
		static AJ_Status AJ_BusRequestName(AJ_BusAttachment^ bus, String^ name, uint32_t flags);
		static AJ_Status AJ_BusReleaseName(AJ_BusAttachment^ bus, String^ name);
		static AJ_Status AJ_BusAdvertiseName(AJ_BusAttachment^ bus, String^ name, uint16_t transportMask, uint8_t op, uint8_t flags);
		static AJ_Status AJ_SetProxyObjectPath(const Array<AJ_Object^>^ proxyObjects, uint32_t msgId, String^ objPath);
		static AJ_Status AJ_MarshalContainer(AJ_Message^ msg, AJ_Arg^ arg, uint8_t typeId);
		static AJ_Status AJ_MarshalCloseContainer(AJ_Message^ msg, AJ_Arg^ arg);
		static AJ_Status AJ_UnmarshalContainer(AJ_Message^ msg, AJ_Arg^ arg, uint8_t typeId);
		static AJ_Status AJ_UnmarshalCloseContainer(AJ_Message^ msg, AJ_Arg^ arg);
		static void AJ_UnmarshalArgsWithDelegate(AJ_Message^ msg, String^ signature, AJ_UnmarshalArgsDelegate^ func);

		/////////////////////////////////////////////////////////////////////////
		// Support functions for introspection
		/////////////////////////////////////////////////////////////////////////

		/*
		* AJ_DESCRIPTION_ID(BusObject base ID, Interface index, Member index, Arg index)
		* Interface, Member, and Arg indexes starts at 1 and represent the readible index in a list.
		* [ a, b, ... ] a would be index 1, b 2, etc.
		*/
		static uint32_t AJ_Description_ID(uint32_t o, uint32_t i, uint32_t m, uint32_t a);

		/*
		* Functions to encode a message or property id from object table index, object path, interface, and member indices.
		*/
		static uint32_t AJ_Encode_Message_ID(uint32_t o, uint32_t p, uint32_t i, uint32_t m);		/**< Encode a message id */
		static uint32_t AJ_Encode_Property_ID(uint32_t o, uint32_t p, uint32_t i, uint32_t m);		/**< Encode a property id */

		/*
		* Functions for encoding the standard bus and applications messages
		*/
		static uint32_t AJ_Bus_Message_ID(uint32_t p, uint32_t i, uint32_t m);						/**< Encode a message id from bus object */
		static uint32_t AJ_App_Message_ID(uint32_t p, uint32_t i, uint32_t m);						/**< Encode a message id from application object */
		static uint32_t AJ_Prx_Message_ID(uint32_t p, uint32_t i, uint32_t m);						/**< Encode a message id from proxy object */

		/*
		* Functions for encoding the standard bus and application properties
		*/
		static uint32_t AJ_Bus_Property_ID(uint32_t p, uint32_t i, uint32_t m);						/**< Encode a property id from bus object */
		static uint32_t AJ_App_Property_ID(uint32_t p, uint32_t i, uint32_t m);						/**< Encode a property id from application object */
		static uint32_t AJ_Prx_Property_ID(uint32_t p, uint32_t i, uint32_t m);						/**< Encode a property id from proxy object */

		/**
		* Function to generate the reply message identifier from method call message. This is the message
		* identifier in the reply context.
		*/
		static uint32_t AJ_Reply_ID(uint32_t id);

	private:
		static ::AJ_Object* RegisterObjects(const Array<AJ_Object^>^);
		static void ReleaseObjects(::AJ_Object*, const Array<AJ_Object^>^);
		static uint32_t PasswordCallback(uint8_t* buffer, uint32_t bufLen);
		static void AuthCallback(const void* context, ::AJ_Status status);
		static ::AJ_Status BusPropGetCallback(::AJ_Message* replyMsg, uint32_t propId, void* context);
		static ::AJ_Status BusPropSetCallback(::AJ_Message* replyMsg, uint32_t propId, void* context);
		static ::AJ_Status UnmarshalArgs(::AJ_Message* msg, const char** sig, Vector<Object^>^ args);
		static ::AJ_Status MarshalArgs(::AJ_Message* msg, const char** sig, std::vector<Object^>* args);
		static std::vector<char*> GetArrayArgs(String^ strVal);
		static std::vector<Object^> GetArrayArgsAsString(String^ strVal);
    };
}
