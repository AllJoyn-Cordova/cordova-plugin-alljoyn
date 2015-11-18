#!/usr/bin/env node

var exec = require('child_process').exec;

module.exports = function (context) {
    var Q = context.requireCordovaModule('q');
    var deferral = new Q.defer();

    console.log('Starting to build the AllJoyn native library using Gradle...');
    var gradleBuild = exec('gradle -p platforms/android/AllJoynLib', function (error, stdout, stderr) {
        if (error === null) {
            deferral.resolve();
        } else {
            console.log('Gradle build error: ' + error);
            deferral.reject();
        }
    });

    gradleBuild.stdout.on('data', function (data) {
        console.log('' + data);
    });

    gradleBuild.stderr.on('data', function (data) {
        console.log('' + data);
    });

    return deferral.promise;
};
