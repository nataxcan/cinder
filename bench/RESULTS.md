# Cinder vs Pumpkin — same-machine bench

- CPU: AMD Ryzen 7 5700G with Radeon Graphics
- Cores: 16
- RAM: 47.0 GiB
- GPU: GPU 0: NVIDIA GeForce RTX 3090 (UUID: GPU-554b4296-7ed0-32be-9d79-919d56d67fe4)
- Status ping: handshake + status request, wall time on the client
- Ready: spawn → first successful status ping

| Server | Binary | Ready | Idle RSS | Ping p50 | Ping p99 | 200 conc. QPS |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| cinder | 1.12 MiB | 43.77 ms (best 35.48) | 7.9 MiB | 0.148 ms | 0.225 ms | 1938 |

Notes:

- Cinder is a status/login/config server plus a vanilla-shaped Full-chunk overworld generator; it is not a full vanilla sim (no entities, no chunk streaming to clients yet).
- The generator runs at startup: with the default 1024-chunk grid the world is built before the listener answers, so "Ready" here is the empty-grid configuration. `bench/WORLD.md` has the worldgen and world-ready numbers.
- Pumpkin is a full (early) vanilla-compatible server, so idle RSS includes world/runtime machinery Cinder does not have yet.
- Startup is measured from process spawn to the first successful Minecraft status ping, not bind().
