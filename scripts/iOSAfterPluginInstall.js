#!/usr/bin/env node

var fs = require('fs');
var path = require('path');
var exec = require('child_process').exec;

module.exports = function(context) {
  var Q = context.requireCordovaModule('q');
  var deferral = new Q.defer();

  var platformPath = path.join('platforms', 'ios');
  // The approach to get the project name has been taken from https://gist.github.com/csantanapr/9fc45c76b4d9a2d5ef85
  var xCodeProjectPath = fs.readdirSync(platformPath).filter(function(e) { return e.match(/\.xcodeproj$/i); })[0];
  var projectName = xCodeProjectPath.substring(0, xCodeProjectPath.indexOf('.xcodeproj'));

  var sourcePath = path.join(platformPath, projectName, 'Plugins', 'org.allseen.alljoyn');

  var patchFile = path.join(path.dirname(context.scriptLocation), 'ajtcl-ios.patch');
  var patchCommand = 'patch -d ' + sourcePath + ' -p1 < ' + patchFile;

  exec(patchCommand, function (error, stdout, stderr) {
      console.log(stdout);
      console.log(stderr);
      if (error !== null) {
        console.log('Patching failed.');
      } else {
        console.log('Patching done.');
      }
      deferral.resolve();
    }
  );

  return deferral.promise;
}
