#! /bin/bash
pkill memcached
rm -f /scratch/peter/memcached_test/result
cat "a\n" > /scratch/peter/memcached_test/result
export LD_LIBRARY_PATH="/scratch/peter/memcached_test/libmemcached/lib"
/scratch/peter/memcached_test/memcached/bin/memcached restart -l 10.0.0.1 -U 11211 -d -m 64 -M
sleep 3
/scratch/peter/memcached_test/data_init 0 1000 eth1 11211 &
/scratch/peter/memcached_test/ping_memcached 8 eth1 100 192.168.3.%d & 
# tail -f result | grep get_ 
