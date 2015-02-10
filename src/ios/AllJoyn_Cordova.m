#define AJ_MODULE BASIC_CLIENT

#import "AllJoyn_Cordova.h"
#include <stdio.h>
#include <stdlib.h>
#include "aj_debug.h"
#include "alljoyn.h"
#include "aj_disco.h"
#include "aj_config.h"


// Used to enable handling the msg responce for a method invocation
// returns true if the handler handled the message. False if not
// in which case other handlers will have a chance at it
typedef bool (^MsgHandler)(AJ_Message*);

typedef struct {
    AJ_Status status;
    unsigned int nextArgumentIndex;
} Marshal_Status;

#define MSG_TIMEOUT 1000 /* 1 sec */
#define METHOD_CALL_TIMEOUT 1000 /* 1 sec */

uint8_t dbgBASIC_CLIENT = 1;

@interface AllJoyn_Cordova  ()
// Dictionary of handlers keyed off of message id
// If a message handler exists for a given message id it will be
// invoked with a pointer to the message when it is being unmarshalled
@property NSMutableDictionary* MessageHandlers;
// Used for thread to handle msg loop and other AllJoyn communication
@property dispatch_queue_t dispatchQueue;
// used for triggering background thread activity
@property dispatch_source_t dispatchSource;
// Indicates if the app is connected to the bus or not
@property Boolean connectedToBus;

// Indicates if there is a callback to the web app in progress
// This usually means we need to stop processing messages on the loop until it is done
@property Boolean callbackInProgress;

@property AJ_Message* callbackMessage;

// Property to hold dynamically registered object lists
@property AJ_Object* proxyObjects;
@property AJ_Object* appObjects;

@property AJ_BusAttachment* busAttachment;;
@end

@implementation AllJoyn_Cordova

// Constructor for plugin class
- (CDVPlugin*)initWithWebView:(UIWebView*)theWebView {
    self = [super initWithWebView:theWebView];

    AJ_InfoPrintf((" -- AllJoyn Plugin Class Init\n"));

    _connectedToBus = false;
    _proxyObjects = NULL;
    _appObjects = NULL;
    _callbackInProgress = false;
    _callbackMessage = NULL;

    _busAttachment = malloc(sizeof(AJ_BusAttachment));
    memset(_busAttachment, 0, sizeof(AJ_BusAttachment));

    // Create a dispatcher for background tasks (msg loop, msg sending, etc. )
    _dispatchQueue = dispatch_queue_create("org.cordova.plugin.AllJoyn", NULL);
    // Dictionary for method reply handlers
    _MessageHandlers = [NSMutableDictionary dictionaryWithObjectsAndKeys: nil];

    [self createDispatcherTimer];
    return self;
}

-(void)connect:(CDVInvokedUrlCommand*)command {
    [self.commandDelegate runInBackground:^{
        if(![self connectedToBus]) {
            printf("Starting ...\n");
            AJ_Status status = AJ_OK;
            AJ_Initialize();

            printf("Starting the dispatcher\n");
            // Start the background task
            dispatch_resume([self dispatchSource]);

            printf("Connecting to the bus...\n");
            status = [self internalConnectBus:[self busAttachment]];
            if(status == AJ_OK) {
                [self setConnectedToBus:true];
                [self sendSuccessMessage:@"Connected" toCallback:[command callbackId] withKeepCallback:false];
            } else {
                [self sendErrorMessage:@"Failed to connect" toCallback:[command callbackId] withKeepCallback:false];
            }
            printf("\n\nStarted!\n");
        }
    }];
}

-(AJ_Object*)createObjectListFromObjectDescriptions:(NSArray*)objectDescriptions {
    AJ_Object* objectList = NULL;

    if(objectDescriptions != nil || [objectDescriptions count] > 0) {
        objectList = AJ_ObjectsCreate();

        for (NSDictionary* objectDescription in objectDescriptions) {
            if([objectDescription isKindOfClass:[NSDictionary class]]) {
                AJ_Object newObject = {0};

                // If flags were present set them
                NSNumber* flags = [objectDescription objectForKey:@"flags"];
                if(flags !=nil && [flags isKindOfClass:[NSNumber class]]) {
                    newObject.flags = [flags unsignedCharValue];
                }

                //TODO: Need to track this memory allocation
                newObject.path = strdup([[objectDescription objectForKey:@"path"] UTF8String]);
                AJ_InterfaceDescription* interfaces = AJ_InterfacesCreate();

                for (NSArray* interface in [objectDescription objectForKey:@"interfaces"]) {
                    if([interface isKindOfClass:[NSArray class]]) {
                        char** ifaceMethods = NULL;
                        for(NSString* member in interface) {
                            if([member isKindOfClass:[NSString class]]) {

                                if(!ifaceMethods) {
                                    printf("InterfaceName: %s\n", [member UTF8String]);
                                    ifaceMethods = AJ_InterfaceDescriptionCreate([member UTF8String]);
                                } else {
                                    if([member length] > 0) {
                                        printf("Member: %s\n", [member UTF8String]);
                                        ifaceMethods = AJ_InterfaceDescriptionAdd(ifaceMethods, [member UTF8String]);
                                    }
                                }
                            }
                        }

                        interfaces = AJ_InterfacesAdd(interfaces, ifaceMethods);
                    }

                }
                newObject.interfaces = interfaces;
                objectList = AJ_ObjectsAdd(objectList, newObject);
            }
        }
        AJ_PrintXML(objectList);
    }

    return objectList;

}

-(void)registerObjects:(CDVInvokedUrlCommand*)command {
    [self.commandDelegate runInBackground:^{
        AJ_Initialize();
        NSArray* appObjects = [command argumentAtIndex:0];
        NSArray* proxyObjects = [command argumentAtIndex:1];

        AJ_Object* appObjectList = [self createObjectListFromObjectDescriptions:appObjects];
        AJ_Object* proxyObjectList = [self createObjectListFromObjectDescriptions:proxyObjects];
        AJ_RegisterObjects(appObjectList, proxyObjectList);
        [self setProxyObjects:proxyObjectList];
        [self setAppObjects:appObjectList];
        [self sendSuccessMessage:@"Registered" toCallback:[command callbackId] withKeepCallback:false];
    }];
}

