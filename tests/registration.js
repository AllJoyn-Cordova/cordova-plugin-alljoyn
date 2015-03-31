describe('Object registration', function () {
    it('registering valid objects', function (done) {
        var applicationObjects = [
            {
                path: '/path',
                interfaces: [
                    [
                        'com.example.application.interface',
                        '?Sample <sas >v',
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
                        'com.example.proxy.interface',
                        '?Sample <sas >v',
                        null
                    ],
                    null
                ]
            },
            null
        ];
        AllJoyn.registerObjects(function () {
            expect(true).toBe(true);
            done();
        }, function () {
            expect(true).toBe(false);
            done();
        }, applicationObjects, proxyObjects);
    });
});
