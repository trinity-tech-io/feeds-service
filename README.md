Feeds Service
=====================
## 1. Introduction

Feeds dApp is an update-to-date social application solution with characteristics of access with DIDs, decentralized servers,  you-own-your-data etc, where feeds service/node is running background as backend service interacting with front feeds dApp.

Any who is interested in feeds can run his own Feeds service and post your new thoughts via Feeds to public. Believe me, that's worth of trying it and you will be guaranteed with the ownership of your data over your experience.

## 2. Build from source

Assumed that you already installed feeds dApp on your mobile device, and want built feeds service to experience the whole dApp in the next.

### Build from source

Just clone source code onto local device and start build process with following commands:

```
$ git clone git@github.com:elastos-trinity/feeds-service.git
$ cd feeds-service
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
By default, all data generated from running would be cached in your $HOME/.feeds unless you update the configuration file with your preference.

### Paring/Binding service

As feeds service started, open browser with address **http://localhost:10080**,  start to conduct the binding/paring procedure with feeds dApp on mobile device.

## 3. Run from Docker
- Build docker image[Optional]
```
docker build -t elastos/feeds-node PROJECT_ROOT
```
- Run docker container
```
docker run --name feeds-node -p 10080:10080 elastos/feeds-node:latest
```

## 4. Installation from packages

Noted that a series of released versions managed at [here](https://github.com/elastos-trinity/feeds-service/releases) , and choose the right installation package according to your system. Please be sure that you are using the latest version.

## 5. Acknowledgments

A sincere thank to the all team and projects we are relying on.

## 6. License

This project is licensed under the term of **GPLv3**