-(void)startAdvertisingName:(CDVInvokedUrlCommand*)command {
    [self.commandDelegate runInBackground:^{
        NSString* nameToAdvertise = [command argumentAtIndex:0];
        NSNumber* portToHostOn = [command argumentAtIndex:1];

        if(![nameToAdvertise isKindOfClass:[NSString class]] || ![portToHostOn isKindOfClass:[NSNumber class]]) {
            [self sendErrorMessage:@"startAdvertisingName: Invalid argument(s)" toCallback:[command callbackId] withKeepCallback:false];
        } else {
            AJ_Status status = AJ_OK;

            AJ_SessionOpts* sessionOptions = NULL;

            printf("Calling AJ_BusBindSessionPort Port=%u\n", [portToHostOn unsignedShortValue]);
            status = AJ_BusBindSessionPort([self busAttachment], [portToHostOn unsignedShortValue], sessionOptions, 0);
            if(status == AJ_OK) {
                uint32_t bindSessionPortReplyId = AJ_REPLY_ID(AJ_METHOD_BIND_SESSION_PORT);
                NSNumber* bindSessionPortReplyKey = [NSNumber numberWithUnsignedInt:bindSessionPortReplyId];
                MsgHandler messageHandler = ^bool(AJ_Message* pMsg) {
                    printf("Got bindSessionPort reply\n");
                    [[self MessageHandlers] removeObjectForKey:bindSessionPortReplyKey];
                    printf("Callling AJ_BusRequestName for %s\n", [nameToAdvertise UTF8String]);
                    AJ_Status status = AJ_BusRequestName([self busAttachment], [nameToAdvertise UTF8String], 0);
                    if(status == AJ_OK) {
                        uint32_t requestNameReplyId = AJ_REPLY_ID(AJ_METHOD_REQUEST_NAME);
                        NSNumber* requestNameReplyKey = [NSNumber numberWithUnsignedInt:requestNameReplyId];
                        MsgHandler requestNameReplyHandler = ^bool(AJ_Message* pMsg) {
                            printf("Got busRequestName reply\n");
                            [[self MessageHandlers] removeObjectForKey:requestNameReplyKey];
                            printf("Calling AJ_BusAdvertiseName\n");
                            AJ_Status status = AJ_BusAdvertiseName([self busAttachment], [nameToAdvertise UTF8String], AJ_TRANSPORT_ANY, AJ_BUS_START_ADVERTISING, 0);
                            if(status == AJ_OK) {
                                uint32_t busAdvertiseNameReplyId = AJ_REPLY_ID(AJ_METHOD_ADVERTISE_NAME);
                                NSNumber* busAdvertiseNameReplyKey = [NSNumber numberWithUnsignedInt:busAdvertiseNameReplyId];
                                MsgHandler busAdvertiseNameReplyHandler = ^bool(AJ_Message* pMsg){
                                    printf("Got busAdvertiseName Reply\n");
                                    [[self MessageHandlers] removeObjectForKey:busAdvertiseNameReplyKey];
                                    if(!pMsg || !(pMsg->hdr) || (pMsg->hdr->msgType == AJ_MSG_ERROR)) {
                                        [self sendErrorMessage:@"startAdvertisingName: Failure reply received." toCallback:[command callbackId] withKeepCallback:false];
                                    } else {
                                        printf("About INIT!\n");
                                        AJ_Status status = AJ_AboutInit([self busAttachment], [portToHostOn unsignedShortValue]);
                                        if(status != AJ_OK) {
                                            printf("Failure initializing about %s\n", AJ_StatusText(status));
                                        }
                                        uint32_t acceptSessionId = AJ_METHOD_ACCEPT_SESSION;
                                        NSNumber* acceptSessionKey = [NSNumber numberWithUnsignedInt:acceptSessionId];
                                        MsgHandler acceptSessionHandler = ^bool(AJ_Message* pMsg) {
                                            AJ_Status status = AJ_BusReplyAcceptSession(pMsg, 1);
                                            if(status != AJ_OK) {
                                                printf("Failure accepting session %s", AJ_StatusText(status));
                                            }

                                            return true;
                                        };

                                        [[self MessageHandlers] setObject:acceptSessionHandler forKey:acceptSessionKey];


                                        [self sendSuccessMessage:@"startAdvertisingName: Success" toCallback:[command callbackId] withKeepCallback:false];
                                    }
                                    return true; // busAdvertiseNameReply
                                };
                                [[self MessageHandlers] setObject:busAdvertiseNameReplyHandler forKey:busAdvertiseNameReplyKey];

                            } else {
                                NSString* failureString = [NSString stringWithFormat:@"startAdvertisingName: Failure in AJ_BusAdvertiseName %s", AJ_StatusText(status)];
                                [self sendErrorMessage:failureString toCallback:[command callbackId] withKeepCallback:false];
                            }

                            return true; // requestNameReplyHandler
                        };
                        [[self MessageHandlers] setObject:requestNameReplyHandler forKey:requestNameReplyKey];

                    } else {
                        NSString* failureString = [NSString stringWithFormat:@"startAdvertisingName: Failure in AJ_BusRequestName %s", AJ_StatusText(status)];
                        [self sendErrorMessage:failureString toCallback:[command callbackId] withKeepCallback:false];
                    }

                    return true; // bindSessionPortHandler
                };

                [[self MessageHandlers] setObject:messageHandler forKey:bindSessionPortReplyKey];
            } else {
                NSString* failureString = [NSString stringWithFormat:@"startAdvertisingName: Failure in AJ_BusBindSessionPort %s", AJ_StatusText(status)];
                [self sendErrorMessage:failureString toCallback:[command callbackId] withKeepCallback:false];
            }
        }
    }];
}

-(void)stopAdvertisingName:(CDVInvokedUrlCommand*)command {
    [self.commandDelegate runInBackground:^{
        NSString* wellKnownName = [command argumentAtIndex:0 withDefault:nil andClass:[NSString class]];
        NSNumber* port = [command argumentAtIndex:1 withDefault:nil andClass:[NSNumber class]];

        AJ_Status status = AJ_BusUnbindSession([self busAttachment], [port unsignedShortValue]);

        if(status == AJ_OK) {
            uint32_t unbindSessionReplyId = AJ_REPLY_ID(AJ_METHOD_UNBIND_SESSION);
            NSNumber* unbindSessionReplyKey = [NSNumber numberWithUnsignedInt:unbindSessionReplyId];
            MsgHandler unbindSessionReplyHandler = ^bool(AJ_Message* pMsg){

                [[self MessageHandlers]removeObjectForKey:unbindSessionReplyKey];

                if(!pMsg || !pMsg->hdr || pMsg->hdr->msgType == AJ_MSG_ERROR) {
                    [self sendErrorStatus:AJ_ERR_FAILURE toCallback:[command callbackId] withKeepCallback:false];
                } else {
                    AJ_Status status = AJ_BusReleaseName([self busAttachment], [wellKnownName UTF8String]);
                    if(status == AJ_OK) {
                        uint32_t releaseNameReplyId = AJ_REPLY_ID(AJ_METHOD_RELEASE_NAME);
                        NSNumber* releaseNameReplyKey = [NSNumber numberWithUnsignedInt:releaseNameReplyId];
                        MsgHandler releaseNameReplyHandler = ^bool(AJ_Message* pMsg) {
                            [[self MessageHandlers] removeObjectForKey:releaseNameReplyKey];

                            if(!pMsg || !pMsg->hdr || pMsg->hdr->msgType == AJ_MSG_ERROR) {
                                [self sendErrorStatus:AJ_ERR_FAILURE toCallback:[command callbackId] withKeepCallback:false];
                            } else {

                                AJ_Status status = AJ_BusAdvertiseName([self busAttachment], [wellKnownName UTF8String], AJ_TRANSPORT_ANY, AJ_BUS_STOP_ADVERTISING, 0);
                                if(status == AJ_OK) {
                                    uint32_t stopAdvertiseNameReplyId = AJ_REPLY_ID(AJ_METHOD_ADVERTISE_NAME);
                                    NSNumber* stopAdvertiseNameReplyKey = [NSNumber numberWithUnsignedInt:stopAdvertiseNameReplyId];
                                    MsgHandler stopAdvertiseNameReplyHandler = ^bool(AJ_Message* pMsg) {
                                        [[self MessageHandlers] removeObjectForKey:stopAdvertiseNameReplyKey];
                                        if(!pMsg || !pMsg->hdr || pMsg->hdr->msgType == AJ_MSG_ERROR) {
                                            [self sendErrorStatus:AJ_ERR_FAILURE toCallback:[command callbackId] withKeepCallback:false];
                                        } else {
                                            [self sendSuccessMessage:@"stopAdvertisingName: Success" toCallback:[command callbackId] withKeepCallback:false];
                                        }
                                        return true;
                                    };

                                    [[self MessageHandlers] setObject:stopAdvertiseNameReplyHandler forKey:stopAdvertiseNameReplyKey];


                                } else {
                                    [self sendErrorStatus:status toCallback:[command callbackId] withKeepCallback:false];
                                }
                            }

                            return true;

                        };

                        [[self MessageHandlers] setObject:releaseNameReplyHandler forKey:releaseNameReplyKey];

                    } else {
                        [self sendErrorStatus:status toCallback:[command callbackId] withKeepCallback:false];
                    }


                }
                return true;
            };

            [[self MessageHandlers] setObject:unbindSessionReplyHandler forKey:unbindSessionReplyKey];

        } else {
            [self sendErrorStatus:status toCallback:[command callbackId] withKeepCallback:false];
        }

    }];
}

