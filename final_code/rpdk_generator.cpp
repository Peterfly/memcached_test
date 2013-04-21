#include "rpdk_generator.h"
#include "Common/properties.h"

using namespace std;

int fexist(const char* filename);
void* generator_thread(void* args);

string trim( const string& s );

rpdk_generator_t::rpdk_generator_t()
{

}// rpdk_generator_t()

rpdk_generator_t::~rpdk_generator_t()
{

}// ~rpdk_generator_t()

int rpdk_generator_t::load_program(int argc, char* argv[])
{
    

    if(properties_basic.read_file(PROP_FILE_BASIC) < 0)
        {
                printf("Could open file : %s\n",PROP_FILE_BASIC);
                exit(1);
        }
        
        // filename : rpdk_generator_program.conf
        // program arguments file
        // prog1 0,0,0 arg1 arg2
        if(properties_basic.has_key("filename"))
                strcpy(filename, properties_basic.get_string("filename").c_str());

        // cid_prefix folders
        if(properties_basic.has_key("cid_prefix"))
                strcpy(cid_prefix, properties_basic.get_string("cid_prefix").c_str());
    
        if(properties_basic.has_key("mnt_dir"))
                strcpy(mnt_dir, properties_basic.get_string("mnt_dir").c_str());

        // number of loopback devices or threads
        if((num_loop=properties_basic.get_int("number_of_loop"))<0)
        {
                printf("Couldn't get loop back device number\n");
                exit(1);
        }
        
        if(!fexist(RAMP_DISK_ORG))
        {
                printf("File ramp_disk not found\n");
                exit(1);
        }

        // original rcS file
        if(!fexist(RCS_ORG))
        {
                printf("File rcs_original not found\n");
                exit(1);
        }
        // get max cid, pid, tid
        get_cpt_id();

        main_load();

        return 0;
}// load_program()

void rpdk_generator_t::get_cpt_id()
{
        // get max cid
    if((c_cid=properties_basic.get_int("c_cid")) < 0)
    {
        printf("Can not get cid\n");
        exit(1);
    }

        // get max pid
    if((c_pid=properties_basic.get_int("c_pid")) < 0)
    {
        printf("Can not get pid\n");
        exit(1);
    }

        // get max tid
    if((c_tid=properties_basic.get_int("c_tid")) < 0)
    {
        printf("Can not get tid\n");
        exit(1);
    }
    
     max_cid=c_cid+1;
     max_pid=c_pid+1;
     max_tid=c_tid+1;
     linux_total = (max_cid)*(max_pid)*(max_tid);
     cpt_vector.resize(linux_total);

}// get_cpt_id

