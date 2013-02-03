#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libmemcached/memcached.h>
#include <sys/time.h>

#define HIT_RATE 81
#define MISS_PENALTY 2

void check_sum(char value[], int len, FILE *file, int core_id) {
    if 
    for (int i = 0; i < len; i++) {
        char correct = ((i + core_id) * len) % 26 + 65;
        if (correct != value[i]) {
            fprintf(file, "data corruption! %s", value);
            break;
        }
    }
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

    char base_addr[12] = "10.0.0.%d";  
    /* By default, port starts from 11211. */
    int port = 11211;
    if (argc < 3) {
        printf("Usage: %s [number of servers] [times for random access-1 for infinite] " 
            + "[is checking sum? 1 for yes] [base addr] [miss latency] [port]", argv[0]);
        exit(0);
    }

    int num_servers = atoi(argv[1]);
    int num_test = atoi(argv[2]);
    bool is_check = (atoi(argv[3]) == 1);
    if (argc > 4) {
        strcpy(base_addr, argv[4]);
    }

    int miss_penalty = 2;
    if (argc > 5) {
        miss_penalty = atoi(argv[5])
    }

    if (argc > 6) {
        port = atoi(argv[6]);
    }

    for (int i = 0; i < num_servers; i++) {
        char addr[12];
        sprintf(addr, base_addr, i+1);
        servers = memcached_server_list_append(servers, addr, port + i, &rc);
    }
    /* Update the memcached structure with the updated server list */
    rc = memcached_server_push(memc, servers);
    if (rc == MEMCACHED_SUCCESS)
        fprintf(stderr,"Successfully added server\n");
    else
        fprintf(stderr,"Couldn't add server: %s\n",memcached_strerror(memc, rc));

    char temp[252] = {};
    FILE *myfile = fopen("key", "a+");
    FILE *timeinter = fopen("inter", "a+");
    FILE *latency = fopen("latency", "a+");
    rewind(latency);
    rewind(myfile);
    rewind(timeinter);
    int interval;
    int count = 0;
    unsigned long before;
    unsigned long after, max, min, avg, t;
    struct timeval tim;
    int key_len;

    int *key_size = (int *) malloc(num_test * sizeof(int));
    if (key_size == NULL) {
        printf("malloc failed\n");
        return;
    }

    while (true) {
        temp = "done";
        char *value = (char *) malloc(5001);
        retvalue = memcached_get(memc, temp, strlen(temp), &retlength, &flags, &rc);
        if (strcmp(temp, retvalue) == 0) {
            printf("Start sampling.\n");
            break;
        } else {
            printf("Waiting for initializer to complete.\n");
            sleep(2);
        }
    }

    int i = 0;
    while (i < num_test) {
        if (fscanf(myfile, "%d\n", &key_len) != EOF) {
            key_size[i] = key_len;
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
                gettimeofday(&tim, NULL);
                before = tim.tv_usec;
                rc = memcached_delete_by_key(memc, temp, strlen(temp), expire);
                gettimeofday(&tim, NULL);
                after = tim.tv_usec;
            } else {
                gen_key(temp, key_size[count], rand_core);
                gettimeofday(&tim, NULL);
                before = tim.tv_usec;
                retvalue = memcached_get(memc, temp, strlen(temp), &retlength, &flags, &rc);
                gettimeofday(&tim, NULL);
                after = tim.tv_usec;
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
            t = after - before;
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
            sleep(MISS_PENALTY); 
        }
        fscanf(timeinter, "%d\n", &interval);
        usleep(interval);
        count++;
        if (count > num_test) {
          break;
        }
    }
    fprintf(latency, "max: %ld min: %ld avg: %ld\n", max, min, avg/count);
    free(temp);
    fclose(timeinter);
    fclose(myfile);
    fclose(latency);
}
