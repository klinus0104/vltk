# Protocol capture evidence

All timestamps are UTC so packet captures and logs can be correlated directly.

- `pcap/gateway_*.pcap`: raw traffic on TCP ports 5001, 5002, 5003, 5622, 5623, 5632, and the internal RootRelay port 7777; files rotate hourly.
- `pcap/s3relay_*.pcap`: raw S3Relay traffic on TCP port 5003; files rotate hourly.
- `logs/paysys_decrypted_YYYYMMDD.log`: reference PaySys output, including decrypted packet bodies.
- `logs/s3relay_decrypted_YYYYMMDD.log`: reference S3Relay output, including decrypted packet bodies.
- `logs/gateway_YYYYMMDD.log`: Gateway entrypoint plus Goddess/Bishop stdout/stderr.
- `../gateway-6.0/Logs/`: native Goddess/Bishop application logs.
- `timeline.csv`: manually recorded client actions.

Record each test action immediately before performing it:

```bash
./scripts/log-test-event.sh "open client"
./scripts/log-test-event.sh "submit login: test account"
./scripts/log-test-event.sh "select server"
./scripts/log-test-event.sh "enter game"
./scripts/log-test-event.sh "logout"
./scripts/log-test-event.sh "disconnect client"
```

Do not put passwords in event descriptions.