void rpdk_generator_t::main_load()
{
    string line;
    int count;
    int ck;

    // open rpdk_generator_program.conf
    // program arguments file
    ifstream myfile (filename);
    if (!myfile.is_open())
    {
        cout << "Unable to open file " << filename << endl; 
        exit(1);
    }
  
    while ( myfile.good() )
    {
        // check
        ck=1;

        // read line by line
        getline (myfile,line);
        if( line.empty())
            continue;

        if (line[0] == '#')     //comment
            continue;

        line += " ";
        istringstream iss( line );
        string word;
        count=0;
        string arr[3];
        // arr[0] : program, arr[1] : range, arr[2] : program's arguments
        while (getline( iss, word, '\t' ) && word!=" ")
        {
            arr[count++] = trim(word);
        }

        // arr[0] program


        // arr[2] arguments
        
        // arr[1] 
        // range
        istringstream iss3(arr[1]);
        // get cid,pid,tid
        int cpt[6] = {-1};
        // cpt 
        // cid_start,cid_end,pid_start,pid_end,tid_start,tid_end
        for(int i=0; i<3; i++)
        {
                        if(getline(iss3,word,','))
                        {
                                if(word.empty())
                                        ck=0;
                                if (word[0] == '[')
                                {
                                        string range;   
                                        istringstream iss4(word);
                                        if(getline(iss4,range,'-'))
                                        {
                                                if(range.empty() || !isdigit(range[1]))
                                                        ck=0;
                                                else
                                                {
                                                        int str_len = range.length();
                                                        cpt[i*2]=atoi(range.substr(1,str_len).c_str());
                                                }
                                
                                        }
                                        if(getline(iss4,range,'-'))
                                        {
                                                if(range.empty() || !isdigit(range[0]))
                                                        ck=0;
                                                else
                                                {
                                                        int str_len = range.length();
                                                        cpt[i*2+1]=atoi(range.c_str());
                                                }
                                
                                        }
                                }// if [        
                                else
                                {
                                        if(!isdigit(word[0]))
                                                ck=0;
                                        cpt[i*2]=cpt[i*2+1]=atoi(word.c_str());
                        
                                }
                
                                if (cpt[i*2]> cpt[i*2+1])
                                        ck=0;
                }
       }// for

                //arr[0] program
                //arr[1] range
                //arr[2] arguments
                //cpt[0-5] = cid1,cid2,pid1,pid2,tid1,tid2

        if(ck)
        {
                        string temp_str;
                        // temp_str : 
                        // program arguments
                        temp_str = arr[0] + " " + arr[2];
                        int position;
                        
                        int temp_size = prog_table.size();
                        prog_table.push_back(temp_str);
       
//			printf("cpt[0]=%d cpt[1]=%d cpt[2]=%d, cpt[3]=%d, cpt[4]=%d, cpt[5]=%d\n",cpt[0],cpt[1],cpt[2],cpt[3],cpt[4],cpt[5]);
                        for(int n=cpt[0];n<=cpt[1];n++)
                        {
                                for( int t=cpt[2];t<=cpt[3];t++)
                                {
                                        for( int p=cpt[4];p<=cpt[5];p++)
                                        {
                                                position = n*max_pid*max_tid + t*max_tid + p;
                                                //cout << arr[0] << " " << arr[1] << " " << arr[2] << endl;
                                                //cout << "c : "<<n<<" p : "<<t<<" t : "<<p<< endl;
                                                //cout << "Position : " << position << " " << arr[1] << " " << temp_str << endl;
                                                if(position < linux_total)
                                                {
                                                        //cpt_vector[position].push_back(temp_str);
                                                        
                                                        cpt_vector[position].push_back(temp_size);

                                                }
                                        }// for p
                                }// for t
                        }// for n


                }// if ck
    }// for

    myfile.close();

}// main_load

void rpdk_generator_t::run()
{
        vector<rpdk_thread_arg_t> rpdk_arg(num_loop);
        vector<pthread_t> rpdk_thread(num_loop);

        char temp[STRSIZE];

        for(int i=0; i<max_cid; i++)
        {
                sprintf(temp, "mkdir %s%d",cid_prefix,i);
                if(system(temp)!=0)
                        goto syscall_error;
        }// for

        for(int i=0; i<num_loop; i++)
        {
                sprintf(temp,"cp ramp_disk.img ramp_disk_%d.img",i);
                if(system(temp)!=0)
                    goto syscall_error;
                
                sprintf(temp, "mkdir %sinitrd_%d",mnt_dir,i);
                if(system(temp)!=0)
                    goto syscall_error;
        }

        for(int i=0; i<num_loop;i++)
        {
                rpdk_arg[i].loop_num = i;
                rpdk_arg[i].prpdk = this;
                pthread_create(&rpdk_thread[i],NULL, generator_thread, &rpdk_arg[i]);
        }//for

        for(int i=0; i<num_loop;i++)
        {
                pthread_join(rpdk_thread[i],NULL);
        }

        return ;
syscall_error:
        printf("System call error\n");
        exit(1);        
}// run()

