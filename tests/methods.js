describe('Implementing methods and replies', function () {
    var connectedBus = null;

    beforeEach(function (done) {
        AllJoyn.connect(function (bus) {
            connectedBus = bus;
            var applicationObjects = [
                {
                    path: '/path',
                    interfaces: [
                        [
                            'com.example.methods.interface',
                            '?DoSomething <s >s',
                            null
                        ],
                        null
                    ]
                },
                null
            ];
            var proxyObjects = [
                {
                    path: '/path',
                    interfaces: [
                        [
                            'com.example.methods.interface',
                            '?DoSomething <s >s',
                            null
                        ],
                        null
                    ]
                },
                null
            ];
            AllJoyn.registerObjects(function () {
                done();
            }, function () {
                expect(true).toBe(false);
                done();
            }, applicationObjects, proxyObjects);
        });
    });

    it('should be able to respond to implemented methods', function (done) {
        var TEST_SERVICE_NAME = 'com.example.application.service';
        var TEST_SERVICE_PORT = 27;

        var doSomethingHandler = function (messageForReply) {
            var receivedMessage = messageForReply.message;
            if (receivedMessage.arguments[0] === 'Incoming success') {
                messageForReply.replySuccess('s', ['Outgoing success']);
            } else {
                messageForReply.replyError('Outgoing error');
            }
        };
        connectedBus.addListenerForReply([1, 0, 0, 0], 's', doSomethingHandler);

        connectedBus.startAdvertisingName(function () {
            connectedBus.joinSession(function (session) {
                var callDoSomething = function (incomingString, successCallback, errorCallback) {
                    session.callMethod(function (message) {
                        successCallback(message.arguments[0]);
                    }, function (status) {
                        errorCallback(status);
                    }, session.sessionHost,
                    // TODO: Passing '/path' as parameter instead of null results
                    // into an error on iOS.
                    null,
                    [2, 0, 0, 0], 's', [incomingString], 's');
                };

                callDoSomething('Incoming success', function (outgoingString) {
                    expect(outgoingString).toBe('Outgoing success');
                    callDoSomething('Incoming error', function (outgoingString) {}, function (status) {
                        // TODO: Check that correct error status is received
                        done();
                    });
                }, function (status) {});
            }, function (status) {
                expect(true).toBe(false);
                done();
            }, {name: TEST_SERVICE_NAME, port: TEST_SERVICE_PORT});
        }, function (status) {
            expect(true).toBe(false);
            done();
        }, TEST_SERVICE_NAME, TEST_SERVICE_PORT);
    });
});
