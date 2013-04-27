#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libmemcached/memcached.h>
#include <libmemcached-1.0/memcached.h>
#include <libmemcachedutil-1.0/ping.h>
#include <libmemcachedutil-1.0/util.h>
#include <sys/time.h>
#include <pthread.h>
#include <ctype.h>
#define HIT_RATE 81
#define MISS_PENALTY 2
#define MAXTHREADS 100 
#define NUM_STARS 10
#define MAX_PING 5000
#define MAX_SERVERS 100

#define MAX_RETRY 10

struct info{
  long total;
  int count;
};

int miss_penalty = 2;
int *key_size;
int *inter_gap;
int num_test;
memcached_st *memc[MAXTHREADS][MAX_SERVERS];
long num_of_threads = 4;
long running_servers = 0;
int num_servers;
char results[MAXTHREADS][100];
long maxes[MAXTHREADS] = {0};
long mines[MAXTHREADS] = {0};
info* sum = new info[MAXTHREADS];
int id_map[MAX_SERVERS] = {-1};
float avges[MAXTHREADS];
bool is_check;
bool is_infi;
FILE *latency;
uint32_t flags;                     
int beat_interval = 2;
memcached_server_st *servers[MAX_SERVERS];
char ip_addr[12] = "10.0.0.%d";

// Fault tolerance
bool sleeping_servers[MAXTHREADS] = {true};
int waken_counter = 0;
int waken_servers[MAXTHREADS] = {-1};
pthread_mutex_t server_mutexes[MAXTHREADS];

char server_addr[MAXTHREADS][50];

long difftime(timeval before, timeval after) {
    long diff = (after.tv_usec - before.tv_usec) + (after.tv_sec - before.tv_sec) * 1000000;
    return diff;
}



bool is_hit() {
  if (rand() % 100 <= HIT_RATE) {
    return true;
  }
  // printf("is hit is false!");
  return false;
}

