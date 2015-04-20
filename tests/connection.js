describe('Connecting to bus', function () {
    it('bus should be returned after connecting', function (done) {
        AllJoyn.connect(function (bus) {
            expect(bus.addListener).toBeDefined();
            // TODO: Check also other members of the bus object
            done();
        });
    });
});
