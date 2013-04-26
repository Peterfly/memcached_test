#! /bin/bash
cd /bin
/bin/busybox usleep 20000
/bin/auto_memcached 2 10000 0 1 2 11211 &
