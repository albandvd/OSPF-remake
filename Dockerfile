# ---- builder: compile the daemon against jansson-dev, nothing else ships ----
FROM alpine:3.20 AS builder

RUN apk add --no-cache gcc make build-base jansson-dev

COPY src /opt/ospf
WORKDIR /opt/ospf
RUN make

# ---- runtime: just the binaries, the runtime jansson lib, and ip tooling ----
FROM alpine:3.20

RUN apk add --no-cache jansson iproute2 iputils dos2unix

COPY --from=builder /opt/ospf/main /opt/ospf/main
COPY --from=builder /opt/ospf/interface /opt/ospf/interface
COPY --from=builder /opt/ospf/show /opt/ospf/show
COPY entrypoint.sh /opt/entrypoint.sh
RUN dos2unix /opt/entrypoint.sh && chmod +x /opt/entrypoint.sh

WORKDIR /opt/ospf
ENTRYPOINT ["/opt/entrypoint.sh"]