// We are passing the listener function to the exec call as its success callback, but in this case,
// it is expected that the callback can be called multiple times. The error callback is passed just because
// exec requires it, but it is not used for anything.
// The listener is also passed as a parameter, because in the Windows implementation, the success callback
// can't be called multiple times.
// exec(listener, function() { }, "AllJoyn", "addListener", [indexList, responseType, listener]);
-(void)addListener:(CDVInvokedUrlCommand*)command {
    [self.commandDelegate runInBackground:^{
        NSArray* indexList = [command argumentAtIndex:0];
        NSString* responseType = [command argumentAtIndex:1];

        if(![indexList isKindOfClass:[NSArray class]] ||
           ![responseType isKindOfClass:[NSString class]]) {
            [self sendErrorMessage:@"addListener: Invalid argument." toCallback:[command callbackId] withKeepCallback:false];
            return;
        }

        if([indexList count] < 4) {
            [self sendErrorMessage:@"addListener: Expected 4 indices in indexList" toCallback:[command callbackId] withKeepCallback:false];
            return;
        }
        NSNumber* listIndex = [indexList objectAtIndex:0];
        NSNumber* objectIndex = [indexList objectAtIndex:1];
        NSNumber* interfaceIndex = [indexList objectAtIndex:2];
        NSNumber* memberIndex = [indexList objectAtIndex:3];

        uint32_t msgId = AJ_ENCODE_MESSAGE_ID(
                                              [listIndex unsignedIntValue],
                                              [objectIndex unsignedIntValue],
                                              [interfaceIndex unsignedIntValue],
                                              [memberIndex unsignedIntValue]);

        printf("Adding listener for msgId=%u\n", msgId);

        AJ_MemberType memberType = AJ_GetMemberType(msgId, NULL, NULL);
        if(memberType == AJ_INVALID_MEMBER) {
            [self sendErrorMessage:@"addListener: Invalid message id/index list" toCallback:[command callbackId] withKeepCallback:false];
            return;
        }

        NSNumber* methodKey = [NSNumber numberWithUnsignedInt:msgId];
        MsgHandler messageHandler = ^bool(AJ_Message* pMsg) {
            NSMutableArray* msgArguments = [NSMutableArray new];

            Marshal_Status marshalStatus = [self unmarshalArgumentsFor:pMsg withSignature:responseType toValues:msgArguments];

            if(marshalStatus.status == AJ_OK) {
                [self sendSuccessArray:msgArguments toCallback:[command callbackId] withKeepCallback:true];
            } else {
                [self sendErrorMessage:[NSString stringWithFormat:@"Error %s", AJ_StatusText(marshalStatus.status)] toCallback:[command callbackId] withKeepCallback:true];
            }
            return true;
        };

        [[self MessageHandlers] setObject:messageHandler forKey:methodKey];
    }];
}

-(void)addAdvertisedNameListener:(CDVInvokedUrlCommand*)command {
    [self.commandDelegate runInBackground:^{
        NSString* name = [command argumentAtIndex:0];

        AJ_Status status = [self findService:[self busAttachment] withName:name];
        if(status != AJ_OK) {
            [self sendErrorMessage:@"Failure starting find" toCallback:[command callbackId] withKeepCallback:false];
            return;
        } else {
            NSNumber* methodKey = [NSNumber numberWithUnsignedInt:AJ_SIGNAL_FOUND_ADV_NAME];
            MsgHandler messageHandler = ^bool(AJ_Message* pMsg) {

                AJ_Arg arg;
                AJ_UnmarshalArg(pMsg, &arg);
                AJ_InfoPrintf(("FoundAdvertisedName(%s)\n", arg.val.v_string));

                NSMutableDictionary* responseDictionary = [NSMutableDictionary new];

                [responseDictionary setObject:[NSString stringWithUTF8String:arg.val.v_string] forKey:@"name"];
                [responseDictionary setObject:[NSString stringWithUTF8String:pMsg->sender] forKey:@"sender"];
                [self sendSuccessDictionary:responseDictionary toCallback:[command callbackId] withKeepCallback:true];
                return true;
            };

            [[self MessageHandlers] setObject:messageHandler forKey:methodKey];
        }
    }];
}

-(void)addInterfacesListener:(CDVInvokedUrlCommand*)command {
    [self.commandDelegate runInBackground:^{
        NSArray* interfaces = [command argumentAtIndex:0];
        AJ_Status status = [self askForAboutAnnouncements:[self busAttachment] forObjectsImplementing:interfaces];
        if(status != AJ_OK) {
            [self sendErrorMessage:@"Failure starting find" toCallback:[command callbackId] withKeepCallback:false];
        } else {
            NSNumber* methodKey = [NSNumber numberWithUnsignedInt:AJ_SIGNAL_ABOUT_ANNOUNCE];
            MsgHandler messageHandler = ^bool(AJ_Message* pMsg) {
                uint16_t aboutVersion, aboutPort;
                AJ_UnmarshalArgs(pMsg, "qq", &aboutVersion, &aboutPort);
                AJ_InfoPrintf((" -- AboutVersion: %d, AboutPort: %d\n", aboutVersion, aboutPort));
                //TODO: Get more about info and convert to proper callback
                NSMutableDictionary* responseDictionary = [NSMutableDictionary new];
                [responseDictionary setObject:[NSNumber numberWithUnsignedInt:aboutVersion] forKey:@"version"];
                [responseDictionary setObject:[NSNumber numberWithUnsignedInt:aboutPort] forKey:@"port"];
                [responseDictionary setObject:[NSString stringWithUTF8String:pMsg->sender] forKey:@"name"];
                [self sendSuccessDictionary:responseDictionary toCallback:[command callbackId] withKeepCallback:true];
                return true;
            };

            [[self MessageHandlers] setObject:messageHandler forKey:methodKey];
        }
    }];
}

-(void)setAcceptSessionListener:(CDVCommandStatus*)command {
    [self.commandDelegate runInBackground:^{
        //TODO: Add implementation
    }];
}

-(void)setSignalRule:(CDVInvokedUrlCommand*)command {
    [self.commandDelegate runInBackground:^{
        NSString* ruleString = [command argumentAtIndex:0];
        NSNumber* ruleType = [command argumentAtIndex:1];

        if(![ruleString isKindOfClass:[NSString class]] ||
           ![ruleType isKindOfClass:[NSNumber class]]) {
            [self sendErrorMessage:@"setSignalRule: Invalid Argument" toCallback:[command callbackId] withKeepCallback:false];
            return;
        }

        AJ_Status status = AJ_BusSetSignalRule([self busAttachment], [ruleString UTF8String], [ruleType unsignedCharValue]);

        if(status == AJ_OK) {
            [self sendSuccessMessage:@"setSignalRule: Success" toCallback:[command callbackId] withKeepCallback:false];
        } else {
            NSString* errorMessage = [NSString stringWithFormat:@"setSignalRule: Failure %s", AJ_StatusText(status)];
            [self sendErrorMessage:errorMessage toCallback:[command callbackId] withKeepCallback:false];
        }
        return;
    }];
}

-(void)joinSession:(CDVInvokedUrlCommand*)command {
    [self.commandDelegate runInBackground:^{
        printf("+joinSessionAsyc\n");
        AJ_Status status = AJ_OK;
        NSDictionary* server = [command argumentAtIndex:0];
        if(![server isKindOfClass:[NSDictionary class]]) {
            [self sendErrorMessage:@"JoinSession: Invalid Argument" toCallback:[command callbackId] withKeepCallback:false];
            return;
        }

        NSNumber* port = [server objectForKey:@"port"];
        NSString* name = [server objectForKey:@"name"];

        status = AJ_BusJoinSession([self busAttachment], [name UTF8String], [port intValue], NULL);
        if(status == AJ_OK) {
            NSNumber* methodKey = [NSNumber numberWithUnsignedInt:AJ_REPLY_ID(AJ_METHOD_JOIN_SESSION)];
            MsgHandler messageHandler = ^bool(AJ_Message* pMsg) {
                [[self MessageHandlers] removeObjectForKey:methodKey];

                AJ_InfoPrintf((" -- Got reply to JoinSession ---\n"));
                AJ_InfoPrintf(("MsgType: %d 0x%x\n", (*pMsg).hdr->msgType, (*pMsg).hdr->msgType));
                uint32_t replyCode;
                uint32_t sessionId;

                if ((*pMsg).hdr->msgType == AJ_MSG_ERROR) {
                    [self sendErrorMessage:@"Failure joining session MSG ERROR" toCallback:[command callbackId] withKeepCallback:false];
                } else {
                    AJ_UnmarshalArgs(pMsg, "uu", &replyCode, &sessionId);
                    if (replyCode == AJ_JOINSESSION_REPLY_SUCCESS) {
                        NSMutableArray* responseArray = [NSMutableArray new];
                        [responseArray addObject:[NSNumber numberWithUnsignedInt:sessionId]];
                        [responseArray addObject:name];
                        [self sendSuccessArray:responseArray toCallback:[command callbackId] withKeepCallback:false];
                    } else {
                        if(replyCode == AJ_JOINSESSION_REPLY_ALREADY_JOINED) {
                            NSMutableArray* responseArray = [NSMutableArray new];
                            [responseArray addObject:[NSNumber numberWithUnsignedInt:pMsg->sessionId]];
                            [responseArray addObject:name];
                            [self sendSuccessArray:responseArray toCallback:[command callbackId] withKeepCallback:false];
                        } else {
                            [self sendErrorMessage:[NSString stringWithFormat:@"Failure joining session replyCode = 0x%x %d", replyCode, replyCode] toCallback:[command callbackId] withKeepCallback:false];
                        }
                    }

                }
                return true;

            };

            [[self MessageHandlers] setObject:messageHandler forKey:methodKey];

        } else {
            [self sendErrorMessage:[NSString stringWithFormat:@"Failed to iniitate join session: %x %d %s", status, status, AJ_StatusText(status)] toCallback:[command callbackId] withKeepCallback:false];
        }
    }];
}

