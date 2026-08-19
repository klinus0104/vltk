FROM --platform=linux/386 i386/ubuntu:18.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        gdb \
        iproute2 \
        libc6 \
        libgcc1 \
        libstdc++6 \
        libuuid1 \
        netcat-openbsd \
        procps \
        python3-minimal \
        socat \
        tcpdump \
        zlib1g \
    && rm -rf /var/lib/apt/lists/*

COPY patch_elf_bss.py /usr/local/bin/patch-elf-bss
COPY trace-goddess.gdb /usr/local/share/jx/trace-goddess.gdb
COPY trace-bishop.gdb /usr/local/share/jx/trace-bishop.gdb
COPY trace-gameserver.gdb /usr/local/share/jx/trace-gameserver.gdb
COPY entrypoint.sh /usr/local/bin/jx-gateway-entrypoint
RUN chmod +x /usr/local/bin/jx-gateway-entrypoint /usr/local/bin/patch-elf-bss

WORKDIR /home/jxser/gateway
ENTRYPOINT ["/usr/local/bin/jx-gateway-entrypoint"]
