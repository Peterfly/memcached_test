#! /bin/bash
touch /etc/passwd
/bin/busybox adduser -D -H peter
/bin/memcached -l 10.0.0.1 -p 11211 -d -m 64 -M -u peter -t 4
sleep 3
/bin/data_init 0 10000 eth0 11211 &
/bin/ping_memcached 8 eth1 100 192.168.3.%d & 
