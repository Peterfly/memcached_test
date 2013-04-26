#! /bin/bash
cd /bin
/bin/busybox adduser -D -H ramp
/bin/memcached -l 10.0.0.1 -U 11211 -d -m 64 -M -u ramp -t 8
/bin/data_init 0 10000 eth0 11211 10
/bin/ping_memcached 8 eth0 100 192.168.3.%d 