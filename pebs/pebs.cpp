#define _GNU_SOURCE
#include "pebs.h"
#define HIGHMARK 50
#define LOWMARK 5
#define INTERVAL 10000000000
#define MAXIMUM_MIGRATION 100
#define SOC1 24
#include <condition_variable>

using namespace std;

// DEFINITION
CPU_TID cputid[TIDNUM];
__u64 event[4];

// exclusive cpus for QEMU thread and PEBS threads
// SAMPLECPU 33, defined in pebs.h

struct perf_event_mmap_page *perf_page[TIDNUM][NPBUFTYPES];
int pfd[TIDNUM][NPBUFTYPES];
char filename[64];
pthread_t sample_thread_t;
std::mutex mutex1;
bool condition = false;
std::condition_variable cv;

static int analysis_first = 1;
static int analysis_finished = 0;
bool kernel_finished = true;
vector<__u64 > migrate;
vector<vector<__u64 >> split;
char buffer[40960];

FILE* fp;



void signal_handler(int signum)
{
	if (signum == SIGUSR1) {
			std::unique_lock<std::mutex> lk(mutex1);
			kernel_finished = true;
			printf("signal\n");
			lk.unlock();
			cv.notify_one();
	}
    // Perform necessary actions upon signal reception
    // Resume execution or handle received data from the kernel module
}


void init(const char* filename)
{
	// cputid should be already initialized inside main() ahead
	__u64  ts = time(NULL);
	// snprintf(filename, sizeof(filename), "profiling_%lu", ts);
	fp = fopen(filename, "w");
	if (!fp) {
		fprintf(stderr, "fopen file[%s] error!\n", filename);
		assert(fp != NULL);
	}
	for (int i=0;i<TIDNUM;++i) {
		for (int j=0;j<NPBUFTYPES;++j) {
			perf_page[i][j] = NULL;
			pfd[i][j] = -1;
		}
	}

	int ret = pfm_initialize();
	if (ret != PFM_SUCCESS) {
		fprintf(stderr, "Cannot initialize library: %s\n", 
			pfm_strerror(ret));
		assert(ret == PFM_SUCCESS);
	}

	// event[0] = _get_local_dram_read_attr();
	if(0>= START) event[0] = _get_read_attr();
	if (1>=START) event[1] = _get_remote_dram_read_attr();
	if(2>= START) event[2] = _get_local_PM_read_attr();
	if(3>= START) event[3] = _get_remote_PM_read_attr();
	// if(4>= START) event[4] = _get_write_attr();
	// printf("%lu\n", event[0]);
	// printf("%lu\n", event[1]);
	// printf("%lu\n", event[2]);
	// printf("%lu\n", event[3]);
	// printf("%lu\n", event[4]);
	perf_setup();
}

void perf_setup()
{
	// arrt1 - READ; attr2 - WRITE
	struct perf_event_attr attr[5];
	for (int i=0;i<TIDNUM;++i) {
		for (int j=START;j<NPBUFTYPES;j++){
			memset(&attr[j], 0, sizeof(struct perf_event_attr));
			attr[j].type = PERF_TYPE_RAW;
			attr[j].size = sizeof(struct perf_event_attr);
			attr[j].config = event[j];
			attr[j].config1 = 0;
			attr[j].sample_period = SAMPLE_PERIOD;
			attr[j].sample_type = PERF_SAMPLE_IP | PERF_SAMPLE_TID| PERF_SAMPLE_ADDR | PERF_SAMPLE_TIME;
			attr[j].disabled = 0;
			attr[j].exclude_kernel = 1;
			attr[j].exclude_hv = 1;
			attr[j].exclude_idle = 1;
			attr[j].exclude_callchain_kernel = 1;
			attr[j].exclude_callchain_user = 1;
			attr[j].precise_ip = 1;
			//
			pfd[i][j] = _perf_event_open(attr+j, cputid[i].tid/*-1*/, cputid[i].cpuid, -1, 0);
			if (pfd[i][j] == -1) {
				if (errno == ESRCH) fprintf(stderr, "No such process(nid=%d)\n", cputid[i].tid);
				assert(pfd[i][j] != -1);
			}
			perf_page[i][j] = _get_perf_page(pfd[i][j]);
			assert(perf_page[i][j] != NULL);
		}
	} // end of setup events for each TID
}



