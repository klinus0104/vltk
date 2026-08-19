FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN dpkg --add-architecture i386 \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        cabextract \
        procps \
        python3 \
        scrot \
        socat \
        tcpdump \
        wine32:i386 \
        wine64 \
        winbind \
        winetricks \
        xdotool \
        xvfb \
    && rm -rf /var/lib/apt/lists/*

COPY docker/wine-relay/entrypoint.sh /usr/local/bin/jx-wine-relay-entrypoint
RUN chmod +x /usr/local/bin/jx-wine-relay-entrypoint

WORKDIR /runtime
ENTRYPOINT ["/usr/local/bin/jx-wine-relay-entrypoint"]
