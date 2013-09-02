/*
 * net/sched/sch_blue.c	RBF-PID using Gradient descent method.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Junlong Qiao, <zheolong@126.com>
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <net/pkt_sched.h>
#include <net/inet_ecn.h>
#include "/root/AQM/blue/include/blue.h"
#include "/root/AQM/blue/include/blue_queue.h"
#include <asm/i387.h>   //to support the Floating Point Operation
#include <linux/rwsem.h>
#include<linux/kthread.h>
#include<linux/interrupt.h>
//#include "tp-blue-trace.h"//trace debug
//#include "tp-blue-vars-trace.h"//trace debug
//DEFINE_TRACE(blue_output);//trace debug
//DEFINE_TRACE(blue_vars_output);//trace debug
//#define TRACE_COUNT_MAX 10
//int trace_count;

//Queue Length Statistics
#define CYC_MAX 20
int cyc_count;

struct task_struct *tsk_prob;
struct task_struct *tsk_stop_prob;


#ifndef SLEEP_MILLI_SEC  
#define SLEEP_MILLI_SEC(nMilliSec)\
do {\
long timeout = (nMilliSec) * HZ / 1000;\
while(timeout > 0)\
{\
timeout = schedule_timeout(timeout);\
}\
}while(0);
#endif


/*	Parameters, settable by user:
	-----------------------------

	limit		- bytes (must be > qth_max + burst)

	Hard limit on queue length, should be chosen >qth_max
	to allow packet bursts. This parameter does not
	affect the algorithms behaviour and can be chosen
	arbitrarily high (well, less than ram size)
	Really, this limit will never be reached
	if RED works correctly.
 */

struct blue_sched_data {
	struct timer_list 	ptimer;	

	u32			limit;		/* HARD maximal queue length */
	unsigned char		flags;

	struct blue_parms	parms;
	struct blue_stats	stats;
	struct Qdisc		*qdisc;
};

struct queue_show queue_show_base_blue[QUEUE_SHOW_MAX];EXPORT_SYMBOL(queue_show_base_blue);
int array_element_blue = 0;EXPORT_SYMBOL(array_element_blue);

static void __inline__ blue_mark_probability(struct Qdisc *sch);


static inline int blue_use_ecn(struct blue_sched_data *q)
{
	return q->flags & TC_BLUE_ECN;
}

static inline int blue_use_harddrop(struct blue_sched_data *q)
{
	return q->flags & TC_BLUE_HARDDROP;
}

