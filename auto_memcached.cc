#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libmemcached/memcached.h>
#include <sys/time.h>

#define HIT_RATE 81
#define MISS_PENALTY 2

void check_sum(char value[], int len, FILE *file) {
    for (int i = 0; i < len; i++) {
        char correct = ((i + 1) * len) % 26 + 65;
        if (correct != value[i]) {
            fprintf(file, "data corruption! %s", value);
            break;
        }
    }
}

void gen_key(char temp[], int len) {
    for (int i = 0; i < len; i++) {
        temp[i] = ((i + 1) * len) % 26 + 65;
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
    key[i] = (7 * i+1) % 26 + 65;
  }
}

int main(int argc, char *argv[])
{
  memcached_server_st *servers = NULL;
  memcached_st *memc;
  memc = memcached_create(NULL);
  memcached_return rc;
  char key[10];
  char value[10];     

  char keystore[1000][100];
  char valuestore[1000][100];

  char *retvalue = NULL;                                                                                                                
  size_t retlength;                                                                                                                     
  uint32_t flags;                     

  char base_addr[12] = "10.0.0.%d";  
  /* By default, port starts from 11211. */
  int port = 11211;
  if (argc < 3) {
    printf("Usage: %s [number of servers] [times for random access-1 for infinite] [base addr] [port]", argv[0]);
    exit(0);
  }

  int num_servers = atoi(argv[1]);
  int num_test = atoi(argv[2]);
  if (argc > 3) {
    strcpy(base_addr, argv[3]);
  }
  if (argc > 4) {
    port = atoi(argv[4]);
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
  unsigned long after;
  struct timeval tim;
  int key_len;
  while (true) {
    if (is_hit()) {
      if (fscanf(myfile, "%d\n", &key_len) != EOF) {
        gen_key(temp, key_len);
        gettimeofday(&tim, NULL);
        before = tim.tv_usec;
        retvalue = memcached_get(memc, temp, strlen(temp), &retlength, &flags, &rc);
        gettimeofday(&tim, NULL);
        after = tim.tv_usec;
        fprintf(latency, "read %d: %ld us\n", count, after - before);
        if (count % 10 == 0 && retvalue != NULL) {
          printf("checking corruption\n");
          check_sum(retvalue, retlength, latency);
        }
        fflush(latency);
      } else {
        break;
      }
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
  
  fclose(timeinter);
  fclose(myfile);
  fclose(latency);
}
