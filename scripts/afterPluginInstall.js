#!/usr/bin/env node

var path = require('path');
var fs = require('fs');
var exec = require('child_process').exec;

module.exports = function (context) {
	console.log("afterPluginInstall.js running.");
}
