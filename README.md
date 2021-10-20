Feeds Service
=====================
|Linux && Mac && Windows|
|:-:|
|[![Build Status](https://github.com/elastos-trinity/feeds-service/workflows/CI/badge.svg)](https://github.com/elastos-trinity/feeds-service/actions)|

## 1. Introduction

Feeds dApp is an up-to-date social application solution that features DIDs, decentralized servers, own-your-data, etc. The Feeds service is running as a back-end service interacting with the front-end Feeds dApp.

Anyone interested in Feeds can run a Feeds service and post new thoughts via Feeds to the public. That's worth trying, and you will be guaranteed the ownership of your data over your experience.

## 2. Build from source

We assumed that you already installed Feeds dApp on your mobile device and want built Feeds service to experience the whole dApp in the next.

### Build from source

Install build prerequisites:

```
$ sudo apt-get install -y automake build-essential cmake git libtool pkg-config
```

Just clone source code onto a local device and start the build process with the following commands:

```
$ git clone https://github.com/elastos-trinity/feeds-service.git
$ cd feeds-service
$ mkdir build
$ cd build
$ cmake -DCMAKE_INSTALL_PREFIX=YOUR-PATH ..
$ make
$ make install
```

### Run in developer mode

As a finished building source, you can directly run the Feeds service in a handy way:

```
$ cd YOUR-PATH/bin
$ ./feedsd
```
By default, all running data will be cached in your $HOME/.feeds unless you update the configuration file with your preference.

### Paring/Binding service

As Feeds service started, open browser with address **http://localhost:10018**,  begin to conduct the binding/pairing procedure with Feeds dApp on a mobile device.

## 3. Run from Docker
- Build docker image[Optional]
```
docker build -t elastos/feeds-node PROJECT_ROOT
```
- Run docker container
```
docker run --name feeds-node -p 10018:10018 -v $(pwd)/data:/feedsd/var elastos/feeds-node:latest
```

## 4. Installation from packages

Noted that a series of released versions are managed [here](https://github.com/elastos-trinity/feeds-service/releases), and choose the right installation package according to your system. Please be sure that you are using the latest version.

## 5. Acknowledgments

A sincere thank you to all the team and projects we are relying on.

## 6. License

This project is licensed under the term **GPLv3**.

