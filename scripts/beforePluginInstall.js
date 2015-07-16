#!/usr/bin/env node

var path = require('path');
var fs = require('fs');
var exec = require('child_process').exec;

module.exports = function (context) {
    var Q = context.requireCordovaModule('q');
    var deferral = new Q.defer();

    var ajtclDirectory = path.join('plugins', 'cordova-plugin-alljoyn', 'src', 'ajtcl');
    var ajtclUpstream = 'https://github.com/AllJoyn-Cordova/ajtcl.git';
    var ajtclBranch = 'RB14.12';

    if (fs.existsSync(ajtclDirectory)) {
        console.log('Found ajtcl from: ' + path.resolve(ajtclDirectory));
        deferral.resolve();
    } else {
        console.log('Cloning ajtcl from: ' + ajtclUpstream);
        exec('git clone  ' + ajtclUpstream + ' ' + ajtclDirectory,
            function (error, stdout, stderr) {
                if (error !== null) {
                    console.log('Git clone failed: ' + error);
                    deferral.resolve();
                } else {
                    console.log('Checking out branch: ' + ajtclBranch);
                    exec('git -C ' + ajtclDirectory + ' checkout ' + ajtclBranch,
                        function (error, stdout, stderr) {
                            if (error !== null) {
                                console.log('Git failed to checkout branch: ' + error);
                            }
                            deferral.resolve();
                        }
                    );
                }
            }
        );
    }

    return deferral.promise;
};
