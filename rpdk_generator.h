#ifndef __RPDK_GENERATOR_H
#define __RPDK_GENERATOR_H

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <cstdlib>
#include <vector>

#include "Common/properties.h"

#define PROP_FILE_BASIC "rpdk_generator_basic.conf"
#define RAMP_DISK_ORG "ramp_disk.img"
#define RCS_ORG "rcs_original"
#define STRSIZE 200
using namespace std;

class rpdk_generator_t
{
public:
    int  c_cid,c_pid,c_tid;
    int  max_cid, max_pid, max_tid;
    int  linux_total;
    int  num_loop;
    char filename[STRSIZE];
    char cid_prefix[STRSIZE];
    char mnt_dir[STRSIZE];
    // first vector: vector of (vector of program)
    // vector of program string in conf file
    vector<string> prog_table;
    // vector of position containing the index of program-string vector
    // 1:[0,1,2]
    // 2:[1]
    vector< vector<int> > cpt_vector;

public:
    properties_t properties_basic;

public:
    rpdk_generator_t();
    ~rpdk_generator_t();        

    int load_program(int argc, char* argv[]);
    void get_cpt_id();
    void run();
        void main_load();
};// rpdk_generator_t

typedef struct{
        rpdk_generator_t * prpdk;
        int loop_num;
}rpdk_thread_arg_t;

#endif