static int blue_enqueue(struct sk_buff *skb, struct Qdisc *sch)
{
	struct blue_sched_data *q = qdisc_priv(sch);
    struct blue_parms *parms = &q->parms;
	struct Qdisc *child = q->qdisc;
	int ret;

	switch (blue_action(&q->parms)) {
	case BLUE_DONT_MARK:
		
		//每CYC_MAX统计一次队列长度值
		//record queue length once every CYC_MAX
		cyc_count++;
		if(cyc_count==CYC_MAX){
			kernel_fpu_begin();
			queue_show_base_blue[array_element_blue].length=sch->q.qlen;
			queue_show_base_blue[array_element_blue].numbers=array_element_blue;
			queue_show_base_blue[array_element_blue].mark_type=BLUE_DONT_MARK;
			queue_show_base_blue[array_element_blue].p=*((long long *)(&parms->proba));
			queue_show_base_blue[array_element_blue].qold=parms->qold;
			queue_show_base_blue[array_element_blue].qcur=parms->qcur;
			queue_show_base_blue[array_element_blue].a=*((long long *)(&parms->a));
			queue_show_base_blue[array_element_blue].b=*((long long *)(&parms->b));

//--------------------------------------------------------------------------------------------
			//历史数据更新(很难保持数据同步，所以还是在入队列以后进行这个更新操作比较好)
			//每次包来的时候调用
			parms->qold = parms->qcur;
			parms->qcur = sch->q.qlen;
			kernel_fpu_end();

			if(array_element_blue < QUEUE_SHOW_MAX-1)  array_element_blue++;
			cyc_count = 0;
		}

		break;

	case BLUE_PROB_MARK:
		
		//每CYC_MAX统计一次队列长度值
		//record queue length once every CYC_MAX
		cyc_count++;
		if(cyc_count==CYC_MAX){
			kernel_fpu_begin();
			queue_show_base_blue[array_element_blue].length=sch->q.qlen;
			queue_show_base_blue[array_element_blue].numbers=array_element_blue;
			queue_show_base_blue[array_element_blue].mark_type=BLUE_PROB_MARK;
			queue_show_base_blue[array_element_blue].p=*((long long *)(&parms->proba));
			queue_show_base_blue[array_element_blue].qold=parms->qold;
			queue_show_base_blue[array_element_blue].qcur=parms->qcur;
			queue_show_base_blue[array_element_blue].a=*((long long *)(&parms->a));
			queue_show_base_blue[array_element_blue].b=*((long long *)(&parms->b));

//--------------------------------------------------------------------------------------------
			//历史数据更新(很难保持数据同步，所以还是在入队列以后进行这个更新操作比较好)
			//每次包来的时候调用
			parms->qold = parms->qcur;
			parms->qcur = sch->q.qlen;
			kernel_fpu_end();

			if(array_element_blue < QUEUE_SHOW_MAX-1)  array_element_blue++;
			cyc_count = 0;
		}

		sch->qstats.overlimits++;
		if (!blue_use_ecn(q) || !INET_ECN_set_ce(skb)) {
			q->stats.prob_drop++;
			goto congestion_drop;
		}

		q->stats.prob_mark++;
		break;
	}

	ret = qdisc_enqueue(skb, child);

//--------------------------------------------------------------------------------------------
	if (likely(ret == NET_XMIT_SUCCESS)) {
		sch->q.qlen++;

		sch->qstats.backlog += skb->len;/*2012-1-21*/
		//sch->qstats.bytes += skb->len;/*2012-1-21*/
		//sch->qstats.packets++;/*2012-1-21*/

	} else if (net_xmit_drop_count(ret)) {
		q->stats.pdrop++;
		sch->qstats.drops++;
	}
	

	return ret;

congestion_drop:
	qdisc_drop(skb, sch);
	return NET_XMIT_CN;
}

static struct sk_buff *blue_dequeue(struct Qdisc *sch)
{
	struct sk_buff *skb;
	struct blue_sched_data *q = qdisc_priv(sch);
	struct Qdisc *child = q->qdisc;

	skb = child->dequeue(child);
	if (skb) {
		qdisc_bstats_update(sch, skb);

		sch->qstats.backlog -= skb->len;/*2012-1-21*/	

		sch->q.qlen--;
	} else {
	}
	return skb;
}

static struct sk_buff *blue_peek(struct Qdisc *sch)
{
	struct blue_sched_data *q = qdisc_priv(sch);
	struct Qdisc *child = q->qdisc;

	return child->ops->peek(child);
}

static unsigned int blue_drop(struct Qdisc *sch)
{
	struct blue_sched_data *q = qdisc_priv(sch);
	struct Qdisc *child = q->qdisc;
	unsigned int len;

	if (child->ops->drop && (len = child->ops->drop(child)) > 0) {

		sch->qstats.backlog -= len;/*2012-1-21*/					

		q->stats.other++;
		sch->qstats.drops++;
		sch->q.qlen--;
		return len;
	}

	return 0;
}

static void blue_reset(struct Qdisc *sch)
{
	struct blue_sched_data *q = qdisc_priv(sch);

	qdisc_reset(q->qdisc);

	sch->qstats.backlog = 0;/*2012-1-21*/	
	
	sch->q.qlen = 0;

	array_element_blue = 0;/*2012-1-21*/
	
	blue_restart(&q->parms);
}

static void blue_destroy(struct Qdisc *sch)
{
	struct blue_sched_data *q = qdisc_priv(sch);
	
	//删除计时器，并将输出数据需要的array_element_blue置为0
    array_element_blue = 0;
/*
	if(tsk_prob!=NULL)
		kthread_stop(tsk_prob);
	if(tsk_stop_prob!=NULL)
		kthread_stop(tsk_stop_prob);
		*/
	qdisc_destroy(q->qdisc);
}

static const struct nla_policy blue_policy[TCA_BLUE_MAX + 1] = {
	[TCA_BLUE_PARMS]	= { .len = sizeof(struct tc_blue_qopt) },
	[TCA_BLUE_STAB]	= { .len = BLUE_STAB_SIZE },
};

