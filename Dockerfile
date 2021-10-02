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
WORKDIR /feedsd/build/dist/etc/feedsd
RUN sed -i 's/localhost/0.0.0.0/' feedsd.conf

FROM ubuntu:18.04 AS production
WORKDIR /feedsd
COPY --from=builder /feedsd/build/dist ./
WORKDIR /feedsd/bin
EXPOSE 10018
VOLUME /var/lib/feedsd
ENTRYPOINT ["./feedsd"]
