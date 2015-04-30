#!/usr/bin/env node

var path = require('path');
var fs = require('fs');
var sys = require('sys');
var exec = require('child_process').exec;

module.exports = function (context) {
    var Q = context.requireCordovaModule('q');
    var deferral = new Q.defer();
    var settingsFileName = path.join('platforms', 'android', 'settings.gradle');
    var settingsFile = fs.openSync(settingsFileName, 'a');

    if (settingsFile) {
        fs.writeSync(settingsFile, 'include ":AllJoynLib"');
        fs.closeSync(settingsFile);
    } else {
        console.log('settings.gradle not found.');
    }

    deferral.resolve();
    return deferral.promise;
};