/*qjl
缩写的一些说明：
Qdisc    		Queue discipline
blue      		blue method
nlattr   		net link attributes
blue_sched_data   blue scheduler data
qdisc_priv           qdisc private（Qdisc中针对特定算法如BLUE的数据）
tca			traffic controll attributes
nla 			net link attributes
*/
static int blue_change(struct Qdisc *sch, struct nlattr *opt)
{
	struct blue_sched_data *q = qdisc_priv(sch);
	struct nlattr *tb[TCA_BLUE_MAX + 1];
	struct tc_blue_qopt *ctl;
	struct Qdisc *child = NULL;
	int err;

	if (opt == NULL)
		return -EINVAL;

	err = nla_parse_nested(tb, TCA_BLUE_MAX, opt, blue_policy);
	if (err < 0)
		return err;

	if (tb[TCA_BLUE_PARMS] == NULL ||
	    tb[TCA_BLUE_STAB] == NULL)
		return -EINVAL;
	
	/*求有效载荷的起始地址*/
	ctl = nla_data(tb[TCA_BLUE_PARMS]);

	if (ctl->limit > 0) {
		child = fifo_create_dflt(sch, &bfifo_qdisc_ops, ctl->limit);
		if (IS_ERR(child))
			return PTR_ERR(child);
	}

	sch_tree_lock(sch);
	q->flags = ctl->flags;
	q->limit = ctl->limit;
	if (child) {
		qdisc_tree_decrease_qlen(q->qdisc, q->qdisc->q.qlen);
		qdisc_destroy(q->qdisc);
		q->qdisc = child;
	}

	//设置算法参数，此函数是在blue.h中定义的
	blue_set_parms(&q->parms, ctl->sampl_period, 
								 ctl->q_ref, ctl->p_init, ctl->p_min, ctl->p_max, 
								 ctl->a, ctl->b, 
								 ctl->Scell_log, nla_data(tb[TCA_BLUE_STAB]));

	//利用trace debug内核代码
	//trace_blue_output(ctl);


	array_element_blue = 0;/*2012-1-21*/

	sch_tree_unlock(sch);

	return 0;
}

int blue_thread_func(void* data)
{
	struct Qdisc *sch=(struct Qdisc *)data;
	do{
		sch=(struct Qdisc *)data;
		blue_mark_probability(sch);
		//SLEEP_MILLI_SEC(5);
		schedule_timeout(10*HZ);
	}while(!kthread_should_stop());
	return 0;
}
int blue_stop_prob_thread_func(void* data)
{
	SLEEP_MILLI_SEC(50000);
	kthread_stop(tsk_prob);
	printk("<1>tsk prob stop");
	return 0;
}

static void __inline__ blue_mark_probability(struct Qdisc *sch)
{
    struct blue_sched_data *data = qdisc_priv(sch);
    struct blue_parms *parms = &data->parms;

	//trace debug
	//struct trace_blue_parms trace_parms;

	kernel_fpu_begin();
//--------------------------BLUE算法丢弃/标记概率更新过程------------------------
	/*p=a*(len-ref)-b*(old-ref)+p，就是下面这句代码的简写*/
    parms->proba = parms->proba + parms->a*(double)(parms->qcur-parms->q_ref)/6500.00 - parms->b*(double)(parms->qold-parms->q_ref)/6500.00;
    if (parms->proba < parms->p_min)
         parms->proba = parms->p_min;
    if (parms->proba > parms->p_max)
         parms->proba = parms->p_max;
	printk(KERN_INFO "%lld\n",*((long long *)&(parms->proba)));
//--------------------------------------------------------------------------------
	kernel_fpu_end();
}


static int blue_init(struct Qdisc *sch, struct nlattr *opt)
{
	struct blue_sched_data *q = qdisc_priv(sch);
	int ret;

	array_element_blue = 0;/*2012-1-21*/

	cyc_count = 0;

	q->qdisc = &noop_qdisc;
	ret=blue_change(sch, opt);

	tsk_prob=kthread_run(blue_thread_func,(void*)sch,"blue");
	tsk_stop_prob=kthread_run(blue_stop_prob_thread_func,(void*)sch,"blue_stop_prob");

	return ret;
}

