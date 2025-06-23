#!/bin/sh

hostname=$(hostname)

if [ "$hostname" = "r1" ]; then
    echo "Je suis r1"
    /opt/ospf/main 1
    tail -f /dev/null
elif [ "$hostname" = "r2" ]; then
    echo "Je suis r2"
    /opt/ospf/main 2
    tail -f /dev/null
elif [ "$hostname" = "r3" ]; then
    echo "Je suis r3"
    /opt/ospf/main 3
    tail -f /dev/null
elif [ "$hostname" = "r4" ]; then
    echo "Je suis r4"
    /opt/ospf/main 4
    tail -f /dev/null
elif [ "$hostname" = "r5" ]; then
    echo "Je suis r5"
    #/opt/ospf/main 5
    tail -f /dev/null
else
    echo "Hostname $hostname not recognized" >> /opt/ospf/error.log
fi