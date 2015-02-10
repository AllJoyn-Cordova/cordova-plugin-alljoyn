#!/usr/bin/env node

var path = require('path');
var fs = require('fs');
var exec = require('child_process').exec;
var sh = require('shelljs');

module.exports = function (context) {
  var Q = context.requireCordovaModule('q');
  var deferral = new Q.defer();

  var ajtclPath = path.join(__dirname, '..', 'src', 'ajtcl');
  var ajtclAndroidPath = path.join(__dirname, '..', 'src', 'ajtcl-android');
  var buildScriptPath = path.join(ajtclAndroidPath, 'buildPlugin.sh'); 
  var destPath = path.join(__dirname, '..', 'src', 'android')

  try {
    process.chdir(ajtclAndroidPath);
    exec(buildScriptPath + ' ' + ajtclPath + ' ' + ajtclAndroidPath,
      function (error, stdout, stderr) {
        if (error !== null) {
          console.log('Building ajtcl assets for android failed: ' + error);
          deferral.resolve();
        } else {
          sh.cp('-f', path.join(ajtclAndroidPath, 'libs', 'armeabi', 'liballjoyn.so'), path.join(destPath, 'armeabi'));
          // sh.cp('-R', path.join(ajtclAndroidPath, 'src', 'alljoyn', '*'), path.join(destPath, 'alljoyn'));
          deferral.resolve();
        }
    });
  } catch (err) {
    console.log("Error building assets.");
    deferral.resolve();
  }

  return deferral.promise;
};