void *sample_thread_func(void *arg)
{
	int cancel_type = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	if (cancel_type)	fprintf(stderr, "thread cancel_type setting failed!\n");
	assert(cancel_type == 0);

	// set affinity
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(SAMPLECPU, &cpuset);
	pthread_t thread = pthread_self();
	int affinity_ret = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
	assert(affinity_ret==0); //if (affinity_ret)	fprintf(stderr, "pthread_setaffinity_np failed!\n");

	// set the sychronization signal handler
	signal(SIGUSR1, signal_handler);

	
	int switch_on = 0;
	__u64 addr;

	while (true) {
		for (int index=0;index<TIDNUM;++index) {
			for (int i=START;i<NPBUFTYPES;++i) {
				// printf("running\n");
				struct perf_event_mmap_page *p = perf_page[index][i];
				char *pbuf = (char *)p + p->data_offset;
				__sync_synchronize();
				// printf("sync\n");
				// printf("scan the buffer\n");
				if (p->data_head == p->data_tail){
				//	printf("continue\n");
					continue;
				}
				//while (p->data_head != p->data_tail) {
					struct perf_event_header *ph = 
						reinterpret_cast<struct perf_event_header*>(pbuf + (p->data_tail % p->data_size));
					assert(ph != NULL);
					struct perf_sample *ps;
					// printf("%d\n", ph->type);
					switch (ph->type) {
						case PERF_RECORD_SAMPLE:
							ps = (struct perf_sample*)ph; assert(ps != NULL);
							
							// Here should be a condition that we should start the analysis.
							if (ps->addr!=0 ){
							   fprintf(fp, "%llu %llu, %d %d\n", (void*)(ps->addr), ps->time, index, i);
							}
							break;
						case PERF_RECORD_THROTTLE:
							printf("PERF_RECORD_THROTTL\n");
							break;
						case PERF_RECORD_UNTHROTTLE: break;
						default: break;//fprintf(stderr, "Unknown perf_sample type %u\n", ph->type);
					} // got the perf sample
					p->data_tail += ph->size;
				//} // extract all the events in the ring buffer
			} // end of loop NPBUFTYPES
		} // end of loop for each sampled thread
	
	} // Repeated Sampling
	return NULL;
}

long _perf_event_open(struct perf_event_attr *hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags)
{
	int ret = syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
	return ret;
}

__u64 _get_read_attr()
{
	struct perf_event_attr attr;
	memset(&attr, 0, sizeof(attr));
	int ret = pfm_get_perf_event_encoding("MEM_LOAD_L3_MISS_RETIRED.LOCAL_DRAM", 
										PFM_PLMH, &attr, NULL, NULL);
	if (ret != PFM_SUCCESS) {
		fprintf(stderr, "Cannot get encoding %s\n", pfm_strerror(ret));
		assert(ret == PFM_SUCCESS);
	}
	// printf("%d\n", ret);

	return attr.config;
}

__u64 _get_write_attr()
{
	struct perf_event_attr attr;
	memset(&attr, 0, sizeof(attr));
	int ret = pfm_get_perf_event_encoding("MEM_INST_RETIRED.STLB_MISS_STORES",
										PFM_PLMH, &attr, NULL, NULL);
	if (ret != PFM_SUCCESS) {
		fprintf(stderr, "Cannot get encoding %s\n", pfm_strerror(ret));
		assert(ret == PFM_SUCCESS);
	}
	return attr.config;
}
 

