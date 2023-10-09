FROM node:20-alpine3.17 AS builder
RUN apk add --no-cache gcc make libtool bash git
COPY . ./arcus-memcached
WORKDIR ./arcus-memcached
RUN ./deps/install.sh /arcus
RUN ./config/autorun.sh
RUN ./configure \
    --prefix=/arcus \
    --enable-zk-integration \
    --with-zk-reconfig
RUN make && make install

FROM node:20-alpine3.17 AS runner
COPY --from=builder /arcus /arcus
ENV PATH ${PATH}:/arcus/bin
ENTRYPOINT ["memcached", \
            "-E", "/arcus/lib/default_engine.so", \
            "-X", "/arcus/lib/ascii_scrub.so", \
            "-u", "root"]
CMD ["-v", \
     "-p", "11211"]