-(void)leaveSession:(CDVInvokedUrlCommand*) command {
    [self.commandDelegate runInBackground:^{
        NSNumber* sessionId = [command argumentAtIndex:0];

        if(![sessionId isKindOfClass:[NSNumber class]]) {
            [self sendErrorMessage:@"leaveSession: Invalid argument." toCallback:[command callbackId] withKeepCallback:false];
            return;
        }

        AJ_Status status = AJ_BusLeaveSession([self busAttachment], [sessionId unsignedIntValue]);
        if(status == AJ_OK) {
            NSString* successMessage = [NSString stringWithFormat:@"Left session %u", [sessionId unsignedIntValue]];
            [self sendSuccessMessage:successMessage toCallback:[command callbackId] withKeepCallback:false];
        } else {
            NSString* failedMessage = [NSString stringWithFormat:@"Failed to leave session %d. Reason = %s", [sessionId unsignedIntValue],
                                       AJ_StatusText(status)];
            [self sendErrorMessage:failedMessage toCallback:[command callbackId] withKeepCallback:false];
        }
    }];
}

-(void)invokeMember:(CDVInvokedUrlCommand*) command {
    [self.commandDelegate runInBackground:^{
        NSNumber* sessionId = [command argumentAtIndex:0];
        NSString* destination = [command argumentAtIndex:1];
        NSString* signature = [command argumentAtIndex:2];
        NSString* path = [command argumentAtIndex:3];
        NSArray* indexList = [command argumentAtIndex:4];
        NSString* parameterTypes = [command argumentAtIndex:5];
        NSArray* parameters = [command argumentAtIndex:6 withDefault:[NSArray new]];
        NSString* outParameterSignature = [command argumentAtIndex:7];
        bool isOwnSession = false;

        AJ_Status status = AJ_OK;

        if( ![signature isKindOfClass:[NSString class]] ||
           ![indexList isKindOfClass:[NSArray class]]) {

            [self sendErrorMessage:@"inokeMember: Invalid Argument" toCallback:[command callbackId] withKeepCallback:false];
            return;
        }

        if([indexList count] < 4) {
            [self sendErrorMessage:@"invokeMember: Expected 4 indices in indexList" toCallback:[command callbackId] withKeepCallback:false];
            return;
        }
        NSNumber* listIndex = [indexList objectAtIndex:0];
        NSNumber* objectIndex = [indexList objectAtIndex:1];
        NSNumber* interfaceIndex = [indexList objectAtIndex:2];
        NSNumber* memberIndex = [indexList objectAtIndex:3];

        if( ![listIndex isKindOfClass:[NSNumber class]] ||
           ![objectIndex isKindOfClass:[NSNumber class]] ||
           ![interfaceIndex isKindOfClass:[NSNumber class]] ||
           ![memberIndex isKindOfClass:[NSNumber class]]) {
            [self sendErrorMessage:@"invokeMember: non-number index encountered" toCallback:[command callbackId] withKeepCallback:false];
        }
        if([sessionId unsignedIntValue] == 0) {
            printf("SessionId is 0, overriding listINdex to 1\n");
            listIndex = [NSNumber numberWithUnsignedInt:1];
            isOwnSession = true;
        }
        uint32_t msgId = AJ_ENCODE_MESSAGE_ID(
                                              [listIndex unsignedIntValue],
                                              [objectIndex unsignedIntValue],
                                              [interfaceIndex unsignedIntValue],
                                              [memberIndex unsignedIntValue]);

        printf("Message id: %u\n", msgId);

        const char* memberSignature = NULL;
        uint8_t isSecure = 0;

        AJ_MemberType memberType = AJ_GetMemberType(msgId, &memberSignature, &isSecure);

        AJ_Message msg;

        if(path != nil && [path length] > 0) {
            if([self proxyObjects]->path) {
                free((void*)[self proxyObjects]->path);
                [self proxyObjects]->path = NULL;
            }
            status = AJ_SetProxyObjectPath([self proxyObjects], msgId, strdup([path UTF8String]));
            if(status != AJ_OK) {
                printf("AJ_SetProxyObjectPath failed with %s\n", AJ_StatusText(status));
                goto e_Exit;
            }
        }

        const char* destinationChars = NULL;
        if(destination != nil) {
            destinationChars = [destination UTF8String];
        }

        printf("MemberType: %u, MemberSignature: %s, IsSecure %u\n", memberType, memberSignature, isSecure);
        switch(memberType) {
            case AJ_METHOD_MEMBER:
                status = AJ_MarshalMethodCall([self busAttachment], &msg, msgId, destinationChars, [sessionId unsignedIntValue], 0, MSG_TIMEOUT);
                if(status != AJ_OK) {
                    printf("Failure marshalling method call");
                    goto e_Exit;
                }
                if(parameterTypes != nil && [parameterTypes length] > 0) {
                    status = [self marshalArgumentsFor:&msg withSignature:parameterTypes havingValues:parameters startingAtIndex:0].status;
                }
                break;
            case AJ_SIGNAL_MEMBER: {
                uint8_t signalFlags = 0;
                uint32_t ttl = 0;
                if(isOwnSession) {
                    signalFlags = AJ_FLAG_GLOBAL_BROADCAST;
                }
                if(destination == nil && isOwnSession) {
                    printf("SESSIONLESS SIGNAL\n");
                    signalFlags |= AJ_FLAG_SESSIONLESS;
                }
                status = AJ_MarshalSignal([self busAttachment], &msg, msgId, [destination UTF8String], [sessionId unsignedIntValue], signalFlags, ttl);
                if(status != AJ_OK) {
                    printf("AJ_MarshalSignal failed with %s\n", AJ_StatusText(status));
                    goto e_Exit;
                }

                if(parameterTypes != nil && [parameterTypes length] > 0) {
                    status = [self marshalArgumentsFor:&msg withSignature:parameterTypes havingValues:parameters startingAtIndex:0].status;
                    if(status != AJ_OK) {
                        printf("Failure marshalling arguments: %s\n", AJ_StatusText(status));
                        goto e_Exit;
                    }
                }
                break;
            }
            case AJ_PROPERTY_MEMBER:
                break;
            default:
                status = AJ_ERR_FAILURE;
                break;
        }

        if(AJ_OK == status) {
            status = AJ_DeliverMsg(&msg);

            if(memberType != AJ_SIGNAL_MEMBER) {
                NSNumber* methodKey = [NSNumber numberWithUnsignedInt:AJ_REPLY_ID(msgId)];
                MsgHandler MsgHandler = ^bool(AJ_Message* pMsg) {
                    AJ_Status status = AJ_OK;
                    NSMutableArray* outValues = [NSMutableArray new];

                    [[self MessageHandlers] removeObjectForKey:methodKey];

                    if(!pMsg || !(pMsg->hdr) || pMsg->hdr->msgType == AJ_MSG_ERROR) {
                        // Error
                        [self sendErrorMessage:@"Error" toCallback:[command callbackId] withKeepCallback:false];
                        return true;
                    }

                    if(outParameterSignature != nil && [outParameterSignature length] > 0) {
                        status = [self unmarshalArgumentsFor:pMsg withSignature:outParameterSignature toValues:outValues].status;
                    }

                    if(status != AJ_OK) {
                        NSString* formattedError = [NSString stringWithFormat:@"Failure unmarshalling response: %s", AJ_StatusText(status)];
                        [self sendErrorMessage:formattedError toCallback:[command callbackId] withKeepCallback:false];
                        return true;
                    }

                    [self sendSuccessArray:outValues toCallback:[command callbackId] withKeepCallback:false];

                    return true;
                };

                [[self MessageHandlers] setObject:MsgHandler forKey:methodKey];
            }
        }

    e_Exit:
        if(status != AJ_OK) {
            [self sendErrorMessage:[NSString stringWithFormat:@"InvokeMember failure: %s", AJ_StatusText(status)] toCallback:[command callbackId] withKeepCallback:false];
        } else if(memberType == AJ_SIGNAL_MEMBER) {
            [self sendSuccessMessage:@"Send Signal success" toCallback:[command callbackId] withKeepCallback:false];
        }

        return;

    }];
}
-(Marshal_Status)unmarshalArgumentFor:(AJ_Message*)pMsg withSignature:(NSString*)signature toValues:(NSMutableArray*)values {
    return [self unmarshalArgumentsFor:pMsg withSignature:signature toValues:values limit:1];
}

