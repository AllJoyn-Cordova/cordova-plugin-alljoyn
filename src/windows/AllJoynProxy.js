/*
* To use the handler, you must have first connected to an AllJoyn bus. The handler is then started
* by passing a reference to your bus attachment.
*
* AllJoynMessageHandler.start(<your bus attachment>);
*
* To register handlers, you call the addHandler by passing the message ids you are interested,
* the signature of your message and the callback function that gets called if such messages arrive.
* The signature is an AllJoyn-specific value that is used to unmarshal the body of the message.
*
* AllJoynMessageHandler.addHandler(<interesting message id>,
*                                  <return value signature>
*                                  function (messageObject, messageBody) {
*                                    // handle the received message
*                                  });
*/
var messageHandler = (function () {
    // Use 0 as unmarshal timeout so that we don't end up blocking
    // the UI while waiting for new messages
    var AJ_UNMARSHAL_TIMEOUT = 0;
    var AJ_METHOD_TIMEOUT = 1000 * 1;
    var AJ_MESSAGE_SLOW_LOOP_INTERVAL = 500;
    var AJ_MESSAGE_FAST_LOOP_INTERVAL = 50;

    var messageHandler = {};
    var messageListeners = {};
    var interval = null;

    messageHandler.start = function (busAttachment) {
        // Flag to store current interval pace
        var runningFast;
        // This function can be called to update the interval based on if
        // the bus attachment had new messages or now. The idea is that if there are
        // messages, we run the loop faster to 'flush the bus' and slower if there is
        // nothing new.
        var updateInterval = function (unmarshalStatus) {
            if (unmarshalStatus === AllJoynWinRTComponent.AJ_Status.aj_OK) {
                if (!runningFast) {
                    clearInterval(interval);
                    runningFast = true;
                    interval = setInterval(handlerFunction, AJ_MESSAGE_FAST_LOOP_INTERVAL);
                }
            }
            if (unmarshalStatus === AllJoynWinRTComponent.AJ_Status.aj_ERR_TIMEOUT) {
                if (runningFast) {
                    clearInterval(interval);
                    runningFast = false;
                    interval = setInterval(handlerFunction, AJ_MESSAGE_SLOW_LOOP_INTERVAL);
                }
            }
        };
        var handlerFunction = function () {
            var ajMessage = new AllJoynWinRTComponent.AJ_Message();
            AllJoynWinRTComponent.AllJoyn.aj_UnmarshalMsg(busAttachment, ajMessage, AJ_UNMARSHAL_TIMEOUT).done(function (status) {
                if (status === AllJoynWinRTComponent.AJ_Status.aj_OK) {
                    var messageObject = ajMessage.get();
                    var receivedMessageId = messageObject.msgId;
                    // Check if we have listeners for this message id
                    if (messageListeners[receivedMessageId]) {
                        var callbacks = messageListeners[receivedMessageId];
                        console.log('Found ' + callbacks.length +  ' listener(s) for message with id: ' + receivedMessageId);

                        // It is expected that the signature is the same for a single message id
                        // and here it is picked from the firstly registered callback
                        var signature = callbacks[0][0];
                        var response = null;

                        // Try unmarshaling only if a signature was given and the message is not an error (AJ_MSG_ERROR = 3)
                        if (signature && messageObject.hdr.msgType !== 3) {
                            // Unmarshal the message arguments
                            var unmarshalReturnArray = AllJoynWinRTComponent.AllJoyn.aj_UnmarshalArgs(ajMessage, signature);
                            var unmarshalArgsStatus = unmarshalReturnArray[0];
                            var messageArgs = unmarshalReturnArray[1];
                            if (unmarshalArgsStatus === AllJoynWinRTComponent.AJ_Status.aj_OK) {
                                console.log('Unmarshaling of arguments from message with id ' + receivedMessageId + ' succeeded');

                                // The messageArgs is an object array created in the Windows Runtime Component
                                // so turn that to a JavaScript array before returning it. Since there can be
                                // nested container types we need to do this recursively.
                                var getClass = {}.toString;
                                var convertArgs = function (itemOrCollection) {
                                    // Check for the collection type we return for containers
                                    if (getClass.call(itemOrCollection) === '[object Windows.Foundation.Collections.IObservableVector`1<Object>]') {
                                        var subArray = [];
                                        for (var j = 0; j < itemOrCollection.length; j++) {
                                            subArray.push(convertArgs(itemOrCollection[j]));
                                        }
                                        return subArray;
                                    } else {
                                        return itemOrCollection;
                                    }
                                };
                                response = convertArgs(messageArgs);
                            } else {
                                console.log('Unmarshaling of arguments from message with id ' + receivedMessageId + ' failed with status ' + unmarshalArgsStatus);
                            }
                        }

                        // If this is a blocking handler
                        if (callbacks[0][2]) {
                            if (callbacks.length > 1) {
                                console.log('It is unexpected to have more than one callbacks for blocking handlers!');
                            }
                            // Stop the message loop until accept session request is handled
                            messageHandler.stop();
                            callbacks[0][1](messageObject, response, ajMessage, function (messagePointer) {
                                // When request handled, close message and continue with the handler loop
                                console.log('Closing a blocking message with id: ' + messagePointer.get().msgId);
                                AllJoynWinRTComponent.AllJoyn.aj_CloseMsg(messagePointer);
                                messageHandler.start(busAttachment);
                            });
                            return;
                        }

                        AllJoynWinRTComponent.AllJoyn.aj_CloseMsg(ajMessage);

                        // Pass the value to all added listeners
                        for (var i = 0; i < callbacks.length; i++) {
                            if (callbacks[i][0] !== signature) {
                                console.log('It is unexpected to have a handler with different signature for a single message id!');
                            }
                            callbacks[i][1](messageObject, response);
                        }
                    } else {
                        console.log('Message with id ' + receivedMessageId + ' passed to default handler');
                        AllJoynWinRTComponent.AllJoyn.aj_BusHandleBusMessage(ajMessage);
                        AllJoynWinRTComponent.AllJoyn.aj_CloseMsg(ajMessage);
                    }
                } else {
                    AllJoynWinRTComponent.AllJoyn.aj_CloseMsg(ajMessage);
                }
                updateInterval(status);
            });
        };
        // Initially start with slower interval
        runningFast = false;
        interval = setInterval(handlerFunction, AJ_MESSAGE_SLOW_LOOP_INTERVAL);
    };

    messageHandler.stop = function () {
        clearInterval(interval);
    };

    messageHandler.addHandler = function (messageId, signature, callback, isBlocking) {
        // Create a list of handlers for this message id if it doesn't exist yet
        if (typeof messageListeners[messageId] !== 'object') {
            messageListeners[messageId] = [];
        }
        messageListeners[messageId].push([signature, callback, isBlocking]);
    };

    messageHandler.removeHandler = function (messageId, callback) {
        messageListeners[messageId] = messageListeners[messageId].filter(
            function (element) {
                // Filter out the given callback function
                return (element[1] !== callback);
            }
        );
    };

    return messageHandler;
})();

