#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libmemcached/memcached.h>

#include <math.h>
#include <iostream>


void gen_data(char val[], int length) {
    length = length % 1000;
    for (int i = 0; i < length; i++) {
        val[i] = ((i + 1) * length) % 26 + 65;
    }
    val[length] = '\0';
}


int main(int argc, char *argv[])                                                                                                        
{                                        
    memcached_server_st *servers = NULL; 
    //  memcached_st *memc[] = {NULL, NULL, NULL, NULL};                                                                                                               
    /* Create an empty memcached interface */                                                                                             
    memcached_st *memc;
    memc = memcached_create(NULL);                                                                                                        
    memcached_return rc;
    char key[200];
    char *retvalue = NULL;
    size_t retlength;
    uint32_t flags;
    char base_addr[12] = "10.0.0.%d";  
    /* By default, port starts from 11211. */
    int port = 11211;
    if (argc < 3) {
        printf("Usage: %s [number of servers] [times for random access, -1 for infinite] [base addr] [port]", argv[0]);
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
        fprintf(stderr, "added server: %s\n", addr);
        servers = memcached_server_list_append(servers, addr, port + i, &rc);
    }
    /* Update the memcached structure with the updated server list. */
    rc = memcached_server_push(memc, servers);
    if (rc == MEMCACHED_SUCCESS)
        fprintf(stderr,"Successfully added server\n");
    else
        fprintf(stderr,"Couldn't add server: %s\n",memcached_strerror(memc, rc));
    FILE *file = fopen("key", "a+");
    rewind(file);
    FILE *value_file = fopen("value", "a+");
    char *temp = (char *) malloc(250);
    int key_len;
    char *value = (char *) malloc(1002);
    int length = 0;
    int count = 0;
    while (fscanf(file, "%d\n", &key_len) != EOF 
        && fscanf(value_file, "%d\n", &length) != EOF) {
        gen_data(value, length);
        gen_data(temp, key_len);
        if (rand() % 11 == 0) {
          fprintf(stderr, "key: %s\n", temp);
          fprintf(stderr, "value: %s\n", value);
        }
        rc = memcached_set(memc, temp, strlen(temp), value, strlen(value), (time_t)0, (uint32_t)0);
        if (rc != MEMCACHED_SUCCESS)
            fprintf(stderr, "Couldn't store key: %s\n", memcached_strerror(memc, rc));
        count++;
        if (count > num_test) {
            break;
        }
    }
    fclose(file);
    fclose(value_file);
}