-(Marshal_Status)unmarshalArgumentsFor:(AJ_Message*)pMsg withSignature:(NSString*)signature toValues:(NSMutableArray*)values {
    return [self unmarshalArgumentsFor:pMsg withSignature:signature toValues:values limit:0];
}

-(Marshal_Status)unmarshalArgumentsFor:(AJ_Message*)pMsg withSignature:(NSString*)signature toValues:(NSMutableArray*)values limit:(unsigned int)argumentLimit{
    Marshal_Status marshalStatus = {AJ_OK, 0};
    printf("+unmarshalArgumentsFor: %s\n", [signature UTF8String]);

    unsigned long len = [signature length];

    AJ_Arg arg = {0};

    for(int i =0; i<len && (argumentLimit == 0 || i < argumentLimit);i++) {
        char currentType = [signature UTF8String][i];

        //Reset arg to initial values
        arg.container = 0;
        arg.flags = 0;
        arg.len = 0;
        arg.sigPtr = 0;
        arg.val.v_data = NULL;
        printf("CurrentType: %c %s\n", currentType, AJ_StatusText(marshalStatus.status));
        switch(currentType) {
            case AJ_ARG_BOOLEAN:
                marshalStatus.status = AJ_UnmarshalArg(pMsg, &arg);
                if(arg.val.v_bool) {
                    [values addObject:[NSNumber numberWithUnsignedInt:*(arg.val.v_bool)]];
                } else {
                    marshalStatus.status = AJ_ERR_MARSHAL;
                }
                break;
            case AJ_ARG_INT16:
                marshalStatus.status = AJ_UnmarshalArg(pMsg, &arg);
                if(arg.val.v_int16) {
                    [values addObject:[NSNumber numberWithShort:*(arg.val.v_int16)]];
                } else {
                    marshalStatus.status = AJ_ERR_MARSHAL;
                }
                break;
            case AJ_ARG_INT32:
                marshalStatus.status = AJ_UnmarshalArg(pMsg, &arg);
                if(arg.val.v_int32) {
                    [values addObject:[NSNumber numberWithInt:*(arg.val.v_int32)]];
                } else {
                    marshalStatus.status = AJ_ERR_MARSHAL;
                }
                break;
            case AJ_ARG_INT64:
                marshalStatus.status = AJ_UnmarshalArg(pMsg, &arg);
                if(arg.val.v_int64) {
                    [values addObject:[NSNumber numberWithLongLong:*(arg.val.v_int64)]];
                } else {
                    marshalStatus.status = AJ_ERR_MARSHAL;
                }
                break;
            case AJ_ARG_UINT16:
                marshalStatus.status = AJ_UnmarshalArg(pMsg, &arg);
                if(arg.val.v_uint16) {
                    [values addObject:[NSNumber numberWithUnsignedShort:*(arg.val.v_uint16)]];
                } else {
                    marshalStatus.status = AJ_ERR_MARSHAL;
                }
                break;
            case AJ_ARG_UINT32:
                marshalStatus.status = AJ_UnmarshalArg(pMsg, &arg);
                if(arg.val.v_uint32) {
                    [values addObject:[NSNumber numberWithUnsignedInt:*(arg.val.v_uint32)]];
                } else {
                    marshalStatus.status = AJ_ERR_MARSHAL;
                }
                break;
            case AJ_ARG_UINT64:
                marshalStatus.status = AJ_UnmarshalArg(pMsg, &arg);
                if(arg.val.v_uint64) {
                    [values addObject:[NSNumber numberWithUnsignedLongLong:*(arg.val.v_uint64)]];
                } else {
                    marshalStatus.status = AJ_ERR_MARSHAL;
                }
                break;
            case AJ_ARG_DOUBLE:
                marshalStatus.status = AJ_UnmarshalArg(pMsg, &arg);
                if(arg.val.v_double) {
                    [values addObject:[NSNumber numberWithDouble:*(arg.val.v_double)]];
                } else {
                    marshalStatus.status = AJ_ERR_MARSHAL;
                }
                break;
            case AJ_ARG_BYTE:
                marshalStatus.status = AJ_UnmarshalArg(pMsg, &arg);
                if(arg.val.v_byte) {
                    [values addObject:[NSNumber numberWithChar:*(arg.val.v_byte)]];
                } else {
                    marshalStatus.status = AJ_ERR_MARSHAL;
                }
                break;

            case AJ_ARG_STRING: {
                marshalStatus.status = AJ_UnmarshalArg(pMsg, &arg);
                if(marshalStatus.status == AJ_OK) {
                    NSString* stringArg = [NSString stringWithUTF8String:arg.val.v_string];
                    [values addObject:stringArg];
                }
                break;
            }
            case AJ_ARG_VARIANT: {
                const char* varSig = NULL;
                marshalStatus.status = AJ_UnmarshalVariant(pMsg, &varSig);
                if(marshalStatus.status == AJ_OK) {
                    // Marshal the actual type
                    marshalStatus.status = [self unmarshalArgumentsFor:pMsg withSignature:[NSString stringWithUTF8String:varSig] toValues:values].status;
                }
                break;
            }
            case AJ_ARG_STRUCT: {
                marshalStatus.status = AJ_UnmarshalContainer(pMsg, &arg, AJ_ARG_STRUCT);
                if(marshalStatus.status == AJ_OK) {
                    NSMutableArray* structValues = [NSMutableArray new];
                    marshalStatus = [self unmarshalArgumentsFor:pMsg withSignature:[signature substringFromIndex:i+1] toValues:structValues];
                    if(marshalStatus.status == AJ_OK) {
                        i += marshalStatus.nextArgumentIndex;
                        [values addObject:structValues];
                        marshalStatus.status = AJ_UnmarshalCloseContainer(pMsg, &arg);
                    }
                }
                break;
            }
            case AJ_ARG_DICT_ENTRY: {
                marshalStatus.status = AJ_UnmarshalContainer(pMsg, &arg, AJ_ARG_DICT_ENTRY);
                if(marshalStatus.status == AJ_OK) {
                    NSMutableArray* dictValues = [NSMutableArray new];
                    marshalStatus = [self unmarshalArgumentsFor:pMsg withSignature:[signature substringFromIndex:i+1] toValues:dictValues];
                    if(marshalStatus.status == AJ_OK && [dictValues count] != 2) {
                        printf("Dictionary entry too large");
                        marshalStatus.status = AJ_ERR_MARSHAL;
                    }
                    if(marshalStatus.status == AJ_OK) {
                        i += marshalStatus.nextArgumentIndex;
                        [values addObject:dictValues];
                        marshalStatus.status = AJ_UnmarshalCloseContainer(pMsg, &arg);
                    }
                }
                break;
            }
            case AJ_ARG_ARRAY: {
                marshalStatus.status = AJ_UnmarshalContainer(pMsg, &arg, AJ_ARG_ARRAY);

                unsigned int maximumArgumentLength = 0;
                NSMutableArray* arrayValues = [NSMutableArray new];
                if(marshalStatus.status == AJ_OK) {
                    while(marshalStatus.status == AJ_OK) {
                        marshalStatus = [self unmarshalArgumentFor:pMsg withSignature:[signature substringFromIndex:i+1] toValues:arrayValues];
                        if(marshalStatus.nextArgumentIndex > maximumArgumentLength) {
                            maximumArgumentLength = marshalStatus.nextArgumentIndex;
                        }
                    }
                }
                if(marshalStatus.status == AJ_ERR_NO_MORE) {
                    i += maximumArgumentLength;
                    [values addObject:arrayValues];
                    marshalStatus.status = AJ_UnmarshalCloseContainer(pMsg, &arg);

                }
                break;
            }
            case '}': {
                break;
            }
            case ')': {
                printf("Done with struct\n");
                // Found the end of struct, return up
                break;
            }
            default: {
                marshalStatus.status = AJ_ERR_FAILURE;
                break;
            }
        }

        if(marshalStatus.status != AJ_OK) {
            break;
        }
        marshalStatus.nextArgumentIndex = i + 1;
    }
e_Exit:
    printf("-unmarshalArgumentsFor: %s %d %s\n", [signature UTF8String], marshalStatus.nextArgumentIndex, AJ_StatusText(marshalStatus.status));

    return marshalStatus;
}

