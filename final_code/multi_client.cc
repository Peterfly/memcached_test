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
#include <signal.h>
#include <execinfo.h>
#include <queue>

#define HIT_RATE 81
#define MISS_PENALTY 2
#define MAXTHREADS 100 
#define NUM_STARS 10
#define MAX_PING 100
#define MAX_SERVERS 130

#define MAX_RETRY 10

#define PING_THREAD 4

using namespace std;

memcached_server_st *servers[MAX_SERVERS];

struct info{
    long total;
    int count;
};

struct req_stat {
    short server_id;
    long latency;
};

struct ping_stat {
    int server_id;
    long duration;
};


queue<ping_stat> ping_queue;
queue<ping_stat> ready_queue;

pthread_mutex_t segv_lock = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t list_lock = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t counter_lock = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t waken_lock = PTHREAD_MUTEX_INITIALIZER;

int bins[MAXTHREADS][35] = {0};

bool servers_seen[MAX_SERVERS];
timeval* servers_ping_alive = new timeval[MAX_SERVERS];
long server_spent[MAX_SERVERS];


int server_counter = 0;

int miss_penalty = 2;
int *key_size;
int *inter_gap;
int num_test;
memcached_st *memc[MAXTHREADS][MAX_SERVERS];
long num_of_threads = 4;
long running_servers = 0;
int num_servers;
// char results[MAXTHREADS][200];
long maxes[MAXTHREADS] = {0};
long mines[MAXTHREADS] = {0};

info* sum = new info[MAXTHREADS];


int id_map[MAX_SERVERS] = {-1};
// float avges[MAXTHREADS];
bool is_check;
bool is_infi;
FILE *latency;
uint32_t flags;                     
int beat_interval = 2;

char ip_addr[12] = "10.0.0.%d";

// Fault tolerance
bool sleeping_servers[MAX_SERVERS] = {true};
bool picked_servers[MAX_SERVERS] = {false};
int waken_counter = 0;
int waken_servers[MAX_SERVERS] = {-1};
pthread_mutex_t server_mutexes[MAX_SERVERS];

char server_addr[MAX_SERVERS][50];
char alternate_addr[MAX_SERVERS][50];

long difftime(timeval before, timeval after) {
    long diff = (after.tv_usec - before.tv_usec) + (after.tv_sec - before.tv_sec) * 1000000;
    return diff;
}

