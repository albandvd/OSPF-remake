# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

OSPF-remake is a lightweight dynamic routing daemon written in C for Linux, implementing a simplified alternative to OSPF (Open Shortest Path First). It uses hop count as its path metric, distributes topology via LSA (Link State Advertisements) over TCP, and programs the Linux kernel routing table directly. The target environment is Alpine Linux router containers.

## Build & Run

### Production (5 routers + 3 terminals)
```bash
docker-compose up -d --build       # Start full network topology
docker-compose down                 # Tear down
docker exec -it r1 sh              # Connect to a router
```

### Inside a router container
```bash
/opt/ospf/show neighbors           # Display known OSPF neighbors
ip route show                      # Inspect kernel routing table
cat /opt/ospf/lsdb.json            # Inspect LSDB state
```

### Development container
```bash
cd dev
docker build -t dev-ospf ./
docker run --name dev --rm -it --cap-add=NET_ADMIN dev-ospf
```

### Manual compile (inside container or with jansson installed)
```bash
cd src
make                               # Builds: main, interface, show
make clean
```

Compiler flags: `gcc -Wall -Wextra -g`, linked with `-ljansson`.

## Architecture

### Components

**`src/main.c`** — Entry point. Calls `lsdb_init()` to bootstrap the LSDB (creates `lsdb.json` from hostname), then triggers neighbor discovery and route computation.

**`src/lsdb.c/h`** — Core Link State Database. Holds the network topology as a JSON structure (`lsdb.json`). Responsible for merging received LSAs, computing shortest paths (hop count), and triggering route updates. This is the most critical file.

**`src/exchanges.c/h`** — TCP networking layer. Serializes/deserializes the LSDB as JSON and exchanges it with neighbors on **port 4242**. Implements both client (send) and server (receive) sides.

**`src/interface.c`** — Interface discovery using `getifaddrs()`. Discovers active interfaces, derives subnet ranges, and scans each subnet for OSPF-capable neighbors by attempting TCP connections.

**`src/route.c/h`** — Linux kernel routing table manipulation via `ioctl(SIOCADDRT/SIOCDELRT)` with `struct rtentry`. Adds/removes routes as shortest paths are computed.

**`src/show.c`** — CLI utility to print neighbor and routing state from `lsdb.json`.

**`src/return.c/h`** — Enum-based return codes (`ReturnCode`) used throughout. Every function returns a `ReturnCode`; never use raw errno directly.

**`src/check-service.c/h`** — Health/service verification.

### Data Flow

```
Startup:
  main.c → lsdb_init()       (create lsdb.json with hostname)
         → interface.c        (enumerate interfaces, derive subnets)
         → exchanges.c        (scan each subnet, TCP connect to neighbors)
         → lsdb.c             (merge received LSAs, compute shortest paths)
         → route.c            (program kernel routing table)
         → exchanges.c        (listen server loop for incoming LSAs)

Topology change:
  Neighbor sends updated LSA → lsdb.c merges → recompute paths → route.c reprograms kernel
```

### LSDB JSON format (`/opt/ospf/lsdb.json`)

```json
{
  "name": "r1",
  "sequence": 1,
  "connected": [
    { "network": "10.1.0.0", "mask": "255.255.255.0", "gateway": "eth0", "cost": 1 }
  ],
  "neighbors": [
    { "name": "r2", "network": "10.1.0.0", "mask": "255.255.255.0",
      "gateway": "10.1.0.2", "next_hop": "10.1.0.2", "hop": 1 }
  ]
}
```

### Network Topology (docker-compose.yml)

5 routers (R1–R5) interconnected across 3 core networks (10.1.0.0/24, 10.2.0.0/24, 10.3.0.0/24) and 3 access networks (192.168.1–3.0/24). 3 terminal nodes (T_A1, T_A2, T_A3) attached to access networks. All router containers require `NET_ADMIN` and `SYS_ADMIN` capabilities for kernel routing table access.

Per-router static routes and role configuration live in `entrypoint.sh`, which branches on `$HOSTNAME`.

## Conventions

- **Function naming:** prefixed by module — `lsdb_*`, `route_*`, `exchanges_*`, `check_*`
- **Return codes:** always use `ReturnCode` enum from `return.h`; `RETURN_SUCCESS` on success
- **Constants:** uppercase — `PORT` (4242), `JSON_FILE_NAME`, `MAX_ROUTES`
- **No GUI:** CLI-only, display via `show` binary
- **Linux-only:** uses `ioctl`, `rtentry`, `getifaddrs` — not portable

## Key Constraints (from cahierdescharges.pdf)

Priority 1 (must implement): path computation by hop count, topology update on change, enable/disable per interface, IPv4 routing table update, neighbor list storage and display, failure tolerance.

Priority 2 (optimization): minimize exchanges, memory, and convergence time; default route origination.

Priority 3 (advanced): bandwidth-aware metrics, exchange authentication/encryption.
