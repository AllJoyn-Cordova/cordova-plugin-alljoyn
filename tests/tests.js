exports.defineAutoTests = function() {
  describe('AllJoyn global object (window.AllJoyn)', function() {
    it("should exist", function() {
      expect(window.AllJoyn).toBeDefined();
    });
  });

  describe('Object registration', function() {
    it("registering valid objects", function(done) {
      var applicationObjects = [
        {
          path: "/path",
          interfaces: [
            [
              "com.example.application.interface",
              "?Sample <sas >v",
              null
            ],
            null
          ]
        },
        null
      ];
      var proxyObjects = [
        {
          path: "/path",
          interfaces: [
            [
              "com.example.proxy.interface",
              "?Sample <sas >v",
              null
            ],
            null
          ]
        },
        null
      ];
      AllJoyn.registerObjects(function() {
        expect(true).toBe(true);
        done();
      }, function() {
        expect(true).toBe(false);
        done();
      }, applicationObjects, proxyObjects);
    });
  });

  describe('Connecting to bus', function() {
    it("bus should be returned after connecting", function(done) {
      AllJoyn.connect(function(bus) {
        expect(bus.addListener).toBeDefined();
        // TODO: Check also other members of the bus object
        done();
      });
    });
  });
};
