char *ptr;
    ptr = (char *) &stat->bytes_read;
    int s = sizeof(stat->bytes_read);
    /*printf("bytes_read size: %d\n", s);
    for (int i = 0; i < s; i++) {
      printf("%02x ", ptr[i]);
    }
    printf("\n");*/
    reverse(ptr, s);

    // bytes written
    ptr = (char *) &stat->bytes_written;
    s = sizeof(stat->bytes_written);
    reverse(ptr, s);

    // time
    ptr = (char *) &stat->time;
    s = sizeof(stat->time);
    reverse(ptr, s);

    // get hits
    ptr = (char *) &stat->get_hits;
    s = sizeof(stat->get_hits);
    reverse(ptr, s);


    // get misses
    *ptr;
    ptr = (char *) &stat->get_misses;
    s = sizeof(stat->get_misses);
    reverse(ptr, s);