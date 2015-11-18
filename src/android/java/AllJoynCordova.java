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
import java.util.List;
import java.util.ArrayList;

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

    private static final long AJ_MESSAGE_SLOW_LOOP_INTERVAL = 500;
    private static final long AJ_MESSAGE_FAST_LOOP_INTERVAL = 50;
    private static final long UNMARSHAL_TIMEOUT = 1000 * 5;
    private static final long CONNECT_TIMEOUT = 1000 * 60;
    private static final long METHOD_TIMEOUT = 100 * 10;

    private static final long AJ_SIGNAL_FOUND_ADV_NAME = (((alljoynConstants.AJ_BUS_ID_FLAG) << 24) | (((1)) << 16) | (((0)) << 8) | (1));   /**< signal for found advertising name */
    private static final long AJ_RED_ID_FLAG = 0x80;
    private static final long AJ_METHOD_JOIN_SESSION = ((long)(((long)alljoynConstants.AJ_BUS_ID_FLAG) << 24) | (((long)(1)) << 16) | (((long)(0)) << 8) | (10));
    private static final long AJ_METHOD_BIND_SESSION_PORT = AJ_Encode_Message_ID(alljoynConstants.AJ_BUS_ID_FLAG, 1, 0, 8);
    private static final long AJ_METHOD_UNBIND_SESSION = AJ_Encode_Message_ID(alljoynConstants.AJ_BUS_ID_FLAG, 1, 0, 9);
    private static final long AJ_METHOD_ADVERTISE_NAME = AJ_Encode_Message_ID(alljoynConstants.AJ_BUS_ID_FLAG, 1, 0, 4);
    private static final long AJ_METHOD_RELEASE_NAME = AJ_Encode_Message_ID(alljoynConstants.AJ_BUS_ID_FLAG, 0, 0, 8);
    private static final long AJ_METHOD_ACCEPT_SESSION = AJ_Encode_Message_ID(alljoynConstants.AJ_BUS_ID_FLAG, 2, 0, 0);
    private static final long AJ_METHOD_REQUEST_NAME = AJ_Encode_Message_ID(alljoynConstants.AJ_BUS_ID_FLAG, 0, 0, 5);

    private AJ_BusAttachment bus;
    private AJ_Object proxyObjects;
    private AJ_Object appObjects;
    private Timer m_pTimer = null;
    private boolean m_bStartTimer = false;
    private HashMap m_pMessageHandlers = new HashMap<String, String>();
    private _AJ_Message m_pMsg = new _AJ_Message();

    // Indicates if there is a callback to the web app in progress
    // This usually means we need to stop processing messages on the loop until it is done
    boolean m_isCallbackInProgress = false;
    _AJ_Message m_pCallbackMessagePtr = null;

    // Indicates if the app is connected to the bus or not
    boolean m_isConnectedToBus;

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
        proxyObjects = new AJ_Object();
        appObjects = new AJ_Object();

        // Initialize timer for msg loop
        m_pTimer = new Timer();
        m_pTimer.scheduleAtFixedRate
        (
                new TimerTask()
                {
                    @Override
                    public void run()
                    {
                        if (!m_bStartTimer || !m_isConnectedToBus || m_isCallbackInProgress)
                        {
                            return;
                        }

                        AJ_Status status = alljoyn.AJ_UnmarshalMsg(bus, m_pMsg, UNMARSHAL_TIMEOUT);

                        if (status == AJ_Status.AJ_OK)
                        {
                            final long msgId = m_pMsg.getMsgId();
                            Log.i(TAG, "Received msgId: " + msgId);

                            if (m_pMessageHandlers.containsKey(msgId))
                            {
                                MsgHandler handler = (MsgHandler)m_pMessageHandlers.get(msgId);

                                try
                                {
                                    handler.callback(m_pMsg);
                                }
                                catch (Exception e)
                                {
                                    Log.i(TAG, "Error in msg loop: " + e.getMessage());
                                }
                            }
                            else
                            {
                                /*
                                 * Pass to the built-in bus message handlers
                                 */
                                Log.i(TAG, "AJ_BusHandleBusMessage() msgId=" + msgId);
                                status = alljoyn.AJ_BusHandleBusMessage(m_pMsg);
                            }

                            if (!m_isCallbackInProgress)
                            {
                                alljoyn.AJ_CloseMsg(m_pMsg);
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
                    }
                },
                AJ_MESSAGE_FAST_LOOP_INTERVAL,
                AJ_MESSAGE_FAST_LOOP_INTERVAL
        );

        m_isConnectedToBus = false;
        m_bStartTimer = false;

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
    public boolean execute(String action, final JSONArray data, final CallbackContext callbackContext) throws JSONException
    {
        if (action.equals("connect"))
        {
            Log.i(TAG, "AllJoyn.connect");
            if (!m_isConnectedToBus)
            {
                new BackgroundTask()
                {
                    public void run()
                    {
                        try
                        {
                            if (bus == null)
                            {
                                bus = new AJ_BusAttachment();
                            }

                            String serviceName = data.getString(0);

                            if (serviceName.length() == 0)
                            {
                                serviceName = null;
                            }

                            long timeout = data.getLong(1);
                            AJ_Status status = null;
                            Log.i(TAG, "AllJoyn.connect("+bus+","+serviceName+","+timeout+")");

                            status = alljoyn.AJ_FindBusAndConnect(bus, serviceName, timeout);

                            Log.i(TAG, "Called AJ_FindBusAndConnect, status = " + status);

                            if (status == AJ_Status.AJ_OK)
                            {
                                m_isConnectedToBus = true;
                                Log.i(TAG, "Connected to router!");
                                callbackContext.success("Connected to router!");
                            }
                            else
                            {
                                Log.i(TAG, "Error connecting to router!");
                                callbackContext.error("Error connecting to router: " + status.toString());
                            }
                        }
                        catch (Exception e)
                        {
                            Log.i(TAG, "Exception finding and connecting to bus: " + e.toString());
                        }
                    }
                };

                return true;
            }

            return true;
        }
        if (action.equals("disconnect"))
        {
            Log.i(TAG, "AllJoyn.disconnect");
            // Disconnect bus
            alljoyn.AJ_Disconnect(bus);
            m_isConnectedToBus = false;
            bus = null;
            System.gc();

            // Stop background tasks
            m_bStartTimer = false;
            callbackContext.success("Disconnected");
            return true;
        }
        else if (action.equals("registerObjects"))
        {
            new BackgroundTask()
            {
                public void run()
                {
                    try
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
                    }
                    catch (Exception e)
                    {
                        Log.i(TAG, "Exception finding and connecting to bus: " + e.toString());
                    }
                }
            };

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
                            _AJ_Arg arg = new _AJ_Arg();
                            alljoyn.AJ_UnmarshalArg(pMsg, arg);
                            Log.i(TAG, "FoundAdvertisedName(" + arg.getVal().getV_string() + ")");

                            // Send results
                            JSONObject responseDictionary = new JSONObject();
                            responseDictionary.put("name", arg.getVal().getV_string());
                            responseDictionary.put("sender", pMsg.getSender());
                            sendSuccessDictionary(responseDictionary, this.callbackContext, true, pMsg);
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
        else if (action.equals("replyAcceptSession"))
        {
            Log.i(TAG, "AllJoyn.replyAcceptSession");
            long msgId = data.getLong(0);
            long response = (data.getBoolean(1) == true ? 1 : 0);

            if (msgId == 0)
            {
                callbackContext.error("replyAcceptSession: Invalid argument");
            }
            else
            {
                // Make sure msgId matches current callback
                if (msgId == alljoyn.getMsgPointer(m_pCallbackMessagePtr))
                {
                    // Accept or reject session
                    AJ_Status status = alljoyn.AJ_BusReplyAcceptSession(m_pMsg, response);

                    if (status != AJ_Status.AJ_OK)
                    {
                        callbackContext.error("Error status: " + status);
                    }
                    else
                    {
                        callbackContext.success("replyAcceptSession: Success");
                        alljoyn.AJ_CloseMsg(m_pMsg);
                    }

                    // Unblock msg queue
                    m_pCallbackMessagePtr = null;
                    m_isCallbackInProgress = false;
                }
                else
                {
                    callbackContext.error("Mismatch, or i failed to compare");
                }
            }

            return true;
        }
        else if (action.equals("setAcceptSessionListener"))
        {
            Log.i(TAG, "AllJoyn.setAcceptSessionListener");
            final long acceptSessionKey = AJ_METHOD_ACCEPT_SESSION;

            m_pMessageHandlers.put
            (
                acceptSessionKey,
                new MsgHandler(callbackContext)
                {
                    public boolean callback(_AJ_Message pMsg) throws JSONException
                    {
                        // Save the msg and stop the msg oop
                        m_pCallbackMessagePtr = pMsg;
                        m_isCallbackInProgress = true;

                        JSONArray retObj = AJ_UnmarshalArgs(pMsg, "qus");
                        AJ_Status status = (AJ_Status)retObj.get(0);
                        JSONArray retArgs = retObj.getJSONArray(1);

                        JSONArray callbackArguments = new JSONArray();
                        callbackArguments.put(retArgs);
                        callbackArguments.put(alljoyn.getMsgPointer(pMsg));

                        sendSuccessMultipart(callbackArguments, this.callbackContext, true);
                        return true;
                    }
                }
            );

            return true;
        }
        else if (action.equals("addListenerForReply"))
        {
            Log.i(TAG, "AllJoyn.addListenerForReply");
            JSONArray indexList = data.getJSONArray(0);
            final String responseType = data.getString(1);

            if (indexList == null || responseType == null || responseType.equals("null"))
            {
                callbackContext.error("addListenerForReply: Invalid argument.");
                return false;
            }

            if(indexList.length() < 4)
            {
                callbackContext.error("addListenerForReply: Expected 4 indices in indexList");
                return false;
            }

            int listIndex = indexList.getInt(0);
            int objectIndex = indexList.getInt(1);
            int interfaceIndex = indexList.getInt(2);
            int memberIndex = indexList.getInt(3);
            long msgId = AJ_Encode_Message_ID(listIndex, objectIndex, interfaceIndex, memberIndex);

            Log.i(TAG, "Adding listener for msgId=" + msgId);

            AJ_MemberType memberType = alljoyn.AJ_GetMemberType(msgId, null, null);

            if(memberType == AJ_MemberType.AJ_INVALID_MEMBER)
            {
                callbackContext.error("addListenerForReply: Invalid message id/index list");
                return false;
            }

            final long methodKey = msgId;

            m_pMessageHandlers.put
            (
                methodKey,
                new MsgHandler(callbackContext)
                {
                    public boolean callback(_AJ_Message pMsg) throws JSONException
                    {
                        // Save the msg and stop the msg oop
                        m_pCallbackMessagePtr = pMsg;
                        m_isCallbackInProgress = true;

                        JSONArray retObj =  AJ_UnmarshalArgs(pMsg, responseType);
                        AJ_Status status = (AJ_Status)retObj.get(0);
                        JSONArray retArgs = retObj.getJSONArray(1);

                        JSONArray callbackArguments = new JSONArray();
                        callbackArguments.put(getMsgInfo(pMsg));
                        callbackArguments.put(retArgs);

                        if (status == AJ_Status.AJ_OK)
                        {
                            JSONArray msgWithResults = new JSONArray();
                            msgWithResults.put(callbackArguments);
                            msgWithResults.put(alljoyn.getMsgPointer(pMsg));
                            msgWithResults.put(null);
                            sendSuccessMultipart(msgWithResults, this.callbackContext, true);
                        }
                        else
                        {
                            callbackContext.error("Error " + alljoyn.AJ_StatusText(status));
                        }

                        return true;
                    }
                }
            );

            m_bStartTimer = true;
            return true;
        }
        else if (action.equals("sendErrorReply"))
        {
            Log.i(TAG, "AllJoyn.sendErrorReply");
            long msgId = data.getLong(0);
            String errorMessage = data.getString(1);

            if (msgId == 0)
            {
                callbackContext.error("sendErrorReply: Invalid argument");
            }
            else
            {
                // Make sure msgId matches current callback
                if (msgId == alljoyn.getMsgPointer(m_pCallbackMessagePtr))
                {
                    _AJ_Message replyMsg = new _AJ_Message();
                    AJ_Status status = alljoyn.AJ_MarshalErrorMsg(m_pMsg, replyMsg, errorMessage);

                    if (status != AJ_Status.AJ_OK)
                    {
                        callbackContext.error("Error status: " + status);
                    }
                    else
                    {
                        status = alljoyn.AJ_DeliverMsg(replyMsg);

                        if (status == AJ_Status.AJ_OK)
                        {
                            callbackContext.success("success");
                        }
                        else
                        {
                            callbackContext.error("Error status: " + status);
                        }

                        alljoyn.AJ_CloseMsg(replyMsg);
                    }

                    alljoyn.AJ_CloseMsg(m_pMsg);

                    // Unblock msg queue
                    m_pCallbackMessagePtr = null;
                    m_isCallbackInProgress = false;
                }
                else
                {
                    callbackContext.error("replyMessage: Invalid argument (msgId mismatch)");
                }
            }

            return true;
        }
        else if (action.equals("sendSuccessReply"))
        {
            Log.i(TAG, "AllJoyn.sendSuccessReply");
            long msgId = data.getLong(0);
            String replyArgumentSignature = data.getString(1);

            if (replyArgumentSignature == null || replyArgumentSignature.equals("null"))
            {
                replyArgumentSignature = "";
            }

            JSONArray replyArguments = data.getJSONArray(2);

            if (replyArguments == null)
            {
                replyArguments = new JSONArray();
            }

            if (msgId == 0)
            {
                callbackContext.error("sendSuccessReply: Invalid argument");
            }
            else
            {
                // Make sure msgId matches current callback
                if (msgId == alljoyn.getMsgPointer(m_pCallbackMessagePtr))
                {
                    _AJ_Message replyMsg = new _AJ_Message();
                    AJ_Status status = alljoyn.AJ_MarshalReplyMsg(m_pMsg, replyMsg);

                    if (status != AJ_Status.AJ_OK)
                    {
                        callbackContext.error("Error status: " + status);
                    }
                    else
                    {
                        status = AJ_MarshalArgs(replyMsg, replyArgumentSignature, replyArguments);

                        if (status == AJ_Status.AJ_OK)
                        {
                            status = alljoyn.AJ_DeliverMsg(replyMsg);

                            if (status == AJ_Status.AJ_OK)
                            {
                                callbackContext.success("success");
                            }
                            else
                            {
                                callbackContext.error("Error status: " + status);
                            }
                        }
                        else
                        {
                            callbackContext.error("Error status: " + status);
                        }

                        alljoyn.AJ_CloseMsg(replyMsg);
                    }

                    alljoyn.AJ_CloseMsg(m_pMsg);

                    // Unblock msg queue
                    m_pCallbackMessagePtr = null;
                    m_isCallbackInProgress = false;
                }
                else
                {
                    callbackContext.error("replyMessage: Invalid argument (msgId mismatch)");
                }
            }

            return true;
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
        else if (action.equals("addListener"))
        {
            Log.i(TAG, "AllJoyn.addListener");
            AJ_Status status = AJ_Status.AJ_OK;
            JSONArray indexList = data.getJSONArray(0);
            final String responseType = data.getString(1);

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
                    public boolean callback(_AJ_Message pMsg) throws JSONException
                    {
                        JSONArray retObj =  AJ_UnmarshalArgs(pMsg, responseType);
                        AJ_Status status = (AJ_Status)retObj.get(0);
                        JSONArray retArgs = retObj.getJSONArray(1);

                        if (status != AJ_Status.AJ_OK)
                        {
                            callbackContext.error("Failure unmarshalling response: " + alljoyn.AJ_StatusText(status));
                            return true;
                        }

                        sendSuccessArray(retArgs, this.callbackContext, true, pMsg);
                        return true;
                    }
                }
            );

            m_bStartTimer = true;
            return true;
        }
        else if (action.equals("startAdvertisingName"))
        {
            final String nameToAdvertise = data.getString(0);
            final int portToHostOn = data.getInt(1);

            if (nameToAdvertise == null || nameToAdvertise.equals("null"))
            {
                callbackContext.error("startAdvertisingName: Invalid argument(s)");
                return false;
            }

            AJ_Status status = AJ_Status.AJ_OK;
            AJ_SessionOpts sessionOptions = null;

            Log.i(TAG, "Calling AJ_BusBindSessionPort Port=" + portToHostOn);
            status = alljoyn.AJ_BusBindSessionPort(bus, portToHostOn, sessionOptions, 0);

            if (status == AJ_Status.AJ_OK)
            {
                final long bindSessionPortReplyKey = AJ_Reply_ID(AJ_METHOD_BIND_SESSION_PORT);

                m_pMessageHandlers.put
                (
                    bindSessionPortReplyKey,
                    new MsgHandler(callbackContext)
                    {
                        public boolean callback(_AJ_Message pMsg) throws JSONException
                        {
                            m_pMessageHandlers.remove(bindSessionPortReplyKey);
                            Log.i(TAG, "Got bindSessionPort reply");
                            Log.i(TAG, "Calling AJ_BusRequestName for " + nameToAdvertise);
                            AJ_Status status = alljoyn.AJ_BusRequestName(bus, nameToAdvertise, 0);

                            if (status == AJ_Status.AJ_OK)
                            {
                                final long requestNameReplyKey = AJ_Reply_ID(AJ_METHOD_REQUEST_NAME);

                                m_pMessageHandlers.put
                                (
                                    requestNameReplyKey,
                                    new MsgHandler(callbackContext)
                                    {
                                        public boolean callback(_AJ_Message pMsg) throws JSONException
                                        {
                                            m_pMessageHandlers.remove(requestNameReplyKey);
                                            Log.i(TAG, "Got busRequestName reply");
                                            Log.i(TAG, "Calling AJ_BusAdvertiseName");
                                            AJ_Status status = alljoyn.AJ_BusAdvertiseName(bus, nameToAdvertise, alljoynConstants.AJ_TRANSPORT_ANY, alljoynConstants.AJ_BUS_START_ADVERTISING, 0);

                                            if (status == AJ_Status.AJ_OK)
                                            {
                                                final long busAdvertiseNameReplyKey = AJ_Reply_ID(AJ_METHOD_ADVERTISE_NAME);

                                                m_pMessageHandlers.put
                                                (
                                                    busAdvertiseNameReplyKey,
                                                    new MsgHandler(callbackContext)
                                                    {
                                                        public boolean callback(_AJ_Message pMsg) throws JSONException
                                                        {
                                                            m_pMessageHandlers.remove(busAdvertiseNameReplyKey);
                                                            Log.i(TAG, "Got busAdvertiseName Reply");

                                                            if (pMsg == null || pMsg.getHdr() == null || pMsg.getHdr().getMsgType() == alljoynConstants.AJ_MSG_ERROR)
                                                            {
                                                                callbackContext.error("startAdvertisingName: Failure reply received.");
                                                            }
                                                            else
                                                            {
                                                                Log.i(TAG, "About INIT!");
                                                                AJ_Status status = alljoyn.AJ_AboutInit(bus, portToHostOn);

                                                                if (status != AJ_Status.AJ_OK)
                                                                {
                                                                    Log.i(TAG, "Failure initializing about " + alljoyn.AJ_StatusText(status));
                                                                }

                                                                Log.i(TAG, "startAdvertisingName: Success");
                                                                callbackContext.success("startAdvertisingName: Success");
                                                            }

                                                            return true; // busAdvertiseNameReply
                                                        }
                                                    }
                                                );
                                            }
                                            else
                                            {
                                                callbackContext.error("startAdvertisingName: Failure in AJ_BusAdvertiseName " + alljoyn.AJ_StatusText(status));
                                            }

                                            return true; // requestNameReplyHandler
                                        }
                                    }
                                );
                            }
                            else
                            {
                                callbackContext.error("startAdvertisingName: Failure in AJ_BusRequestName " + alljoyn.AJ_StatusText(status));
                            }

                            return true; // bindSessionPortHandler
                        }
                    }
                );
            }
            else
            {
                callbackContext.error("startAdvertisingName: Failure in AJ_BusBindSessionPort " + alljoyn.AJ_StatusText(status));
                return false;
            }

            return true;
        }
        else if (action.equals("stopAdvertisingName"))
        {
            final String wellKnownName = data.getString(0);
            final int port = data.getInt(1);
            AJ_Status status = alljoyn.AJ_BusUnbindSession(bus, port);

            if(status == AJ_Status.AJ_OK)
            {
                final long unbindSessionReplyKey = AJ_Reply_ID(AJ_METHOD_UNBIND_SESSION);

                m_pMessageHandlers.put
                (
                    unbindSessionReplyKey,
                    new MsgHandler(callbackContext)
                    {
                        public boolean callback(_AJ_Message pMsg) throws JSONException
                        {
                            m_pMessageHandlers.remove(unbindSessionReplyKey);

                            if (pMsg == null || pMsg.getHdr() == null || pMsg.getHdr().getMsgType() == alljoynConstants.AJ_MSG_ERROR)
                            {
                                callbackContext.error("stopAdvertisingName has failed with status: " + AJ_Status.AJ_ERR_FAILURE);
                            }
                            else
                            {
                                AJ_Status status = alljoyn.AJ_BusReleaseName(bus, wellKnownName);

                                if (status == AJ_Status.AJ_OK)
                                {
                                    final long releaseNameReplyKey = AJ_Reply_ID(AJ_METHOD_RELEASE_NAME);

                                    m_pMessageHandlers.put
                                    (
                                        releaseNameReplyKey,
                                        new MsgHandler(callbackContext)
                                        {
                                            public boolean callback(_AJ_Message pMsg) throws JSONException
                                            {
                                                m_pMessageHandlers.remove(releaseNameReplyKey);

                                                if (pMsg == null || pMsg.getHdr() == null || pMsg.getHdr().getMsgType() == alljoynConstants.AJ_MSG_ERROR)
                                                {
                                                    callbackContext.error("stopAdvertisingName has failed with status: " + AJ_Status.AJ_ERR_FAILURE);
                                                }
                                                else
                                                {
                                                    AJ_Status status = alljoyn.AJ_BusAdvertiseName(bus, wellKnownName, alljoynConstants.AJ_TRANSPORT_ANY, alljoynConstants.AJ_BUS_STOP_ADVERTISING, 0);

                                                    if (status == AJ_Status.AJ_OK)
                                                    {
                                                        final long stopAdvertiseNameReplyKey = AJ_Reply_ID(AJ_METHOD_ADVERTISE_NAME);

                                                        m_pMessageHandlers.put
                                                        (
                                                            stopAdvertiseNameReplyKey,
                                                            new MsgHandler(callbackContext)
                                                            {
                                                                public boolean callback(_AJ_Message pMsg) throws JSONException
                                                                {
                                                                    m_pMessageHandlers.remove(stopAdvertiseNameReplyKey);

                                                                    if (pMsg == null || pMsg.getHdr() == null || pMsg.getHdr().getMsgType() == alljoynConstants.AJ_MSG_ERROR)
                                                                    {
                                                                        callbackContext.error("stopAdvertisingName has failed with status: " + AJ_Status.AJ_ERR_FAILURE);
                                                                    }
                                                                    else
                                                                    {
                                                                        callbackContext.success("stopAdvertisingName: Success");
                                                                    }

                                                                    return true;
                                                                }
                                                            }
                                                        );
                                                    }
                                                    else
                                                    {
                                                        callbackContext.error("stopAdvertisingName has failed with status: " + alljoyn.AJ_StatusText(status));
                                                    }
                                                }

                                                return true;
                                            }
                                        }
                                    );

                                    return true;
                                }

                                callbackContext.success("startAdvertisingName: Success");
                            }

                            return true;
                        }
                    }
                );

                return true;
            }
            else
            {
                callbackContext.error("stopAdvertisingName has failed.");
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
            final String name = (String)server.get("name");
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
                                JSONArray args = AJ_UnmarshalArgs(pMsg, "uu");
                                replyCode = args.getJSONArray(1).getLong(0);
                                sessionId = args.getJSONArray(1).getLong(1);
                                Log.i(TAG, "replyCode=" + replyCode +  " sessionId=" + sessionId);

                                if (replyCode == alljoynConstants.AJ_JOINSESSION_REPLY_SUCCESS)
                                {
                                    // Init responseArray
                                    JSONArray responseArray = new JSONArray();
                                    responseArray.put(sessionId);
                                    responseArray.put(name);
                                    sendSuccessArray(responseArray, this.callbackContext, false, pMsg);
                                    return true;
                                }
                                else
                                {
                                    if (replyCode == alljoynConstants.AJ_JOINSESSION_REPLY_ALREADY_JOINED)
                                    {
                                        // Init responseArray
                                        JSONArray responseArray = new JSONArray();
                                        responseArray.put(pMsg.getSessionId());
                                        responseArray.put(name);
                                        sendSuccessArray(responseArray, this.callbackContext, false, pMsg);
                                        return true;
                                    }
                                    else
                                    {
                                        callbackContext.error("Failure joining session replyCode = " + replyCode);
                                        return false;
                                    }
                                }
                            }

                            return true;
                        }
                    }
                );

                return true;
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
            long sessionId = data.getLong(0);
            AJ_Status status = alljoyn.AJ_BusLeaveSession(bus, sessionId);

            if (status == AJ_Status.AJ_OK)
            {
                callbackContext.success("Left session " + sessionId);
                return true;
            }
            else
            {
                callbackContext.error("Failed to leave session " + sessionId + ". Reason = " + alljoyn.AJ_StatusText(status));
                return false;
            }
        }
        else if (action.equals("invokeMember"))
        {
            new BackgroundTask()
            {
                public void run()
                {
                    try
                    {
                        Log.i(TAG, "AllJoyn.invokeMember");
                        long sessionId = data.getLong(0);
                        String destination = data.getString(1);
                        String signature = data.getString(2);
                        String path = data.getString(3);
                        JSONArray indexList = data.getJSONArray(4);
                        String parameterTypes = data.getString(5);
                        JSONArray parameters = data.getJSONArray(6);
                        final String outParameterSignature = (data.length() == 7) ? null : data.getString(7);
                        boolean isOwnSession = false;
                        AJ_Status status = AJ_Status.AJ_OK;

                        if (signature == null || indexList == null)
                        {
                            callbackContext.error("invokeMember: Invalid Argument");
                            return;
                        }

                        if (indexList.length() < 4)
                        {
                            callbackContext.error("invokeMember: Expected 4 indices in indexList");
                            return;
                        }

                        int listIndex = indexList.getInt(0);
                        int objectIndex = indexList.getInt(1);
                        int interfaceIndex = indexList.getInt(2);
                        int memberIndex = indexList.getInt(3);

                        if (sessionId == 0)
                        {
                            Log.i(TAG, "SessionId is 0, overriding listIndex to 1");
                            listIndex = 1;
                            isOwnSession = true;
                        }

                        long msgId = AJ_Encode_Message_ID(listIndex, objectIndex, interfaceIndex, memberIndex);
                        Log.i(TAG, "Message id: " + msgId);

                        SWIGTYPE_p_p_char memberSignature = new SWIGTYPE_p_p_char();
                        SWIGTYPE_p_uint8_t isSecure = new SWIGTYPE_p_uint8_t();

                        AJ_MemberType memberType = alljoyn.AJ_GetMemberType(msgId, memberSignature, isSecure);
                        _AJ_Message msg = new _AJ_Message();

                        if (path != null && path.length() > 0 && !path.equals("null")) // Checking null parameter passed from JS layer
                        {
                            status = alljoyn.AJ_SetProxyObjectPath(proxyObjects, msgId, path);

                            if(status != AJ_Status.AJ_OK)
                            {
                                Log.i(TAG, "AJ_SetProxyObjectPath failed with " + alljoyn.AJ_StatusText(status));
                                callbackContext.error("InvokeMember failure: " + alljoyn.AJ_StatusText(status));
                                return;
                            }
                        }

                        String destinationChars = "";

                        if (destination != null && !destination.equals("null"))
                        {
                            destinationChars = destination;
                        }

                        if (memberType == AJ_MemberType.AJ_METHOD_MEMBER)
                        {
                            status = alljoyn.AJ_MarshalMethodCall(bus, msg, msgId, destinationChars, sessionId, 0, 0);

                            if (status != AJ_Status.AJ_OK)
                            {
                                Log.i(TAG, "Failure marshalling method call");
                                callbackContext.error("InvokeMember failure: " + alljoyn.AJ_StatusText(status));
                                return;
                            }

                            if (parameterTypes != null && parameterTypes.length() > 0 && !parameterTypes.equals("null"))
                            {
                                status = AJ_MarshalArgs(msg, parameterTypes, parameters);
                            }
                        }
                        else if (memberType == AJ_MemberType.AJ_SIGNAL_MEMBER)
                        {
                            int signalFlags = 0;
                            long ttl = 0;

                            if (isOwnSession)
                            {
                                signalFlags = alljoynConstants.AJ_FLAG_GLOBAL_BROADCAST;
                            }

                            if (sessionId == 0 && destinationChars == "")
                            {
                                Log.i(TAG, "Sessionless signal");
                                signalFlags |= alljoynConstants.AJ_FLAG_SESSIONLESS;
                            }

                            status = alljoyn.AJ_MarshalSignal(bus, msg, msgId, destinationChars, sessionId, signalFlags, ttl);

                            if (status != AJ_Status.AJ_OK)
                            {
                                Log.i(TAG, "AJ_MarshalSignal failed with " + alljoyn.AJ_StatusText(status));
                                callbackContext.error("InvokeMember failure: " + alljoyn.AJ_StatusText(status));
                                return;
                            }

                            if (parameterTypes != null && parameterTypes.length() > 0)
                            {
                                status = AJ_MarshalArgs(msg, parameterTypes, parameters);

                                if (status != AJ_Status.AJ_OK)
                                {
                                    Log.i(TAG, "Failure marshalling arguments: " + alljoyn.AJ_StatusText(status));
                                    callbackContext.error("InvokeMember failure: " + alljoyn.AJ_StatusText(status));
                                    return;
                                }
                            }
                        }
                        else if (memberType == AJ_MemberType.AJ_PROPERTY_MEMBER)
                        {
                            // Do nothing
                        }
                        else
                        {
                            status = AJ_Status.AJ_ERR_FAILURE;
                        }

                        if (AJ_Status.AJ_OK == status)
                        {
                            status = alljoyn.AJ_DeliverMsg(msg);

                            if (memberType != AJ_MemberType.AJ_SIGNAL_MEMBER)
                            {
                                final long replyMsgId = AJ_Reply_ID(msgId);

                                m_pMessageHandlers.put
                                (
                                    replyMsgId,
                                    new MsgHandler(callbackContext)
                                    {
                                        public boolean callback(_AJ_Message pMsg) throws JSONException
                                        {
                                            AJ_Status status = AJ_Status.AJ_OK;
                                            JSONArray outValues = null;
                                            m_pMessageHandlers.remove(replyMsgId);

                                            if (pMsg == null || pMsg.getHdr() == null)
                                            {
                                                // Error
                                                callbackContext.error("Error" + alljoyn.AJ_StatusText(status));
                                                return true;
                                            }

                                            if (pMsg.getHdr().getMsgType() == alljoynConstants.AJ_MSG_ERROR)
                                            {
                                                callbackContext.error(pMsg.getError());
                                                return true;
                                            }

                                            if (outParameterSignature != null && outParameterSignature.length() > 0 && !outParameterSignature.equals("null"))
                                            {
                                                JSONArray retObj =  AJ_UnmarshalArgs(pMsg, outParameterSignature);
                                                status = (AJ_Status)retObj.get(0);
                                                outValues = retObj.getJSONArray(1);
                                            }

                                            if (status != AJ_Status.AJ_OK)
                                            {
                                                callbackContext.error("Failure unmarshalling response: " + alljoyn.AJ_StatusText(status));
                                                return true;
                                            }

                                            sendSuccessArray(outValues, this.callbackContext, false, pMsg);
                                            return true;
                                        }
                                    }
                                );
                            }
                            else
                            {
                                callbackContext.success("Send signal successfully!");
                            }
                        }
                    }
                    catch (Exception e)
                    {
                        Log.i(TAG, "Exception: " + e.toString());
                    }
                }
            };

            return true;
        }

        return false;
    }

    public static long AJ_Encode_Message_ID(int o, int p, int i, int m)
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

    void sendSuccessArray(JSONArray argumentValues, CallbackContext callbackContext, boolean keepCallback, _AJ_Message pMsg) throws JSONException
    {
        // Init message info
        JSONObject msgInfo = getMsgInfo(pMsg);

        // Init callback results
        JSONArray callbackResults = new JSONArray();
        callbackResults.put(msgInfo);
        callbackResults.put(argumentValues);
        callbackResults.put(null);

        // Send plugin result
        PluginResult pluginResult = new PluginResult(PluginResult.Status.OK, callbackResults);
        pluginResult.setKeepCallback(keepCallback);
        callbackContext.sendPluginResult(pluginResult);
    }

    void sendSuccessMultipart(JSONArray array, CallbackContext callbackContext, boolean keepCallback) throws JSONException
    {
        // Convert from JSONArray to List
        List<PluginResult> results = new ArrayList<PluginResult>();

        for (int i = 0; i < array.length(); i++)
        {
            if (array.isNull(i))
            {
                results.add(i, new PluginResult(PluginResult.Status.OK));
            }
            else if (array.get(i) instanceof String)
            {
                results.add(i, new PluginResult(PluginResult.Status.OK, array.getString(i)));
            }
            else if (array.get(i) instanceof JSONArray)
            {
                results.add(i, new PluginResult(PluginResult.Status.OK, array.getJSONArray(i)));
            }
            else if (array.get(i) instanceof JSONObject )
            {
                results.add(i, new PluginResult(PluginResult.Status.OK, array.getJSONObject(i)));
            }
            else if (array.get(i) instanceof Integer)
            {
                results.add(i, new PluginResult(PluginResult.Status.OK, array.getInt(i)));
            }
            else if (array.get(i) instanceof Float)
            {
                results.add(i, new PluginResult(PluginResult.Status.OK, new Float(array.getDouble(i))));
            }
            else if (array.get(i) instanceof Boolean)
            {
                results.add(i, new PluginResult(PluginResult.Status.OK, array.getBoolean(i)));
            }
        }

        // Send plugin result
        PluginResult pluginResult = new PluginResult(PluginResult.Status.OK, results);
        pluginResult.setKeepCallback(keepCallback);
        callbackContext.sendPluginResult(pluginResult);
    }

    void sendSuccessDictionary(JSONObject argumentValues, CallbackContext callbackContext, boolean keepCallback, _AJ_Message pMsg) throws JSONException
    {
        // Init message info
        JSONObject msgInfo = getMsgInfo(pMsg);

        // Init callback results
        JSONArray callbackResults = new JSONArray();
        callbackResults.put(msgInfo);
        callbackResults.put(argumentValues);
        callbackResults.put(null);

        // Send plugin result
        PluginResult pluginResult = new PluginResult(PluginResult.Status.OK, callbackResults);
        pluginResult.setKeepCallback(keepCallback);
        callbackContext.sendPluginResult(pluginResult);
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

    public abstract class BackgroundTask implements Runnable
    {
        public BackgroundTask()
        {
            new Thread(this).start();
        }

        public abstract void run();
    }

    // --------------------------------------------------------------------------
    // Marshal and Unmarshal
    // --------------------------------------------------------------------------

    private static final int AJ_SCALAR         = 0x10;
    private static final int AJ_CONTAINER      = 0x20;
    private static final int AJ_STRING         = 0x40;
    private static final int AJ_VARIANT        = 0x80;

    /**
     * Characterizes the various argument types
     */
    static final int TypeFlags[] = new int[]
    {
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

    /*
    * Message argument types
    */
    private static final int AJ_ARG_INVALID             = '\0';     /**< AllJoyn invalid type */
    private static final int AJ_ARG_ARRAY               = 'a';      /**< AllJoyn array container type */
    private static final int AJ_ARG_BOOLEAN             = 'b';      /**< AllJoyn boolean basic type */
    private static final int AJ_ARG_DOUBLE              = 'd';      /**< AllJoyn IEEE 754 double basic type */
    private static final int AJ_ARG_SIGNATURE           = 'g';      /**< AllJoyn signature basic type */
    private static final int AJ_ARG_HANDLE              = 'h';      /**< AllJoyn socket handle basic type */
    private static final int AJ_ARG_INT32               = 'i';      /**< AllJoyn 32-bit signed integer basic type */
    private static final int AJ_ARG_INT16               = 'n';      /**< AllJoyn 16-bit signed integer basic type */
    private static final int AJ_ARG_OBJ_PATH            = 'o';      /**< AllJoyn Name of an AllJoyn object instance basic type */
    private static final int AJ_ARG_UINT16              = 'q';      /**< AllJoyn 16-bit unsigned integer basic type */
    private static final int AJ_ARG_STRING              = 's';      /**< AllJoyn UTF-8 NULL terminated string basic type */
    private static final int AJ_ARG_UINT64              = 't';      /**< AllJoyn 64-bit unsigned integer basic type */
    private static final int AJ_ARG_UINT32              = 'u';      /**< AllJoyn 32-bit unsigned integer basic type */
    private static final int AJ_ARG_VARIANT             = 'v';      /**< AllJoyn variant container type */
    private static final int AJ_ARG_INT64               = 'x';      /**< AllJoyn 64-bit signed integer basic type */
    private static final int AJ_ARG_BYTE                = 'y';      /**< AllJoyn 8-bit unsigned integer basic type */
    private static final int AJ_ARG_STRUCT              = '(';      /**< AllJoyn struct container type */
    private static final int AJ_ARG_DICT_ENTRY          = '{';      /**< AllJoyn dictionary or map container type - an array of key-value pairs */
    private static final int AJ_STRUCT_CLOSE            = ')';
    private static final int AJ_DICT_ENTRY_CLOSE        = '}';

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
    int TYPE_FLAG(char typeId)
    {
        return TypeFlags[((typeId) == '(' || (typeId) == ')') ? (typeId) - '(' : (((typeId) < 'a' || (typeId) > '}') ? '}' + 2 - 'a' : (typeId) + 2 - 'a') ];
    }

    boolean AJ_IsContainerType(char typeId)
    {
        return (TYPE_FLAG(typeId) & AJ_CONTAINER) != 0;
    }

    /*
     *  Returns true if the specified type is represented as a number
     */
    boolean AJ_IsScalarType(char typeId)
    {
        return (TYPE_FLAG(typeId) & AJ_SCALAR) != 0;
    }

    boolean AJ_IsStringType(char typeId)
    {
        return (TYPE_FLAG(typeId) & AJ_STRING) != 0;
    }

    /*
     * A basic type is a scalar or one of the string types
     */
    boolean AJ_IsBasicType(char typeId)
    {
        return (TYPE_FLAG(typeId) & (AJ_STRING | AJ_SCALAR)) != 0;
    }

    int AJ_GetTypeSize(char typeId)
    {
        return (TYPE_FLAG(typeId) & 0xF);
    }

    AJ_Status UnmarshalArgs(_AJ_Message msg, StringBuffer sig, JSONArray args, StringBuffer nested) throws JSONException
    {
        _AJ_Arg structArg = new _AJ_Arg();
        _AJ_Arg arg = new _AJ_Arg();
        AJ_Status status = AJ_Status.AJ_OK;

        while (sig.length() != 0)
        {
            char typeId = sig.charAt(0);
            nested.append(typeId);
            sig.deleteCharAt(0);
            char nextTypeId = (sig.length() == 0) ? '\0' : sig.charAt(0);

            if (!AJ_IsBasicType(typeId))
            {
                if ((typeId == AJ_ARG_STRUCT) || (typeId == AJ_ARG_DICT_ENTRY))
                {
                    status = alljoyn.AJ_UnmarshalContainer(msg, structArg, typeId);

                    if (status != AJ_Status.AJ_OK)
                    {
                        break;
                    }

                    status = UnmarshalArgs(msg, sig, args, nested);

                    if (status == AJ_Status.AJ_OK)
                    {
                        int lastNestedTypeId = nested.charAt(nested.length() - 1);

                        if ((lastNestedTypeId == AJ_STRUCT_CLOSE) || (lastNestedTypeId == AJ_DICT_ENTRY_CLOSE))
                        {
                            status = alljoyn.AJ_UnmarshalCloseContainer(msg, structArg);

                            if (status != AJ_Status.AJ_OK)
                            {
                                break;
                            }
                        }
                        else
                        {
                            status = AJ_Status.AJ_ERR_MARSHAL;
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
                        _AJ_Arg arrayArg = new _AJ_Arg();
                        status = alljoyn.AJ_UnmarshalContainer(msg, arrayArg, AJ_ARG_ARRAY);
                        JSONArray vArgs = new JSONArray();

                        do
                        {
                            _AJ_Arg inArg = new _AJ_Arg();
                            status = alljoyn.AJ_UnmarshalArg(msg, arg);

                            if (status != AJ_Status.AJ_OK)
                            {
                                break;
                            }

                            vArgs.put(inArg.getVal().getV_string());
                        }
                        while (status == AJ_Status.AJ_OK);

                        args.put(vArgs);
                        status = alljoyn.AJ_UnmarshalCloseContainer(msg, arrayArg);
                    }
                    else
                    {
                        _AJ_Arg arrayArg = new _AJ_Arg();
                        status = alljoyn.AJ_UnmarshalContainer(msg, arrayArg, AJ_ARG_ARRAY);
                        JSONArray vArgs = new JSONArray();

                        do
                        {
                            status = alljoyn.AJ_UnmarshalArg(msg, arg);

                            if (status != AJ_Status.AJ_OK)
                            {
                                break;
                            }

                            int sizeOfType = (TYPE_FLAG(nextTypeId) & 0xF);

                            switch (sizeOfType)
                            {
                                case 1:
                                    vArgs.put(Integer.parseInt(alljoyn.getV_byte(arg.getVal().getV_byte())));
                                    break;

                                case 2:
                                    if (nextTypeId == 'n')
                                    {
                                        vArgs.put(Integer.parseInt(alljoyn.getV_int16(arg.getVal().getV_int16())));
                                    }
                                    else
                                    {
                                        vArgs.put(Integer.parseInt(alljoyn.getV_uint16(arg.getVal().getV_uint16())) & 0xFFFF);
                                    }
                                    break;

                                case 4:
                                    if (nextTypeId == 'i')
                                    {
                                        vArgs.put(Long.parseLong(alljoyn.getV_int32(arg.getVal().getV_int32())));
                                    }
                                    else
                                    {
                                        vArgs.put(Long.parseLong(alljoyn.getV_uint32(arg.getVal().getV_uint32())) & 0xFFFFFFFF);
                                    }
                                    break;

                                case 8:
                                    if (nextTypeId == 'd')
                                    {
                                        try
                                        {
                                            vArgs.put(Double.parseDouble(alljoyn.getV_double(arg.getVal().getV_double())));
                                        }
                                        catch (Exception e)
                                        {
                                            Log.i(TAG, "AJ_UnmarshalArgs(): AJ_ERR_READ");
                                            return AJ_Status.AJ_ERR_READ;
                                        }
                                    }
                                    else if (nextTypeId == 'x')
                                    {
                                        vArgs.put(Long.parseLong(alljoyn.getV_int64(arg.getVal().getV_int64())));
                                    }
                                    else
                                    {
                                        // TODO: handle big unsigned values
                                        vArgs.put(Long.parseLong(alljoyn.getV_uint64(arg.getVal().getV_uint64())));
                                    }
                                    break;
                            }
                        }
                        while (status == AJ_Status.AJ_OK);

                        args.put(vArgs);
                        status = alljoyn.AJ_UnmarshalCloseContainer(msg, arrayArg);
                    }

                    sig.deleteCharAt(0);
                    continue;
                }

                if ((typeId == AJ_STRUCT_CLOSE) || (typeId == AJ_DICT_ENTRY_CLOSE))
                {
                    break;
                }

                if (typeId == AJ_ARG_VARIANT)
                {
                    StringBuffer inSig = new StringBuffer();
                    status = AJ_UnmarshalVariant(msg, inSig);
                    args.put(inSig.toString());
                    status = UnmarshalArgs(msg, inSig, args, nested);

                    if (status != AJ_Status.AJ_OK)
                    {
                        break;
                    }

                    continue;
                }

                if ((typeId == AJ_ARG_ARRAY) && !AJ_IsBasicType(nextTypeId))
                {
                    _AJ_Arg arrayArg = new _AJ_Arg();
                    String subSig = new String();
                    char closeContainer = (nextTypeId == '(') ? ')' : '}';
                    subSig = sig.toString().substring(0, sig.toString().indexOf(closeContainer) + 1);
                    status = alljoyn.AJ_UnmarshalContainer(msg, arrayArg, AJ_ARG_ARRAY);
                    JSONArray vArgs = new JSONArray();

                    do
                    {
                        StringBuffer inSig = new StringBuffer(subSig);
                        StringBuffer inNested = new StringBuffer();
                        JSONArray inArgs = new JSONArray();

                        status = UnmarshalArgs(msg, inSig, inArgs, inNested);
                        int len = inArgs.length();

                        if (len != 0)
                        {
                            vArgs.put(inArgs);
                        }
                    }
                    while (status == AJ_Status.AJ_OK);

                    int len = vArgs.length();
                    args.put(vArgs);
                    status = alljoyn.AJ_UnmarshalCloseContainer(msg, arrayArg);

                    if (status != AJ_Status.AJ_OK)
                    {
                        break;
                    }

                    sig.delete(0, subSig.length());
                    continue;
                }

                Log.i(TAG, "AJ_UnmarshalArgs(): AJ_ERR_UNEXPECTED");
                status = AJ_Status.AJ_ERR_UNEXPECTED;
                break;
            }
            else // scalar and string
            {
                status = alljoyn.AJ_UnmarshalArg(msg, arg);

                if (status != AJ_Status.AJ_OK)
                {
                    break;
                }

                if (arg.getTypeId() != typeId)
                {
                    Log.i(TAG, "AJ_UnmarshalArgs(): AJ_ERR_UNMARSHAL");
                    status = AJ_Status.AJ_ERR_UNMARSHAL;
                    break;
                }

                if (AJ_IsScalarType(typeId))
                {
                    int sizeOfType = (TYPE_FLAG(typeId) & 0xF);

                    switch (sizeOfType)
                    {
                        case 1:
                            args.put(Integer.parseInt(alljoyn.getV_byte(arg.getVal().getV_byte())));
                            break;

                        case 2:
                            if (typeId == 'n')
                            {
                                args.put(Integer.parseInt(alljoyn.getV_int16(arg.getVal().getV_int16())));
                            }
                            else
                            {
                                args.put(Integer.parseInt(alljoyn.getV_uint16(arg.getVal().getV_uint16())) & 0xFFFF);
                            }
                            break;

                        case 4:
                            if (typeId == 'i')
                            {
                                args.put(Long.parseLong(alljoyn.getV_int32(arg.getVal().getV_int32())));
                            }
                            else
                            {
                                args.put(Long.parseLong(alljoyn.getV_uint32(arg.getVal().getV_uint32())) & 0xFFFFFFFF);
                            }
                            break;

                        case 8:
                            if (typeId == 'd')
                            {
                                try
                                {
                                    args.put(Double.parseDouble(alljoyn.getV_double(arg.getVal().getV_double())));
                                }
                                catch (Exception e)
                                {
                                    Log.i(TAG, "AJ_UnmarshalArgs(): AJ_ERR_READ");
                                    return AJ_Status.AJ_ERR_READ;
                                }
                            }
                            else if (typeId == 'x')
                            {
                                args.put(Long.parseLong(alljoyn.getV_int64(arg.getVal().getV_int64())));
                            }
                            else
                            {
                                // TODO: handle big unsigned values
                                args.put(Long.parseLong(alljoyn.getV_uint64(arg.getVal().getV_uint64())));
                            }
                            break;
                    }
                }
                else
                {
                    args.put(arg.getVal().getV_string());
                }
            }
        }

        return status;
    }

    JSONArray AJ_UnmarshalArgs(_AJ_Message msg, String signature)
    {
        JSONArray retObj = new JSONArray();

        try
        {
            JSONArray args = new JSONArray();
            AJ_Status status = UnmarshalArgs(msg, new StringBuffer(signature), args, new StringBuffer());
            retObj.put(status);
            retObj.put(args);
        }
        catch (Exception e)
        {
            Log.i(TAG, "AJ_UnmarshalArgs(): AJ_ERR_UNMARSHAL");
            AJ_Status status = AJ_Status.AJ_ERR_UNMARSHAL;
            retObj.put(status);
        }

        return retObj;
    }

    AJ_Status AJ_UnmarshalVariant(_AJ_Message msg, StringBuffer sig)
    {
        _AJ_Arg arg = new _AJ_Arg();
        AJ_Status status = alljoyn.AJ_UnmarshalArg(msg, arg);

        if (status == AJ_Status.AJ_OK)
        {
            if (sig != null)
            {
                sig.append(arg.getVal().getV_string());
            }
        }

        return status;
    }

    public AJ_Status AJ_MarshalArgs(_AJ_Message msg, String signature, JSONArray args)
    {
        try
        {
            AJ_Status status = MarshalArgs(msg, new StringBuffer(signature), args, new StringBuffer());
            return status;
        }
        catch (Exception e)
        {
            Log.i(TAG, "AJ_MarshalArgs(): AJ_ERR_MARSHAL");
            return AJ_Status.AJ_ERR_MARSHAL;
        }
    }

    public AJ_Status MarshalArgs(_AJ_Message msg, StringBuffer sig, JSONArray args, StringBuffer nested) throws JSONException
    {
        AJ_Status status = AJ_Status.AJ_OK;

        while (sig.length() != 0)
        {
            _AJ_Arg structArg = new _AJ_Arg();
            char typeId = sig.charAt(0);
            nested.append(typeId);
            sig.deleteCharAt(0);
            char nextTypeId = (sig.length() == 0) ? '\0' : sig.charAt(0);

            if (!AJ_IsBasicType(typeId))
            {
                if ((typeId == AJ_ARG_STRUCT) || (typeId == AJ_ARG_DICT_ENTRY))
                {
                    status = alljoyn.AJ_MarshalContainer(msg, structArg, typeId);

                    if (status != AJ_Status.AJ_OK)
                    {
                        break;
                    }

                    status = MarshalArgs(msg, sig, args, nested);

                    if (status == AJ_Status.AJ_OK)
                    {
                        int lastNestedTypeId = nested.charAt(nested.length() - 1);

                        if ((lastNestedTypeId == AJ_STRUCT_CLOSE) || (lastNestedTypeId == AJ_DICT_ENTRY_CLOSE))
                        {
                            status = alljoyn.AJ_MarshalCloseContainer(msg, structArg);

                            if (status != AJ_Status.AJ_OK)
                            {
                                break;
                            }
                        }
                        else
                        {
                            status = AJ_Status.AJ_ERR_MARSHAL;
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
                        _AJ_Arg arrayArg = new _AJ_Arg();
                        status = alljoyn.AJ_MarshalContainer(msg, arrayArg, AJ_ARG_ARRAY);

                        if (status != AJ_Status.AJ_OK)
                        {
                            break;
                        }

                        JSONArray vArgs = args.getJSONArray(0);
                        args = JSONArray_Remove(args, 0);
                        int len = vArgs.length();

                        for (int k = 0; k < len; k++)
                        {
                            _AJ_Arg arg = new _AJ_Arg();
                            arg.getVal().setV_string(vArgs.getString(k));
                            alljoyn.AJ_InitArg(arg, nextTypeId, 0, arg.getVal().getV_data(), 0);
                            status = alljoyn.AJ_MarshalArg(msg, arg);
                        }

                        status = alljoyn.AJ_MarshalCloseContainer(msg, arrayArg);
                    }
                    else
                    {
                        _AJ_Arg arrayArg = new _AJ_Arg();
                        status = alljoyn.AJ_MarshalContainer(msg, arrayArg, AJ_ARG_ARRAY);

                        if (status != AJ_Status.AJ_OK)
                        {
                            break;
                        }

                        JSONArray vArgs = args.getJSONArray(0);
                        args = JSONArray_Remove(args, 0);
                        int len = vArgs.length();
                        int sizeOfType = (TYPE_FLAG(nextTypeId) & 0xF);

                        for (int k = 0; k < len; k++)
                        {
                            _AJ_Arg arg = new _AJ_Arg();
                            SWIGTYPE_p_uint32_t p_bool = new SWIGTYPE_p_uint32_t();
                            SWIGTYPE_p_uint8_t p_uint8_t = new SWIGTYPE_p_uint8_t();
                            SWIGTYPE_p_uint16_t p_uint16_t = new SWIGTYPE_p_uint16_t();
                            SWIGTYPE_p_uint32_t p_uint32_t = new SWIGTYPE_p_uint32_t();
                            SWIGTYPE_p_uint64_t p_uint64_t = new SWIGTYPE_p_uint64_t();
                            SWIGTYPE_p_int16_t p_int16_t = new SWIGTYPE_p_int16_t();
                            SWIGTYPE_p_int32_t p_int32_t = new SWIGTYPE_p_int32_t();
                            SWIGTYPE_p_int64_t p_int64_t = new SWIGTYPE_p_int64_t();
                            SWIGTYPE_p_double p_double = new SWIGTYPE_p_double();

                            switch (sizeOfType)
                            {
                                case 1:
                                    arg.getVal().setV_byte(alljoyn.setV_byte(vArgs.get(k).toString()));
                                    break;

                                case 2:
                                    if (nextTypeId == 'n')
                                    {
                                        arg.getVal().setV_int16(alljoyn.setV_int16(vArgs.get(k).toString()));
                                    }
                                    else
                                    {
                                        arg.getVal().setV_uint16(alljoyn.setV_uint16(vArgs.get(k).toString()));
                                    }

                                    break;

                                case 4:
                                    String strVal = vArgs.getString(k);

                                    if (strVal.equals("true") || strVal.equals("TRUE"))
                                    {
                                        arg.getVal().setV_uint32(alljoyn.setV_uint32("1"));
                                    }
                                    else if (strVal.equals("false") || strVal.equals("FALSE"))
                                    {
                                        arg.getVal().setV_uint32(alljoyn.setV_uint32("0"));
                                    }
                                    else if (nextTypeId == 'i')
                                    {
                                        arg.getVal().setV_int32(alljoyn.setV_int32(vArgs.get(k).toString()));
                                    }
                                    else
                                    {
                                        arg.getVal().setV_uint32(alljoyn.setV_uint32(vArgs.get(k).toString()));
                                    }

                                    break;

                                case 8:
                                    if (nextTypeId == 'd')
                                    {
                                        arg.getVal().setV_double(alljoyn.setV_double(vArgs.get(k).toString()));
                                    }
                                    else if (nextTypeId == 'x')
                                    {
                                        arg.getVal().setV_int64(alljoyn.setV_int64(vArgs.get(k).toString()));
                                    }
                                    else
                                    {
                                        arg.getVal().setV_uint64(alljoyn.setV_uint64(vArgs.get(k).toString()));
                                    }

                                    break;
                            }

                            alljoyn.AJ_InitArg(arg, nextTypeId, 0, arg.getVal().getV_data(), 0);
                            status = alljoyn.AJ_MarshalArg(msg, arg);
                        }

                        status = alljoyn.AJ_MarshalCloseContainer(msg, arrayArg);
                    }

                    sig.deleteCharAt(0);
                    continue;
                }

                if ((typeId == AJ_STRUCT_CLOSE) || (typeId == AJ_DICT_ENTRY_CLOSE))
                {
                    break;
                }

                if (typeId == AJ_ARG_VARIANT)
                {
                    String _sig = new String(args.getString(0));
                    args = JSONArray_Remove(args, 0);
                    status = alljoyn.AJ_MarshalVariant(msg, _sig);
                    status = MarshalArgs(msg, new StringBuffer(_sig), args, new StringBuffer());

                    if (status != AJ_Status.AJ_OK)
                    {
                        break;
                    }

                    continue;
                }

                if ((typeId == AJ_ARG_ARRAY) && !AJ_IsBasicType(nextTypeId))
                {
                    _AJ_Arg arrayArg = new _AJ_Arg();
                    String subSig = new String();
                    char closeContainer = (nextTypeId == '(') ? ')' : '}';
                    subSig = sig.toString().substring(0, sig.toString().indexOf(closeContainer) + 1);
                    status = alljoyn.AJ_MarshalContainer(msg, arrayArg, AJ_ARG_ARRAY);

                    if (status != AJ_Status.AJ_OK)
                    {
                        break;
                    }

                    JSONArray vArgs = args.getJSONArray(0);

                    while (vArgs.length() != 0)
                    {
                        JSONArray inArgs = vArgs.getJSONArray(0);
                        status = MarshalArgs(msg, new StringBuffer(subSig), inArgs, new StringBuffer());

                        if (status != AJ_Status.AJ_OK)
                        {
                            break;
                        }

                        vArgs = JSONArray_Remove(vArgs, 0);
                    }

                    args = JSONArray_Remove(args, 0);
                    status = alljoyn.AJ_MarshalCloseContainer(msg, arrayArg);

                    if (status != AJ_Status.AJ_OK)
                    {
                        break;
                    }

                    sig.delete(0, subSig.length());
                    continue;
                }

                Log.i(TAG, "AJ_MarshalArgs(): AJ_ERR_MARSHAL");
                status = AJ_Status.AJ_ERR_UNEXPECTED;
                break;
            }
            else
            {
                _AJ_Arg arg = new _AJ_Arg();

                if (AJ_IsScalarType(typeId))
                {
                    int sizeOfType = (TYPE_FLAG(typeId) & 0xF);
                    SWIGTYPE_p_uint32_t p_bool = new SWIGTYPE_p_uint32_t();
                    SWIGTYPE_p_uint8_t p_uint8_t = new SWIGTYPE_p_uint8_t();
                    SWIGTYPE_p_uint16_t p_uint16_t = new SWIGTYPE_p_uint16_t();
                    SWIGTYPE_p_uint32_t p_uint32_t = new SWIGTYPE_p_uint32_t();
                    SWIGTYPE_p_uint64_t p_uint64_t = new SWIGTYPE_p_uint64_t();
                    SWIGTYPE_p_int16_t p_int16_t = new SWIGTYPE_p_int16_t();
                    SWIGTYPE_p_int32_t p_int32_t = new SWIGTYPE_p_int32_t();
                    SWIGTYPE_p_int64_t p_int64_t = new SWIGTYPE_p_int64_t();
                    SWIGTYPE_p_double p_double = new SWIGTYPE_p_double();

                    switch (sizeOfType)
                    {
                        case 1:
                            arg.getVal().setV_byte(alljoyn.setV_byte( args.get(0).toString()));
                            args = JSONArray_Remove(args, 0);
                            break;

                        case 2:
                            if (typeId == 'n')
                            {
                                arg.getVal().setV_int16(alljoyn.setV_int16(args.get(0).toString()));
                            }
                            else
                            {
                                arg.getVal().setV_uint16(alljoyn.setV_uint16(args.get(0).toString()));
                            }

                            args = JSONArray_Remove(args, 0);
                            break;

                        case 4:
                            String strVal = args.getString(0);

                            if (strVal.equals("true") || strVal.equals("TRUE"))
                            {
                                arg.getVal().setV_uint32(alljoyn.setV_uint32("1"));
                            }
                            else if (strVal.equals("false") || strVal.equals("FALSE"))
                            {
                                arg.getVal().setV_uint32(alljoyn.setV_uint32("0"));
                            }
                            else if (typeId == 'i')
                            {
                                arg.getVal().setV_int32(alljoyn.setV_int32(args.get(0).toString()));
                            }
                            else
                            {
                                arg.getVal().setV_uint32(alljoyn.setV_uint32(args.get(0).toString()));
                            }

                            args = JSONArray_Remove(args, 0);
                            break;

                        case 8:
                            if (typeId == 'd')
                            {
                                arg.getVal().setV_double(alljoyn.setV_double(args.get(0).toString()));
                            }
                            else if (typeId == 'x')
                            {
                                arg.getVal().setV_int64(alljoyn.setV_int64(args.get(0).toString()));
                            }
                            else
                            {
                                arg.getVal().setV_uint64(alljoyn.setV_uint64(args.get(0).toString()));
                            }

                            args = JSONArray_Remove(args, 0);
                            break;
                    }

                    alljoyn.AJ_InitArg(arg, typeId, 0, arg.getVal().getV_data(), 0);
                }
                else
                {
                    arg.getVal().setV_string(args.getString(0));
                    args = JSONArray_Remove(args, 0);
                    alljoyn.AJ_InitArg(arg, typeId, 0, arg.getVal().getV_data(), 0);
                }

                status = alljoyn.AJ_MarshalArg(msg, arg);
            }
        }

        return status;
    }

    // Helper function used for API level < 19
    public JSONArray JSONArray_Remove(JSONArray jsonArray, int index) throws JSONException
    {
        JSONArray retArray = new JSONArray();

        for (int i = 0; i < jsonArray.length();i++)
        {
            if (i != index)
            {
                retArray.put(jsonArray.get(i));
            }
        }

        return retArray;
    }
}
