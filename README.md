# How to build the Feeds service

## 1. Download service code

```
$ git clone https://gitlab.com/elastos/Elastos.Service.Feeds/
```

## 2. Create a directory to store compiled files

For example：

```
$ cd /Users/XXX/Workspace/
$ mkdir FeedsService
```

## 3. Start to compile

```
$ cd /Users/XXX/Workspace/Elastos.Service.Feeds/build
$ cmake -DCMAKE_INSTALL_PREFIX=/Users/XXX/Workspace/FeedsService ..
$ make -j4  # Overcoming the wall for the first compilation
$ make install
```

## 4. Start the Feeds service

Excuting the command to start a Feeds Service：

```
$ cd /Users/XXX/Workspace/FeedsService/bin
$ ./feedsd -c ../etc/feedsd/feedsd.conf
```

Carrier address will be generated after executing the command. Open in the browser:

http://localhost:10080/

The configuration here is localhost, which can be modified in the configuration file "feeds.conf".


# Build and Run using docker


```
docker build -t feeds .

docker run -d -p 10080:10080 feeds
//you can change the first 10080 to what port you want to expose
```
