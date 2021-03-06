#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libmemcached/memcached.h>
#include <libmemcached-1.0/memcached.h>
#include <libmemcachedutil-1.0/ping.h>
#include <libmemcachedutil-1.0/util.h>
#include <sys/time.h>

#define HIT_RATE 81
#define MISS_PENALTY 2

void check_sum(char value[], int len, FILE *file, int core_id) {
    for (int i = 0; i < len; i++) {
        char correct = ((i + core_id) * len) % 26 + 65;
        if (correct != value[i]) {
            fprintf(file, "data corruption! %s", value);
            break;
        }
    }
}

long difftime(timeval before, timeval after) {
    long diff = (after.tv_usec - before.tv_usec) + (after.tv_sec - before.tv_sec) * 1000000;
    printf("diff before %d, %d after %d %d\n", before.tv_sec, before.tv_usec, after.tv_sec, after.tv_usec);
    return diff;
}

void gen_key(char temp[], int len, int core_id) {
    for (int i = 0; i < len; i++) {
        temp[i] = ((i + core_id) * len) % 26 + 65;
    }
    temp[len] = '\0';
}


bool is_hit() {
  if (rand() % 100 <= HIT_RATE) {
    return true;
  }
  return false;
}

void gen_miss(char key[]) {
  int size = rand() % 240;
  for (int i = 0; i > size; i++) {
    key[i] = rand() % 26 + 97;
  }
}

int main(int argc, char *argv[])
{
    memcached_server_st *servers = NULL;
    memcached_st *memc;
    memc = memcached_create(NULL);
    time_t expire = 0;
    memcached_return rc;  


    char *retvalue = NULL;                                                                                                                
    size_t retlength;
    uint32_t flags;                     

    char ip_addr[12] = "10.0.0.%d";  
    /* By default, port starts from 11211. */
    int port = 11211;
    if (argc < 3) {
        printf("Usage: %s [number of servers] [times for random access-1 for infinite] [is checking sum? 1 for yes] [base addr] [miss latency] [port]", argv[0]);
        exit(0);
    }

    int num_servers = atoi(argv[1]);
    int num_test = atoi(argv[2]);
    bool is_check = (atoi(argv[3]) == 1);
    if (argc > 4) {
        strcpy(ip_addr, argv[4]);
    }

    int miss_penalty = 2;
    if (argc > 5) {
        miss_penalty = atoi(argv[5]);
    }

    if (argc > 6) {
        port = atoi(argv[6]);
    }
    char addr[12];
    for (int i = 0; i < num_servers; i++) {
        sprintf(addr, ip_addr, i+1);
        servers = memcached_server_list_append(servers, addr, port, &rc);
    }
    /* Update the memcached structure with the updated server list */
    fprintf(stderr, "added server: %s\n", addr);
    rc = memcached_server_push(memc, servers);
    if (rc == MEMCACHED_SUCCESS)
        fprintf(stderr,"Successfully added server\n");
    else
        fprintf(stderr,"Couldn't add server: %s\n",memcached_strerror(memc, rc));
    for (int i = 0; i < num_servers; i++) {
        memcached_return_t instance_rc;
        const char *hostname= servers[i].hostname;
        in_port_t port= servers[i].port;
        while (libmemcached_util_ping2(hostname, port, NULL, NULL, &instance_rc) == false);
    }
    char temp[252] = {};
    FILE *myfile = fopen("key", "a+");
    FILE *timeinter = fopen("inter", "a+");
    FILE *latency = fopen("latency", "a+");
    rewind(latency);
    rewind(myfile);
    rewind(timeinter);
    int interval;
    int count = 0;
    
    long diff, max, min, avg, t;
    avg = 0;
    struct timeval tim;
    struct timeval before, after;
    int key_len;
    // printf("size is %d\n", num_test*sizeof(int));
    int *key_size = (int *) malloc(num_test * sizeof(int));
    if (key_size == NULL) {
        printf("malloc failed\n");
        return 1;
    }
    printf("start pinging\n");
    while (true) {
        strcpy(temp, "done");
        char *value = (char *) malloc(5001);
        retvalue = memcached_get(memc, temp, strlen(temp), &retlength, &flags, &rc);
        // printf("getting value\n");
        if (retvalue && (strcmp(temp, retvalue) == 0)) {
            printf("Start sampling.\n");
            break;
        } else {
            printf("Waiting for initializer to complete.\n");
            // sleep(2);
        }
    }

    int i = 0;
//    rewind(myfile);
    while (i < num_test) {
        if (fscanf(myfile, "%d\n", &key_len) != EOF) {
            key_size[i] = key_len;
        }
        // printf("%d\n", key_size[i]);
        if (key_len > 250) {
            printf("%d %d\n", i, key_len);
            return 1;
        }       
        i++;
        if (i > num_test) {
            break;
        }
    }
    num_test = i;
    while (true) {
        if (is_hit()) {
            int rand_core = rand() % num_servers;
            if (count % 3 == 0) {
                gen_key(temp, key_size[count], rand_core);
                gettimeofday(&before, NULL);
                rc = memcached_delete(memc, temp, strlen(temp), expire);
                gettimeofday(&after, NULL);
            } else {
                // printf("key size %d %d\n", key_size[count], count);
                gen_key(temp, key_size[count], rand_core);
                gettimeofday(&before, NULL);
                retvalue = memcached_get(memc, temp, strlen(temp), &retlength, &flags, &rc);
                gettimeofday(&after, NULL);
                // printf("got key %s\n", temp);
                // fprintf(latency, "read %d: %ld us\n", count, after - before);
                if (retvalue == NULL) {
                    printf("null return value for %s\n", temp);
                }
                if (is_check && count % 10 == 0 && retvalue != NULL) {
                    printf("checking data corruption\n");
                    check_sum(retvalue, retlength, latency, rand_core);
                }
                fflush(latency);
            }
            t = difftime(before, after);
            printf("diff time %d \n", t);
            if (count == 0) {
                min = t;
                max = t;
                avg = t;
            }
            if (t < min) {
                min = t;
            }
            if (t > max) {
                max = t;
            }
            avg += t;
        } else {
            gen_miss(temp);
            retvalue = memcached_get(memc, temp, strlen(temp), &retlength, &flags, &rc);
            // sleep(miss_penalty); 
        }
        fscanf(timeinter, "%d\n", &interval);
        // usleep(interval);
        count++;
        if (count >= num_test) {
          break;
        }
    }
    fprintf(latency, "max: %d min: %d avg: %f, count %d\n", max, min, (float)avg/count, count);
    fclose(timeinter);
    fclose(myfile);
    fclose(latency);
}
