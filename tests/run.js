#!/usr/bin/env node

var platform = process.platform;
var cordovaPlatformMapping = {
    darwin: 'ios',
    linux: 'android',
    win32: 'windows'
};
var cordovaPlatform = cordovaPlatformMapping[platform];

var buildOnly = process.argv[2] === 'build-only';
var requestRouter = process.argv[2] === 'request-router';

// Require alljoyn only if a router is requested
var alljoyn = requestRouter && require('alljoyn') || null;
var path = require('path');
var os = require('os');
var spawn = require('child_process').spawn;

// Run AllJoun router so that the test app can
// make a connection
var runRouter = function () {
    var bus = alljoyn.BusAttachment('dummyBus');

    var interface = alljoyn.InterfaceDescription();
    bus.createInterface('org.alljoyn.bus.dummy.interface', interface);

    var object = alljoyn.BusObject('/dummyObject');
    object.addInterface(interface);

    bus.registerSignalHandler(
        object,
        function (message, info) {
            console.log('Message received: ', message, info);
        },
        interface,
        'Dummy'
    );

    bus.start();
    bus.connect();
};

if (requestRouter) {
    runRouter();
}

var temporaryDirectory = path.join(os.tmpdir(), 'testApp');
var pluginDirectory = path.join(__dirname, '..');

var parameticArguments = [
    'node_modules/cordova-paramedic/main.js',
    '--platform',
    cordovaPlatform,
    '--plugin',
    pluginDirectory,
    '--tempProjectPath',
    temporaryDirectory,
    '--removeTempProject=true'
];

if (buildOnly) {
    parameticArguments.push('--buildOnly=true');
}

if (platform === 'win32') {
    parameticArguments.push('--architecture=x86');
}

var runProcess = spawn('node', parameticArguments);

runProcess.stdout.on('data', function (data) {
    console.log('' + data);
});

runProcess.stderr.on('data', function (data) {
    console.log('' + data);
});

runProcess.on('exit', function (code) {
    process.exit(code);
});
