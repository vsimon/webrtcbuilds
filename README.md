# webrtcbuilds
==============

The goal of webrtcbuilds is to provide a single standalone static library and package for WebRTC.

## Current Platforms and Prerequisites
* OSX (highly recommend [Homebrew](http://brew.sh/) is installed)
* Windows (highly recommend [Visual Studio Community 2013](http://www.chromium.org/developers/how-tos/build-instructions-windows) at least and a [Bash shell such as Git for Windows](https://msysgit.github.io) is installed)
* Linux (tested on Ubuntu 16.04 64-bit)

## How to run
`./build.sh` to build the latest version of WebRTC.

Or optionally another version specified by git SHA:

```
./build.sh [OPTIONS]

OPTIONS:
   -h            Show this message
   -m MSVSVER    Microsoft Visual Studio (C++) version (e.g. 2013). Default is Chromium build default.
   -o OUTDIR     Output directory. Default is 'out'
   -r REVISION   Git SHA revision. Default is latest revision.
```

## Where is the package
`out/webrtcbuilds-<rev>-<sha>-<plat>.zip`
where `<rev>` is the revision number of the commit, `<sha>` is the short git SHA of the commit, and `<plat>` is the platform (linux64, windows, osx).