void* generator_thread(void* args)
{
        rpdk_thread_arg_t* arg = (rpdk_thread_arg_t*) args;
        int loop_dev = arg->loop_num;
        int total_linux = arg->prpdk->linux_total;
        int total_loop = arg->prpdk->num_loop;  
        int max_cid = arg->prpdk->max_cid;
        int max_pid = arg->prpdk->max_pid;
        int max_tid = arg->prpdk->max_tid;      

        int i;
        char temp[STRSIZE];

        for( i=0; (i*total_loop+loop_dev)<total_linux; i++)
        {
                int pos = i*total_loop+loop_dev;
                int cid = pos/(max_pid*max_tid);
                int pid = (pos - (cid*max_pid*max_tid))/max_tid;
                int tid = pos - (cid*max_pid*max_tid + max_tid*pid);
        
                cout <<"Thread : " << loop_dev << " Index: " << pos << endl;    
                
                sprintf(temp,"mount -o loop ramp_disk_%d.img %sinitrd_%d -text4",loop_dev,arg->prpdk->mnt_dir,loop_dev);
                while(system(temp)!=0)
                {
                        sleep(5);
                        cout << "System repeatedly mount \n";
                }       
                
                sprintf(temp,"cat rcs_original > /tmp/tmp_%d",loop_dev);
                if(system(temp)!=0)
                        goto syscall_error;
                
                sprintf(temp,"echo \"ifconfig eth0 10.%d.%d.%d netmask 255.255.255.0 up\" >> /tmp/tmp_%d",cid,pid,tid+1,loop_dev);
                if(system(temp)!=0)
                       goto syscall_error; 

                sprintf(temp,"echo \"busybox route add default gw 10.%d.%d.254\" >> /tmp/tmp_%d",cid,pid,loop_dev);
                if(system(temp)!=0)
                       goto syscall_error; 

                
/*              sprintf(temp,"echo \"ifconfig eth0\" >> /tmp/tmp_%d",loop_dev);
                if(system(temp)!=0)
                        goto syscall_error;*/
                
                // add new program
                if(!arg->prpdk->cpt_vector[pos].empty())
                {
                        for(int j=0; j<arg->prpdk->cpt_vector[pos].size();j++)
                        {
                                //sprintf(temp,"echo %s >> /tmp/tmp_%d",arg->prpdk->cpt_vector[pos][j].c_str(),loop_dev);
                                sprintf(temp,"echo %s >> /tmp/tmp_%d",arg->prpdk->prog_table[arg->prpdk->cpt_vector[pos][j]].c_str(),loop_dev);
                                //cout << temp << endl;
                                if(system(temp)!=0)
                                        goto syscall_error;
                        }
                }// if
                
                //replace the rcS
                sprintf(temp,"mv /tmp/tmp_%d %sinitrd_%d/etc/init.d/rcS",loop_dev,arg->prpdk->mnt_dir,loop_dev);
                if(system(temp)!=0)
                         goto syscall_error;

                //set the permission
                sprintf(temp,"chmod +x %sinitrd_%d/etc/init.d/rcS",arg->prpdk->mnt_dir,loop_dev);
                if(system(temp)!=0)
                         goto syscall_error;
                 
                sprintf(temp,"umount %sinitrd_%d",arg->prpdk->mnt_dir,loop_dev);
                while(system(temp)!=0)
                {
                        sleep(5);
                        cout << "System repeatedly umount\n";
                }

                // cp ramp_disk_*.img to cid_dir_*
                sprintf(temp,"cp ramp_disk_%d.img %s%d/ramp_disk_%d_%d.img",loop_dev,arg->prpdk->cid_prefix,cid,pid,tid);
                if(system(temp)!=0)
                        goto syscall_error;
        }// for

        //clean up the temporary file
        sprintf(temp, "rm ramp_disk_%d.img", loop_dev);
        if (system(temp)!=0)
                goto syscall_error;

        sprintf(temp,"rm -rf %sinitrd_%d",arg->prpdk->mnt_dir,loop_dev);
        system(temp);

        return 0;
syscall_error:
        sprintf(temp,"umount %sinitrd_%d",arg->prpdk->mnt_dir,loop_dev);
        system(temp);

        sprintf(temp,"rm -rf %sinitrd_%d",arg->prpdk->mnt_dir,loop_dev);
        system(temp);

        printf("loop%d System call error\n", loop_dev);
        exit(1); 
}// run

rpdk_generator_t rpdk_generator;
int main(int argc, char* argv[])
{
    int ret = rpdk_generator.load_program(argc, argv);
    if(ret) return ret;
    rpdk_generator.run();

    return 0;
}//main

string trim( const string& s )
{
    string result( s );
    result.erase( result.find_last_not_of( " " ) + 1 );
    result.erase( 0, result.find_first_not_of( " " ) );
    return result;
}

int fexist(const char* filename)
{
    struct stat buffer;
    if(stat(filename,&buffer)==0) 
        return 1;
    return 0;
}
