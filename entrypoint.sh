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

add_route() {
    destination=$1
    gateway=$2
    ip route add "$destination" via "$gateway"
}

case "$hostname" in
  r1)
    wait_for_interface eth1
    wait_for_interface eth2
    /opt/ospf/main
    /opt/ospf/interface add eth1
    /opt/ospf/interface add eth2

    add_route 192.168.2.0/24 10.1.0.4     # via r4 (PAS r2)
    add_route 10.2.0.0/24 10.1.0.4
    add_route 10.3.0.0/24 10.1.0.4
    add_route 192.168.3.0/24 10.1.0.4
    ;;

  r2)
    wait_for_interface eth1
    wait_for_interface eth2
    wait_for_interface eth3
    /opt/ospf/main
    /opt/ospf/interface add eth1
    /opt/ospf/interface add eth2
    /opt/ospf/interface add eth3

    # Routes classiques, au cas où on le réactive un jour
    add_route 192.168.1.0/24 10.1.0.1
    add_route 192.168.3.0/24 10.2.0.5
    add_route 10.3.0.0/24 10.2.0.5
    ;;

  r3)
    wait_for_interface eth1
    wait_for_interface eth2
    /opt/ospf/main
    /opt/ospf/interface add eth1
    /opt/ospf/interface add eth2

    add_route 192.168.1.0/24 10.3.0.5
    add_route 192.168.2.0/24 10.3.0.5
    add_route 10.1.0.0/24 10.3.0.5
    add_route 10.2.0.0/24 10.3.0.5
    ;;

  r4)
    wait_for_interface eth0
    wait_for_interface eth1
    /opt/ospf/main
    /opt/ospf/interface add eth0
    /opt/ospf/interface add eth1

    add_route 192.168.1.0/24 10.1.0.1
    add_route 192.168.2.0/24 10.2.0.5   # via r5 (PAS r2)
    add_route 192.168.3.0/24 10.2.0.5
    add_route 10.3.0.0/24 10.2.0.5
    ;;

  r5)
    wait_for_interface eth0
    wait_for_interface eth1
    /opt/ospf/main
    /opt/ospf/interface add eth0
    /opt/ospf/interface add eth1

    add_route 192.168.1.0/24 10.2.0.4  # via r4
    add_route 192.168.2.0/24 10.2.0.4
    add_route 10.1.0.0/24 10.2.0.4
    ;;

  *)
    echo "Hostname $hostname not recognized" >> /opt/ospf/error.log
    ;;
esac

tail -f /dev/null