void check_sum(char value[], int len, FILE *file, int core_id) {
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

void gen_key_del(char val[], int length, int core_id) {
    // length = length % 1000;
    for (int i = 0; i < length; i++) {
        val[i] = ((i + core_id) * length + 1) % 26 + 65;
    }
    val[0] = 'A';
    val[1] = 'A';
    val[length] = '\0';
}

void gen_miss(char key[]) {
  int size = rand() % 240;
  for (int i = 0; i > size; i++) {
    key[i] = rand() % 26 + 97;
  }
}

void quicksort(double arr[], int beg, int end) {
      int i = beg, j = end;
      double pivot = arr[(beg + end)/2];

      while (i <= j) {
          while (pivot < arr[j]) j--;
          while (pivot > arr[i]) i++;
          if (i <= j) {
              double temp = arr[i];
              arr[i] = arr[j];
              arr[j] = temp;
              i++;
              j--;
          }
      }
      if (beg < j) {
          quicksort(arr, beg, j);
      }
      if (end > i) {
          quicksort(arr, i, end);
      }
}


void plot(int arr[]) {
    int increment = (arr[num_test - 1] - arr[0])/NUM_STARS;
    int cur = arr[0];
    int stars = 0;
    printf("(%.2f) - %.2f: ", cur, cur + increment);
    for (int i = 0; i < num_test; i++) {
        if (arr[i] <= cur + increment) {
            stars++;
        } else {
            for (int j = 0; j < stars; j++) {
                printf("*");
            }
            printf("\n");
            cur += increment;
            stars = 0;
            printf("(%.2f) - %.2f: ", cur, cur + increment);
        }
        for (int j = 0; j < stars; j++) {
            printf("*");
        }
        printf("\n");
   }
}

void *heart_beat(void *arg) {
    long times = 0;
    FILE *output = (FILE *) arg;
    while (true) {
        for (int i = 0; i < num_of_threads; i++) {
          pthread_mutex_lock(&server_mutexes[i]);
          fprintf(output, "Thread %d, %d, %d\n", i, sum[i].total, sum[i].count);
          fprintf(stderr, "Console: Thread %d, %d, %d\n", i, sum[i].total, sum[i].count);
          pthread_mutex_unlock(&server_mutexes[i]);
        }
        times ++;
        fflush(output);
        usleep(500000);
    }
    pthread_exit(NULL);
}

void *check_server(void *arg) {
    char temp[252] = {};
    char *retvalue = NULL;
    size_t retlength;
    memcached_return rc; 
    int count = 0;
    while (true) {
        for (int i = 0; i < num_servers; i++) {
            if (!sleeping_servers[i]) {
                continue;
            }
            memcached_return_t instance_rc;
            const char *hostname= servers[i][0].hostname;
            in_port_t port= servers[i][0].port;
            if (libmemcached_util_ping2(hostname, port, NULL, NULL, &instance_rc)) {
                while (true) {
                    strcpy(temp, "done");
                    char *value = (char *) malloc(5001);
                    retvalue = memcached_get(memc[0][i], temp, strlen(temp), &retlength, &flags, &rc);
                    // printf("getting value\n");
                    if (rc != MEMCACHED_SUCCESS)
                        fprintf(stderr, "Couldn't store key: %s\n", memcached_strerror(memc[0][i], rc));
         
                    if (retvalue && (strcmp(temp, retvalue) == 0)) {
                        // printf("Start sampling.\n");
                        sleeping_servers[i] = false;
                        waken_servers[waken_counter] = i;
                        waken_counter++;
                        break;
                    } else {
                        // printf("Waiting for initializer to complete.\n");
                        count++;

                    }
                    if (count < 1000) {
                        // sleep(2);
                        usleep(200000);
                    } else { 
                        // sleep(4); 
                        usleep(400000);
                    }
                }
            }
        }
    }
    pthread_exit(NULL);
}

int get_id(char *str) {
    char *str2;
    unsigned char value[4] = {0};
    size_t index = 0;

    str2 = str; /* save the pointer */
    while (*str) {
        if (isdigit((unsigned char)*str)) {
            value[index] *= 10;
            value[index] += *str - '0';
        } else {
            index++;
        }
        str++;
    }
    return value[1] * 32 * 32 + value[2] * 32 + value[3];
}

void *routine(void *arg) {
    memcached_return rc;  
    long thread_id = (long) arg;
    int count = 0;
    time_t expire = 0;
    char temp[252] = {};
    struct timeval before, after;
    long max = 0;
    long min = 2147483647;
    long avg = 0;
    char *retvalue = NULL;
    size_t retlength;
    int miss_count = 0;
    while (true) {
        if (count % 500 == 0) {
          printf("thread %d finshed %d requests out of %d \n", thread_id, count, num_test);
        }
        int index = rand() % waken_counter;
        int rand_core = waken_servers[index];
        int core_key = id_map[rand_core];
        if (rand_core < 0) {
            printf("something weird happends!\n");
            continue;
        }
        if (is_hit()) {
            int retry = 0;
            do {
                if (count % 3 == 0) {
                    gen_key_del(temp, key_size[count], core_key);
                    if (retry == 0) {
                        gettimeofday(&before, NULL);
                    } else {
                        printf("Thread %d, request %d retry: %d on server %s reason: %s\n", thread_id, count, retry,
                            server_addr[rand_core], memcached_strerror(memc[thread_id][rand_core], rc));
                    }
                    rc = memcached_delete(memc[thread_id][rand_core], temp, strlen(temp), expire);
                    gettimeofday(&after, NULL);
                } else {
                    gen_key(temp, key_size[count], core_key);
                    if (retry == 0) {
                        gettimeofday(&before, NULL);
                    } else {
                        printf("Thread %d, request %d retry: %d on server %s reason: %s\n", thread_id, count, retry,
                            server_addr[rand_core], memcached_strerror(memc[thread_id][rand_core], rc));
                    }
                    retvalue = memcached_get(memc[thread_id][rand_core], temp, strlen(temp), &retlength, &flags, &rc);
                    if (retvalue == NULL) {
                        miss_count++;
                    }
                    gettimeofday(&after, NULL);

                    // TODO: clean up this part if necessary.  
                    /*if (is_check && count % 10 == 0 && retvalue != NULL) {
                        printf("checking data corruption\n");
                        check_sum(retvalue, retlength, latency, rand_core);
                        fflush(latency);
                    }*/
                }
                retry++;
            } while ((rc == MEMCACHED_TIMEOUT || rc == MEMCACHED_SERVER_TEMPORARILY_DISABLED) && retry < MAX_RETRY);
        } else {
            gen_miss(temp);
            int retry = 0;
            do {
                if (retry == 0) {
                    gettimeofday(&before, NULL);
                } else {
                    printf("Thread %d, request %d retry: %d on server %s, reason: %s\n", thread_id, count, retry,
                        server_addr[rand_core], memcached_strerror(memc[thread_id][rand_core], rc));
                }
                if (count % 3 == 0) {
                   // rc = memcached_delete(memc[thread_id], temp, strlen(temp), expire);
                   retvalue = memcached_get(memc[thread_id][rand_core], temp, strlen(temp), &retlength, &flags, &rc);
                } else {
                   retvalue = memcached_get(memc[thread_id][rand_core], temp, strlen(temp), &retlength, &flags, &rc);
                }
                gettimeofday(&after, NULL);
                usleep(miss_penalty * 1000);
                retry++;
            } while ((rc == MEMCACHED_TIMEOUT || rc == MEMCACHED_SERVER_TEMPORARILY_DISABLED) && retry < MAX_RETRY);
        }

        long t = difftime(before, after);
        if (rc != MEMCACHED_SUCCESS && rc != MEMCACHED_NOTFOUND && count != 0) {
            fprintf(stderr,"Memcached error thread %d at server %s request %d: %s\n", thread_id,
                server_addr[rand_core], count, memcached_strerror(memc[thread_id][rand_core], rc));
        }
        
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

        if (inter_gap[count] > 0)
            usleep(inter_gap[count]);
        count++;
        maxes[thread_id] = max;
        mines[thread_id] = min;
        pthread_mutex_lock (&server_mutexes[thread_id]);
        sum[thread_id].total = avg;
        sum[thread_id].count = count;
        pthread_mutex_unlock (&server_mutexes[thread_id]);
        if (count >= num_test && !is_infi) {
            break;
        }
    }
    printf("thread %d is done", thread_id);
    char msg[100];
    // sprintf(msg, "max: %d min: %d avg: %f, count %d\n", max, min, (float)avg/count, count);
    sprintf(msg, "%d,%d,%f,%d\n", max, min, (float)avg/count, count);
    strcpy(results[thread_id], msg);
    // printf("%d: recording result %s\n", thread_id, msg);
    // printf("setting t to 0, min %d, max %d, avg: %d", min, max, avg);
        if (count == 0) {
      printf("count is zero");
    }
    // fprintf(latency, "%d,%d,%d\n", thread_id, avg, count);
    avges[thread_id] = avg/count;
    // quicksort(latency_record, 0, num_test);
    // plot(latency_record);    
    pthread_exit(NULL);
}





int main(int argc, char *argv[])
{
    time_t expire = 0;
    memcached_return rc;  

    num_of_threads = 4;
    char *retvalue = NULL;                                                                                                                
    size_t retlength;

    char ip_addr[12] = "10.0.0.%d";  
    /* By default, port starts from 11211. */
    int port = 11211;
    if (argc < 3) {
        printf("Usage: %s [number of servers] [times for random access-1 for infinite] [is checking sum? 1 for yes] [base addr] [miss latency] [port]", argv[0]);
        exit(0);
    }

    num_servers = atoi(argv[1]);
    for (int k = 0; k < num_servers; k++) {
        server_mutexes[k] = PTHREAD_MUTEX_INITIALIZER;
    }
    num_test = atoi(argv[2]);
    // Whether the client is running infinitely after it uses up all the keys.
    is_infi = (atoi(argv[3]) == 1);
    is_check = false;
    if (argc > 4) {
        num_of_threads = atoi(argv[4]);
    }

    miss_penalty = 5;
    if (argc > 5) {
        miss_penalty = atoi(argv[5]);
    }

    if (argc > 6) {
        port = atoi(argv[6]);
    }
  
    if (argc > 7) {
        // num_of_threads = atoi(argv[7]);
    }

    if (argc > 8) {
        beat_interval = atoi(argv[8]);
    }
    
    char addr[12];
    FILE *configuration = fopen("./configuration", "r");
    int total_addr = 0;
    while (fscanf(configuration, "%s\n", server_addr[total_addr]) != EOF) {
        printf("%d: got ip addr %s\n", total_addr, server_addr[total_addr]);
        id_map[total_addr] = get_id(server_addr[total_addr]);
        total_addr++;
    }
    if (total_addr != num_servers) {
        fprintf(stderr, "Might be a bug in configuration. \n");
    }
    total_addr = num_servers;
    for (int i = 0; i < total_addr; i++) {
        servers[i] = memcached_server_list_append(servers[i], server_addr[i], port, &rc);
    }
    for (int i = 0; i < num_of_threads; i++){
        for (int j = 0; j < num_servers; j++) {
            memc[i][j] = memcached_create(NULL);
            rc = memcached_server_push(memc[i][j], servers[j]);
            if (rc != MEMCACHED_SUCCESS)
                fprintf(stderr,"Couldn't add server: %s\n",memcached_strerror(memc[i][j], rc));
        }
    }
    
    /*for (int i = 0; i < num_servers; i++) {
        sprintf(addr, ip_addr, i+1);
        servers = memcached_server_list_append(servers, addr, port, &rc);
    }
    fprintf(stderr, "added server: %s\n", addr);
        
    for (int i = 0; i < num_of_threads; i++){
        memc[i] = memcached_create(NULL);
        rc = memcached_server_push(memc[i], servers);
        if (rc == MEMCACHED_SUCCESS)
            fprintf(stderr,"Successfully added server\n");
        else
            fprintf(stderr,"Couldn't add server: %s\n",memcached_strerror(memc[i], rc));
    }*/




    /* Start querying servers and recording results. */
    int ping_times;
    while (true) {
        for (int i = 0; i < num_servers; i++) {
            memcached_return_t instance_rc;
            const char *hostname= servers[i][0].hostname;
            in_port_t port= servers[i][0].port;
            ping_times = 0;
            while (libmemcached_util_ping2(hostname, port, NULL, NULL, &instance_rc) == false) {
                if (ping_times > MAX_PING) {
                    printf("server %d larger than max ping", i);
                    break;
                }
                usleep(2000);
                // usleep(200000);
                ping_times++;
            }
            if (libmemcached_util_ping2(hostname, port, NULL, NULL, &instance_rc) == true) {
                sleeping_servers[i] = false;
                waken_counter++;
                // printf("Server %d is awake. \n", i);
            }
        }
        // We can start sampling if at least one server is awake.
        if (waken_counter != 0) {
            break;
        }
    }
        
    char temp[252] = {};
    FILE *myfile = fopen("./key", "r");
    FILE *timeinter = fopen("./inter", "r");
    latency = fopen("/dev/ramptalk", "w");

    pthread_t hb;
    int hh = pthread_create(&hb, NULL, heart_beat, (void *)latency);
    pthread_t checking;
    int cc = pthread_create(&checking, NULL, check_server, (void *)0);

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
    key_size = (int *) malloc(num_test * sizeof(int));
    inter_gap = (int *) malloc(num_test * sizeof(int));
    if (key_size == NULL) {
        printf("malloc failed\n");
        return 1;
    }
    // printf("start pinging\n");
    int server_counter = 0;
    for (int i = 0; i < num_servers; i++) {
        if (sleeping_servers[i]) {
            continue;
        }
        while (true) {
            strcpy(temp, "done");
            char *value = (char *) malloc(5001);
            printf("waiting for server %d\n", i);
            retvalue = memcached_get(memc[0][i], temp, strlen(temp), &retlength, &flags, &rc);
            // printf("getting value\n");
            if (retvalue && (strcmp(temp, retvalue) == 0)) {
                printf("Server %d is ready. Start sampling. \n", i);
                waken_servers[server_counter] = i;
                server_counter++;
                break;
            } else {
                printf("Waiting for initializer to complete.\n");
                // sleep(1);
                usleep(200000);
            }
        }
    }
        

    int i = 0;
    // rewind(myfile);
    while (i < num_test) {
       if (fscanf(myfile, "%d\n", &key_len) != EOF) {
            key_size[i] = key_len % 250;
        }
        if (fscanf(timeinter, "%d\n", &key_len) != EOF) {
            inter_gap[i] = key_len;
        }
        // printf("%d\n", key_size[i]);
        if (key_len > 250) {
            printf("%d %d\n", i, key_len);
            // return 1;
        }       
        i++;
        if (i > num_test) {
            break;
        }
    }

    num_test = i;
    
    int ret = 0;
    pthread_t threads[num_of_threads];
    for (long t = 0; t < num_of_threads; t++) {
        printf("created thread %d total %d\n", t, num_of_threads);
        ret = pthread_create(&threads[t], NULL, routine, (void *)t);
    }
    for (long t = 0; t < num_of_threads; t++) {
        pthread_join(threads[t], NULL);
    } 

    max = maxes[0];
    min = mines[0];
    avg = 0;

    /*fprintf(latency, "thread: max, min, avg, count\n");
    for (int i = 0; i < num_of_threads; i++) {
        if (maxes[i] > max) {
            max = maxes[i];
        }
        if (mines[i] < min) {
            min = mines[i];
        }
        avg += avges[i];
        fprintf(latency, "%d: %s\n", i, results[i]);
    }*/

    // fprintf(latency, "T: max: %d min: %d avg: %f, count %d\n", max, min, (float)avg/count, count);
    fclose(timeinter);
    fclose(myfile);
    fclose(latency);
    pthread_cancel(hb);
    pthread_cancel(checking);
    
    for (int i = 0; i < num_of_threads; i++) {
        for (int j = 0; j < num_servers; j++)
        memcached_free(memc[i][j]);
        pthread_cancel(threads[i]);
    }
    pthread_exit(NULL);
}
