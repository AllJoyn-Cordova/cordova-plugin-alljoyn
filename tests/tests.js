exports.defineAutoTests = function() {
  describe('AllJoyn global object (window.AllJoyn)', function() {
    it("should exist", function() {
      expect(window.AllJoyn).toBeDefined();
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