// This initialization call is required once before doing
// other AllJoyn operations.
AllJoynWinRTComponent.AllJoyn.aj_Initialize();

var AJ_CONNECT_TIMEOUT = 1000 * 5;
var AJ_BUS_START_FINDING = 0;
var AJ_BUS_STOP_FINDING = 1;

var busAttachment = new AllJoynWinRTComponent.AJ_BusAttachment();

var registeredProxyObjects = null;

var getMessageId = function (indexList) {
    return AllJoynWinRTComponent.AllJoyn.aj_Encode_Message_ID(indexList[0], indexList[1], indexList[2], indexList[3]);
};

var getMsgInfo = function (ajMsg) {
    var msgInfo = {};
    if (ajMsg.sender !== undefined) {
        msgInfo.sender = ajMsg.sender;
    }
    if (ajMsg.signature !== undefined) {
        msgInfo.signature = ajMsg.signature;
    }
    if (ajMsg.iface !== undefined) {
        msgInfo.iface = ajMsg.iface;
    }
    return msgInfo;
};

cordova.commandProxy.add('AllJoyn', {
    connect: function (success, error) {
        var daemonName = '';
        var status = AllJoynWinRTComponent.AllJoyn.aj_FindBusAndConnect(busAttachment, daemonName, AJ_CONNECT_TIMEOUT);
        if (status === AllJoynWinRTComponent.AJ_Status.aj_OK) {
            messageHandler.start(busAttachment);
            success();
        } else {
            error(status);
        }
    },
    registerObjects: function (success, error, parameters) {
        // This function turns the JavaScript objects
        // into format that the AllJoyn Windows Runtime Component expects.
        // The complexity of the conversion comes from the fact that
        // we can't pass null JavaScript values to functions that expect
        // strings, but they have to be first converted into empty strings.
        var getAllJoynObjects = function (objects) {
            if (objects === null || objects.length === 0) {
                return [null];
            }
            var allJoynObjects = [];
            for (var i = 0; i < objects.length; i++) {
                if (objects[i] === null) {
                    allJoynObjects.push(null);
                    break;
                }
                var allJoynObject = new AllJoynWinRTComponent.AJ_Object();
                allJoynObject.path = objects[i].path;
                var interfaces = objects[i].interfaces;
                for (var j = 0; j < interfaces.length; j++) {
                    var interface = interfaces[j];
                    if (interface === null) {
                        break;
                    }
                    var lastIndex = interface.length - 1;
                    if (interface[lastIndex] === null) {
                        interface[lastIndex] = '';
                    }
                }
                allJoynObject.interfaces = interfaces;
                allJoynObjects.push(allJoynObject);
            }
            return allJoynObjects;
        };
        var applicationObjects = getAllJoynObjects(parameters[0]);
        var proxyObjects = getAllJoynObjects(parameters[1]);
        AllJoynWinRTComponent.AllJoyn.aj_RegisterObjects(applicationObjects, proxyObjects);
        registeredProxyObjects = proxyObjects;
        success();
    },
    addAdvertisedNameListener: function (success, error, parameters) {
        var name = parameters[0];
        var callback = parameters[1];

        var status = AllJoynWinRTComponent.AllJoyn.aj_BusFindAdvertisedName(busAttachment, name, AJ_BUS_START_FINDING);
        if (status !== AllJoynWinRTComponent.AJ_Status.aj_OK) {
            error(status);
            return;
        }

        var foundAdvertisedNameMessageId = AllJoynWinRTComponent.AllJoyn.aj_Bus_Message_ID(1, 0, 1);
        messageHandler.addHandler(
            foundAdvertisedNameMessageId, 's',
            function (messageObject, messageBody) {
                callback([
                    getMsgInfo(messageObject),
                    {
                        name: messageBody[0]
                    }
                ]);
            }
        );
    },
    startAdvertisingName: function (success, error, parameters) {
        var wellKnownName = parameters[0];
        var port = parameters[1];

        var sessionOptions = null;
        var status;

        status = AllJoynWinRTComponent.AllJoyn.aj_BusBindSessionPort(busAttachment, port, sessionOptions, 0);
        if (status === AllJoynWinRTComponent.AJ_Status.aj_OK) {
            var bindReplyId = AllJoynWinRTComponent.AllJoyn.aj_Reply_ID(AllJoynWinRTComponent.AJ_Std.aj_Method_Bind_Session_Port);
            messageHandler.addHandler(
                bindReplyId, null,
                function (messageObject, messageBody) {
                    console.log('Got bindReplyId');
                    messageHandler.removeHandler(bindReplyId, this[1]);
                    status = AllJoynWinRTComponent.AllJoyn.aj_BusRequestName(busAttachment, wellKnownName, 0);
                    if (status === AllJoynWinRTComponent.AJ_Status.aj_OK) {
                        var requestNameReplyId = AllJoynWinRTComponent.AllJoyn.aj_Reply_ID(AllJoynWinRTComponent.AJ_Std.aj_Method_Request_Name);
                        messageHandler.addHandler(
                            requestNameReplyId, null,
                            function (messageObject, messageBody) {
                                console.log('Got requestNameReplyId');
                                messageHandler.removeHandler(requestNameReplyId, this[1]);
                                // 65535 == TRANSPORT_ANY
                                var transportMask = 65535;
                                // 0 == AJ_BUS_START_ADVERTISING
                                var op = 0;
                                status = AllJoynWinRTComponent.AllJoyn.aj_BusAdvertiseName(busAttachment, wellKnownName, transportMask, op, 0);
                                if (status === AllJoynWinRTComponent.AJ_Status.aj_OK) {
                                    var advertiseNameReplyId = AllJoynWinRTComponent.AllJoyn.aj_Reply_ID(AllJoynWinRTComponent.AJ_Std.aj_Method_Advertise_Name);
                                    messageHandler.addHandler(
                                        advertiseNameReplyId, null,
                                        function (messageObject, messageBody) {
                                            console.log('Got advertiseNameReplyId');
                                            messageHandler.removeHandler(advertiseNameReplyId, this[1]);
                                            success();
                                        }
                                    );
                                } else {
                                    error(status);
                                }
                            }
                        );
                    } else {
                        error(status);
                    }
                }
            );
        } else {
            error(status);
        }
    },
    stopAdvertisingName: function (success, error, parameters) {
        var wellKnownName = parameters[0];
        var port = parameters[1];

        var status;

        status = AllJoynWinRTComponent.AllJoyn.aj_BusUnbindSession(busAttachment, port);
        if (status === AllJoynWinRTComponent.AJ_Status.aj_OK) {
            var unbindReplyId = AllJoynWinRTComponent.AllJoyn.aj_Reply_ID(AllJoynWinRTComponent.AJ_Std.aj_Method_Unbind_Session);
            messageHandler.addHandler(
                unbindReplyId, null,
                function (messageObject, messageBody) {
                    console.log('Got unbindReplyId');
                    messageHandler.removeHandler(unbindReplyId, this[1]);
                    status = AllJoynWinRTComponent.AllJoyn.aj_BusReleaseName(busAttachment, wellKnownName);
                    if (status === AllJoynWinRTComponent.AJ_Status.aj_OK) {
                        var releaseNameReplyId = AllJoynWinRTComponent.AllJoyn.aj_Reply_ID(AllJoynWinRTComponent.AJ_Std.aj_Method_Release_Name);
                        messageHandler.addHandler(
                            releaseNameReplyId, null,
                            function (messageObject, messageBody) {
                                console.log('Got releaseNameReplyId');
                                messageHandler.removeHandler(releaseNameReplyId, this[1]);
                                // 65535 == TRANSPORT_ANY
                                var transportMask = 65535;
                                // 1 == AJ_BUS_STOP_ADVERTISING
                                var op = 1;
                                status = AllJoynWinRTComponent.AllJoyn.aj_BusAdvertiseName(busAttachment, wellKnownName, transportMask, op, 0);
                                if (status === AllJoynWinRTComponent.AJ_Status.aj_OK) {
                                    var cancelAdvertiseNameReplyId = AllJoynWinRTComponent.AllJoyn.aj_Reply_ID(AllJoynWinRTComponent.AJ_Std.aj_Method_Cancel_Advertise);
                                    messageHandler.addHandler(
                                        cancelAdvertiseNameReplyId, null,
                                        function (messageObject, messageBody) {
                                            console.log('Got cancelAdvertiseNameReplyId');
                                            messageHandler.removeHandler(cancelAdvertiseNameReplyId, this[1]);
                                            success();
                                        }
                                    );
                                } else {
                                    error(status);
                                }
                            }
                        );
                    } else {
                        error(status);
                    }
                }
            );
        } else {
            error(status);
        }
    },
    joinSession: function (success, error, parameters) {
        var service = parameters[0];

        // Use null value as session options, which means that AllJoyn will use the default options
        var sessionOptions = null;
        var status = AllJoynWinRTComponent.AllJoyn.aj_BusJoinSession(busAttachment, service.name, service.port, sessionOptions);
        if (status === AllJoynWinRTComponent.AJ_Status.aj_OK) {
            var joinSessionReplyId = AllJoynWinRTComponent.AllJoyn.aj_Reply_ID(AllJoynWinRTComponent.AllJoyn.aj_Bus_Message_ID(1, 0, 10));
            messageHandler.addHandler(
                joinSessionReplyId, 'uu',
                function (messageObject, messageBody) {
                    messageHandler.removeHandler(joinSessionReplyId, this[1]);
                    if (messageBody !== null) {
                        var sessionId = messageBody[1];
                        var sessionHost = service.name;
                        success([getMsgInfo(messageObject), [sessionId, sessionHost]]);
                    } else {
                        // TODO: How to get the error code, is it in the message header?
                        error();
                    }
                }
            );
        } else {
            error(status);
        }
    },
    leaveSession: function (success, error, parameters) {
        var sessionId = parameters[0];
        var status = AllJoynWinRTComponent.AllJoyn.aj_BusLeaveSession(busAttachment, sessionId);
        if (status === AllJoynWinRTComponent.AJ_Status.aj_OK) {
            success();
        } else {
            error();
        }
    },
    invokeMember: function (success, error, parameters) {
        var sessionId = parameters[0],
        destination = parameters[1],
        signature = parameters[2],
        path = parameters[3],
        indexList = parameters[4],
        argsType = parameters[5],
        args = parameters[6],
        responseType = parameters[7];

        var isSignal = (signature.lastIndexOf('!') === 0);

        var messageId = getMessageId(indexList);
        var message = new AllJoynWinRTComponent.AJ_Message();
        // An empty string is used as a destination, because that ends up being converted to null platform string
        // in the Windows Runtime Component.
        destination = destination || '';

        if (path) {
            AllJoynWinRTComponent.AllJoyn.aj_SetProxyObjectPath(registeredProxyObjects, messageId, path);
        }

        var status;

        if (isSignal) {
            var signalFlags = 0;

            // If no session id or destination specified then assume the signal is sessionless
            if (!sessionId && destination === '') {
                signalFlags = AllJoynWinRTComponent.AJ_MsgFlag.aj_Flag_Sessionless;
            }
            status = AllJoynWinRTComponent.AllJoyn.aj_MarshalSignal(busAttachment, message, messageId, destination, sessionId, signalFlags, 0);
            console.log('aj_MarshalSignal resulted in a status of ' + status);
        } else {
            status = AllJoynWinRTComponent.AllJoyn.aj_MarshalMethodCall(busAttachment, message, messageId, destination, sessionId, 0, 0, 0);
            console.log('aj_MarshalMethodCall resulted in a status of ' + status);
        }

        if (status === AllJoynWinRTComponent.AJ_Status.aj_OK && args) {
            status = AllJoynWinRTComponent.AllJoyn.aj_MarshalArgs(message, argsType, args);
            console.log('aj_MarshalArgs resulted in a status of ' + status);
        }

        if (status === AllJoynWinRTComponent.AJ_Status.aj_OK) {
            status = AllJoynWinRTComponent.AllJoyn.aj_DeliverMsg(message);
            console.log('aj_DeliverMsg resulted in a status of ' + status);
        }

        // Messages must be closed to free resources.
        AllJoynWinRTComponent.AllJoyn.aj_CloseMsg(message);

        if (status === AllJoynWinRTComponent.AJ_Status.aj_OK) {
            if (isSignal) {
                success();
            } else {
                var replyMessageId = AllJoynWinRTComponent.AllJoyn.aj_Reply_ID(messageId);
                messageHandler.addHandler(
                    replyMessageId, responseType,
                    function (messageObject, messageBody) {
                        messageHandler.removeHandler(replyMessageId, this[1]);
                        // AJ_MSG_ERROR = 3
                        if (messageObject.hdr.msgType === 3) {
                            error(messageObject.error);
                        } else {
                            success([getMsgInfo(messageObject), messageBody]);
                        }
                    }
                );
            }
        } else {
            error(status);
        }
    },
    addListener: function (success, error, parameters) {
        var indexList = parameters[0],
        responseType = parameters[1],
        callback = parameters[2];
        var messageId = getMessageId(indexList);
        messageHandler.addHandler(
            messageId, responseType,
            function (messageObject, messageBody) {
                callback([getMsgInfo(messageObject), messageBody]);
            }
        );
    },
    addListenerForReply: function (success, error, parameters) {
        var indexList = parameters[0],
        responseType = parameters[1],
        callback = parameters[2];
        var messageId = getMessageId(indexList);
        messageHandler.addHandler(
            messageId, responseType,
            function (messageObject, messageBody, messagePointer, doneCallback) {
                callback([getMsgInfo(messageObject), messageBody], messagePointer, doneCallback);
            },
            true
        );
    },
    sendSuccessReply: function (success, error, parameters) {
        var messagePointer = parameters[0];
        var argsType = parameters[1];
        var args = parameters[2];

        var replyMessage = new AllJoynWinRTComponent.AJ_Message();
        var status;
        status = AllJoynWinRTComponent.AllJoyn.aj_MarshalReplyMsg(messagePointer, replyMessage);
        if (status === AllJoynWinRTComponent.AJ_Status.aj_OK && args) {
            status = AllJoynWinRTComponent.AllJoyn.aj_MarshalArgs(replyMessage, argsType, args);
            console.log('Marshaling success reply arguments resulted in a status of ' + status);
        }
        if (status === AllJoynWinRTComponent.AJ_Status.aj_OK) {
            status = AllJoynWinRTComponent.AllJoyn.aj_DeliverMsg(replyMessage);
            console.log('Delivering success reply resulted in a status of ' + status);
        }
        AllJoynWinRTComponent.AllJoyn.aj_CloseMsg(replyMessage);

        if (status === AllJoynWinRTComponent.AJ_Status.aj_OK) {
            success();
        } else {
            error();
        }
    },
    sendErrorReply: function (success, error, parameters) {
        var messagePointer = parameters[0];
        var errorMessage = parameters[1];

        var replyMessage = new AllJoynWinRTComponent.AJ_Message();
        var status;
        status = AllJoynWinRTComponent.AllJoyn.aj_MarshalErrorMsg(messagePointer, replyMessage, errorMessage);
        if (status === AllJoynWinRTComponent.AJ_Status.aj_OK) {
            status = AllJoynWinRTComponent.AllJoyn.aj_DeliverMsg(replyMessage);
            console.log('Delivering error reply resulted in a status of ' + status);
        }
        AllJoynWinRTComponent.AllJoyn.aj_CloseMsg(replyMessage);

        if (status === AllJoynWinRTComponent.AJ_Status.aj_OK) {
            success();
        } else {
            error();
        }
    },
    replyAcceptSession: function (success, error, parameters) {
        var messagePointer = parameters[0];
        var response = parameters[1];

        if (response === true) {
            AllJoynWinRTComponent.AllJoyn.aj_BusReplyAcceptSession(messagePointer, 1);
        } else {
            AllJoynWinRTComponent.AllJoyn.aj_BusReplyAcceptSession(messagePointer, 0);
        }

        success();
    },
    setAcceptSessionListener: function (success, error, parameters) {
        var acceptSessionListener = parameters[0];

        var acceptSessionId = AllJoynWinRTComponent.AJ_Std.aj_Method_Accept_Session;
        messageHandler.addHandler(
            acceptSessionId, 'qus',
            function (messageObject, messageBody, messagePointer, doneCallback) {
                acceptSessionListener(messageBody, messagePointer, doneCallback);
            },
            true
        );
    },
    setSignalRule: function (success, error, parameters) {
        var ruleString = parameters[0];
        var applyRule = parameters[1];

        var status = AllJoynWinRTComponent.AllJoyn.aj_BusSetSignalRule(busAttachment, ruleString, applyRule);

        if (status === AllJoynWinRTComponent.AJ_Status.aj_OK) {
            success();
        } else {
            error(status);
        }
    }
});
