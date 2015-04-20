var exec = require('cordova/exec');
var cordova = require('cordova');

var registeredObjects = [];
var connectedBus = null;

var getSignature = function (indexList, objectsList) {
    var objects = objectsList[indexList[0]];
    var object = objects[indexList[1]];
    var interfaces = object.interfaces;
    var signature = interfaces[indexList[2]][indexList[3] + 1];
    return signature;
};

var getSignalRuleString = function (member, interface) {
    return 'type=\'signal\',member=\'' + member + '\',interface=\'' + interface + '\'';
};

var buildMsgFromMsgArguments = function (msgInfoAndArguments) {
    var msg = {
        arguments: msgInfoAndArguments[1]
    };
    var msgInfo = msgInfoAndArguments[0];
    for (var msgInfoProp in msgInfo) {
        if (msgInfo.hasOwnProperty(msgInfoProp)) {
            msg[msgInfoProp] = msgInfo[msgInfoProp];
        }
    }
    return msg;
};

var wrapMsgInfoReceivingCallback = function (callback) {
    return function (msgInfoAndArguments) {
        var msg = buildMsgFromMsgArguments(msgInfoAndArguments);
        callback(msg);
    };
};

var AllJoyn = {
    connect: function (success, error) {
        var successCallback = function () {
            var bus = {
                addListener: function (indexList, responseType, listener) {
                    // We are passing the listener function to the exec call as its success callback, but in this case,
                    // it is expected that the callback can be called multiple times. The error callback is passed just because
                    // exec requires it, but it is not used for anything.
                    // The listener is also passed as a parameter, because in the Windows implementation, the success callback
                    // can't be called multiple times.
                    var wrappedListener = wrapMsgInfoReceivingCallback(listener);
                    exec(wrappedListener, function () {}, 'AllJoyn', 'addListener', [indexList, responseType, wrappedListener]);
                },
                addListenerForReply: function (indexList, responseType, listener) {
                    // Called when we get a message that matches the index
                    // messagePointer needs to be sent back to the to open the message loop back up
                    // doneCallback is used in WinRT implementation
                    var listenerForReply = function (messageBody, messagePointer, doneCallback) {
                        var getClass = {}.toString;
                        var replyCompleted = function () {
                            if (doneCallback && getClass.call(doneCallback) === '[object Function]') {
                                doneCallback(messagePointer);
                            }
                        };
                        var msgForReply = {
                            message: buildMsgFromMsgArguments(messageBody),
                            replySuccess: function (parameterTypes, parameters) {
                                exec(
                                    replyCompleted,
                                    function () {}, 'AllJoyn', 'sendSuccessReply', [messagePointer, parameterTypes, parameters]);
                            },
                            replyError: function (errorMessage) {
                                exec(
                                    replyCompleted,
                                    function () {}, 'AllJoyn', 'sendErrorReply', [messagePointer, errorMessage]);
                            }
                        };

                        listener(msgForReply);
                    };

                    exec(listenerForReply, function () {}, 'AllJoyn', 'addListenerForReply', [indexList, responseType, listenerForReply]);
                },
                // joinSessionRequest = {
                //   port: 12,
                //   sender: 'afa-f',
                //   sessionId: 123,
                //   response: function // to be called with either true or false
                // }
                // Usage: bus.acceptSessionListener = myListenerfunction (joinSessionRequest);
                acceptSessionListener: function (joinSessionRequest) {
                    joinSessionRequest.response(true);
                },
                addSignalRule: function (success, error, member, interfaceName) {
                    var ruleString = getSignalRuleString(member, interfaceName);
                    exec(success, error, 'AllJoyn', 'setSignalRule', [ruleString, 0]);
                },
                removeSignalRule: function (success, error, member, interfaceName) {
                    var ruleString = getSignalRuleString(member, interfaceName);
                    exec(success, error, 'AllJoyn', 'setSignalRule', [ruleString, 1]);
                },
                /*
                 * When name found, listener is called with parameter { name: 'the.name.found' }
                 */
                addAdvertisedNameListener: function (name, listener) {
                    var getAdvertisedNameObject = function (message) {
                        return {
                            message: message,
                            name: message.arguments.name
                        };
                    };
                    var wrappedListener = wrapMsgInfoReceivingCallback(function (message) {
                        listener(getAdvertisedNameObject(message));
                    });
                    exec(wrappedListener, function () {}, 'AllJoyn', 'addAdvertisedNameListener', [name, wrappedListener]);
                },
                addInterfacesListener: function (interfaceNames, listener) {
                    var aboutAnnouncementRule = 'interface=\'org.alljoyn.About\',sessionless=\'t\'';
                    if (interfaceNames) {
                        if (interfaceNames.constructor !== Array) {
                            interfaceNames = [interfaceNames];
                        }
                        interfaceNames.forEach(function (currentValue, index, array) {
                            aboutAnnouncementRule += ',implements=\'' + currentValue + '\'';
                        });
                    }

                    console.log('aboutAnnouncementRule: ' + aboutAnnouncementRule);
                    var onAboutAnnouncementReceived = function (message) {
                        var aboutAnnouncement = {
                            message: message, // Original announcement message
                            objects: [], // Holds the object descriptions
                            properties: {} // Properties from the announcement
                        };
                        console.log('onAboutAnnouncementReceived: ' + JSON.stringify(arguments));
                        if (message && message.arguments) {
                            var msgArgs = message.arguments;
                            if (msgArgs[0] !== undefined) {
                                aboutAnnouncement.version = msgArgs[0];
                            }
                            if (msgArgs[1] !== undefined) {
                                aboutAnnouncement.port = msgArgs[1];
                            }

                            // Get the object description from the announcement
                            // the 'a(oas)' part of the signature
                            if (msgArgs[2] && msgArgs[2].constructor === Array) {
                                msgArgs[2].forEach(function (objectDescription) {
                                    if (objectDescription.constructor === Array) {
                                        var object = {};
                                        object.path = objectDescription[0];
                                        object.interfaces = objectDescription[1];
                                        aboutAnnouncement.objects.push(object);
                                    }
                                });
                            }

                            // Get the properties from the announcement
                            // the 'a{sv}' part of the signature
                            if (msgArgs[3] && msgArgs[3].constructor === Array) {
                                msgArgs[3].forEach(function (objectProperty) {
                                    if (objectProperty.constructor === Array) {
                                        aboutAnnouncement.properties[objectProperty.shift()] = objectProperty;
                                    }
                                });
                            }
                        }
                        console.log('AboutAnnouncement: ' + JSON.stringify(aboutAnnouncement));
                        listener(aboutAnnouncement);
                    };
                    var onAddAboutAnnouncementRuleSuccess = function () {
                        var aboutAnnouncementIndexList = [0, 5, 1, 3]; // AJ_SIGNAL_ABOUT_ANNOUNCE
                        bus.addListener(aboutAnnouncementIndexList, 'qqa(oas)a{sv}', onAboutAnnouncementReceived);
                    };
                    exec(onAddAboutAnnouncementRuleSuccess, function () {}, 'AllJoyn', 'setSignalRule', [aboutAnnouncementRule, 0]);
                },
                startAdvertisingName: function (success, error, name, port) {
                    exec(success, error, 'AllJoyn', 'startAdvertisingName', [name, port]);
                },
                stopAdvertisingName: function (success, error, name, port) {
                    exec(success, error, 'AllJoyn', 'stopAdvertisingName', [name, port]);
                },
                /*
                var service = {
                name: 'name.of.the.service',
                port: 12
                };
                */
                joinSession: function (success, error, service) {
                    var successCallback = function (msg) {
                        var sessionId = msg.arguments[0];
                        var sessionHost = msg.arguments[1];
                        var session = {
                            sessionId: sessionId,
                            sessionHost: sessionHost,
                            message: msg,
                            callMethod: function (callMethodSuccess, callMethodError, destination, path, indexList, inParameterType, parameters, outParameterType) {
                                var signature = getSignature(indexList, registeredObjects);
                                var wrappedSuccessCallback = wrapMsgInfoReceivingCallback(callMethodSuccess);
                                exec(wrappedSuccessCallback, callMethodError, 'AllJoyn', 'invokeMember', [sessionId, destination, signature, path, indexList, inParameterType, parameters, outParameterType]);
                            },
                            sendSignal: function (sendSignalSuccess, sendSignalError, destination, path, indexList, inParameterType, parameters) {
                                var signature = getSignature(indexList, registeredObjects);
                                exec(sendSignalSuccess, sendSignalError, 'AllJoyn', 'invokeMember', [sessionId, destination, signature, path, indexList, inParameterType, parameters]);
                            },
                            leave: function (leaveSuccess, leaveError) {
                                exec(leaveSuccess, leaveError, 'AllJoyn', 'leaveSession', [sessionId]);
                            }
                        };
                        success(session);
                    };
                    var wrappedSuccessCallback = wrapMsgInfoReceivingCallback(successCallback);
                    exec(wrappedSuccessCallback, error, 'AllJoyn', 'joinSession', [service]);
                },
                sendSignal: function (sendSignalSuccess, sendSignalError, indexList, inParameterType, parameters) {
                    var signature = getSignature(indexList, registeredObjects);
                    exec(sendSignalSuccess, sendSignalError, 'AllJoyn', 'invokeMember', [null, null, signature, null, indexList, inParameterType, parameters]);
                }
            };

            var acceptSessionListener = function (messageBody, messagePointer, doneCallback) {
                var getClass = {}.toString;
                var responseCallback = function (response) {
                    exec(function () {
                        if (doneCallback && getClass.call(doneCallback) === '[object Function]') {
                            doneCallback(messagePointer);
                        }
                    }, function () {}, 'AllJoyn', 'replyAcceptSession', [messagePointer, response]);
                };

                var joinSessionRequest = {
                    port: messageBody[0],
                    sessionId: messageBody[1],
                    sender: messageBody[2],
                    response: responseCallback
                };
                bus.acceptSessionListener(joinSessionRequest);
            };
            exec(acceptSessionListener, function () {}, 'AllJoyn', 'setAcceptSessionListener', [acceptSessionListener]);

            connectedBus = bus;
            success(bus);
        };
        if (connectedBus === null) {
            exec(successCallback, error, 'AllJoyn', 'connect', ['', 5000]);
        } else {
            success(connectedBus);
        }
    },
    registerObjects: function (success, error, applicationObjects, proxyObjects) {
        exec(function () {
            registeredObjects = [null, applicationObjects, proxyObjects];
            success();
        }, error, 'AllJoyn', 'registerObjects', [applicationObjects, proxyObjects]);
    },
    AJ_OK: 0
};

module.exports = AllJoyn;
