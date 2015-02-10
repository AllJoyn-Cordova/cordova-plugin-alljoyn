var AJ_CONNECT_TIMEOUT = 1000 * 5;
var AJ_BUS_START_FINDING = 0;
var AJ_BUS_STOP_FINDING = 1;

var METHOD_ACCEPT_SESSION_ID = AllJoynWinRTComponent.AJ_Std.aj_Method_Accept_Session;

var busAttachment = new AllJoynWinRTComponent.AJ_BusAttachment();

var registeredProxyObjects = null;

var getMessageId = function(indexList) {
  return AllJoynWinRTComponent.AllJoyn.aj_Encode_Message_ID(indexList[0], indexList[1], indexList[2], indexList[3]);
}

cordova.commandProxy.add("AllJoyn", {
  connect: function(success, error) {
    var daemonName = "";
    var status = AllJoynWinRTComponent.AllJoyn.aj_FindBusAndConnect(busAttachment, daemonName, AJ_CONNECT_TIMEOUT);
    if (status == AllJoynWinRTComponent.AJ_Status.aj_OK) {
      messageHandler.start(busAttachment);
      success();
    } else {
      error(status);
    }
  },
  registerObjects: function(success, error, parameters) {
    // This function turns the JavaScript objects
    // into format that the AllJoyn Windows Runtime Component expects.
    // The complexity of the conversion comes from the fact that
    // we can't pass null JavaScript values to functions that expect
    // strings, but they have to be first converted into empty strings.
    var getAllJoynObjects = function(objects) {
      if (objects == null || objects.length == 0) return [null];
      var allJoynObjects = [];
      for (var i = 0; i < objects.length; i++) {
        if (objects[i] == null) {
          allJoynObjects.push(null);
          break;
        }
        var allJoynObject = new AllJoynWinRTComponent.AJ_Object();
        allJoynObject.path = objects[i].path;
        var interfaces = objects[i].interfaces;
        for (var j = 0; j < interfaces.length; j++) {
          var interface = interfaces[j];
          if (interface === null) break;
          var lastIndex = interface.length - 1;
          if (interface[lastIndex] === null) interface[lastIndex] = "";
        }
        allJoynObject.interfaces = interfaces;
        allJoynObjects.push(allJoynObject);
      }
      return allJoynObjects;
    }
    var applicationObjects = getAllJoynObjects(parameters[0]);
    var proxyObjects = getAllJoynObjects(parameters[1]);
    AllJoynWinRTComponent.AllJoyn.aj_RegisterObjects(applicationObjects, proxyObjects);
    registeredProxyObjects = proxyObjects;
    success();
  },
  addAdvertisedNameListener: function(success, error, parameters) {
    var name = parameters[0];
    var callback = parameters[1];

    var status = AllJoynWinRTComponent.AllJoyn.aj_BusFindAdvertisedName(busAttachment, name, AJ_BUS_START_FINDING);
    if (status != AllJoynWinRTComponent.AJ_Status.aj_OK) {
      error(status);
      return;
    }

    var foundAdvertisedNameMessageId = AllJoynWinRTComponent.AllJoyn.aj_Bus_Message_ID(1, 0, 1);
    messageHandler.addHandler(
      foundAdvertisedNameMessageId, 's',
      function(messageObject, messageBody) {
        callback({ name: messageBody[0] });
      }
    );
  },
  addInterfacesListener: function(success, error, parameters) {
    var interfaceNames = parameters[0];
    var callback = parameters[1];

    var ruleString = "interface='org.alljoyn.About',sessionless='t'";
    var implementsString = ",implements='";
    for (var i = 0; i < interfaceNames.length; i++) {
      var interfaceName = interfaceNames[i];
      ruleString += implementsString;
      if (interfaceName.indexOf("$") === 0) {
        ruleString += interfaceName.substring(1);
      } else {
        ruleString += interfaceName;
      }
      ruleString += "'";
    }

    // AJ_BUS_SIGNAL_ALLOW == 0
    var status = AllJoynWinRTComponent.AllJoyn.aj_BusSetSignalRule(busAttachment, ruleString, 0);
    if (status != AllJoynWinRTComponent.AJ_Status.aj_OK) {
      error(status);
      return;
    }

    var aboutAnnounceId = AllJoynWinRTComponent.AJ_Std.aj_Signal_About_Announce;
    messageHandler.addHandler(
      aboutAnnounceId, 'qq',
      function(messageObject, messageBody) {
        callback({
          version: messageBody[0],
          port: messageBody[1],
          name: messageObject.sender
        });
      }
    );
  },
  startAdvertisingName: function(success, error, parameters) {
    var wellKnownName = parameters[0];
    var port = parameters[1];

    var sessionOptions = null;
    var status;

    status = AllJoynWinRTComponent.AllJoyn.aj_BusBindSessionPort(busAttachment, port, sessionOptions, 0);
    if (status == AllJoynWinRTComponent.AJ_Status.aj_OK) {
      var bindReplyId = AllJoynWinRTComponent.AllJoyn.aj_Reply_ID(AllJoynWinRTComponent.AJ_Std.aj_Method_Bind_Session_Port);
      messageHandler.addHandler(
        bindReplyId, null,
        function(messageObject, messageBody) {
          console.log("Got bindReplyId");
          messageHandler.removeHandler(bindReplyId, this[1]);
          status = AllJoynWinRTComponent.AllJoyn.aj_BusRequestName(busAttachment, wellKnownName, 0);
          if (status == AllJoynWinRTComponent.AJ_Status.aj_OK) {
            var requestNameReplyId = AllJoynWinRTComponent.AllJoyn.aj_Reply_ID(AllJoynWinRTComponent.AJ_Std.aj_Method_Request_Name);
            messageHandler.addHandler(
              requestNameReplyId, null,
              function(messageObject, messageBody) {
                console.log("Got requestNameReplyId");
                messageHandler.removeHandler(requestNameReplyId, this[1]);
                // 65535 == TRANSPORT_ANY
                var transportMask = 65535;
                // 0 == AJ_BUS_START_ADVERTISING
                var op = 0;
                status = AllJoynWinRTComponent.AllJoyn.aj_BusAdvertiseName(busAttachment, wellKnownName, transportMask, op, 0);
                if (status == AllJoynWinRTComponent.AJ_Status.aj_OK) {
                  var advertiseNameReplyId = AllJoynWinRTComponent.AllJoyn.aj_Reply_ID(AllJoynWinRTComponent.AJ_Std.aj_Method_Advertise_Name);
                  messageHandler.addHandler(
                    advertiseNameReplyId, null,
                    function(messageObject, messageBody) {
                      console.log("Got advertiseNameReplyId");
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
  stopAdvertisingName: function(success, error, parameters) {
    var wellKnownName = parameters[0];
    var port = parameters[1];

    var status;

    status = AllJoynWinRTComponent.AllJoyn.aj_BusUnbindSession(busAttachment, port);
    if (status == AllJoynWinRTComponent.AJ_Status.aj_OK) {
      var unbindReplyId = AllJoynWinRTComponent.AllJoyn.aj_Reply_ID(AllJoynWinRTComponent.AJ_Std.aj_Method_Unbind_Session);
      messageHandler.addHandler(
        unbindReplyId, null,
        function(messageObject, messageBody) {
          console.log("Got unbindReplyId");
          messageHandler.removeHandler(unbindReplyId, this[1]);
          status = AllJoynWinRTComponent.AllJoyn.aj_BusReleaseName(busAttachment, wellKnownName);
          if (status == AllJoynWinRTComponent.AJ_Status.aj_OK) {
            var releaseNameReplyId = AllJoynWinRTComponent.AllJoyn.aj_Reply_ID(AllJoynWinRTComponent.AJ_Std.aj_Method_Release_Name);
            messageHandler.addHandler(
              releaseNameReplyId, null,
              function(messageObject, messageBody) {
                console.log("Got releaseNameReplyId");
                messageHandler.removeHandler(releaseNameReplyId, this[1]);
                // 65535 == TRANSPORT_ANY
                var transportMask = 65535;
                // 1 == AJ_BUS_STOP_ADVERTISING
                var op = 1;
                status = AllJoynWinRTComponent.AllJoyn.aj_BusAdvertiseName(busAttachment, wellKnownName, transportMask, op, 0);
                if (status == AllJoynWinRTComponent.AJ_Status.aj_OK) {
                  var cancelAdvertiseNameReplyId = AllJoynWinRTComponent.AllJoyn.aj_Reply_ID(AllJoynWinRTComponent.AJ_Std.aj_Method_Cancel_Advertise);
                  messageHandler.addHandler(
                    cancelAdvertiseNameReplyId, null,
                    function(messageObject, messageBody) {
                      console.log("Got cancelAdvertiseNameReplyId");
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
  joinSession: function(success, error, parameters) {
    var service = parameters[0];

    // Use null value as session options, which means that AllJoyn will use the default options
    var sessionOptions = null;
    var status = AllJoynWinRTComponent.AllJoyn.aj_BusJoinSession(busAttachment, service.name, service.port, sessionOptions);
    if (status == AllJoynWinRTComponent.AJ_Status.aj_OK) {
      var joinSessionReplyId = AllJoynWinRTComponent.AllJoyn.aj_Reply_ID(AllJoynWinRTComponent.AllJoyn.aj_Bus_Message_ID(1, 0, 10));
      messageHandler.addHandler(
        joinSessionReplyId, 'uu',
        function(messageObject, messageBody) {
          if (messageBody != null) {
            var sessionId = messageBody[1];
            var sessionHost = messageObject.sender;
            success([sessionId, sessionHost]);
          } else {
            // TODO: How to get the error code, is it in the message header?
            error();
          }
          messageHandler.removeHandler(joinSessionReplyId, this[1]);
        }
      );
    } else {
      error(status);
    }
  },
  leaveSession: function(success, error, parameters) {
    var sessionId = parameters[0];
    var status = AllJoynWinRTComponent.AllJoyn.aj_BusLeaveSession(busAttachment, sessionId);
    if (status == AllJoynWinRTComponent.AJ_Status.aj_OK) {
      success();
    } else {
      error();
    }
  },
  invokeMember: function(success, error, parameters) {
    var sessionId = parameters[0],
      destination = parameters[1],
      signature = parameters[2],
      path = parameters[3],
      indexList = parameters[4],
      argsType = parameters[5],
      args = parameters[6],
      responseType = parameters[7];

    var isSignal = (signature.lastIndexOf("!") === 0);

    var messageId = getMessageId(indexList);
    var message = new AllJoynWinRTComponent.AJ_Message();
    // An empty string is used as a destination, because that ends up being converted to null platform string
    // in the Windows Runtime Component.
    var destination = destination || "";

    if (path) {
      AllJoynWinRTComponent.AllJoyn.aj_SetProxyObjectPath(registeredProxyObjects, messageId, path);
    }

    var status;

    if (isSignal) {
      status = AllJoynWinRTComponent.AllJoyn.aj_MarshalSignal(busAttachment, message, messageId, destination, sessionId, 0, 0);
      console.log("aj_MarshalSignal resulted in a status of " + status);
    } else {
      status = AllJoynWinRTComponent.AllJoyn.aj_MarshalMethodCall(busAttachment, message, messageId, destination, sessionId, 0, 0, 0);
      console.log("aj_MarshalMethodCall resulted in a status of " + status);
    }

    if (status == AllJoynWinRTComponent.AJ_Status.aj_OK && args) {
      status = AllJoynWinRTComponent.AllJoyn.aj_MarshalArgs(message, argsType, args);
      console.log("aj_MarshalArgs resulted in a status of " + status);
    }

    if (status == AllJoynWinRTComponent.AJ_Status.aj_OK) {
      status = AllJoynWinRTComponent.AllJoyn.aj_DeliverMsg(message);
      console.log("aj_DeliverMsg resulted in a status of " + status);
    }

    // Messages must be closed to free resources.
    AllJoynWinRTComponent.AllJoyn.aj_CloseMsg(message);

    if (status == AllJoynWinRTComponent.AJ_Status.aj_OK) {
      if (isSignal) {
        success();
      } else {
        replyMessageId = AllJoynWinRTComponent.AllJoyn.aj_Reply_ID(messageId);
        messageHandler.addHandler(
          replyMessageId, responseType,
          function(messageObject, messageBody) {
            success(messageBody);
            messageHandler.removeHandler(replyMessageId, this[1]);
          }
        );
      }
    } else {
      error(status);
    }
  },
  addListener: function(success, error, parameters) {
    var indexList = parameters[0],
      responseType = parameters[1],
      callback = parameters[2];
    var messageId = getMessageId(indexList);
    messageHandler.addHandler(
      messageId, responseType,
      function(messageObject, messageBody) {
        callback(messageBody);
      }
    );
  },
  setAcceptSessionListener: function(success, error, parameters) {
    var acceptSessionListener = parameters[0];

    var acceptSessionId = METHOD_ACCEPT_SESSION_ID;
    messageHandler.addHandler(
      acceptSessionId, 'qus',
      function(messageObject, messageBody, messagePointer, doneCallback) {
        var responseCallback = function(response) {
          if (response === true) {
            AllJoynWinRTComponent.AllJoyn.aj_BusReplyAcceptSession(messagePointer, 1);
          } else {
            AllJoynWinRTComponent.AllJoyn.aj_BusReplyAcceptSession(messagePointer, 0);
          }
          doneCallback();
        }
        var joinSessionRequest = {
          port: messageBody[0],
          sessionId: messageBody[1],
          sender: messageBody[2],
          response: responseCallback
        }
        acceptSessionListener(joinSessionRequest);
      }
    );
  },
  setSignalRule: function(success, error, parameters) {
    var ruleString = parameters[0];
    var applyRule = parameters[1];

    var status = AllJoynWinRTComponent.AllJoyn.aj_BusSetSignalRule(busAttachment, ruleString, applyRule);

    if (status == AllJoynWinRTComponent.AJ_Status.aj_OK) {
      success();
    } else {
      error(status);
    }
  }
});

// This initialization call is required once before doing
// other AllJoyn operations.
AllJoynWinRTComponent.AllJoyn.aj_Initialize();

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
 *                                  function(messageObject, messageBody) {
 *                                    // handle the received message
 *                                  });
 */
var messageHandler = (function() {
  // Use 0 as unmarshal timeout so that we don't end up blocking
  // the UI while waiting for new messages
  var AJ_UNMARSHAL_TIMEOUT = 0;
  var AJ_METHOD_TIMEOUT = 1000 * 1;
  var AJ_MESSAGE_SLOW_LOOP_INTERVAL = 500;
  var AJ_MESSAGE_FAST_LOOP_INTERVAL = 50;

  var messageHandler = {};
  var messageListeners = {};
  var interval = null;

  messageHandler.start = function(busAttachment) {
    // Flag to store current interval pace
    var runningFast;
    // This function can be called to update the interval based on if
    // the bus attachment had new messages or now. The idea is that if there are
    // messages, we run the loop faster to "flush the bus" and slower if there is
    // nothing new.
    var updateInterval = function(unmarshalStatus) {
      if (unmarshalStatus == AllJoynWinRTComponent.AJ_Status.aj_OK) {
        if (!runningFast) {
          clearInterval(interval);
          runningFast = true;
          interval = setInterval(handlerFunction, AJ_MESSAGE_FAST_LOOP_INTERVAL);
        }
      }
      if (unmarshalStatus == AllJoynWinRTComponent.AJ_Status.aj_ERR_TIMEOUT) {
        if (runningFast) {
          clearInterval(interval);
          runningFast = false;
          interval = setInterval(handlerFunction, AJ_MESSAGE_SLOW_LOOP_INTERVAL);
        }
      }
    }
    var handlerFunction = function() {
      var aj_message = new AllJoynWinRTComponent.AJ_Message();
      AllJoynWinRTComponent.AllJoyn.aj_UnmarshalMsg(busAttachment, aj_message, AJ_UNMARSHAL_TIMEOUT).done(function(status) {
        if (status == AllJoynWinRTComponent.AJ_Status.aj_OK) {
          var messageObject = aj_message.get();
          var receivedMessageId = messageObject.msgId;
          // Check if we have listeners for this message id
          if (messageListeners[receivedMessageId]) {
            // Pass the value to listeners
            var callbacks = messageListeners[receivedMessageId];
            for (var i = 0; i < callbacks.length; i++) {
              var signature = callbacks[i][0];
              var response = null;
              if (signature) {
                // Unmarshal the message body
                var messageBody = AllJoynWinRTComponent.AllJoyn.aj_UnmarshalArgs(aj_message, signature);
                // First item in the list is the status of the unmarshaling
                if (messageBody[0] == AllJoynWinRTComponent.AJ_Status.aj_OK) {
                  // The messageBody is an object array created in the Windows Runtime Component
                  // so turn that to a JavaScript array before returning it.
                  var response = [];
                  for (var j = 1; j < messageBody.length; j++) {
                    response.push(messageBody[j]);
                  }
                } else {
                  console.log('Unmarshaling of message with id ' + receivedMessageId + ' failed with status ' + messageBody[0]);
                }
              }
              if (receivedMessageId == METHOD_ACCEPT_SESSION_ID) {
                callbacks[i][1](messageObject, response, aj_message, function() {
                  // When request handled, close message and continue with the handler loop
                  AllJoynWinRTComponent.AllJoyn.aj_CloseMsg(aj_message);
                  messageHandler.start(busAttachment);
                });
                // Stop the message loop until accept session request is handled
                messageHandler.stop();
                return;
              } else {
                callbacks[i][1](messageObject, response);
                AllJoynWinRTComponent.AllJoyn.aj_CloseMsg(aj_message);
              }
            }
          } else {
            console.log('Message with id ' + receivedMessageId + ' passed to default handler');
            AllJoynWinRTComponent.AllJoyn.aj_BusHandleBusMessage(aj_message);
            AllJoynWinRTComponent.AllJoyn.aj_CloseMsg(aj_message);
          }
        }
        updateInterval(status);
      });
    }
    // Initially start with slower interval
    runningFast = false;
    interval = setInterval(handlerFunction, AJ_MESSAGE_SLOW_LOOP_INTERVAL);
  }

  messageHandler.stop = function() {
    clearInterval(interval);
  }

  messageHandler.addHandler = function(messageId, signature, callback) {
    // Create a list of handlers for this message id if it doesn't exist yet
    if (typeof messageListeners[messageId] != "object") {
      messageListeners[messageId] = [];
    }
    messageListeners[messageId].push([signature, callback]);
  }

  messageHandler.removeHandler = function(messageId, callback) {
    messageListeners[messageId] = messageListeners[messageId].filter(
      function(element) {
        // Filter out the given callback function
        return (element[1] !== callback);
      }
    );
  }

  return messageHandler;
})();
