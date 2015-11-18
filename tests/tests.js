exports.defineAutoTests = function () {
    var testModules = cordova.require('cordova/plugin_list');
    testModules.forEach(function (testModule) {
        if (testModule.id.indexOf('cordova-plugin-alljoyn-tests.') === 0) {
            cordova.require(testModule.id);
        }
    });
};