static int blue_dump(struct Qdisc *sch, struct sk_buff *skb)
{
	struct blue_sched_data *q = qdisc_priv(sch);
	struct nlattr *opts = NULL;
	struct tc_blue_qopt opt = {
		.limit		= q->limit,
		.sampl_period	= q->parms.sampl_period,
		.q_ref		= q->parms.q_ref,
		.p_max		= q->parms.p_max,
		.p_min		= q->parms.p_min,
		.p_init		= q->parms.p_init,
		.a		= q->parms.a,
		.b		= q->parms.b,
	};

	sch->qstats.backlog = q->qdisc->qstats.backlog;
	opts = nla_nest_start(skb, TCA_OPTIONS);
	if (opts == NULL)
		goto nla_put_failure;
	NLA_PUT(skb, TCA_BLUE_PARMS, sizeof(opt), &opt);
	return nla_nest_end(skb, opts);

nla_put_failure:
	nla_nest_cancel(skb, opts);
	return -EMSGSIZE;
}

static int blue_dump_stats(struct Qdisc *sch, struct gnet_dump *d)
{
	struct blue_sched_data *q = qdisc_priv(sch);
	struct tc_blue_xstats st = {
		.early	= q->stats.prob_drop + q->stats.forced_drop,
		.pdrop	= q->stats.pdrop,
		.other	= q->stats.other,
		.marked	= q->stats.prob_mark + q->stats.forced_mark,
	};

	return gnet_stats_copy_app(d, &st, sizeof(st));
}

static int blue_dump_class(struct Qdisc *sch, unsigned long cl,
			  struct sk_buff *skb, struct tcmsg *tcm)
{
	struct blue_sched_data *q = qdisc_priv(sch);

	tcm->tcm_handle |= TC_H_MIN(1);
	tcm->tcm_info = q->qdisc->handle;
	return 0;
}

static int blue_graft(struct Qdisc *sch, unsigned long arg, struct Qdisc *new,
		     struct Qdisc **old)
{
	struct blue_sched_data *q = qdisc_priv(sch);

	if (new == NULL)
		new = &noop_qdisc;

	sch_tree_lock(sch);
	*old = q->qdisc;
	q->qdisc = new;
	qdisc_tree_decrease_qlen(*old, (*old)->q.qlen);
	qdisc_reset(*old);
	sch_tree_unlock(sch);
	return 0;
}

static struct Qdisc *blue_leaf(struct Qdisc *sch, unsigned long arg)
{
	struct blue_sched_data *q = qdisc_priv(sch);
	return q->qdisc;
}

static unsigned long blue_get(struct Qdisc *sch, u32 classid)
{
	return 1;
}

static void blue_put(struct Qdisc *sch, unsigned long arg)
{
}

static void blue_walk(struct Qdisc *sch, struct qdisc_walker *walker)
{
	if (!walker->stop) {
		if (walker->count >= walker->skip)
			if (walker->fn(sch, 1, walker) < 0) {
				walker->stop = 1;
				return;
			}
		walker->count++;
	}
}

static const struct Qdisc_class_ops blue_class_ops = {
	.graft		=	blue_graft,
	.leaf		=	blue_leaf,
	.get		=	blue_get,
	.put		=	blue_put,
	.walk		=	blue_walk,
	.dump		=	blue_dump_class,
};

static struct Qdisc_ops blue_qdisc_ops __read_mostly = {
	.id		=	"blue",
	.priv_size	=	sizeof(struct blue_sched_data),
	.cl_ops	=	&blue_class_ops,
	.enqueue	=	blue_enqueue,
	.dequeue	=	blue_dequeue,
	.peek		=	blue_peek,
	.drop		=	blue_drop,
	.init		=	blue_init,
	.reset		=	blue_reset,
	.destroy	=	blue_destroy,
	.change	=	blue_change,
	.dump		=	blue_dump,
	.dump_stats	=	blue_dump_stats,
	.owner		=	THIS_MODULE,
};

static int __init blue_module_init(void)
{
	return register_qdisc(&blue_qdisc_ops);
}

static void __exit blue_module_exit(void)
{
	unregister_qdisc(&blue_qdisc_ops);
}

module_init(blue_module_init)
module_exit(blue_module_exit)

MODULE_LICENSE("GPL");
