cordova-plugin-alljoyn
======================

A Cordova plugin to expose the [AllJoyn](https://allseenalliance.org/alljoyn-framework-tutorial) Thin Client (AJTCL 14.12) to cross platform applications written in Javascript. 

Purpose
--------------------
To provide a plugin which allows using the AllJoyn Thin Client library across all mobile platforms without requiring the user to deal with implementing and compiling the native code for each operating system.

Current Platforms:
* iOS
* Windows Modern App
* Windows Phone
* Android (in-progress)

An effort has been made to expose as many of the AJTCL features to Javascript as possible, while maintaining a clean Javascript API.  Features are prioritized based on which scenarios they unblock.  

For Plugin Developers / Contributors
--------------------
After cloning this repository, a plugin developer needs to get ajtcl by running these commands in the project root folder:

```
$ git clone https://git.allseenalliance.org/gerrit/core/ajtcl src/ajtcl
$ git -C src/ajtcl checkout RB14.12
```

For plugin users, above is taken care of by a hook run after plugin is added with Cordova scripts.

Using the plugin for Windows platforms
--------------------------------------
```
$ cd /path/to/your/cordova/app
$ cordova add [/path/to/plugin or <url to this git repo>]
$ cordova platform add windows
```

Running with Cordova scripts:

```
// To run on Windows Phone 8.1 emulator
$ cordova run windows --emulator --archs="x86" -- -phone
// Running on Windows Phone 8.1 device
$ cordova run windows --device --archs="arm" -- -phone
// To run on desktop (current default is Windows 8.0 build)
$ cordova run windows --device --archs="x64" -- -win
```

Alternative for running with Cordova scripts is to open the solution file generated after "cordova platform add windows"-command in Visual Studio and running the wanted app project. In this case, these is a need to manually select the correct architecture from build configuration.

Caveats
-------
* Building a Cordova app with the plugin for the windows platform requires two unreleased features:
  * https://issues.apache.org/jira/browse/CB-7911
  * https://issues.apache.org/jira/browse/CB-8123
    * This one is still under development so it should be considered as a moving target
