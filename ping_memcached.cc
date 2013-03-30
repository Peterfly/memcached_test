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

void print_all(memcached_stat_st *stat, FILE *file) {
    fprintf(file, "bytes_read: %llu\n", stat->bytes_read);
    fprintf(file, "bytes_written: %llu\n", stat->bytes_written);
    
    fprintf(file, "time: %llu\n", stat->time);
    fprintf(file, "get_hits: %llu\n", stat->get_hits);
    fprintf(file, "get_misses: %llu\n", stat->get_misses);

    fprintf(file, "curr_items: %llu\n", stat->curr_items);
    fprintf(file, "total_items: %llu\n", stat->total_items);
    fprintf(file, "rusage_user: %llu\n", stat->total_items);
    fprintf(file, "total_items: %llu\n", stat->total_items);
    fprintf(file, "curr_connections: %llu\n", stat->curr_connections);
    fprintf(file, "curr_items: %llu\n", stat->curr_items);
    fprintf(file, "rusage_system_seconds: %llu\n", stat->rusage_system_seconds);
    fprintf(file, "rusage_system_microseconds: %llu\n", stat->rusage_system_microseconds);

    fprintf(file, "rusage_user_seconds: %llu\n", stat->rusage_user_seconds);
    fprintf(file, "rusage_user_microseconds: %llu\n", stat->rusage_user_microseconds);
    fprintf(file, "threads: %llu\n", stat->threads);
    fprintf(file, "total_connections: %llu\n", stat->total_connections);
    fprintf(file, "total_items: %llu\n", stat->total_items);
    fprintf(file, "bytes: %llu\n", stat->bytes);
    fprintf(file, "cmd_get: %llu\n", stat->cmd_get);
    fprintf(file, "cmd_set: %llu\n", stat->cmd_set);
}



void proc(FILE *result) {
  FILE *fs = fopen("/proc/meminfo", "r");
  char temp[100] = {};
  char x[100] = {};
  fscanf(fs, "%s%s\n", temp, x);
  //printf("%s%s\n", temp, x);
  fprintf(result, "%s%s\n", temp, x);
  fscanf(fs, "%s\n", temp);
  fscanf(fs, "%s%s\n", temp, x);
  //printf("%s%s\n", temp, x);
  fprintf(result, "%s%s\n", temp, x);
  fclose(fs);
  
  fs = fopen("/proc/stat", "r");
  fscanf(fs, "%s", temp);
  fprintf(result, "%s", temp);
  for (int i = 0; i < 9; i++) {
    fscanf(fs, "%s ", x);
    fprintf(result, "%s ", x);
  }
  fprintf(result, "\n");
  fclose(fs);
  fflush(result);
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
  printf("%s\n", addr);
  server = memcached_server_list_append(server, addr, 11211, &rc);
  rc = memcached_server_push(memc, server);
  int interval = atoi(argv[1]);
  if (rc == MEMCACHED_SUCCESS)
    fprintf(stderr,"Successfully added server\n");
  else
    fprintf(stderr,"Couldn't add server: %s\n",memcached_strerror(memc, rc));
  memcached_stat_st *stat = (memcached_stat_st *) malloc(sizeof(memcached_stat_st));
  memcached_return_t err;
 
  int count = 0;
  FILE *result_file = fopen("result", "a+");
  if (result_file == 0) {
    fprintf(stderr, "failed\n");
  }
  time_t stamp;
  while (true) {
    stat = memcached_stat(memc, NULL, &err);
    stamp = time(NULL);
    // printf("the time is %ld\n", time(NULL));
    print_all(stat, result_file);
    stamp = time(NULL) - stamp;
    fprintf(result_file, "time stamp in sec: %ld\n", time(NULL));
    fflush(result_file);
    count++;
    if (count > max_ping) {
      // break;
    }
    sleep(interval);
    if (count % 2 == 0) {
      proc(result_file);
    }
  }
  fclose(result_file);
  free(stat);
  
}

