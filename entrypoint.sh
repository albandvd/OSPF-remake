#!/bin/sh

echo 1 > /proc/sys/net/ipv4/ip_forward

hostname=$(hostname)

wait_for_interface() {
    iface=$1
    while [ ! -d "/sys/class/net/$iface" ]; do
        echo "En attente de l'interface $iface..."
        sleep 1
    done
}

case "$hostname" in
  r1)
    wait_for_interface eth1
    /opt/ospf/main
    /opt/ospf/interface add eth1
    ;;
  r2)
    wait_for_interface eth1
    wait_for_interface eth2
    /opt/ospf/main
    /opt/ospf/interface add eth1
    /opt/ospf/interface add eth2
    ;;
  r3)
    wait_for_interface eth1
    /opt/ospf/main
    /opt/ospf/interface add eth1
    ;;
  r4)
    wait_for_interface eth0
    wait_for_interface eth1
    /opt/ospf/main
    /opt/ospf/interface add eth0
    /opt/ospf/interface add eth1
    ;;
  r5)
    wait_for_interface eth0
    wait_for_interface eth1
    /opt/ospf/main
    /opt/ospf/interface add eth0
    /opt/ospf/interface add eth1
    ;;
  *)
    echo "Hostname $hostname not recognized" >> /opt/ospf/error.log
    ;;
esac

tail -f /dev/null
