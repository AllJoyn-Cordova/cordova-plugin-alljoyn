package org.allseen.alljoyn;

import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import android.os.Looper;
import android.os.Message;

import org.apache.cordova.CallbackContext;
import org.apache.cordova.CordovaPlugin;
import org.apache.cordova.PluginResult;
import org.apache.cordova.CordovaInterface;
import org.apache.cordova.CordovaWebView;

import org.json.JSONArray;
import org.json.JSONObject;
import org.json.JSONException;

import java.util.Timer;
import java.util.TimerTask;
import java.util.HashMap;


public class AllJoynCordova extends CordovaPlugin
{
    /* Load the native alljoyn library. */
    static
    {
        System.loadLibrary("alljoyn");
    }

    private static final String TAG = "AllJoynCordova";
    private static final short  CONTACT_PORT=42;
    private static final String DAEMON_AUTH = "ALLJOYN_PIN_KEYX";
    private static final String DAEMON_PWD = "1234"; // 000000 or 1234
    private AJ_BusAttachment bus;

    private static final long AJ_MESSAGE_SLOW_LOOP_INTERVAL = 500;
    private static final long AJ_MESSAGE_FAST_LOOP_INTERVAL = 50;
    private static final long UNMARSHAL_TIMEOUT = 1000 * 5;
    private static final long CONNECT_TIMEOUT = 1000 * 60;
    private static final long METHOD_TIMEOUT = 100 * 10;

    private static final long AJ_SIGNAL_FOUND_ADV_NAME = (((alljoynConstants.AJ_BUS_ID_FLAG) << 24) | (((1)) << 16) | (((0)) << 8) | (1));   /**< signal for found advertising name */
    private static final long AJ_RED_ID_FLAG = 0x80;
    private static final long AJ_METHOD_JOIN_SESSION = ((long)(((long)alljoynConstants.AJ_BUS_ID_FLAG) << 24) | (((long)(1)) << 16) | (((long)(0)) << 8) | (10));

    private Timer m_pTimer = null;
    private boolean m_bStartTimer = false;
    private HashMap m_pMessageHandlers = new HashMap<String, String>();

    /**
     * Sets the context of the Command. This can then be used to do things like
     * get file paths associated with the Activity.
     *
     * @param cordova The context of the main Activity.
     * @param webView The CordovaWebView Cordova is running in.
     */
    @Override
    public void initialize(final CordovaInterface cordova, CordovaWebView webView)
    {
        super.initialize(cordova, webView);
        Log.i(TAG, "Initialization running.");
        alljoyn.AJ_Initialize();
        bus = new AJ_BusAttachment();

        // Initialize timer for msg loop
        m_pTimer = new Timer();
        m_pTimer.scheduleAtFixedRate
        (
                new TimerTask()
                {
                    @Override
                    public void run()
                    {
                        if (!m_bStartTimer)
                        {
                            return;
                        }

                        _AJ_Message msg = new _AJ_Message();
                        AJ_Status status = alljoyn.AJ_UnmarshalMsg(bus, msg, UNMARSHAL_TIMEOUT);

                        if (status == AJ_Status.AJ_OK)
                        {
                            final long msgId = msg.getMsgId();

                            if (m_pMessageHandlers.containsKey(msgId))
                            {
                                MsgHandler handler = (MsgHandler)m_pMessageHandlers.get(msgId);

                                try
                                {
                                    handler.callback(msg);
                                }
                                catch (Exception e)
                                {
                                    Log.i(TAG, e.toString());
                                }
                            }
                            else
                            {
                                /*
                                 * Pass to the built-in bus message handlers
                                 */
                                Log.i(TAG, "AJ_BusHandleBusMessage() msgId=" + msgId);
                                status = alljoyn.AJ_BusHandleBusMessage(msg);
                            }
                        }
                        else if(status == AJ_Status.AJ_ERR_TIMEOUT)
                        {
                            // Nothing to do for now, continue i guess
                            Log.i(TAG, "Timeout getting MSG. Will try again...");
                            status = AJ_Status.AJ_OK;
                        }
                        else if (status == AJ_Status.AJ_ERR_NO_MATCH)
                        {
                            // Ignore unknown messages
                            Log.i(TAG, "AJ_ERR_NO_MATCH in main loop. Ignoring!");
                            status = AJ_Status.AJ_OK;
                        }
                        else
                        {
                            Log.i(TAG, " -- MainLoopError AJ_UnmarshalMsg returned status=" + alljoyn.AJ_StatusText(status));
                        }

                        alljoyn.AJ_CloseMsg(msg);
                    }
                },
                AJ_MESSAGE_SLOW_LOOP_INTERVAL,
                AJ_MESSAGE_SLOW_LOOP_INTERVAL
        );

        Log.i(TAG, "Initialization completed.");
    }