-(NSString*)getNextToken:(NSString*)signature {
    NSString* nextToken = nil;
    NSMutableArray* containerStack = [NSMutableArray new];
    unsigned int tokenEndIndex = 0;

    NSString* structContainer = @"(";
    NSString* dictContainer = @"{";
    NSString* prevContainer = nil;

    // Only do work if signature exists and has some value
    if(signature != nil && [signature length] > 0) {
        for(tokenEndIndex = 0;tokenEndIndex< [signature length]; tokenEndIndex++) {
            switch([signature UTF8String][tokenEndIndex]) {
                case 'a':
                    continue;
                    break;
                case '(':
                    [containerStack addObject:structContainer];
                    break;
                case ')':
                    if([containerStack count] < 1) {
                        printf("Error: Invalid signature (unmatched structure marker): %s", [signature UTF8String]);
                        goto ErrorExit;
                    }
                    prevContainer = [containerStack lastObject];
                    [containerStack removeLastObject];
                    if([prevContainer compare:structContainer]) {
                        printf("Error: Invalid signature (unmatched structure marker): %s", [signature UTF8String]);
                        goto ErrorExit;
                    }
                    break;
                case '{':
                    [containerStack addObject:dictContainer];
                    break;
                case '}':
                    if([containerStack count] < 1) {
                        printf("Error: Invalid signature (unmatched dictionary marker): %s", [signature UTF8String]);
                        goto ErrorExit;
                    }
                    prevContainer = [containerStack lastObject];
                    [containerStack removeLastObject];
                    if([prevContainer compare:dictContainer] ) {
                        printf("Error: Invalid signature (unmatched dictionary marker): %s", [signature UTF8String]);
                        goto ErrorExit;
                    }
                    break;
                default:
                    break;
            }

            // We found one type without being in an open container.
            if([containerStack count] == 0) {
                break;
            }

        }
    }

    nextToken = [signature substringToIndex:tokenEndIndex + 1];

    return nextToken;
ErrorExit:
    return nil;
}

