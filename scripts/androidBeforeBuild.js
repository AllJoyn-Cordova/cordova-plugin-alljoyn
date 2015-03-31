#!/usr/bin/env node

var path = require('path');
var fs = require('fs');
var sys = require('sys');
var exec = require('child_process').exec;

module.exports = function (context) {
    var Q = context.requireCordovaModule('q');
    var deferral = new Q.defer();

    exec('gradle -p platforms/android/AllJoynLib', function (error, stdout, stderr) {
        sys.puts(stdout);
        deferral.resolve();
    });

    return deferral.promise;
};