__u64 _get_local_dram_read_attr(){

	struct perf_event_attr attr;
	memset(&attr, 0, sizeof(attr));
	// here we will change the evert name 
	int ret = pfm_get_perf_event_encoding("MEM_LOAD_L3_MISS_RETIRED.LOCAL_DRAM",
										PFM_PLMH, &attr, NULL, NULL);
	if (ret != PFM_SUCCESS) {
		fprintf(stderr, "Cannot get encoding %s\n", pfm_strerror(ret));
		assert(ret == PFM_SUCCESS);
	}
		printf("%d\n", ret);

	return attr.config;
}


 __u64 _get_remote_dram_read_attr(){

	struct perf_event_attr attr;
	memset(&attr, 0, sizeof(attr));
	// here we will change the evert name 
	int ret = pfm_get_perf_event_encoding("MEM_LOAD_L3_MISS_RETIRED.REMOTE_DRAM",
										PFM_PLMH, &attr, NULL, NULL);
	if (ret != PFM_SUCCESS) {
		fprintf(stderr, "Cannot get encoding %s\n", pfm_strerror(ret));
		assert(ret == PFM_SUCCESS);
	}
		// printf("%d\n", ret);

	return attr.config;

}

__u64 _get_local_PM_read_attr(){

	struct perf_event_attr attr;
	memset(&attr, 0, sizeof(attr));
	// here we will change the evert name 
	int ret = pfm_get_perf_event_encoding("MEM_LOAD_RETIRED.LOCAL_PMM",
										PFM_PLMH, &attr, NULL, NULL);
	if (ret != PFM_SUCCESS) {
		fprintf(stderr, "Cannot get encoding %s\n", pfm_strerror(ret));
		assert(ret == PFM_SUCCESS);
	}
		// printf("%d\n", ret);
	return attr.config;
}


__u64 _get_remote_PM_read_attr(){
	
	struct perf_event_attr attr;
	memset(&attr, 0, sizeof(attr));
	// here we will change the evert name 
	int ret = pfm_get_perf_event_encoding("MEM_LOAD_L3_MISS_RETIRED.REMOTE_PMM",
										PFM_PLMH, &attr, NULL, NULL);
	if (ret != PFM_SUCCESS) {
		fprintf(stderr, "Cannot get encoding %s\n", pfm_strerror(ret));
		assert(ret == PFM_SUCCESS);
	}
		// printf("%d\n", ret);
	return attr.config;

}


struct perf_event_mmap_page *_get_perf_page(int pfd)
{
	// for this config; they map 4KB * PERF_PAGES. ()
	size_t mmap_size = sysconf(_SC_PAGESIZE) * PERF_PAGES;
	// printf("mmap_size %ld\n", mmap_size);
	struct perf_event_mmap_page *p =
		reinterpret_cast<struct perf_event_mmap_page *>(mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, 
																MAP_SHARED  , pfd, 0));

	if (p == MAP_FAILED) {
		fprintf(stderr, "mmap for pfd(%d) failed!\n", pfd);
		assert(p != MAP_FAILED);
	}
		// printf("%d\n", ret);

	return p;
}

void INThandler(int sig)
{
	signal(sig, SIG_IGN);
	int ret_cancel = pthread_cancel(sample_thread_t);
	if (ret_cancel)	fprintf(stderr, "pthread_cancel failed!\n");
	assert(ret_cancel==0);
	// Do cleaning
	for (int i=0;i<TIDNUM;++i) {
		for (int j=0;j<NPBUFTYPES;++j) {
			if (perf_page[i][j]) {
				munmap(perf_page[i][j], sysconf(_SC_PAGESIZE)*PERF_PAGES);
				perf_page[i][j] = NULL;
			}
			if (pfd[i][j] != -1) {
				ioctl(pfd[i][j], PERF_EVENT_IOC_DISABLE, 0);
				close(pfd[i][j]);
				pfd[i][j] = -1;
			}
		}
	}
	// fclose(fp);
	// fp = NULL;
}
