#!/bin/sh

hostname=$(hostname)

if [ "$hostname" = "r1" ]; then
    /opt/ospf/main
    /opt/ospf/interface add eth0
    /opt/ospf/interface add eth1

    tail -f /dev/null

elif [ "$hostname" = "r2" ]; then
    /opt/ospf/main
    /opt/ospf/interface add eth0
    /opt/ospf/interface add eth1
    /opt/ospf/interface add eth2

    tail -f /dev/null

elif [ "$hostname" = "r3" ]; then
    /opt/ospf/main
    /opt/ospf/interface add eth0
    /opt/ospf/interface add eth1

    tail -f /dev/null

elif [ "$hostname" = "r4" ]; then
    /opt/ospf/main
    /opt/ospf/interface add eth0
    /opt/ospf/interface add eth1

    tail -f /dev/null

elif [ "$hostname" = "r5" ]; then
    /opt/ospf/main
    /opt/ospf/interface add eth0
    /opt/ospf/interface add eth1

    tail -f /dev/null

else
    echo "Hostname $hostname not recognized" >> /opt/ospf/error.log
fi