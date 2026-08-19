FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        cmake \
        freetds-dev \
        g++ \
        make \
    && rm -rf /var/lib/apt/lists/*

COPY s3relayserver_cpp /src/s3relayserver_cpp
RUN cmake -S /src/s3relayserver_cpp -B /build -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    && cmake --build /build --parallel

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        libsybdb5 \
        libstdc++6 \
        python3 \
        socat \
        tcpdump \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /build/s3relayserver /usr/local/bin/s3relayserver-cpp
COPY docker/s3relay-reference/entrypoint.sh /usr/local/bin/jx-s3relay-entrypoint
RUN chmod +x /usr/local/bin/jx-s3relay-entrypoint /usr/local/bin/s3relayserver-cpp

WORKDIR /work
ENTRYPOINT ["/usr/local/bin/jx-s3relay-entrypoint"]
