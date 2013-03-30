/*
 ** ramp_disk - RAMP Gold disk driver
 ** 
 ** Copyright (C) 2011 by Zhangxi Tan (xtan@cs.berkeley.edu)
 **
 */

#define DEVICE_NAME "RAMPDISK"

#include <linux/major.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/bitops.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/interrupt.h>
#include <linux/of_device.h>

#include <asm/setup.h>
#include <asm/pgtable.h>
#include <linux/spinlock.h>

static struct rampdisk_device {
    unsigned long size;
    spinlock_t lock;
    struct gendisk *gd;
} Device;

#ifdef CONFIG_SECOND_DISK
static struct rampdisk_device Device1;
#endif


struct rampdisk_buf_t {
    char read[512];
    char write[512];
}*appserver_buf;

static int major_num = 0;
module_param(major_num, int, 0);
static int logical_block_size = 512;
module_param(logical_block_size, int, 0);
static int nsectors = 1024*2*3000; /* How big the drive is (now set to 3G) */
module_param(nsectors, int, 0);

#define TOHOST_DIAB     0x81
#define RAMP_DISK_READ  0x01
#define RAMP_DISK_WRITE 0x02

//#define MAGIC_MEM_ADDR  0xf0000a60
#define MAGIC_MEM_ADDR 0xf0000a40

volatile int *magic_mem;

DEFINE_SPINLOCK(magic_mem_lock_irq);
EXPORT_SYMBOL(magic_mem_lock_irq);
/*
   static void print_buffer(char *buf) 
   {
   int i;
   for (i=0;i<logical_block_size;i++)
   printk("%02x ",(unsigned int)*buf++);
   printk("\n");
   }
   */
#ifdef CONFIG_SECOND_DISK
static void do_rampdisk_request1(struct request_queue *q)
{
    struct request *req;
    unsigned long start, nsect, i/*,j*/;
    int err;

    req = blk_fetch_request(q);
    while (req) {
        if (req == NULL || (req->cmd_type != REQ_TYPE_FS)) {
            printk (KERN_NOTICE "Skip non-CMD request\n");
            __blk_end_request_all(req, -EIO);
            continue;
        }

        start = blk_rq_pos(req) * logical_block_size;
        nsect  = blk_rq_cur_sectors(req);
        err = 0;

        if (start + nsect * logical_block_size > Device.size) {
            pr_err(DEVICE_NAME ": bad access: block=%llu, "
                    "count=%u\n",
                    (unsigned long long)blk_rq_pos(req),
                    blk_rq_cur_sectors(req));
            err = -EIO;
            goto done;
        }
        i = 0;
        if (rq_data_dir(req) == READ) {

            while (nsect > i) {
                //get data from the appserver
                spin_lock(&magic_mem_lock);
                magic_mem[7] = 0;
                magic_mem[1] = RAMP_DISK_READ;
                magic_mem[2] = start + i * logical_block_size;   //disk address
                magic_mem[3] = __pa(appserver_buf->read);    //memory address
                magic_mem[4] = 1;
                magic_mem[5] = 0;
                asm volatile("mov %%asr15,%0" : "=r"(magic_mem[6]): : "memory");
                magic_mem[0] = TOHOST_DIAB;   //TOHOST

                while (magic_mem[7] == 0) ;

                //             printk("Disk address = %x,phys addr=%x, virt addr=%x\nData: ", start+ i * logical_block_size, (unsigned int)magic_mem[3], appserver_buf->read);
                //             print_buffer(appserver_buf->read);
                //ret = magic_mem[1];
                spin_unlock(&magic_mem_lock);

                /*             chksum = 0;psum = (uint32_t*)appserver_buf->read;
                               for (j=0;j<logical_block_size/4;j++) 
                               chksum += *psum++;

                               printk("[rbd]%08x\n", chksum);*/
                memcpy(req->buffer+i*logical_block_size, appserver_buf->read, logical_block_size);
                i++;
            }
        }
        else {
            while (nsect > i) {
                memcpy(appserver_buf->write, req->buffer+i*logical_block_size, logical_block_size);
                //put data to appserver     
                spin_lock(&magic_mem_lock);
                magic_mem[7] = 0;
                magic_mem[1] = RAMP_DISK_WRITE;
                magic_mem[2] = start + i * logical_block_size;      //disk address
                magic_mem[3] = __pa(appserver_buf->write);  //memory address
                magic_mem[4] = 1;
                magic_mem[5] = 0;
                asm volatile("mov %%asr15,%0" : "=r"(magic_mem[6]) : : "memory");    
                magic_mem[0] = TOHOST_DIAB;   //TOHOST

                while (magic_mem[7] == 0) ;
                //ret = magic_mem[1];
                spin_unlock(&magic_mem_lock);
                i++;
            }

        }
done:
        if (!__blk_end_request_cur(req, err))
            req = blk_fetch_request(q);
    }
}
#endif

