/*qjl*/

#define QUEUE_SHOW_MAX 30000

struct queue_show{
           int numbers;
           __u32 length;
		   int mark_type; //BLUE_DONT_MARK or BLUE_PROB_MARK
		   long long p;
		   long long a;
		   long long b;
		   __u32 qold;
		   __u32 qcur;
};
extern struct queue_show queue_show_base_blue[QUEUE_SHOW_MAX];
extern int array_element_blue;
extern struct queue_show queue_show_base1[30000];
extern int array_element1;
