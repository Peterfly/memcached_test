#include <stdio.h>
#include <unistd.h>
#include <libmemcached/memcached.h>
#include <sys/time.h>
#include <time.h>

#include <unistd.h> 
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h> /* for strncpy */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <signal.h>
#include <queue>

using namespace std;

static queue<char*> myqueue;
static FILE *result_file;

void inline reverse(char *ptr, int size) {
  for (int i = 0; i < (size >> 1); i++) {
    char temp = ptr[i];
    ptr[i] = ptr[size - i - 1];
    ptr[size - i] = temp;
  }
}

void print_all(memcached_stat_st *stat, FILE *file) {
    // char *temp = (char *) malloc(60);
    // sprintf(temp, "bytes_read: %llu\n", stat->bytes_read);
    // Bytes_read
    // printf("NOTE:writing stats\n");
   /* 
    fprintf(stderr, "%llu,%llu,%llu,%llu,%llu\n", stat->bytes_read,
        stat->bytes_written, stat->time, stat->get_hits, stat->get_misses);

    fprintf(stderr, "this is lu! %lu,%lu,%lu,%lu,%lu\n", stat->bytes_read,
        stat->bytes_written, stat->time, stat->get_hits, stat->get_misses);
  */

    // bytes read
    


    fprintf(file, "%llu,%llu,%lu,%llu,%llu\n", stat->bytes_read,
        stat->bytes_written, stat->time, stat->get_hits, stat->get_misses);
    fflush(file);
    /*fprintf(file, "r:%llu\n", stat->bytes_read);
    // myqueue.push(temp);
    // temp = (char *) malloc(40);
    // temp = 
    // Bytes_written
    fprintf(file, "w:%llu\n", stat->bytes_written);
    // sprintf(temp, "bytes_written: %llu\n", stat->bytes_written);
    // myqueue.push(temp);
    // temp = (char *) malloc(40);
    
    fprintf(file, "t:%llu\n", stat->time);
    // sprintf(temp, "time: %llu\n", stat->time);
    // myqueue.push(temp);
    // temp = (char *) malloc(40);

    // sprintf(temp, "get_hits: %llu\n", stat->get_hits);
    fprintf(file, "h:%llu\n", stat->get_hits);
    // sprintf(temp, "time: %llu\n", stat->time);
    // myqueue.push(temp);
    // temp = (char *) malloc(40);

    fprintf(file, "m:%llu\n", stat->get_misses);
    // sprintf(temp, "get_misses: %llu\n", stat->get_misses);
    // myqueue.push(temp);
    // temp = (char *) malloc(40);

    // fprintf(file, "curr_items: %llu\n", stat->curr_items);
    // fprintf(file, "total_items: %llu\n", stat->total_items);
    // fprintf(file, "rusage_user: %llu\n", stat->rusage_user);
    // fprintf(file, "curr_connections: %llu\n", stat->curr_connections);
    // fprintf(file, "curr_items: %llu\n", stat->curr_items);
    // fprintf(file, "rusage_system_seconds: %llu\n", stat->rusage_system_seconds);
    // fprintf(file, "rusage_system_microseconds: %llu\n", stat->rusage_system_microseconds);

    // fprintf(file, "rusage_user_seconds: %llu\n", stat->rusage_user_seconds);
    // fprintf(file, "rusage_user_microseconds: %llu\n", stat->rusage_user_microseconds);
    // fprintf(file, "threads: %llu\n", stat->threads);
    // fprintf(file, "total_connections: %llu\n", stat->total_connections);
    // fprintf(file, "total_items: %llu\n", stat->total_items);
    // fprintf(file, "bytes: %llu\n", stat->bytes);
    // fprintf(file, "cmd_get: %llu\n", stat->cmd_get);
    // fprintf(file, "cmd_set: %llu\n", stat->cmd_set);*/
}



void proc(FILE *result) {
    char copier[60];
    FILE *fs = fopen("/proc/meminfo", "r");
    char temp[100] = {};
    char x[100] = {};
    fscanf(fs, "%s%s\n", temp, x);
    sprintf(copier, "%s%s\n", temp, x);
    // myqueue.push(copier);
    // fprintf(result, "%s", copier);
    

    // fprintf(result, "%s%s\n", temp, x);
    fscanf(fs, "%s\n", temp);
    fscanf(fs, "%s%s\n", temp, x);
    sprintf(copier, "%s%s\n", temp, x);
    fprintf(result, "%s", copier);
    // myqueue.push(copier);
    sprintf(copier, "");
    // (result, "%s%s\n", temp, x);
    // fclose(fs);

    fs = fopen("/proc/stat", "r");
    fscanf(fs, "%s", temp);
    sprintf(copier, "%s%s", copier, temp);
    for (int i = 0; i < 9; i++) {
        fscanf(fs, "%s ", x);
        // fprintf(result, "%s ", x);
        sprintf(copier, "%s %s", copier, x);
    }
    sprintf(copier, "%s\n", copier);
    // printf("copier is %s\n", copier);
    // myqueue.push(copier);
    fprintf(result, "%s", copier);
    fclose(fs);
    // fflush(result);
}

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

int main(int argc, char *argv[]) {
  printf("ping program starts!\n");
  memcached_server_st *server = NULL;
  memcached_st *memc;
  memcached_return rc;
  char device_name[8];
  memc = memcached_create(NULL);
  struct timeval tim;
  // int core = atoi(argv[2]);
  strcpy(device_name, argv[2]);
  int max_ping = atoi(argv[3]);
  char base_addr[50] = "192.168.3.%d";
  if (argc > 4) {
	strcpy(base_addr, argv[4]);	
  } 
  char addr[12];
  // sprintf(addr, base_addr, core+1);
  strcpy(addr, get_ip(device_name));
  printf("Server address %s\n", addr);
  server = memcached_server_list_append(server, addr, 11211, &rc);
  rc = memcached_server_push(memc, server);
  int interval = atoi(argv[1]);
  if (rc != MEMCACHED_SUCCESS)
    fprintf(stderr,"Couldn't add server: %s\n",memcached_strerror(memc, rc));
  memcached_stat_st *stat = (memcached_stat_st *) malloc(sizeof(memcached_stat_st));
  memcached_return_t err;
 
  int count = 0;
  result_file = fopen("/dev/ramptalk", "w");
  printf("NOTE: writing to ramptalk\n");
  fprintf(result_file, "read written time hits misses\n");
  fflush(result_file);
  if (result_file == 0) {
    fprintf(stderr, "failed\n");
  }
  time_t stamp;
  while (true) {
    stat = memcached_stat(memc, NULL, &err);
    stamp = time(NULL);
    // printf("the time is %ld\n", time(NULL));
    if (count % 5 == 0) print_all(stat, result_file);
    stamp = time(NULL) - stamp;
    // fprintf(result_file, "time stamp in sec: %ld\n", time(NULL));
    // fflush(result_file);
    count++;
    if (count > max_ping) {
      // break;
    }
    // sleep(interval);
    
    // if (count % 2 == 0) {
    proc(result_file);
    // }
    free(stat);
    usleep(200000);
  }
  fclose(result_file);
  free(stat);
  
}