void sigsegv_handler(int sig) 
{
  void *array[10]; 
  size_t size; 
 
  pthread_mutex_lock(&segv_lock);
  printf("-----------Thread %x SEGV backtrace------------\n", (unsigned int)pthread_self());
  // get void*'s for all entries on the stack 
  size = backtrace(array, 10); 
 
  // print out all the frames to stderr 
  backtrace_symbols_fd(array, size, 2);
  pthread_mutex_unlock(&segv_lock);
  exit(1); 
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


void gen_miss(char key[], int len) {
  int size = len % 250;
  if (len == 0) len = 1;
  for (int i = 0; i > size; i++) {
    key[i] = rand() % 26 + 97;
  }
  key[0] = 'X';
  key[len] = '\0';
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
    int s = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    long times = 0;
    FILE *output = (FILE *) arg;
    while (true) {
        for (int i = 0; i < num_of_threads; i++) {
          pthread_mutex_lock(&server_mutexes[i]);
          fprintf(output, "Thread %d, %d, %d\n", i, sum[i].total, sum[i].count);
          // fprintf(stderr, "Console: Thread %d, %d, %d\n", i, sum[i].total, sum[i].count);
          pthread_mutex_unlock(&server_mutexes[i]);
        }
        times ++;
        fflush(output);
        usleep(500000);
    }
    // pthread_exit(NULL);
}

void *new_ping(void *arg) {
    uint32_t flags;
    long proxy = (long) arg;
    int start_server =(int) proxy;
    memcached_return_t instance_rc;
    int s = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    char temp[252] = {};
    char *retvalue = (char *) malloc((1000) * sizeof(char));;
    size_t retlength;
    memcached_return rc;
    char *value = (char *) malloc(5001 * sizeof(char));

    while (true) {
        pthread_mutex_lock(&list_lock);
        if (ping_queue.empty()) {
            pthread_mutex_unlock(&list_lock);
            break;
        }

        ping_stat next = ping_queue.front();
        ping_queue.pop();
        pthread_mutex_unlock(&list_lock);

        int i = next.server_id;
        struct timeval start_time, end_time;
        gettimeofday(&start_time, NULL);
        const char *hostname = servers[i][0].hostname;
        in_port_t port= servers[i][0].port;
        printf("ping: %s\n", hostname);
        if (!libmemcached_util_ping2(hostname, port,
                                NULL, NULL, &instance_rc)) {
            gettimeofday(&end_time, NULL);
            next.duration += end_time.tv_usec - start_time.tv_usec +
                1000000 * (end_time.tv_sec - start_time.tv_sec);
            
            pthread_mutex_lock(&list_lock);
            ping_queue.push(next);
            pthread_mutex_unlock(&list_lock);
            printf("failed ping: %s\n", hostname);
            continue;
        }
        gettimeofday(&end_time, NULL);
        next.duration += end_time.tv_usec - start_time.tv_usec +
            1000000 * (end_time.tv_sec - start_time.tv_sec);
        long ss = start_time.tv_sec * 1000000 + start_time.tv_usec;
        long ee = end_time.tv_sec * 1000000 + end_time.tv_usec;        
        fprintf(latency, "[duration]: %s: %d, %d, %d, %d, %d, %d\n", servers[next.server_id][0].hostname,
            start_time.tv_sec, start_time.tv_usec, end_time.tv_sec,
            end_time.tv_usec, ee, next.duration);
        strcpy(temp, "done");
        int howManyTimesPing = 0;
        do {
            printf("getting done key %s %d\n", hostname, port);
            retvalue = memcached_get(memc[0][i], temp, strlen(temp), &retlength, &flags, &rc);
            // printf("getting value\n");
            if (rc != MEMCACHED_SUCCESS)
                fprintf(stderr, "Couldn't get back done key: %s\n", memcached_strerror(memc[0][i], rc));
            usleep(200000);
        } while (!(retvalue && (strcmp(temp, retvalue) == 0)));
        printf("server ip %s port %d is ready\n", hostname, port);  
        pthread_mutex_lock(&waken_lock);
        ready_queue.push(next);
        waken_servers[server_counter] = i;
        server_counter++;
        pthread_mutex_unlock(&waken_lock);
    }
    printf("one thread has ended pinging\n");
    free(retvalue);
    free(value);
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
    uint32_t flags;
    memcached_return rc;  
    long thread_id = (long) arg;
    int failed_count = 0;
    int count = 0;
    time_t expire = 0;
    char temp[252] = {};
    struct timeval before, after, starting, ending;
    long max = 0;
    long min = 2147483647;
    long avg = 0;
    char *retvalue = NULL;
    size_t retlength;
    int miss_count = 0;
    gettimeofday(&starting, NULL);
    req_stat* req_record = new req_stat[num_test];
    long total_retry = 0;
    while (true) {
        if (count % 500 == 0) {
          printf("thread %d finshed %d requests out of %d \n", thread_id, count, num_test);
        }
        int index = rand() % server_counter;
        int rand_core = waken_servers[index];
        // int core_key = id_map[rand_core];
        int core_key = rand_core;
        /*if (core_key < 0 || core_key >= num_servers) {
            printf("sth wrong with id map %d\n", core_key);
            // continue;
        }*/
        if (rand() % 2 == 0) {
            server_addr[core_key][0] = '2';
        } else {
            server_addr[core_key][0] = '1';
        }
        printf("sending to addr %s \n", server_addr[core_key]);
        int retry = 0;
        if (is_hit()) {
            do {
                if (count % 3 == 0) {
                    gen_key_del(temp, key_size[count], core_key);
                    if (retry == 0) {
                        gettimeofday(&before, NULL);
                    } else {
                        printf("Thread %d, request %d retry: %d on server %s reason: %s\n", thread_id, count, retry,
                            server_addr[core_key], memcached_strerror(memc[thread_id][core_key], rc));
                    }
                    rc = memcached_delete(memc[thread_id][core_key], temp, strlen(temp), expire);
                    gettimeofday(&after, NULL);
                } else {
                    gen_key(temp, key_size[count], core_key);
                    if (retry == 0) {
                        gettimeofday(&before, NULL);
                    } else {
                        printf("Thread %d, request %d retry: %d on server %s reason: %s\n", thread_id, count, retry,
                            server_addr[core_key], memcached_strerror(memc[thread_id][core_key], rc));
                    }
                    retvalue = memcached_get(memc[thread_id][core_key], temp, strlen(temp), &retlength, &flags, &rc);
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
            gen_miss(temp, key_size[count]);
            
            do {
                if (retry == 0) {
                    gettimeofday(&before, NULL);
                } else {
                    printf("Thread %d, request %d retry: %d on server %s, reason: %s\n", thread_id, count, retry,
                        server_addr[core_key], memcached_strerror(memc[thread_id][core_key], rc));
                }
                if (count % 3 == 0) {
                   // rc = memcached_delete(memc[thread_id], temp, strlen(temp), expire);
                   retvalue = memcached_get(memc[thread_id][core_key], temp, strlen(temp), &retlength, &flags, &rc);
                } else {
                   retvalue = memcached_get(memc[thread_id][core_key], temp, strlen(temp), &retlength, &flags, &rc);
                }
                gettimeofday(&after, NULL);
                usleep(miss_penalty * 1000);
                retry++;
            } while ((rc == MEMCACHED_TIMEOUT || rc == MEMCACHED_SERVER_TEMPORARILY_DISABLED) && retry < MAX_RETRY);
        }

        total_retry += retry-1;

        if (retry > 1) {
            failed_count += 1;
        }

        long t = difftime(before, after);

        if (rc != MEMCACHED_SUCCESS && rc != MEMCACHED_NOTFOUND) {
            fprintf(stderr,"Memcached error thread %d at server %s request %d: %s\n", thread_id,
                server_addr[core_key], count, memcached_strerror(memc[thread_id][core_key], rc));
        }
        
        if (count == 0) {
            min = t;
            max = t;
        }
        if (retry == 1) {
            avg += t;
        }
        if (t < min) {
            min = t;
        }
        if (t > max) {
            max = t;
        }
        int ind = (t - 50)/5;
        if (ind < 0) {
            ind = 0;
        }
        if (ind > 30) {
            ind = 30;
        }
        bins[thread_id][ind]++;

        if (inter_gap[count] > 0)
            usleep(inter_gap[count]);
        req_record[count].server_id = (short) core_key;
        req_record[count].latency = t;
        count++;
        maxes[thread_id] = max;
        mines[thread_id] = min;

        pthread_mutex_lock (&server_mutexes[thread_id]);
        
        if (retry == 1) {
            sum[thread_id].total = avg;
        } else {
            // if this is a failed requesst
            // sum[thread_id].total = -1;
        }
        sum[thread_id].count = count - failed_count;
            
        pthread_mutex_unlock (&server_mutexes[thread_id]);
        if (count >= num_test && !is_infi) {
            printf("thread: %d breaking out of the loop, count : %d\n", thread_id, count);
            break;
        }
    }
    gettimeofday(&ending, NULL);
    // char msg[200] = {};
    // sprintf(msg, "max: %d min: %d avg: %f, count %d\n", max, min, (float)avg/count, count);
    // snprintf(msg, 200, "%d,%d,%f,%d\n", max, min, (float)avg/count, count);
    // strncpy(results[thread_id], msg, 199);
    // results[thread_id][199] = '\0';
    // printf("%d: recording result %s\n", thread_id, msg);
    // printf("setting t to 0, min %d, max %d, avg: %d", min, max, avg);
    if (count == 0) {
      printf("count is zero");
    }
    // fprintf(latency, "%d,%d,%d\n", thread_id, avg, count);
    // avges[thread_id] = avg/count;
    // quicksort(latency_record, 0, num_test);
    // plot(latency_record);
    printf("thread %d finishes, printing latency stats\r\n", thread_id);
    /*for (int i = 0; i <= 30; i++) {
        if (i == 0) {
            printf("[Thread %d] bins smaller than 50-> %d\n",
                thread_id, bins[thread_id][i]);
        } else if (i == 30) {
            printf("[Thread %d] bins larger than 200: %d\n",
                thread_id, bins[thread_id][i]);
        } else {
            printf("[Thread %d] bins %d to %d: %d\n",
                thread_id, (i-1)*5+50, i*5+50, bins[thread_id][i]);
        }
    }*/
    for (int i = 0; i < num_test; i++) {
        fprintf(latency, "[latency] %d, %d\r\n", req_record[i].server_id,
            req_record[i].latency);

    }
    fprintf(latency, "[thread_summary]: thread %d retry: %d, microsecond: %d\r\n", thread_id, total_retry,
       difftime(starting, ending));
    pthread_exit(NULL);
}





int main(int argc, char *argv[])
{
    latency = fopen("/dev/ramptalk", "w");
    signal(SIGSEGV, sigsegv_handler);
    time_t expire = 0;
    memcached_return rc;

    num_of_threads = 4;
    char *retvalue = NULL;                                                                                                                
    size_t retlength;
    
    struct timeval now;
    gettimeofday(&now, NULL);
    srand(now.tv_usec);

    char ip_addr[12] = "10.0.0.%d";  
    /* By default, port starts from 11211. */
    int port = 11211;
    if (argc < 3) {
        printf("Usage: %s [number of servers] [times for random access-1 for infinite] [is checking sum? 1 for yes] [base addr] [miss latency] [port]", argv[0]);
        exit(0);
    }

    num_servers = atoi(argv[1]);

    // Make sure we ping server in a random order
    int kunuth_shuffle[MAX_SERVERS] = {0};
    for (int i = 0; i < num_servers; i++) {
        kunuth_shuffle[i] = i;
    }
    for (int i = 0; i < num_servers; i++) {
        int target = rand() % num_servers;
        int temp_swap = kunuth_shuffle[i];
        kunuth_shuffle[i] = kunuth_shuffle[target];
        kunuth_shuffle[target] = temp_swap;
    }

    int target_server = 0;
    for (int j = 0; j < num_servers; j++) {
        target_server = kunuth_shuffle[j];
        ping_stat stat;
        stat.server_id = target_server;
        stat.duration = 0;
        ping_queue.push(stat);
    }

    for (int jj = 0; jj < num_servers; jj++) {
        sleeping_servers[jj] = true;
        picked_servers[jj] = false;
        waken_servers[jj] = -1;

    }
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
        // id_map[total_addr] = 
        //get_id(server_addr[total_addr]);
        total_addr++;
    }
    if (total_addr != num_servers) {
        fprintf(stderr, "Number of ip addresses doesn't match number of server. \n");
    }
    total_addr = num_servers;
    for (int i = 0; i < total_addr; i++) {
        servers[i] = memcached_server_list_append(servers[i], server_addr[i], port, &rc);
        strcpy(alternate_addr[i], server_addr[i]);
        alternate_addr[i][0] = '2';
        servers[i] = memcached_server_list_append(servers[i], alternate_addr[i], port, &rc);
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
    pthread_t ping_threads[8];
    int ping_times;
    int pp;
    for (int kk = 0; kk < PING_THREAD; kk++)
        pp = pthread_create(&ping_threads[kk], NULL, new_ping, (void *)(rand() % num_servers));
    while (true) {
        usleep(200);
        // We can start sampling if at least one server is awake.
        if (server_counter != 0) {
            break;
        }
    }
        
    char temp[252] = {};
    FILE *myfile = fopen("./key", "r");
    FILE *timeinter = fopen("./inter", "r");
    

    pthread_t hb;
    int hh = pthread_create(&hb, NULL, heart_beat, (void *)latency);
    // pthread_t checking;
    // int cc = pthread_create(&checking, NULL, check_server, (void *)0);

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
    key_size = (int *) malloc((num_test + 5) * sizeof(int));
    inter_gap = (int *) malloc((num_test + 5) * sizeof(int));
    if (key_size == NULL) {
        printf("malloc failed\n");
        return 1;
    }
    // printf("start pinging\n");
    

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
    /*for (int i = 0; i <= 30; i++) {
        int sum_of_all = 0;
        for (int j = 0; j < num_of_threads; j++) {
            sum_of_all += bins[j][i];
        }
        if (i == 0) {
            printf("[TOTAL] bins smaller than 50-> %d\n",
                sum_of_all);
        } else if (i == 30) {
            printf("[TOTAL] bins larger than 200: %d\n",
                sum_of_all);
        } else {
            printf("[TOTAL] bins %d to %d: %d\n",
                (i-1)*5+50, i*5+50, sum_of_all);
        }
    }*/

    /* max = maxes[0];
    min = mines[0];
    avg = 0;

    fprintf(latency, "thread: max, min, avg, count\n");
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
    
    
    
    for (int i = 0; i < num_of_threads; i++) {
        for (int j = 0; j < num_servers; j++)
        memcached_free(memc[i][j]);
        // pthread_cancel(threads[i]);
    }

    pthread_mutex_lock(&list_lock);
    pthread_mutex_lock(&waken_lock);
    while (!ready_queue.empty()) {
        ping_stat temp_stat = ready_queue.front();
        ready_queue.pop();
        fprintf(latency, "[duration at the end]: %d: %d\r\n", temp_stat.server_id,
            temp_stat.duration);
    }
    pthread_mutex_unlock(&list_lock);
    pthread_mutex_unlock(&waken_lock);
    pthread_cancel(hb);
    // pthread_cancel(checking);
    fclose(timeinter);
    fclose(myfile);
    fclose(latency);
    free(key_size);
    free(inter_gap);
}
