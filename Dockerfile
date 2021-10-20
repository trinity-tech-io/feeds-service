FROM ubuntu:18.04 AS builder
RUN apt-get update -y && apt-get install cmake git build-essential automake libtool pkg-config -y
WORKDIR /feedsd
COPY cmake cmake/
COPY debian debian/
COPY deps deps/
COPY scripts scripts/
COPY src src/
COPY CMakeLists.txt ./
WORKDIR /feedsd/build
RUN cmake -DCMAKE_INSTALL_PREFIX=dist -DCMAKE_BUILD_TYPE=Release .. && make -j1 && make install

FROM ubuntu:18.04 AS production
RUN apt-get update -y && \
    apt-get install -y --no-install-recommends ca-certificates && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*
WORKDIR /feedsd/bin
COPY --from=builder /feedsd/build/dist/bin/feedsd .
WORKDIR /feedsd/etc
COPY --from=builder /feedsd/build/dist/etc/feedsd/feedsd.conf .
RUN sed -i \
        -e '/^log-level/ s/=.*/= 5/' \
        -e '/^log-file/  s/=.*/= "\/feedsd\/var\/log\/feedsd.log"/' \
        -e '/^data-dir/  s/=.*/= "\/feedsd\/var\/lib"/' \
        feedsd.conf
WORKDIR /feedsd/var/lib
WORKDIR /feedsd/var/log
RUN chown -Rv 1000:1000 /feedsd
USER 1000:1000
WORKDIR /feedsd/bin
EXPOSE 10018
VOLUME /feedsd/var
ENTRYPOINT ["./feedsd"]
