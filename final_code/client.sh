#! /bin/bash
export LD_LIBRARY_PATH="/scratch/peter/memcached_test/libmemcached/lib"
pkill memcached
rm -f /scratch/peter/memcached_test/latency
/scratch/peter/memcached_test/auto_memcached 2 10000 0 1 2 11211 &
