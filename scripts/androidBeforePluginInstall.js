#!/usr/bin/env node

var path = require('path');
var fs = require('fs');
var exec = require('child_process').exec;

module.exports = function (context) {
  var Q = context.requireCordovaModule('q');
  var deferral = new Q.defer();

  var ajtclDirectory = path.join('plugins', 'org.allseen.alljoyn', 'src', 'ajtcl-android');
  var ajtclUpstream = 'https://github.com/irjudson/ajtcl-android.git';

  if (fs.existsSync(ajtclDirectory)) {
    console.log("Found ajtcl-android from: " + path.resolve(ajtclDirectory));
  } else {
    console.log('Cloning ajtcl-android from: ' + ajtclUpstream);
    exec('git clone  ' + ajtclUpstream + ' ' + ajtclDirectory,
      function (error, stdout, stderr) {
        if (error !== null) {
          console.log('Git clone failed: ' + error);
        } 
        deferral.resolve();
    });
  }

  return deferral.promise;
};
