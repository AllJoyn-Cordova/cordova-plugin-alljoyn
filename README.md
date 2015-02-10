cordova-plugin-alljoyn
======================

Cordova plugin for [AllJoyn](https://allseenalliance.org/alljoyn-framework-tutorial).

Implementation Notes
--------------------
After cloning this repository, a plugin developer needs to get ajtcl by running these commands in the project root folder:

```
$ git clone https://git.allseenalliance.org/gerrit/core/ajtcl src/ajtcl
$ git -C src/ajtcl checkout RB14.12
```

For plugin users, above is taken care of by a hook run after plugin is added with Cordova scripts.

Here are my current implementation plans:

* Integrating with AllJoyn Core SDK 14.06.00a THIN CLIENT LIBRARY (AJTCL)
* Add Windows native support (x64) by building WinRT component port of the necessary libraries, these can be based on the AJTCL Win32 targets for net and crypto.
* Windows build requires Windows SDK 8.0 and Windows Phone build Windows Phone SDK 8.1

Using the plugin for Windows platform:

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