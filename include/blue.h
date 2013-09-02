/*qjl*/
#ifndef __NET_SCHED_BLUE_H
#define __NET_SCHED_BLUE_H

#include <linux/types.h>
#include <net/pkt_sched.h>
#include <net/inet_ecn.h>
#include <net/dsfield.h>
#include <asm/i387.h>   //为了支持浮点运算
/* The largest number rand will return (same as INT_MAX).  */
#define	RAND_MAX	2147483647

/*	BLUE algorithm.
	=======================================

*/
#define BLUE_STAB_SIZE	256
#define BLUE_STAB_MASK	(BLUE_STAB_SIZE - 1)

struct blue_stats {
	u32		prob_drop;	/* Early probability drops */
	u32		prob_mark;	/* Early probability marks */
	u32		forced_drop;	/* Forced drops, qavg > max_thresh */
	u32		forced_mark;	/* Forced marks, qavg > max_thresh */
	u32		pdrop;          /* Drops due to queue limits */
	u32		other;          /* Drops due to drop() calls */
};

struct blue_parms {
	/* Parameters */
	int 	sampl_period;   //采样时间
	//队列控制
	int q_ref;//参考队列长度
	double p_init;//初始丢弃概率
	double p_min;//最小丢弃概率
	double p_max;//最大丢弃概率
	//BLUE
	double a;//
	double b;//
		
	/* Variables */
	//队列控制
	double 	proba;	/* Packet marking probability */
	//BLUE
	int 	qcount;	/* Number of packets since last random*/
	int 	qold; /* last queue_len */
	int 	qcur; /* current queue_len*/

	u32		Scell_max;
	u8		Scell_log;
	u8		Stab[BLUE_STAB_SIZE];
};

static inline void blue_set_parms(struct blue_parms *p, int sampl_period, 
                             int q_ref, double p_init, double p_min, double p_max, 
                             double a, double b,
				 u8 Scell_log, u8 *stab)
{
	/* Reset average queue length, the value is strictly bound
	 * to the parameters below, reseting hurts a bit but leaving
	 * it might result in an unreasonable qavg for a while. --TGR
	 */
	p->sampl_period	= sampl_period;

	/* Parameters */
	p->q_ref		= q_ref;	// 参考队列长度 设置为300
	p->p_init		= p_init;	// 初始丢弃/标记概率  设置为0
	p->p_min		= p_min;	// 最小丢弃/标记概率  设置为0
	p->p_max		= p_max;	// 最大丢弃/标记概率  设置为1

	p->a		= a;	// 
	p->b		= b;	//

	/* Variables */
	p->proba = p_init;	/* Packet marking probability */
	p->qcount = 0;	/* Number of packets since last random*/
	p->qold = 0;
	p->qcur = 0;

	p->Scell_log	= Scell_log;
	p->Scell_max	= (255 << Scell_log);

	memcpy(p->Stab, stab, sizeof(p->Stab));
}

static inline void blue_restart(struct blue_parms *p)
{
	//待定？？？？
}


/*-------------------------------------------------*/

enum {
	BLUE_BELOW_PROB,
	BLUE_ABOVE_PROB,
};

static inline int blue_cmp_prob(struct blue_parms *p)
{
	int p_random,current_p;
	p_random = abs(net_random());

	//p->p_k will be written by another thread, so when reading it's value in a diffent thread, "current_p_sem" should be set
	kernel_fpu_begin();
	current_p = (int)(p->proba*RAND_MAX);
	kernel_fpu_end();
		
	if ( p_random < current_p){
		return BLUE_BELOW_PROB;
	}
	else{
		return BLUE_ABOVE_PROB;
	}
}

enum {
	BLUE_DONT_MARK,
	BLUE_PROB_MARK,
};

static inline int blue_action(struct blue_parms *p)
{
	switch (blue_cmp_prob(p)) {
		case BLUE_ABOVE_PROB:
			return BLUE_DONT_MARK;

		case BLUE_BELOW_PROB:
			return BLUE_PROB_MARK;
	}

	BUG();
	return BLUE_DONT_MARK;
}

#endif