static void do_rampdisk_request(struct request_queue *q)
{
    struct request *req;
    unsigned long start, nsect, i;
    int err;
#ifdef CONFIG_DISK_CHKSUM
    unsigned long j;
    uint32_t chksum, *psum;
#endif

    req = blk_fetch_request(q);
    while (req) {
        if (req == NULL || (req->cmd_type != REQ_TYPE_FS)) {
            printk (KERN_NOTICE "Skip non-CMD request\n");
            __blk_end_request_all(req, -EIO);
            continue;
        }

        start = blk_rq_pos(req) * logical_block_size;
        nsect  = blk_rq_cur_sectors(req);
        err = 0;

        if (start + nsect * logical_block_size > Device.size) {
            pr_err(DEVICE_NAME ": bad access: block=%llu, "
                    "count=%u\n",
                    (unsigned long long)blk_rq_pos(req),
                    blk_rq_cur_sectors(req));
            err = -EIO;
            goto done;
        }
        i = 0;
        if (rq_data_dir(req) == READ) {

            while (nsect > i) {
                //get data from the appserver
#ifdef CONFIG_DISK_CHKSUM
retry:
#endif
                spin_lock(&magic_mem_lock);
                magic_mem[7] = 0;
                magic_mem[1] = RAMP_DISK_READ;
                magic_mem[2] = start + i * logical_block_size;  //disk address
                magic_mem[3] = __pa(appserver_buf->read);   //memory address
                magic_mem[4] = 0;
                magic_mem[5] = 0;
                asm volatile("mov %%asr15,%0" : "=r"(magic_mem[6]): : "memory");
                magic_mem[0] = TOHOST_DIAB;   //TOHOST

                while (magic_mem[7] == 0) ;

                //             printk("Disk address = %x,phys addr=%x, virt addr=%x\nData: ", start+ i * logical_block_size, (unsigned int)magic_mem[3], appserver_buf->read);
                //             print_buffer(appserver_buf->read);
                //ret = magic_mem[1];
                spin_unlock(&magic_mem_lock);

#ifdef CONFIG_DISK_CHKSUM
                chksum = 0;psum = (uint32_t*)appserver_buf->read;
                for (j=0;j<logical_block_size/4;j++) 
                    chksum += *psum++;

                if (chksum != (uint32_t) magic_mem[5]) {
                    printk("ERROR: checksum not match while reading at disk address %x \n", magic_mem[2]);
                    printk("Checksum is said to be %x, but it's %x \n", (uint32_t) magic_mem[5], chksum);
                    printk("Retrying to retrieve the same block.\n");
                    goto retry;
                    // panic("checksum error");
                }
                //if (magic_mem[2] == 0 && chksum!= (uint32_t) magic_mem[5])
                //    printk("**for add0: Checksum is said to be %x, but it's %x \n", (uint32_t) magic_mem[5], chksum);
#endif



                memcpy(req->buffer+i*logical_block_size, appserver_buf->read, logical_block_size);
                i++;
            }
        }
        else {
            while (nsect > i) {
                memcpy(appserver_buf->write, req->buffer+i*logical_block_size, logical_block_size);

#ifdef CONFIG_DISK_CHKSUM
                chksum = 0;psum = (uint32_t*)appserver_buf->write;
                for (j=0;j<logical_block_size/4;j++) 
                    chksum += *psum++;
#endif
                //put data to appserver     
                spin_lock(&magic_mem_lock);
                magic_mem[7] = 0;
                magic_mem[1] = RAMP_DISK_WRITE;
                magic_mem[2] = start + i * logical_block_size;      //disk address
                magic_mem[3] = __pa(appserver_buf->write);  //memory address
                magic_mem[4] = 0;
#ifdef CONFIG_DISK_CHKSUM
                magic_mem[5] = chksum;
#else
                magic_mem[5] = 0;
#endif
                asm volatile("mov %%asr15,%0" : "=r"(magic_mem[6]) : : "memory");   
                magic_mem[0] = TOHOST_DIAB;   //TOHOST

                while (magic_mem[7] == 0) ;
                //ret = magic_mem[1];
                spin_unlock(&magic_mem_lock);
                i++;
            }

        }
done:
        if (!__blk_end_request_cur(req, err))
            req = blk_fetch_request(q);
    }
}


