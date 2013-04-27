#include <unistd.h>
#include <libmemcached/memcached.h>
#include <sys/time.h>
#include <time.h>

#include <libmemcachedutil-1.0/ping.h>
#include <libmemcachedutil-1.0/util.h>

#include <unistd.h> 
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h> /* for strncpy */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libmemcached/memcached.h>

#include <math.h>
#include <iostream>

#define MAXTHREADS 21

int num_test;
int *key_size;
int *value_size;
memcached_st *memc[MAXTHREADS];
int threads_total = 20;
int chunk;
int core_id;




char *get_ip(char device_name[]) {
    int fd;
    struct ifreq ifr;

    fd = socket(AF_INET, SOCK_DGRAM, 0);

    /* I want to get an IPv4 IP address */
    ifr.ifr_addr.sa_family = AF_INET;

    /* I want IP address attached to "eth0" */
    strncpy(ifr.ifr_name, device_name, IFNAMSIZ-1);

    ioctl(fd, SIOCGIFADDR, &ifr);
    close(fd);
    return inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
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


void gen_data(char val[], int length, int core_id) {
    // length = length % 1000;
    for (int i = 0; i < length; i++) {
        val[i] = ((i + core_id) * length) % 26 + 65;
    }
    val[length] = '\0';
}

void gen_data_del(char val[], int length, int core_id) {
    // length = length % 1000;
    for (int i = 0; i < length; i++) {
        val[i] = ((i + core_id) * length + 1) % 26 + 65;
    }
    val[0] = 'A';
    val[1] = 'A';
    val[length] = '\0';
}


void *initialize(void *arg) {
    char *temp = (char *) malloc(250);
    char *value = (char *) malloc(5001);
    long thread_id = (long) arg;
    int start = chunk * thread_id;
    int save = start;
    int end = start + chunk;
		// printf("thread %d: start: %d, end: %d\n", thread_id, start, end);
    memcached_return rc;
    while (start < end) {
        gen_data(value, value_size[start] % 5000, core_id);
        // gen_data(value, value_size[start], core_id);
        // printf("count: %d, len %d id %d", count, key_len, core_id);
        /*if (count % 5000 == 0) {
        
            printf("count: %d, len %d id %d", count, key_len, core_id);
          } */
        if (start % 3 == 0) {
            gen_data_del(temp, key_size[start] % 250, core_id);    
        } else {
            gen_data(temp, key_size[start] % 250, core_id);
        }
				// printf("thread %d: iteration: %d; key:%d value %d\n", thread_id, start, key_size[start], value_size[start]);
            
        if (start % 500 == 0) {
          // printf("iteration %d\n", start);
          printf("Thread %d finished %d out of %d\n", thread_id, start - save, chunk);
          //fprintf(stderr, "key: %s\n", temp);
          //fprintf(stderr, "value: %s\n", value);
        }
        // printf("Iteration %d core_id %d setting key: %s\n", count, core_id, temp);
        rc = memcached_set(memc[thread_id], temp, strlen(temp), value, strlen(value), (time_t) 0, (uint32_t) 0);
        usleep(200);
        if (rc != MEMCACHED_SUCCESS)
            fprintf(stderr, "Couldn't store key: %s\n", memcached_strerror(memc[thread_id], rc));
        start++;
    }

	printf("Thread %d finished\n", thread_id);
    pthread_exit(NULL);
}

int main(int argc, char *argv[])                                                                                                        
{
    if (argc > 5) {
        threads_total = atoi(argv[5]);
    }
    memcached_server_st *servers = NULL; 
    //  memcached_st *memc[] = {NULL, NULL, NULL, NULL};                                                                                                               
    /* Create an empty memcached interface */                                                                                             
    // memcached_st *memc;
    for (int i = 0; i < threads_total; i++) {
        memc[i] = memcached_create(NULL);                                                                                                        
    }
    memcached_return rc;
    char key[200];
    char *retvalue = NULL;
    size_t retlength;
    uint32_t flags;
    char ip_addr[12] = "10.0.0.%d";  
    /* By default, port starts from 11211. */
    int port = 11211;
    if (argc < 3) {
        printf("Usage: %s [number of servers] [times for random access, -1 for infinite] [base addr] [port]", argv[0]);
        exit(0);
    }
    core_id = atoi(argv[1]);

    num_test = atoi(argv[2]);
    chunk = num_test/threads_total;

    char device_name[8];
    if (argc > 3) {
        strcpy(device_name, argv[3]);
    }
    if (argc > 4) {
        port = atoi(argv[4]);
    }

    char addr[12];
    // sprintf(addr, ip_addr, core_id + 1);
    // fprintf(stderr, "added server: %s\n", addr);
    // printf("ip addr %s\n", get_ip(device_name));
    strcpy(addr, get_ip(device_name));
    memcached_return_t instance_rc;
    while (libmemcached_util_ping2(addr, port, NULL, NULL, &instance_rc) == false) {
        usleep(2000);
        printf("waiting for server to wake up\n");
    }
    core_id = get_id(addr);
    servers = memcached_server_list_append(servers, addr, port, &rc);
    /* Update the memcached structure with the updated server list. */
    for (int i = 0; i < threads_total; i++) {
        rc = memcached_server_push(memc[i], servers);
        if (rc != MEMCACHED_SUCCESS)
            fprintf(stderr,"Couldn't add server: %s\n",memcached_strerror(memc[i], rc));
    }
       
    FILE *file = fopen("./key", "r");
    if (file == NULL) {
        perror("fopen(key): ");
        exit(1);
    }
    // rewind(file);
    FILE *value_file = fopen("./value", "r");
    if (value_file == NULL) {
        perror("fopen(value): ");
        exit(1);
    }

    char *temp = (char *) malloc(250);
    char *value = (char *) malloc(5001);
    int key_len;
    
    int length = 0;
    int count = 0;
    key_size = (int *) malloc(num_test * sizeof(int));
    value_size = (int *) malloc(num_test * sizeof(int));
    while (fscanf(file, "%d\n", &key_len) != EOF 
        && fscanf(value_file, "%d\n", &length) != EOF) {
        key_size[count] = key_len;
        value_size[count] = length;
        count++;
        if (count > num_test) {
            break;
        }
    }


    /** Kickstart initialization. */
    pthread_t threads[threads_total];
    int ret = 0;
    for (long t = 0; t < threads_total; t++) {
        printf("created thread %d total %d\n", t, threads_total);
        ret = pthread_create(&threads[t], NULL, initialize, (void *)t);
    }

    for (long t = 0; t < threads_total; t++) {
        pthread_join(threads[t], NULL);
    } 

	printf("all threads finished\n");

	strcpy(temp, "done");
    strcpy(value, "done");
    rc = memcached_set(memc[0], temp, strlen(temp), value, strlen(value), (time_t)0, (uint32_t)0);
    if (rc == MEMCACHED_SUCCESS) {
	   fprintf(stderr, "Successfully initialized servers.\n");
    } else {
	   fprintf(stderr, "Failed to set the final key.\n");
    }
    fclose(file);
    fclose(value_file);
    // free(temp);
    // free(value);
}
