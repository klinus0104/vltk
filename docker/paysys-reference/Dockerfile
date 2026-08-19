FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        cmake \
        freetds-dev \
        g++ \
        make \
    && rm -rf /var/lib/apt/lists/*

COPY sword3paysys_cpp /src/sword3paysys_cpp
RUN cmake -S /src/sword3paysys_cpp -B /build -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    && cmake --build /build --parallel

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        libsybdb5 \
        libstdc++6 \
        python3 \
        socat \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /build/sword3paysys /usr/local/bin/sword3paysys-cpp
COPY docker/paysys-reference/entrypoint.sh /usr/local/bin/jx-paysys-entrypoint
RUN chmod +x /usr/local/bin/jx-paysys-entrypoint /usr/local/bin/sword3paysys-cpp

WORKDIR /work
ENTRYPOINT ["/usr/local/bin/jx-paysys-entrypoint"]
