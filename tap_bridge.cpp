#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdio.h>

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <net/ethernet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <assert.h>
#include <netpacket/packet.h>


#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/ip.h>

#include <linux/if_tun.h>

#define MAX_PACKET_SIZE 4096 
#define RAMP_ETHERTYPE 0x8888
//#include <unistd.h>
//#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <pthread.h>

#include <map>




#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/if_tun.h>

#define max(a,b) ((a)>(b) ? (a):(b))

using namespace std;
map<uint64_t, int> addr;
int fd_list[64] = {-1};
int sock;
int num_threads;
struct sockaddr_ll myaddr;

int tun_alloc(char *dev)
{

    struct ifreq ifr;
    int fd, err;

    if( (fd = open("/dev/net/tun", O_RDWR)) < 0 )
        printf("open");
    memset(&ifr, 0, sizeof(ifr));

    /* Flags: IFF_TUN   - TUN device (no Ethernet headers) 
    *         IFF_TAP   - TAP device  
    * 
    *         IFF_NO_PI - Do not provide packet information  
    *                             */ 
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    if( *dev )
        strncpy(ifr.ifr_name, dev, IFNAMSIZ);

    if( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ){
        close(fd);
        printf("err");
        return err;
    }
    int tmp;
    if (tmp = ioctl(fd, TUNSETNOCSUM, 1) < 0) {
        printf("can't disable check sum %d\n", fd);
    }
    if(ioctl(fd, TUNSETOWNER, "peterfly") < 0){
        perror("TUNSETOWNER");
        exit(1);
    }
    if(ioctl(fd, TUNSETPERSIST, 1) < 0){
        perror("enabling TUNSETPERSIST");
        exit(1);
    }
    // strcpy(dev, ifr.ifr_name);
    return fd;
} 

int convert(char *packet, bool src) {
    int result = 0;
    int incre = src ? 6 : 0;
    result += packet[2 + incre] << 24;
    result += packet[3 + incre] << 16;
    result += packet[4 + incre] << 8;
    result += packet[5 + incre];
    return result;
}



void forwarding() {
    map<int, int> addr;

    char buf[MAX_PACKET_SIZE];
    int fd_list[64] = {-1};
    int fm = 0, l;
    fd_set fds;

    for (int i = 0; i < num_threads; i++) {
        sprintf(buf, "tap%d", i);
        fd_list[i] = tun_alloc(buf);
        int tmp = ioctl(fd_list[i], TUNSETNOCSUM, 1); 
        if (tmp < 0 ) {
            printf("err when diabling %s %d\n", buf, fd_list[i]);
        }
        if (fd_list[i] > fm) {
            fm = fd_list[i];
        }
    }
    fm++;

    while(1){
        FD_ZERO(&fds);
        for (int i = 0; i < num_threads; i++) {
            FD_SET(fd_list[i], &fds);
        }
        
        select(fm, &fds, NULL, NULL, NULL);

        for (int i = 0; i < num_threads; i++) {
            if (FD_ISSET(fd_list[i], &fds)) {
                l = read(fd_list[i], buf, sizeof(buf));
                // learn src mac addr 
                int key = convert(buf, true);
                if (addr.find(key) == addr.end()) {
                    addr[key] = fd_list[i];
                }
                key = convert(buf, false);
                if (addr.find(key) != addr.end()) {
                    write(addr[key], buf, l);
                } else {
                    // broadcast
                    for (int j = 0; j < num_threads; j++) {
                        if (j != i) {
                            write(fd_list[j], buf, l);
                        }
                    }
                }
            }
        }
    }

}

void *bridging(void *arg) {
    char buf[MAX_PACKET_SIZE];
    int ret;
    int l;
    long thread_id = (long) arg;
    int file_desc;
    bool is_eth = false;
    if (thread_id == num_threads) {
        file_desc = sock;   
        is_eth = true;     
    } else {
        file_desc = fd_list[thread_id];
    }
    while(1){
        if (is_eth) {
            ret = read(file_desc, buf, MAX_PACKET_SIZE);
            if (ret < 0) {
              perror("read() error");
              break;
            }
            uint64_t dest = *((uint64_t *)buf) & 0xffffffffffffUL;
            if (addr.find(dest) == addr.end()) {
                continue;
            }
            int dest_fd = addr[dest];
            // printf("the file desc is %d\n", dest_fd);
            if (dest_fd > 0) {
                write(dest_fd, buf, ret);
            }
        } else {
            l = read(file_desc, buf, sizeof(buf));
            // learn src mac addr
            // uint64_t tmp = *((uint64_t *)buf) & 0xffffffffffffUL;
            //      printf("send to %08lx %d\n", tmp, l);
            /*      for (int j=0;j<l;j++) 
                  printf("%x ", (unsigned int)((uint8_t)buf[j]));
                printf("\n");*/
            sendto(sock, buf, l, 0, (sockaddr*)&myaddr,sizeof(myaddr));
        }
    }
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    if(argc != 2 && argc != 3 && argc !=4) {
        printf("Usage: bridge num_threads [eth* if bridge mode] [tap_base]\n");
        exit(1);
    }

    num_threads = atoi(argv[1]);

    if (argc == 2) {
        printf("forwarding mode \n");
        forwarding();
    }
    printf("%s bridge mode\n", argv[2]);

    
    sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

    memset(&myaddr, 0, sizeof(myaddr));
    myaddr.sll_ifindex = if_nametoindex(argv[2]);
    myaddr.sll_family = AF_PACKET;
    int ret = bind(sock, (struct sockaddr *)&myaddr, sizeof(myaddr));
    if (ret <0) {
    perror("bind() error");
    exit(-1);   
    }
   
    char buf[MAX_PACKET_SIZE];

    int l;
    
     
    int tap_base = 0;
    if (argc == 4)
        tap_base = atoi(argv[3]);

    for (int i = 0; i < num_threads; i++) {
        sprintf(buf, "tap%d", i+tap_base);
        fd_list[i] = tun_alloc(buf);
        int tmp = ioctl(fd_list[i], TUNSETNOCSUM, 1); 
        if (tmp < 0 ) {
            printf("err when disabling %s %d\n", buf, fd_list[i]);
        }

        uint64_t mac = 0;
        struct ifreq ifrp;
        strcpy(ifrp.ifr_name, buf);    
        ret = ioctl(/*fd_list[i]*/sock, SIOCGIFHWADDR, (char *)&ifrp);  //this is a hack
        if (ret < 0) {
            perror("ioctl() error");
            exit(-1);
        }
        memcpy(&mac, &ifrp.ifr_ifru.ifru_hwaddr.sa_data, 6);
        for (int j = 0; j < 6; j++) {
            printf("%02x ", ((mac >> j*8) & 0xff));
        }
        
        printf(" mapped to tap%d\n", i+tap_base);
        addr[mac] = fd_list[i];
    }

    int total = num_threads + 1;
    pthread_t threads[total];
    for (long t = 0; t < total; t++) {
        ret = pthread_create(&threads[t], NULL, bridging, (void *)t);
    }
    pthread_join(threads[0], NULL);
}
