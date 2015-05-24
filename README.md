# webrtcbuilds-builder
=======
[![Build Status](https://travis-ci.org/vsimon/webrtcbuilds-builder.svg?branch=master)](https://travis-ci.org/vsimon/webrtcbuilds-builder)

How [webrtcbuilds](https://github.com/vsimon/webrtcbuilds) gets built. The goal of [webrtcbuilds](https://github.com/vsimon/webrtcbuilds) is to provide a single standalone WebRTC static library and package.

## Current Platforms and Prerequisites
* OSX (highly recommend [Homebrew](http://brew.sh/) is installed)
* Windows (highly recommend [Visual Studio Community 2013](http://www.chromium.org/developers/how-tos/build-instructions-windows) at least and a [Bash shell such as Git for Windows](https://msysgit.github.io) is installed)
* Linux (tested on Ubuntu 12.04/14.04 64-bit)

## How to run
`./build.sh` to build the latest version of WebRTC.

Or optionally another version specified by git SHA:

```
./build.sh options

OPTIONS:
   -h   Show this message
   -r   Revision represented as a git SHA
```

## Where is the package
`out/webrtcbuilds-<rev>-<sha>-<plat>.zip`
where `<rev>` is the revision number of the commit, `<sha>` is the short git SHA of the commit, and `<plat>` is the platform (linux64, windows, osx).
