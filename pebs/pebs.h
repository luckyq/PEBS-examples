#ifndef PEBS_H
#define PEBS_H

#include <stdio.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#include <asm/unistd.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <sys/mman.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <cstring>
#include <queue>
#include <map>
#include <set>
#include <perfmon/pfmlib.h>
#include <perfmon/pfmlib_perf_event.h>
#include <err.h>
#include <signal.h>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <mutex>
// using namespace std;

#define START 2

//#define PERF_PAGES 65
#define PERF_PAGES (1 + (1<<6))
#define SAMPLE_PERIOD 2000
#define TIDNUM 24
#define SAMPLECPU 30
#define ANALYSISCPU 31

// typedef unsigned long long __u64;

struct perf_sample {
	struct perf_event_header header;
	__u64 ip; /* if PERF_SAMPLE_IP */
	__u32 pid, tid; /* if PERF_SAMPLE_TID */
	__u64 time; /* if PERF_SAMPLE_TIME */
	__u64 addr; /* if PERF_SAMPLE_ADDR */
	//__u64 phys_addr; /* if PERF_SAMPLE_PHYS_ADDR */
};

enum pbuftype {
    L_D=0,
	R_PM=1,
	L_PM=2,
	
	// WRITE,
	NPBUFTYPES
};

typedef struct CPU_TID {
	int cpuid;
	int tid;
}CPU_TID;

extern CPU_TID cputid[TIDNUM];

extern void *sample_thread_func(void*);
extern pthread_t sample_thread_t;

extern __u64 event1, event2;
extern struct perf_event_mmap_page *perf_page[TIDNUM][NPBUFTYPES];
extern int pfd[TIDNUM][NPBUFTYPES];

extern char filename[64];
extern FILE *fp;
extern void signal_handler(int signum);

extern void INThandler(int);
extern void analysis_profiling_results(std::vector< __u64 > *migrate,
								std::vector<std::vector< __u64 >> *split, 
								std::set< __u64 > *s_s1,
								std::map< __u64, int> *soc1
								);
extern long _perf_event_open(struct perf_event_attr *hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags);
extern __u64 _get_read_attr();
extern __u64 _get_write_attr();
//local dram
extern __u64 _get_local_dram_read_attr();

//remote dram
extern __u64 _get_remote_dram_read_attr();

//local pm
extern __u64 _get_local_PM_read_attr();

//remote pm
extern __u64 _get_remote_PM_read_attr();

extern struct perf_event_mmap_page *_get_perf_page(int pfd);

extern void init(const char* filename, int spllit_t);
extern void perf_setup();

extern void *analysis_thread_func();

#endif
