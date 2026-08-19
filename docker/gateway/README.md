# Gateway Docker Runner

Runs the 32-bit Linux gateway binaries under a `linux/386` container:

- `goddess_y` listens on `5001`.
- `bishop_y` listens on `5622`, `5623`, and `5632` according to `gateway/bishop.cfg`.
- Container `127.0.0.1:5002` is forwarded to host `host.docker.internal:5002`, so the existing `AccSvrIP = 127.0.0.1` can stay unchanged while PaySys C++ runs on the macOS host.
- On Docker Desktop/macOS Apple Silicon, qemu-i386 rejects these old ELF files with `PT_LOAD with non-writable bss`. A temporary-copy patcher exists behind `PATCH_ELF_BSS=1`, but these binaries then fail their own integrity check. In practice, run this compose on real x86_64 Linux, not Apple Silicon emulation.

Before starting, stop any local probe using `5622` or `5623`.

```bash
docker compose -f docker-compose.gateway.yml up --build
```

Useful checks:

```bash
docker logs -f jx_gateway
docker exec -it jx_gateway ss -ltnp
```
