#!/bin/sh

# Enable IP forwarding and disable rp_filter (routers need asymmetric forwarding)
echo 1 > /proc/sys/net/ipv4/ip_forward 2>/dev/null || true
echo 0 > /proc/sys/net/ipv4/conf/all/rp_filter 2>/dev/null || true
echo 0 > /proc/sys/net/ipv4/conf/default/rp_filter 2>/dev/null || true

hostname=$(hostname)

# Return the interface name whose address matches the given IP prefix
iface_for_prefix() {
    prefix=$1
    ip -o addr show 2>/dev/null \
        | awk "/inet $prefix/ {print \$2}" \
        | cut -d'@' -f1 \
        | head -1
}

# Start the OSPF daemon in background and wait for lsdb.json to appear
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
    echo "Hostname '$hostname' non reconnu" >> /opt/ospf/error.log
    ;;
esac

tail -f /dev/null
