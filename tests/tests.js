exports.defineAutoTests = function() {
  describe('AllJoyn global object (window.AllJoyn)', function() {
    it("should exist", function() {
      expect(window.AllJoyn).toBeDefined();
    });
  });
};