    /**
     * Executes the request and returns PluginResult.
     *
     * @param action            The action to execute.
     * @param args              JSONArray of arguments for the plugin.
     * @param callbackContext   The callback context used when calling back into JavaScript.
     * @return                  True when the action was valid, false otherwise.
     */
    @Override
    public boolean execute(String action, JSONArray data, CallbackContext callbackContext) throws JSONException
    {
        if (action.equals("connect"))
        {
            String serviceName = data.getString(0);

            if (serviceName.length() == 0)
            {
                serviceName = null;
            }

            long timeout = data.getLong(1);
            AJ_Status status = null;
            Log.i(TAG, "AllJoyn.connect("+bus+","+serviceName+","+timeout+")");

            try
            {
                status = alljoyn.AJ_FindBusAndConnect(bus, serviceName, timeout);
            }
            catch (Exception e)
            {
                Log.i(TAG, "Exception finding and connecting to bus: " + e.toString());
            }

            Log.i(TAG, "Called AJ_FindBusAndConnect, status = " + status);

            if (status == AJ_Status.AJ_OK)
            {
                callbackContext.success("Connected to router!");
                return true;
            }
            else
            {
                callbackContext.error("Error connecting to router: " + status.toString());
                return false;
            }
        }
        else if (action.equals("registerObjects"))
        {
            AJ_Status status = null;
            AJ_Object local = null;
            JSONArray localObjects = null;
            JSONArray remoteObjects = null;
            AJ_Object remote = null;

            Log.i(TAG, "AllJoyn.registerObjects()");

            if (data.isNull(0))
            {
                Log.i(TAG, "AllJoyn.registerObjects: arg 0 null");
            }
            else
            {
                localObjects = data.getJSONArray(0);
                local = alljoyn.AJ_ObjectsCreate();

                for (int i = 0; i < localObjects.length() - 1; i++)
                {
                    JSONObject object = localObjects.getJSONObject(i);
                    AJ_Object nObj = new AJ_Object();

                    // Init path
                    nObj.setPath(object.getString("path"));

                    // Init interfaces
                    JSONArray interfacesDesc = object.getJSONArray("interfaces");
                    SWIGTYPE_p_p_p_char interfaces = alljoyn.AJ_InterfacesCreate();
                    for (int j = 0; j < interfacesDesc.length(); j++)
                    {
                        if (!interfacesDesc.isNull(j))
                        {
                            JSONArray interfaceDesc = interfacesDesc.getJSONArray(j);
                            SWIGTYPE_p_p_char ifaceMethods = null;

                            for (int k = 0; k < interfaceDesc.length(); k++)
                            {
                                if (ifaceMethods == null)
                                {
                                    ifaceMethods = alljoyn.AJ_InterfaceDescriptionCreate(interfaceDesc.getString(k));
                                }
                                else
                                {
                                    if (interfaceDesc.getString(k).length() > 0)
                                    {
                                        ifaceMethods = alljoyn.AJ_InterfaceDescriptionAdd(ifaceMethods, interfaceDesc.getString(k));
                                    }
                                }
                            }

                            interfaces = alljoyn.AJ_InterfacesAdd(interfaces, ifaceMethods);
                        }
                    }
                    nObj.setInterfaces(interfaces);

                    local = alljoyn.AJ_ObjectsAdd(local, nObj);
                }

                Log.i(TAG, "AllJoyn.registerObjects() Local: " + localObjects.toString() + " => " + local.toString());
            }

            if (data.isNull(1))
            {
                Log.i(TAG, "AllJoyn.registerObjects: arg 1 null");
            }
            else
            {
                remoteObjects = data.getJSONArray(1);
                remote = alljoyn.AJ_ObjectsCreate();

                for (int i = 0; i < remoteObjects.length() - 1; i++)
                {
                    JSONObject object = remoteObjects.getJSONObject(i);
                    AJ_Object nObj = new AJ_Object();

                    // Init path
                    nObj.setPath(object.getString("path"));

                    // Init interfaces
                    JSONArray interfacesDesc = object.getJSONArray("interfaces");
                    SWIGTYPE_p_p_p_char interfaces = alljoyn.AJ_InterfacesCreate();
                    for (int j = 0; j < interfacesDesc.length(); j++)
                    {
                        if (!interfacesDesc.isNull(j))
                        {
                            JSONArray interfaceDesc = interfacesDesc.getJSONArray(j);
                            SWIGTYPE_p_p_char ifaceMethods = null;
                            for (int k = 0; k < interfaceDesc.length(); k++)
                            {
                                if (ifaceMethods == null)
                                {
                                    ifaceMethods = alljoyn.AJ_InterfaceDescriptionCreate(interfaceDesc.getString(k));
                                }
                                else
                                {
                                    if (interfaceDesc.getString(k).length() > 0)
                                    {
                                        ifaceMethods = alljoyn.AJ_InterfaceDescriptionAdd(ifaceMethods, interfaceDesc.getString(k));
                                    }
                                }
                            }
                            interfaces = alljoyn.AJ_InterfacesAdd(interfaces, ifaceMethods);
                        }
                    }
                    nObj.setInterfaces(interfaces);

                    remote = alljoyn.AJ_ObjectsAdd(remote, nObj);
                }

                Log.i(TAG, "AllJoyn.registerObjects() Remote: " + remoteObjects.toString() + " => " + remote.toString());
            }

            alljoyn.AJ_RegisterObjects(local, remote);
            Log.i(TAG, "AllJoyn.registerObjects succeeded.");
            callbackContext.success("Registered objects!");
            return true;
        }
        else if (action.equals("addAdvertisedNameListener"))
        {
            Log.i(TAG, "AllJoyn.addAdvertisedNameListener");
            String serviceName = data.getString(0);
            AJ_Status status = alljoyn.AJ_BusFindAdvertisedName(bus, serviceName, alljoynConstants.AJ_BUS_START_FINDING);
            m_bStartTimer = true;

            if( status == AJ_Status.AJ_OK)
            {
                final long msgId = AJ_SIGNAL_FOUND_ADV_NAME;

                m_pMessageHandlers.put
                (
                    msgId,
                    new MsgHandler(callbackContext)
                    {
                        public boolean callback(_AJ_Message pMsg) throws JSONException
                        {
                            m_pMessageHandlers.remove(msgId);
                            _AJ_Arg arg = new _AJ_Arg();
                            alljoyn.AJ_UnmarshalArg(pMsg, arg);
                            Log.i(TAG, "FoundAdvertisedName(" + arg.getVal().getV_string() + ")");

                            // Init responseDictionary
                            JSONObject responseDictionary = new JSONObject();
                            responseDictionary.put("name", arg.getVal().getV_string());
                            responseDictionary.put("sender", pMsg.getSender());

                            // Init message info
                            JSONObject msgInfo = getMsgInfo(pMsg);

                            // Init callback results
                            JSONArray callbackResults = new JSONArray();
                            callbackResults.put(msgInfo);
                            callbackResults.put(responseDictionary);
                            callbackResults.put(null);

                            // Send plugin result
                            PluginResult pluginResult = new PluginResult(PluginResult.Status.OK, callbackResults);
                            pluginResult.setKeepCallback(true);
                            this.callbackContext.sendPluginResult(pluginResult);
                            return true;
                        }
                    }
                );

                return true;
            }
            else
            {
                callbackContext.error("Failure starting find");
                return false;
            }
        }
        else if (action.equals("setSignalRule"))
        {
            Log.i(TAG, "AllJoyn.setSignalRule");
            AJ_Status status = AJ_Status.AJ_OK;
            String ruleString = data.getString(0);
            int rule = data.getInt(1);

            try
            {
                status = alljoyn.AJ_BusSetSignalRule(bus, ruleString, rule);
            }
            catch (Exception e)
            {
                Log.i(TAG, "Exception in setSignalRule: " + e.toString());
            }

            if( status == AJ_Status.AJ_OK)
            {
                callbackContext.success("setSignalRule successfully!");
                return true;
            }
            else
            {
                callbackContext.error("Error in setSignalRule: " + status.toString());
                return false;
            }
        }
        else if (action.equals("addInterfacesListener"))
        {
            Log.i(TAG, "AllJoyn.addInterfacesListener");
            AJ_Status status = AJ_Status.AJ_OK;

            if (status == AJ_Status.AJ_OK)
            {
                callbackContext.success("Yay!");
                return true;
            }
            else
            {
                callbackContext.error("Error: " + status.toString());
                return false;
            }
        }
        else if (action.equals("addListener"))
        {
            Log.i(TAG, "AllJoyn.addListener");
            AJ_Status status = AJ_Status.AJ_OK;
            JSONArray indexList = data.getJSONArray(0);
            String responseType = data.getString(1);

            if (indexList == null || responseType == null)
            {
                Log.i(TAG, "addListener: Invalid argument.");
                callbackContext.error("Error: " + status.toString());
                return false;
            }

            if(indexList.length() < 4)
            {
                Log.i(TAG, "addListener: Expected 4 indices in indexList");
                callbackContext.error("Error: " + status.toString());
                return false;
            }

            Log.i(TAG, "indexList=" + indexList.toString());
            Log.i(TAG, "responseType=" + responseType.toString());

            int listIndex = indexList.getInt(0);
            int objectIndex = indexList.getInt(1);
            int interfaceIndex = indexList.getInt(2);
            int memberIndex = indexList.getInt(3);
            final long msgId = AJ_Encode_Message_ID(listIndex, objectIndex, interfaceIndex, memberIndex);

            m_pMessageHandlers.put
            (
                msgId,
                new MsgHandler(callbackContext)
                {
                    public boolean callback(_AJ_Message pMsg)
                    {
                        m_pMessageHandlers.remove(msgId);
                        this.callbackContext.success("Yay!");
                        return true;
                    }
                }
            );

            m_bStartTimer = true;

            if( status == AJ_Status.AJ_OK)
            {
                callbackContext.success("Yay!");
                return true;
            }
            else
            {
                callbackContext.error("Error: " + status.toString());
                return false;
            }
        }
        else if (action.equals("joinSession"))
        {
            Log.i(TAG, "AllJoyn.joinSession");
            AJ_Status status = AJ_Status.AJ_OK;

            if (data.isNull(0))
            {
                callbackContext.error("JoinSession: Invalid Argument");
                return false;
            }

            JSONObject server = data.getJSONObject(0);
            int port = (Integer)server.get("port");
            String name = (String)server.get("name");
            status = alljoyn.AJ_BusJoinSession(bus, name, port, null);

            if (status == AJ_Status.AJ_OK)
            {
                final long msgId = AJ_Reply_ID(AJ_METHOD_JOIN_SESSION);
                m_pMessageHandlers.put
                (
                    msgId,
                    new MsgHandler(callbackContext)
                    {
                        public boolean callback(_AJ_Message pMsg) throws JSONException
                        {
                            m_pMessageHandlers.remove(msgId);
                            Log.i(TAG, " -- Got reply to JoinSession ---");
                            Log.i(TAG, "MsgType: " + pMsg.getHdr().getMsgType());
                            long replyCode;
                            long sessionId;

                            if (pMsg.getHdr().getMsgType() == alljoynConstants.AJ_MSG_ERROR)
                            {
                                callbackContext.error("Failure joining session MSG ERROR");
                            }
                            else
                            {
//                                AJ_Status status = alljoyn.AJ_UnmarshalArgs(pMsg, "uu");
//                                replyCode = 0;
//                                sessionId = 0;
//                                Log.i(TAG, "replyCode=" + replyCode +  " sessionId=" + sessionId);
//
//                                if (replyCode == alljoynConstants.AJ_JOINSESSION_REPLY_SUCCESS)
//                                {
//
//                                }
//                                else
//                                {
//                                    if (replyCode == alljoynConstants.AJ_JOINSESSION_REPLY_ALREADY_JOINED)
//                                    {
//
//                                    }
//                                    else
//                                    {
//
//                                    }
//                                }
                            }

                            return true;
                        }
                    }
                );
            }
            else
            {
                callbackContext.error("Error: " + status.toString());
                return false;
            }
        }
        else if (action.equals("leaveSession"))
        {
            Log.i(TAG, "AllJoyn.leaveSession");
            AJ_Status status = AJ_Status.AJ_OK;

            if (status == AJ_Status.AJ_OK)
            {
                callbackContext.success("Yay!");
                return true;
            }
            else
            {
                callbackContext.error("Error: " + status.toString());
                return false;
            }
        }
        else if (action.equals("invokeMember"))
        {
            Log.i(TAG, "AllJoyn.invokeMember");
            AJ_Status status = AJ_Status.AJ_OK;

            if ( status == AJ_Status.AJ_OK)
            {
                callbackContext.success("Yay!");
                return true;
            }
            else
            {
                callbackContext.error("Error: " + status.toString());
                return false;
            }
        }

        return false;
    }

    long AJ_Encode_Message_ID(int o, int p, int i, int m)
    {
        return ((o << 24) | ((p) << 16) | ((i) << 8) | (m));
    }

    long AJ_Reply_ID(long id)
    {
        return ((id) | (long)((long)(AJ_RED_ID_FLAG) << 24));
    }

    JSONObject getMsgInfo(_AJ_Message pMsg) throws JSONException
    {
        JSONObject msgInfo = null;

        if (pMsg != null)
        {
            msgInfo = new JSONObject();

            if (pMsg.getSender() != null)
            {
                msgInfo.put("sender", pMsg.getSender());
            }

            if (pMsg.getSignature()!= null)
            {
                msgInfo.put("signature", pMsg.getSignature());
            }

            if (pMsg.getIface() != null)
            {
                msgInfo.put("iface", pMsg.getIface());
            }
        }

        return msgInfo;
    }

    public abstract class MsgHandler
    {
        public CallbackContext callbackContext;

        public MsgHandler(CallbackContext callbackContext)
        {
            this.callbackContext = callbackContext;
        }

        public abstract boolean callback(_AJ_Message pMsg) throws JSONException;
    }
}
