# Spectrecoin
[![GitHub version](https://badge.fury.io/gh/spectrecoin%2Fspectre.svg)](https://badge.fury.io/gh/spectrecoin%2Fspectre) [![HitCount](http://hits.dwyl.io/spectrecoin/https://github.com/spectrecoin/spectre.svg)](http://hits.dwyl.io/spectrecoin/https://github.com/spectrecoin/spectre)

Spectrecoin is a Secure Proof-of-Stake (PoSv3) Network with Anonymous Transaction Capability.

Spectrecoin utilizes a range of proven cryptographic techniques to achieve un-linkable, un-traceable and anonymous transactions on its underlaying blockchain and also protects the users identity by running all the network nodes as Tor hidden services.

# Social
- Visit our website [Spectrecoin](https://spectreproject.io/) (XSPEC).
- Please join us on our [Discord](https://discord.gg/ckkrb8m) server!
- Visit our thread at [BitcoinTalk](https://bitcointalk.org/index.php?topic=2103301.0)

## Key Privacy Technology

Anonymous token creation: Through the use of dual key stealth technology Spectrecoin provides the ability to generate ‘anonymous tokens’ (SPECTRE) by consuming XSPEC. SPECTRE can then be sent anonymously through an implementation of ring signatures based on the Cryptonote protocol to eliminate any transaction history. The wallet offers the opportunity to transfer your balance between public coins, XSPEC, and ‘anonymous tokens’, SPECTRE. We are currently working on improving this technology to improve functionality and privacy.

Built in Tor: The Spectrecoin software offers a full integration of Tor (https://www.torproject.org/) so that the Spectrecoin client runs exclusive as a Tor hidden service using a .onion address to connect to other clients in the network. Your real IP address is therefore protected at all times.

## Basic Coin Specs
<table>
<tr><td>Algo</td><td>PoSv3</td></tr>
<tr><td>Block Time</td><td>64 Seconds</td></tr>
<tr><td>Difficulty Retargeting</td><td>Every Block</td></tr>
<tr><td>Initial Coin Supply</td><td>20,000,000 XSPEC</td></tr>
<tr><td>Max Coin Supply (PoS Phase)</td><td>5% annual inflation</td></tr>
<tr><td>Min stake age</td><td>8 hours & 288 blocks</td></tr>
</table>

## Building from source

**NOTE** that these instructions are relevant for building from master, which is the latest code in development. It is generally stable but can contain features that have had less testing than released versions. If you want to build a stable version of Spectrecoin, please check out the latest release tag before you start building.

### Dependencies

To build the Spectrecoin wallet from source, you will need the following dependencies:

 * OpenSSL 1.1
 * libevent
 * libseccomp
 * libcap
 * boost
 * tor & obfs4proxy (since 2.1.0 tor is run as separate process and thus tor is only a runtime dependency)
 * Qt 5 with Qt Webengine if you want to build the GUI wallet. Qt is not needed for the console wallet.

Additionally, you'll need the native C/C++ compiler for your platform and the basic dependencies needed for any kind of development. Because of Qt Webengine cross compiling is currently not possible:

 * macOS - Xcode with Command Line Tools and clang, QTs QMAKE
 * Windows - [vcpkg](https://github.com/Microsoft/vcpkg) and MSVC, QTs QMAKE
 * Linux - GCC

### Windows

The Windows wallet is build with QTs **QMAKE**. Instructions are found in separate doc at https://github.com/spectrecoin/spectre/blob/master/doc/Windows-build-instructions-README.md

### macOS

To check all dependencies and install missing ones on **macOS** (this uses the [Homebrew](https://brew.sh/) package manager; if you use something else then adjust the commands accordingly):

    brew install autoconf automake libtool pkg-config openssl@1.1 libevent boost gcc wget

The macOS wallet itself is build with QTs **QMAKE**. See https://github.com/spectrecoin/spectre/blob/develop/src/osx.pri for instructions.

### Linux

Check the dockerfile of your corresponding platform for which packages must be installed using the package manager:

 * CentOS - https://github.com/spectrecoin/spectre-builder/blob/develop/CentOS/Dockerfile
 * Debian - https://github.com/spectrecoin/spectre-base/blob/develop/Debian/Dockerfile
 * Fedora - https://github.com/spectrecoin/spectre-base/blob/develop/Fedora/Dockerfile
 * RasperryPi - https://github.com/spectrecoin/spectre-base/blob/develop/RaspberryPi/Dockerfile
 * Ubuntu - https://github.com/spectrecoin/spectre-base/blob/develop/Ubuntu/Dockerfile
  
#### Building 

To fetch the source code and build the wallet run the following commands:

    git clone --recursive https://github.com/spectrecoin/spectre
    cd spectre
    ./autogen.sh
    ./configure --enable-gui 
    make -j2  # use a higher number if you have many cores and memory, leave -j2 out if you are on a very low-powered system like Raspberry Pi

The resulting binaries will be in the `src` directory and called `spectre` for the GUI wallet and `spectrecoind` for the console wallet.

Distribution specfic instructions are found in the corresponding dockerfile. See https://github.com/spectrecoin/spectre/tree/develop/Docker

## Using Docker

This repository contains a Dockerfile to create a ready to use image with spectrecoind. The image is based on our 
spectre base image [spectreproject/spectre-base](https://github.com/spectrecoin/spectre-base), which is based on 
Debian on his side. A dedicated user _spectre_ with UID _1000_ and GID _1000_ is used to run [spectrecoind](https://github.com/spectrecoin/spectre).


#### Start new container
You should prepare a directory on the host machine, which contains all 
spectrecoind data. Per default this is the directory _~/.spectrecoin_ and you
can use this directory also with the Docker container by mounting it into the
container.

Explanation of start command:

```
docker run \
    --name <name> \                 # Name of the container to create
    --rm \                          # Container will automatically be removed after stop
    -it \                           # Start in interactive mode with a virtual terminal
    -v <local-path>:/home/spectre/.spectrecoin/ \ # Mapping of local data folder into the container
    -d \                            # Run in daemon mode
    spectreproject/spectre:latest   # Image to use
```

**IMPORTANT: You must use option _'-v...'_ to mount a host folder into the container, if you use option _'--rm'_! 
Otherwise you might loose your wallet, if the container is stopped!**


To start the container just use

```
docker run --name spectre --rm -it -v ~/.spectrecoin/:/home/spectre/.spectrecoin/ -d spectreproject/spectre:latest
```

#### Stop container
Use cmd _docker stop_ to shutdown a running container. Because of option _--rm_ 
it will be removed automatically afterwards.

```
docker stop <name>
```

#### Restart stopped container
This is only possible, if you do _not_ use option _--rm_ on container start. 
After _docker stop_ the container will stay in place and you can restart it using

```
docker start spectre
```

#### Remove stopped container
This is only possible, if you do _not_ use option _--rm_ on container start. 
After _docker stop_ the container can be removed using

```
docker rm spectre
```

#### Build image yourself
```
docker build -t spectreproject/spectre:latest .
```

#### Using more than one core
If multiple cores available for build, you can pass the amount of cores
to use to the build command:

```
docker build -t spectreproject/spectre:latest --build-arg BUILD_THREADS=12 .
```

Default value is BUILD_THREADS=6
