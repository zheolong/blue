#ifndef __KERNEL__
#define __KERNEL__
#endif

#ifndef MODULE
#define MODULE
#endif

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "/root/AQM/blue/include/blue_queue.h"
#include "/root/AQM/blue/include/blue.h"
#include <linux/moduleparam.h>

static struct proc_dir_entry *entry_queuedata;
static int queue_array_count = 0;
static int count=1;

static void *l_start(struct seq_file *m, loff_t * pos)
{
	int i;
        loff_t index = *pos;
	 struct queue_show * l_s;
        if (index == 0) {
	 	if (queue_array_count == 0)
	 	{
	 		seq_printf(m, "Show queue length:\n"
                                "%s\t%s\t%s\t%s\t\n", "numbers", 
				   "queue_length","mark_type","probability");
		}                
		if (array_element_blue > queue_array_count)
		{
			count=1;
			l_s = (struct queue_show *)queue_show_base_blue;
			for (i=1; i<=queue_array_count; i++)
			{
				++l_s;
			}
			return l_s;
			//return (rto_show_base + rto_array_count * sizeof(struct rto_show));
		}else
			return NULL;            
        }
        else
                return NULL;
}

static void *l_next(struct seq_file *m, void *p, loff_t * pos)
{
        struct queue_show * l = (struct queue_show *)p;
		++queue_array_count;
		++count;
		if ((*pos != 0) && ((queue_array_count >= array_element_blue) || (count>40))) {
                return NULL;
        }
		++l;
		//l += sizeof(struct rto_show);
        ++*pos;
        return l;
}

static void l_stop(struct seq_file *m, void *p)
{
}

static int l_show(struct seq_file *m, void *p)
{
        struct queue_show * s = (struct queue_show *)p;

        seq_printf(m, "%u\t%u\t%u\t%lld\t%u\t%u\t%lld\t%lld\t", 
					s->numbers, s->length, s->mark_type, *((long long *)&s->p), s->qold, s->qcur, *((long long *)&s->a), *((long long *)&s->b));
		seq_printf(m, "\n");

        return 0;
}

static struct seq_operations queuedata_seq_op = {
        .start = l_start,
        .next  = l_next,
        .stop  = l_stop,
        .show  = l_show
};

static int queuedata_seq_open(struct inode *inode, struct file *file)
{
        return seq_open(file, &queuedata_seq_op);
}

static struct file_operations queuedata_seq_fops = {
        .open = queuedata_seq_open,
        .read = seq_read,
        .llseek = seq_lseek,
        .release = seq_release,
};

int init_module(void)
{
        entry_queuedata = create_proc_entry("data_seq_file", 0, NULL);
        if (entry_queuedata)
              entry_queuedata->proc_fops = &queuedata_seq_fops;

        return 0;
}

void cleanup_module(void)
{
        remove_proc_entry("data_seq_file", NULL);
}

/*因为2.4和2.6内核的不同，所以这里的模块参数传递有所改变
在2.4内核中，用宏MODULE_PARM传递参数，在/include/linux/module.h中定义，"i"表示int类型，例如MODULE_PARM(queue_array_count,"i");
在2.6内核中，用函数module_param传递参数，在/include/linux/moduleparam.h中定义*/
module_param(queue_array_count,int,S_IRUGO|S_IWUSR);

MODULE_PARM_DESC(queue_array_count,"An read count!");

MODULE_DESCRIPTION("A  reading module");
MODULE_AUTHOR("Junlong Qiao");
MODULE_LICENSE("GPL");
