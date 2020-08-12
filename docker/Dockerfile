FROM ubuntu:18.04 

RUN apt-get update && apt-get install cmake git build-essential libsqlite3-dev sqlite3 libcurl4-openssl-dev libzip-dev automake libtool pkg-config -y

RUN git clone https://gitlab.com/elastos/Elastos.Service.Feeds.git

WORKDIR /Elastos.Service.Feeds/build

RUN cmake -DCMAKE_INSTALL_PREFIX=dist -DCMAKE_BUILD_TYPE=Release .. && make -j && make install

WORKDIR /Elastos.Service.Feeds/build/dist/bin/

RUN cp ../etc/feedsd/feedsd.conf . && sed -i 's/localhost/0.0.0.0/' feedsd.conf

EXPOSE 10080

CMD [ "./feedsd", "-c",  "feeds.conf" ]
