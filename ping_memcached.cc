#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libmemcached/memcached.h>
#include <sys/time.h>
#include <time.h>
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

int main(int argc, char *argv[]) {
  memcached_server_st *server = NULL;
  memcached_st *memc;
  memcached_return rc;
  memc = memcached_create(NULL);
  struct timeval tim;
  int core = atoi(argv[2]);
  int max_ping = atoi(argv[3]);
  char base_addr[12] = "10.0.0.%d";  
  char addr[12];
  sprintf(addr, base_addr, core+1);
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
    printf("the time is %ld\n", time(NULL));
    fprintf(result_file, "bytes_read: %llu\n", stat->bytes_read);
    fprintf(result_file, "bytes_written: %llu\n", stat->bytes_written);
    stamp = time(NULL) - stamp;
    fprintf(result_file, "time stamp in sec: %ld\n", time(NULL));
    fflush(result_file);
    count++;
    if (count > max_ping) {
      break;
    }
    sleep(interval);
    if (count % 2 == 0) {
      proc(result_file);
    }
  }
  fclose(result_file);
  free(stat);
  
}

