#!/usr/bin/env node

var alljoyn = require('alljoyn');
var path = require('path');
var os = require('os');
var spawn = require('child_process').spawn;

// Run AllJoun router so that the test app can
// make a connection
(function() {
  var bus = alljoyn.BusAttachment('dummyBus');

  var interface = alljoyn.InterfaceDescription();
  bus.createInterface('org.alljoyn.bus.dummy.interface', interface);

  var object = alljoyn.BusObject("/dummyObject");
  object.addInterface(interface);

  bus.registerSignalHandler(object,
      function(message, info) {
          console.log("Message received: ", message, info);
      },
      interface,
      "Dummy"
  );

  bus.start();
  bus.connect();
})();

var temporaryDirectory = path.join(os.tmpdir(), 'testApp');
var pluginDirectory = path.join(__dirname, '..');

var runProcess = spawn(
  'node',
  [
    'node_modules/cordova-paramedic/main.js',
    '--platform',
    'ios',
    '--plugin',
    pluginDirectory,
    '--tempProjectPath',
    temporaryDirectory,
    '--removeTempProject=true'
  ]
);

runProcess.stdout.on('data', function (data) {
  console.log('' + data);
});

runProcess.stderr.on('data', function (data) {
  console.log('' + data);
});

runProcess.on('exit', function (code) {
  process.exit(code);
});