-(Marshal_Status)marshalArgumentsFor:(AJ_Message*)pMsg withSignature:(NSString*)signature havingValues:(NSArray*)values startingAtIndex:(unsigned int)initialArgumentIndex {

    printf("marshalArgumentsFor %s InitialIndex: %d\n", [signature UTF8String], initialArgumentIndex);
    printf("Next Token: %s\n", [[self getNextToken:signature] UTF8String]);
    Marshal_Status marshalStatus = {AJ_OK, initialArgumentIndex};

    // Check parameters
    if(!pMsg ||
       signature == nil || [signature length] < 1 ||
       values == nil || [values count] <= initialArgumentIndex) {
        marshalStatus.status = AJ_ERR_INVALID;
        return marshalStatus;
    }

    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    double d;
    const char* varSig = NULL;

    AJ_Arg arg = {0};

    for(unsigned int signatureIndex = 0; signatureIndex < [signature length]; signatureIndex++) {

        char current = [signature UTF8String][signatureIndex];

        // Structure closing is handled by parent
        if(current == ')') {
            break;
        }

        if(marshalStatus.nextArgumentIndex >= [values count]) {
            marshalStatus.status = AJ_ERR_MARSHAL;
            break;
        }
        //Reset arg to initial values
        arg.container = 0;
        arg.flags = 0;
        arg.len = 0;
        arg.sigPtr = 0;
        arg.val.v_data = NULL;
        arg.typeId = (uint8_t)current;


        //        /*
        //         * Message argument types
        //         */
        //#define AJ_ARG_INVALID           '\0'   /**< AllJoyn invalid type */
        //#define AJ_ARG_ARRAY             'a'    /**< AllJoyn array container type */
        //#define AJ_ARG_BOOLEAN           'b'    /**< AllJoyn boolean basic type */
        //#define AJ_ARG_DOUBLE            'd'    /**< AllJoyn IEEE 754 double basic type */
        //#define AJ_ARG_SIGNATURE         'g'    /**< AllJoyn signature basic type */
        //#define AJ_ARG_HANDLE            'h'    /**< AllJoyn socket handle basic type */
        //#define AJ_ARG_INT32             'i'    /**< AllJoyn 32-bit signed integer basic type */
        //#define AJ_ARG_INT16             'n'    /**< AllJoyn 16-bit signed integer basic type */
        //#define AJ_ARG_OBJ_PATH          'o'    /**< AllJoyn Name of an AllJoyn object instance basic type */
        //#define AJ_ARG_UINT16            'q'    /**< AllJoyn 16-bit unsigned integer basic type */
        //#define AJ_ARG_STRING            's'    /**< AllJoyn UTF-8 NULL terminated string basic type */
        //#define AJ_ARG_UINT64            't'    /**< AllJoyn 64-bit unsigned integer basic type */
        //#define AJ_ARG_UINT32            'u'    /**< AllJoyn 32-bit unsigned integer basic type */
        //#define AJ_ARG_VARIANT           'v'    /**< AllJoyn variant container type */
        //#define AJ_ARG_INT64             'x'    /**< AllJoyn 64-bit signed integer basic type */
        //#define AJ_ARG_BYTE              'y'    /**< AllJoyn 8-bit unsigned integer basic type */
        //#define AJ_ARG_STRUCT            '('    /**< AllJoyn struct container type */
        //#define AJ_ARG_DICT_ENTRY        '{'    /**< AllJoyn dictionary or map container type - an array of key-value pairs */

        switch (current) {

            case AJ_ARG_BOOLEAN:
                u32 = [[values objectAtIndex:marshalStatus.nextArgumentIndex++] unsignedIntValue];
                arg.val.v_bool = &u32;
                break;
            case AJ_ARG_INT16:
                i16 = [[values objectAtIndex:marshalStatus.nextArgumentIndex++] shortValue];
                arg.val.v_int16 = &i16;
                break;
            case AJ_ARG_INT32:
                i32 = [[values objectAtIndex:marshalStatus.nextArgumentIndex++] intValue];
                arg.val.v_int32 = &i32;
                break;
            case AJ_ARG_INT64:
                i64 = [[values objectAtIndex:marshalStatus.nextArgumentIndex++] longLongValue];
                arg.val.v_int64 = &i64;
                break;
            case AJ_ARG_UINT16:
                u16 = [[values objectAtIndex:marshalStatus.nextArgumentIndex++] unsignedShortValue];
                arg.val.v_uint16 = &u16;
                break;
            case AJ_ARG_UINT32:
                u32 = [[values objectAtIndex:marshalStatus.nextArgumentIndex++] unsignedIntValue];
                arg.val.v_uint32 = &u32;
                break;
            case AJ_ARG_UINT64:
                u64 = [[values objectAtIndex:marshalStatus.nextArgumentIndex++] unsignedLongLongValue];
                arg.val.v_uint64 = &u64;
                break;
            case AJ_ARG_DOUBLE:
                d = [[values objectAtIndexedSubscript:marshalStatus.nextArgumentIndex++] doubleValue];
                arg.val.v_double = &d;
                break;
            case AJ_ARG_BYTE:
                u8 = [[values objectAtIndexedSubscript:marshalStatus.nextArgumentIndex++] charValue];
                arg.val.v_byte = &u8;
                break;
            case AJ_ARG_STRING:
                arg.val.v_string = [[values objectAtIndexedSubscript:marshalStatus.nextArgumentIndex++] UTF8String];
                break;
            case AJ_ARG_ARRAY:
                // Determine the signature for array members
                // For each argument in the array paraemter marshal it using the member signature
                // Move signature index forward past array member signature
                arg.typeId = AJ_ARG_ARRAY;
                marshalStatus.status = AJ_MarshalContainer(pMsg, &arg, AJ_ARG_ARRAY);
                if(marshalStatus.status == AJ_OK) {
                    NSArray* arrayValue = [values objectAtIndex:marshalStatus.nextArgumentIndex++];
                    if(![arrayValue isKindOfClass:[NSArray class]]) {
                        marshalStatus.status = AJ_ERR_MARSHAL;
                    } else {
                        unsigned int arrayIndex = 0;
                        Marshal_Status arrayMarshalStatus = {AJ_OK, 0};
                        NSString* arrayMemberSignature = [self getNextToken:[signature substringFromIndex:signatureIndex+1]];
                        if(arrayMemberSignature == nil) {
                            marshalStatus.status = AJ_ERR_MARSHAL;
                        } else {
                            while(arrayIndex < [arrayValue count] && marshalStatus.status == AJ_OK) {
                                arrayMarshalStatus = [self marshalArgumentsFor:pMsg withSignature:arrayMemberSignature havingValues:arrayValue startingAtIndex:arrayIndex++];
                                marshalStatus.status = arrayMarshalStatus.status;
                            }
                            if(marshalStatus.status == AJ_OK) {
                                marshalStatus.status = AJ_MarshalCloseContainer(pMsg, &arg);
                                signatureIndex += [arrayMemberSignature length];

                            }
                        }
                    }
                }
                break;
            case AJ_ARG_DICT_ENTRY:
                // Marshal the open container
                arg.typeId = AJ_ARG_DICT_ENTRY;
                marshalStatus.status = AJ_MarshalContainer(pMsg, &arg, AJ_ARG_DICT_ENTRY);
                if(marshalStatus.status == AJ_OK) {
                    // Get the dictionary entry values from the argument list
                    // Currently we expect this to be an array.
                    // TODO: Consider how to handle case where an object is passed in intending to be an array of dictionary entries
                    NSArray* dictEntryValues = [values objectAtIndex:marshalStatus.nextArgumentIndex++];
                    if(![dictEntryValues isKindOfClass:[NSArray class]]) {
                        marshalStatus.status = AJ_ERR_MARSHAL;
                    } else {
                        // Marshal the key
                        NSString* keySignature = [self getNextToken:[signature substringFromIndex:(signatureIndex + 1)]];
                        if(keySignature == nil) {
                            marshalStatus.status = AJ_ERR_MARSHAL;
                        } else {
                            signatureIndex += [keySignature length];
                            Marshal_Status dictEntryMarshalStatus = [ self marshalArgumentsFor:pMsg withSignature:keySignature havingValues:dictEntryValues startingAtIndex:0];

                            if(dictEntryMarshalStatus.status == AJ_OK) {
                                // marshal the value
                                // The key could only have been a simple type so we start at the 2nd element
                                // in the value array
                                NSString* valueSignature = [self getNextToken:[signature substringFromIndex:(signatureIndex+1)]];
                                if(valueSignature ==nil) {
                                    marshalStatus.status = AJ_ERR_MARSHAL;
                                } else {
                                    dictEntryMarshalStatus = [self marshalArgumentsFor:pMsg withSignature:valueSignature havingValues:dictEntryValues startingAtIndex:1];
                                    marshalStatus.status = dictEntryMarshalStatus.status;
                                    if(dictEntryMarshalStatus.status == AJ_OK) {
                                        signatureIndex += [valueSignature length];
                                        // If we are not at the closing dictionary entry marker then the signature
                                        // is invalid. It should be something like {<token><token>}
                                        if([signature UTF8String][signatureIndex] == '}') {
                                            marshalStatus.status = AJ_MarshalCloseContainer(pMsg, &arg);
                                        } else {
                                            marshalStatus.status = AJ_ERR_MARSHAL;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                break;
            case AJ_ARG_HANDLE:
            case AJ_ARG_INVALID:
                marshalStatus.status = AJ_ERR_INVALID;
                break;
            case AJ_ARG_OBJ_PATH:
            case AJ_ARG_SIGNATURE:
            case AJ_ARG_STRUCT:
                arg.typeId = AJ_ARG_STRUCT;
                marshalStatus.status = AJ_MarshalContainer(pMsg, &arg, AJ_ARG_STRUCT);
                if(marshalStatus.status == AJ_OK) {
                    NSArray* structValues = [values objectAtIndex:marshalStatus.nextArgumentIndex++];
                    if(![structValues isKindOfClass:[NSArray class]]) {
                        marshalStatus.status = AJ_ERR_MARSHAL;
                    } else {
                        NSString* structureSignature = [self getNextToken:[signature substringFromIndex:signatureIndex]];
                        // Marshal the structure value - Use the structure signature starting after the opening '('
                        // When the function finds the corresponding ')' it will return back (see below case)
                        Marshal_Status structureMarshalStatus = [self marshalArgumentsFor:pMsg withSignature:[structureSignature substringFromIndex:1] havingValues:structValues startingAtIndex:0];
                        marshalStatus.status = structureMarshalStatus.status;
                        if(marshalStatus.status == AJ_OK) {
                            signatureIndex += [structureSignature length];
                            marshalStatus.status = AJ_MarshalCloseContainer(pMsg, &arg);
                        }
                    }
                }
                break;
            case ')': // Close Structure
                return marshalStatus;
                break;
            case AJ_ARG_VARIANT:
                varSig = [[values objectAtIndex:marshalStatus.nextArgumentIndex++] UTF8String];
                printf("Marshalling Variant with signature %s\n", varSig);

                // Marshal the variant string
                marshalStatus.status = AJ_MarshalVariant(pMsg, varSig);
                if(marshalStatus.status != AJ_OK) {
                    break;
                }

                // Marshal the actual type
                // Note that we also update our nextArgumentIndex value here.
                // This is because we do not know how many arguments the variant will use before the call
                marshalStatus = [self marshalArgumentsFor:pMsg withSignature:[NSString stringWithUTF8String:varSig] havingValues:values startingAtIndex:marshalStatus.nextArgumentIndex];
                if(marshalStatus.status != AJ_OK) {
                    break;
                }

                // Already marshalled the variant arg no need to call marshal arg again
                continue;
                break;

            default:
                marshalStatus.status = AJ_ERR_UNKNOWN;
                break;
        }

        if(marshalStatus.status == AJ_OK) {
            // The container types handle their own marshalling (
            if(arg.typeId != AJ_ARG_ARRAY && arg.typeId != AJ_ARG_STRUCT && arg.typeId != AJ_ARG_DICT_ENTRY) {
                marshalStatus.status = AJ_MarshalArg(pMsg, &arg);
            }
        } else {
            break;
        }
    }


e_Exit:

    return marshalStatus;
}

// Creates a timer but does NOT start it
dispatch_source_t CreateDispatchTimer(uint64_t interval,
                                      uint64_t leeway,
                                      dispatch_queue_t queue,
                                      dispatch_block_t block) {

    dispatch_source_t timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER,
                                                     0, 0, queue);
    if (timer)
    {
        dispatch_source_set_timer(timer, dispatch_time(DISPATCH_TIME_NOW, interval), interval, leeway);
        dispatch_source_set_event_handler(timer, block);
    }
    return timer;
}

// create a timer and set it to the property
-(void)createDispatcherTimer {
    dispatch_source_t aTimer = CreateDispatchTimer(1ull * NSEC_PER_SEC/10,
                                                   1ull * NSEC_PER_SEC /100,
                                                   _dispatchQueue,
                                                   ^{
                                                       [self msgLoop];
                                                   });
    // Store it somewhere for later use.
    if (aTimer)
    {
        [self setDispatchSource:aTimer];
    }
}

- (void) disconnect:(CDVInvokedUrlCommand*) command {
    AJ_Disconnect([self busAttachment]);
    [self setConnectedToBus:false];
    memset([self busAttachment], 0, sizeof(AJ_BusAttachment));

    // Stop background tasks
    dispatch_suspend([self dispatchSource]);
    [self sendSuccessMessage:@"Disconnected" toCallback:[command callbackId] withKeepCallback:false];
}

-(void) sendSuccessArray:(NSArray*)array toCallback:(NSString*)callbackId withKeepCallback:(Boolean)keepCallback {
    CDVPluginResult* pluginResult = nil;
    pluginResult = [CDVPluginResult resultWithStatus:CDVCommandStatus_OK messageAsArray:array];
    [pluginResult setKeepCallbackAsBool:keepCallback];
    [self.commandDelegate sendPluginResult:pluginResult callbackId:callbackId];

}

-(void) sendSuccessDictionary:(NSDictionary*)dictionary toCallback:(NSString*)callbackId withKeepCallback:(Boolean)keepCallback {
    CDVPluginResult* pluginResult = nil;
    pluginResult = [CDVPluginResult resultWithStatus:CDVCommandStatus_OK messageAsDictionary:dictionary];
    [pluginResult setKeepCallbackAsBool:keepCallback];
    [self.commandDelegate sendPluginResult:pluginResult callbackId:callbackId];

}

-(void) sendSuccessMessage:(NSString*)message toCallback:(NSString*) callbackId withKeepCallback:(Boolean)keepCallback {
    printf("SENDING: %s\n", [message UTF8String]);
    CDVPluginResult* pluginResult = nil;
    pluginResult = [CDVPluginResult resultWithStatus:CDVCommandStatus_OK messageAsString:message];
    [pluginResult setKeepCallbackAsBool:keepCallback];
    [self.commandDelegate sendPluginResult:pluginResult callbackId:callbackId];
}

-(void)sendErrorStatus:(AJ_Status)status toCallback:(NSString*) callbackId withKeepCallback:(Boolean)keepCallback {
    printf("SENDING ERROR: %s\n", AJ_StatusText(status));
    CDVPluginResult* pluginResult = nil;
    pluginResult = [CDVPluginResult resultWithStatus:CDVCommandStatus_ERROR messageAsInt:status];
    [pluginResult setKeepCallbackAsBool:keepCallback];
    [self.commandDelegate sendPluginResult:pluginResult callbackId:callbackId];
}

-(void)sendErrorMessage:(NSString*)message toCallback:(NSString*) callbackId withKeepCallback:(Boolean)keepCallback {
    printf("SENDING ERROR: %s\n", [message UTF8String]);
    CDVPluginResult* pluginResult = nil;
    pluginResult = [CDVPluginResult resultWithStatus:CDVCommandStatus_ERROR messageAsString:message];
    [pluginResult setKeepCallbackAsBool:keepCallback];
    [self.commandDelegate sendPluginResult:pluginResult callbackId:callbackId];
}

// After this method we should have bus
-(AJ_Status)internalConnectBus:(AJ_BusAttachment*) bus
{
    AJ_Status status = AJ_OK;

    status = AJ_FindBusAndConnect(bus, NULL, AJ_CONNECT_TIMEOUT);

    if (status != AJ_OK) {
        printf("Failed to AJ_FindAndConnect 0x%x %d %s\n", status, status, AJ_StatusText(status));
        goto Fail;
    }
Fail:
    return status;
}

-(AJ_Status)findService:(AJ_BusAttachment*)bus withName:(NSString*)serviceName {
    AJ_Status status = AJ_OK;
    status = AJ_BusFindAdvertisedName(bus, [serviceName UTF8String], AJ_BUS_START_FINDING);
    AJ_InfoPrintf(("AJ_StartClient(): AJ_BusFindAdvertisedName() %s\n", AJ_StatusText(status)));
    return status;
}

-(AJ_Status)askForAboutAnnouncements:(AJ_BusAttachment*)bus forObjectsImplementing:(NSArray*) interfaces
{
    AJ_Status status = AJ_OK;
    size_t ruleLen = 0;
    const char* base = "interface='org.alljoyn.About',sessionless='t'";
    const char* impl = ",implements='";
    char* rule = NULL;

    /*
     * Kick things off by registering for the Announce signal.
     * Optionally add the implements clause per given interface
     */
    ruleLen = strlen(base) + 1;
    if (interfaces != nil) {
        for(id interface in interfaces)
        {
            if([interface isKindOfClass:[NSString class]]) {
                ruleLen += strlen(impl) + strlen([interface UTF8String]);
            }

        }
    }
    rule = (char*) AJ_Malloc(ruleLen);
    if (rule == NULL) {
        status = AJ_ERR_RESOURCES;
        goto Fail;
    }
    strcpy(rule, base);
    if (interfaces != nil) {
        for(id interface in interfaces)
        {
            if([interface isKindOfClass:[NSString class]])
            {
                const char* currentIface = [interface UTF8String];
                strcat(rule, impl);
                if ((currentIface)[0] == '$') {
                    strcat(rule, &(currentIface)[1]);
                } else {
                    strcat(rule, currentIface);
                }
                strcat(rule, "'");
            }
        }
    }
    status = AJ_BusSetSignalRule(bus, rule, AJ_BUS_SIGNAL_ALLOW);
    AJ_InfoPrintf(("AJ_StartClient(): Client SetSignalRule: %s\n", rule));
Fail:
    if(rule)
    {
        AJ_Free(rule);
    }
    return status;
}

-(void)msgLoop
{
    AJ_InfoPrintf((" --- MSG LOOP ---\n"));
    AJ_InfoPrintf((" MsgHandlerCount: %lu\n", (unsigned long)[[self MessageHandlers] count]));
    if(![self connectedToBus]) {
        return;
    }

    AJ_Status status = AJ_OK;
    AJ_Message msg;
    // get next message
    status = AJ_UnmarshalMsg([self busAttachment], &msg, MSG_TIMEOUT);

    // Check for errors we can ignore
    if(status == AJ_ERR_TIMEOUT) {
        // Nothing to do for now, continue i guess
        AJ_InfoPrintf(("Timeout getting MSG. Will try again...\n"));
        status = AJ_OK;
    } else if (status == AJ_ERR_NO_MATCH) {
        AJ_InfoPrintf(("AJ_ERR_NO_MATCH in main loop. Ignoring!\n"));
        // Ignore unknown messages
        status = AJ_OK;
    } else if (status != AJ_OK) {
        AJ_ErrPrintf((" -- MainLoopError AJ_UnmarshalMsg returned status=%s\n", AJ_StatusText(status)));
    } else {
        AJ_InfoPrintf((" Executing handlers if any ... \n"));
        // If somebody has requested a handler for a specific msg
        NSNumber* msgIdAsNumber = [NSNumber numberWithUnsignedInt:msg.msgId];
        MsgHandler handler = [self MessageHandlers][msgIdAsNumber];
        bool handled = false;
        if (handler != nil && handler != NULL) {
            handled = handler(&msg);
        }
        if(!handled) {
            AJ_InfoPrintf((" Done Executing handlers if any ... \n"));
            switch (msg.msgId) {
                    //
                    // Method reples
                    //
                case AJ_SIGNAL_SESSION_LOST_WITH_REASON:
                    /*
                     * Force a disconnect
                     */
                {
                    uint32_t id, reason;
                    AJ_UnmarshalArgs(&msg, "uu", &id, &reason);
                    AJ_InfoPrintf(("Session lost. ID = %u, reason = %u", id, reason));
                    //                    [self sendSuccessMessage:@"lost session :("];
                    
                    AJ_ErrPrintf((" -- (): AJ_SIGNAL_SESSION_LOST_WITH_REASON: AJ_ERR_READ\n"));
                }
                    //                    status = AJ_ERR_READ;
                    break;
                    
                default:
                    printf("Dunno msg %u\n", msg.msgId);
                    const char* member = NULL;
                    AJ_GetMemberType(msg.msgId, &member, NULL);
                    printf("Member: %s\n", member);
                    /*
                     * Pass to the built-in bus message handlers
                     */
                    AJ_InfoPrintf((" -- (): AJ_BusHandleBusMessage()\n"));
                    status = AJ_BusHandleBusMessage(&msg);
                    break;
            }
        }
        AJ_CloseMsg(&msg);
    }

    if(status != AJ_OK) {
        printf("ERROR: Main loop had a non-succesful iteration. Exit status: %d 0x%x %s", status, status, AJ_StatusText(status));
        //        [self sendErrorMessage:[NSString stringWithFormat:@"Error encountered: %d 0x%x %s", status, status, AJ_StatusText(status)]];
        return;
    }
}

@end
