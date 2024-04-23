#include "pebs.h"
#include <sys/resource.h>

int main(int argc, char **argv) {

	// if (argc != TIDNUM+1) {
	// 	fprintf(stderr, "Usage: ./pebs qemupid tid0 tid1 tid2 tid3\n The number of tid is %d\n", TIDNUM);
	// }

	// assert(argc == TIDNUM+1);
	// // exclusive cpus for vCPUs
	// int cpuid_map[TIDNUM] = {34, 35, 36, 37, 38};
	// int tids[TIDNUM];
	// for (int i=1;i<=TIDNUM;++i) {
	// 	tids[i-1] = atoi(argv[i]);
	// 	assert(tids[i-1] > 0);
	// 	assert(cpuid_map[i-1] > 0);
	// 	cputid[i-1].cpuid = cpuid_map[i-1];
	// 	cputid[i-1].tid = tids[i-1];
	// }

	if (argc != 3 ){
		printf("argv[1] is filename. argv[2] is the split \n");
	}


	for(int i=0;i<TIDNUM;i++){
		cputid[i].cpuid = i;
		cputid[i].tid = -1;
	}

	init(argv[1],atoi(argv[2]));
	signal(SIGINT, INThandler);
	// lauch the sampling threads
	
	int ret = pthread_create(&sample_thread_t, NULL, sample_thread_func, NULL);
	if (ret)	fprintf(stderr, "pthread_create failed!\n");
	assert(ret == 0);

	// Wait for sampling thread finish
	void *ret_thread;
	int join_ret = pthread_join(sample_thread_t, &ret_thread);
	if (join_ret)	fprintf(stderr, "pthread_join failed!\n");
	assert(join_ret==0);
	if (ret_thread != PTHREAD_CANCELED)	fprintf(stderr, "pthread_cancel failed!\n");
	assert(ret_thread == PTHREAD_CANCELED);
	
	return 0;
}
