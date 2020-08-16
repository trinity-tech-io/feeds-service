Elastos.Net.Feeds.Node
=====================
## 1. Introduction

Feeds dApp is an update-to-date social dApp soultion with characteristics of access with DIDs, dentralized servers,  you-own-your-data and more. Where feeds service/node is backend service for front feeds dApp on mobile devices. 
Anyone who is inetresting to feeds want to share his great ideas with his friends or even with public,  he is recommended to setup his  own feeds service/node and "speak" on your own feeds with comfortable name. And remember, you will own your all datum from your feeds.

## 3. Build from source

We assumed that you already installed feeds dApp on your mobile device, and want built feeds service to experience the whole dApp in the next.

#### Prerequisites

Feeds service/node is currently a CMake-based project, and deeply depending on the following projects:

- [Elastos.NET.Carrier.Native.SDK](https://github.com/elastos/Elastos.NET.Carrier.Native.SDK)
- [Elastos.DID.Native.SDK](https://github.com/elastos/Elastos.DID.Native.SDK)

Please carefully read build instructions on README.md of repositories above and prepare all build tools.

### Build from source

With all prerequisites installed, then clone source code onto local device and start build process with following commands:

```
$ git clone https://gitlab.com/elastos/Elastos.Service.Feeds
$ cd Elastos.Service.Feeds
$ mkdir build
$ cmake -DCMAKE_INSTALL_PREFIX=YOUR-PATH ../..
$ make -j4
$ make install
```

### Run in developer mode

As finished building source, you can directly run feeds service in handy way:

```
$ cd YOUR-PATH/bin
$ ./feedsd
```
Defaultly, all data generated from running would be cached in your $HOME/.feeds unless you update the configuration file with your preference.

### Paring/Binding service

As feeds service started, open browser with address **http://localhost:10080**,  start to conduct the binding/paring procedure with feeds dApp on mobile device.

## 4. Run from Docker
- Build docker image[Optional]
```
docker build -t elastos/feeds-node PROJECT_ROOT
```
- Run docker container
```
docker run --name feeds-node -p 10080:10080 elastos/feeds-node:latest
```

## 5. Acknowledgments

A sincere thank to the all team and projects we are relying on.

## 6. License

This project is licensed under the term of **GPLv3**