static const struct block_device_operations rampdisk_fops =
{
    .owner          = THIS_MODULE
};


static struct request_queue *rampdisk_queue;

#ifdef CONFIG_SECOND_DISK
static struct request_queue *rampdisk_queue1;
#endif

    static int __init 
rampdisk_init(void)
{
    int ret, error;

    struct device_node *rootnp, *np;
    struct property *pp;
    int len;
    rootnp = of_find_node_by_path("/"); 

    if (rootnp) {   
        np = of_find_node_by_name(rootnp, "ramp_cpu");
        if (np) {
            pp = of_find_property(np, "magic_mem", &len);
            if (pp) 
                magic_mem = (volatile int *)(*((unsigned int*)(pp->value)));        
        }
    }

    /*
     * Set up our internal device.
     */
    ret = -ENOMEM;   
    Device.size = nsectors * logical_block_size;

    spin_lock_init(&Device.lock);

    appserver_buf = (struct rampdisk_buf_t*)__get_free_page(GFP_KERNEL);
    if (appserver_buf == NULL) 
        goto err;

    error = register_blkdev(major_num, "rbd");

#ifdef CONFIG_SECOND_DISK
    Device1.size = Device.size;
    Device1.lock = Device.lock;
    if (error <= 0 || register_blkdev(2, "rbdm")) {
        printk(KERN_WARNING "RAMP disk: unable to get major number, code=%d\n", error);
        goto err;
    }
#endif

    Device.gd = alloc_disk(1);

    if (!Device.gd)
        goto out_disk;

    rampdisk_queue = blk_init_queue(do_rampdisk_request, &Device.lock);
    if (!rampdisk_queue)
        goto out_queue;
    blk_queue_logical_block_size(rampdisk_queue, logical_block_size);

    Device.gd->major = major_num;
    Device.gd->first_minor = 0;
    Device.gd->fops = &rampdisk_fops;
    sprintf(Device.gd->disk_name, "rbd0");
    set_capacity(Device.gd, nsectors);
    Device.gd->queue = rampdisk_queue;
    add_disk(Device.gd);

#ifdef CONFIG_SECOND_DISK
    Device1.gd = alloc_disk(1);
    rampdisk_queue1 = blk_init_queue(do_rampdisk_request1, &Device1.lock);
    blk_queue_logical_block_size(rampdisk_queue1, logical_block_size);
    Device1.gd->major = 2;
    Device1.gd->first_minor = 0;
    Device1.gd->fops = &rampdisk_fops;
    sprintf(Device1.gd->disk_name, "rbdm0");
    set_capacity(Device1.gd, nsectors);
    Device1.gd->queue = rampdisk_queue1;
    add_disk(Device1.gd);
    printk("Second RAMP disk is registered at /dev/rdbm0 \n");
#endif

    //  magic_mem = (volatile int *)MAGIC_MEM_ADDR;
    printk("RAMP disk is registered at /dev/rbd @%x\n", (unsigned int)magic_mem);
    return 0;

out_queue:
    put_disk(Device.gd);
out_disk:
    unregister_blkdev(major_num, "rbd");
err:
    free_page((unsigned long)appserver_buf);
    return ret;
}

static void __exit rampdisk_exit(void)
{
    unregister_blkdev(major_num, "rbd");
    del_gendisk(Device.gd);
    put_disk(Device.gd);
    blk_cleanup_queue(rampdisk_queue);
    free_page((unsigned long)appserver_buf);
} 

module_init(rampdisk_init);
module_exit(rampdisk_exit);
MODULE_LICENSE("BSD");
