#!/bin/sh

hostname=$(hostname)

# Docker Compose already sets ip_forward/rp_filter declaratively via the
# `sysctls:` block in docker-compose.yml — doing it again here only produced
# confusing "Read-only file system" errors in some container runtimes
# without adding anything.

# Docker injects a default route via the bridge gateway (x.x.x.254) on every
# network. Routers must not keep it: if they did, traffic could silently
# leave through the bridge instead of taking the path OSPF computed, which
# would defeat the whole point of the demo. Terminals keep their default
# route (set explicitly in docker-compose.yml) since they are not routers.
ip route del default 2>/dev/null || true

# Print the interface name whose address falls in the given /prefix
iface_for_prefix() {
    prefix=$1
    ip -o addr show 2>/dev/null \
        | awk "/inet $prefix/ {print \$2}" \
        | cut -d'@' -f1 \
        | head -1
}

# Start the OSPF daemon in the background and give it a moment to create
# lsdb.json before `interface add` tries to read it.
/opt/ospf/main &
sleep 2

case "$hostname" in
  r1)
    # N_A1 (192.168.1.x) and N_C1 (10.1.x.x)
    /opt/ospf/interface add "$(iface_for_prefix '10.1.')"
    ;;

  r2)
    # N_C1 (10.1.x.x), N_C2 (10.2.x.x), N_A2 (192.168.2.x)
    /opt/ospf/interface add "$(iface_for_prefix '10.1.')"
    /opt/ospf/interface add "$(iface_for_prefix '10.2.')"
    ;;

  r3)
    # N_C3 (10.3.x.x), N_A3 (192.168.3.x)
    /opt/ospf/interface add "$(iface_for_prefix '10.3.')"
    ;;

  r4)
    # N_C1 (10.1.x.x), N_C2 (10.2.x.x)
    /opt/ospf/interface add "$(iface_for_prefix '10.1.')"
    /opt/ospf/interface add "$(iface_for_prefix '10.2.')"
    ;;

  r5)
    # N_C2 (10.2.x.x), N_C3 (10.3.x.x)
    /opt/ospf/interface add "$(iface_for_prefix '10.2.')"
    /opt/ospf/interface add "$(iface_for_prefix '10.3.')"
    ;;

  *)
    echo "Unrecognized hostname '$hostname'" >> /opt/ospf/error.log
    ;;
esac

tail -f /dev/null
