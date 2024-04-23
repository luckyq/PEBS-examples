#define _GNU_SOURCE
#include "pebs.h"
#define HIGHMARK 200
#define LOWMARK 30
#define INTERVAL 5000000000
#define MAXIMUM_MIGRATION 500
#define SOC1 24
#include <condition_variable>

using namespace std;

// DEFINITION
CPU_TID cputid[TIDNUM];
__u64 event[5];
int split_enable ;

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

pthread_mutex_t mutex;

std::map<__u64  , int> soc1 ;
	std::map<__u64  , int> soc2 ;
	std::set<__u64 > s_s1 ;
	std::set<__u64 > s_s2 ;

	std::map<__u64  , int> soc1_cp ;
	std::map<__u64  , int> soc2_cp ;
	std::set<__u64 > s_s1_cp ;
	std::set<__u64 > s_s2_cp ;

struct thread_arg{
	std::map<__u64  , int>* soc1;
	std::map<__u64  , int>* soc2;

	std::set<__u64 >* s_s1;
	std::set<__u64 >* s_s2;
};


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


void init(const char* filename, int split_t)
{
	// cputid should be already initialized inside main() ahead
	__u64  ts = time(NULL);
	split_enable = split_t;
	// snprintf(filename, sizeof(filename), "profiling_%lu", ts);
	FILE* fp = fopen(filename, "w");
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
	// system("echo 0 > /proc/sys/kernel/numa_balancing");
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

void analysis_profiling_results(vector< __u64 > *migrate,
								vector<vector< __u64 >> *split, 
								set< __u64 > *s_s1,
								map< __u64, int > *soc1
								){

	__u64  base = 0; 
	base = *(s_s1->begin()); 
	__u64  base_pfn = base >> 9;
	__u64  pfn;

	int total, average, max, min;
    total = average = max = min = 0;
	std::priority_queue<int> pq;
	int i = 0 ;
	for (auto it = s_s1->begin(); it != s_s1->end();it++){
		pfn = *it>>9;
		if (pfn != base_pfn){
			// migrate
			if (total > HIGHMARK){
				migrate->push_back(*it);
			}
			//split
			else if (total < LOWMARK){
				//Directly discard this record
				;
			}
			else{
				int avearge = total / i;
				vector<__u64 > tmp;
				while (!pq.empty()){
					tmp.push_back(pq.top());
					pq.pop();
				}
				split->push_back(tmp);
			}
			base_pfn = pfn;
		}
		else{
			if(pq.size() < 5){
				pq.push(*it);
			}
			else{
				if (pq.top() < (*soc1)[*it]){
					pq.pop();
					pq.push(*it);
				}
			} 
			total += (*soc1)[*it];
			if ((*soc1)[*it] < min){
				min = (*soc1)[*it];
			}
			if ((*soc1)[*it] > max){
				max = (*soc1)[*it];
			}
			i++;
		}
	}
	printf("finish analysis\n");
}

void *analysis_thread_func(void* args)
{
	// set affinity
	struct thread_arg* targ = (struct thread_arg*)args;
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(ANALYSISCPU, &cpuset);
	pthread_t thread = pthread_self();
	int affinity_ret = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
	assert(affinity_ret==0); //if (affinity_ret)	fprintf(stderr, "pthread_setaffinity_np failed!\n");
	
	__u64  start_pfn = 0; 
	start_pfn = *(targ->s_s1->begin()); 
	__u64  base_pfn = start_pfn >> 9;
	__u64  pfn;

	int total, average, max, min;
	total = average = max = min = 0;
	std::priority_queue<int> pq;
	migrate.clear();
	split.clear();
	
	analysis_profiling_results(&migrate, &split, targ->s_s1, targ->soc1);
	// analysis_profiling_results(&migrate, &split, targ->s_s2, targ->soc2);

	printf("Huge page migrate size %d \n", migrate.size());
	printf("Huge page split size %d \n", split.size());


	
	int index = 0;
	int mi_num = 0;
	for (auto it = migrate.begin(); it != migrate.end();it++){
		std::snprintf(buffer+index, sizeof(buffer), "%.16llx ", *it);
		index += 17;
		mi_num++;
		if(mi_num ==  MAXIMUM_MIGRATION){
			printf("Reach the maximum page num\n");
			// index -= 17;
			break;
		}
	}
	// printf("%s\n", buffer);
	// output to the file
	// here we will wait for the synchronization from the kernel space.
	std::unique_lock<std::mutex> lk(mutex1);
	printf("wait for the kernel to be finished\n");
	cv.wait(lk, []{return kernel_finished;});
	kernel_finished = false;
	lk.unlock();

	int flag1 = 0;
	int flag2 = 0;
	index = 0;
	if(index >=17){
		buffer[index-1] = '\0';
		int cur = 0 ;
		for (; cur < index; cur+=1020){
			buffer[cur-1] ='\0';
			FILE* fp = fopen("/proc/migrate_huge_page", "w");
			if(fp != NULL){
				fprintf(fp, "%s", buffer+cur);
				fclose(fp);
			}
			else {
				printf("can not open /proc/migrate_huge_page\n");
			}
		}
	}
	else{
		buffer[0] = 'N';
		buffer[1] = '\0';
		FILE *fp = fopen("/proc/migrate_huge_page", "w");
		if(fp != NULL){
			fprintf(fp,"N");
			fclose(fp);
		}
		else {
			printf("can not open /proc/migrate_huge_page\n");
		}
	}
	index = 0;
	mi_num = 0;
	for (auto it = split.begin(); it != split.end();it++){
		for (auto it2 = it->begin(); it2 != it->end();it2++){
			std::snprintf(buffer+index, sizeof(buffer), "%.16llx ", *it2);
			index += 17;
			if(mi_num == MAXIMUM_MIGRATION){
				printf("Reach the maximum page num\n");
				break;
			}
		}
		if (index > 0){buffer[index-1] = '\n';}
	}
	if (!split_enable)
	{
		index = 0;
	}
	// index = 0;
	if(index >= 17){
		buffer[index-1] = '\0';
		int cur = 0 ;
		for (; cur < index; cur+=1020){
			buffer[cur-1] ='\0';
			FILE* fp = fopen("/proc/base_page", "w");
			if(fp != NULL){
				fprintf(fp, "%s", buffer+cur);
				fclose(fp);
			}
			else {
				printf("can not open /proc/base_page\n");
			}
		}
	}
	else{
		buffer[0] = 'N';
		buffer[1] = '\0';
		FILE* fp = fopen("/proc/base_page", "w");
		if(fp != NULL){
			fprintf(fp,"NNNN");
			printf("%s %d \n", buffer, sizeof(buffer));

			fclose(fp);
		}
		else {
			printf("can not open /proc/base_page\n");
		}
	}


	//TODO: we should know thedmesgre should have synchronization between kernel and user space.

	targ->soc1->clear();
	targ->s_s1->clear();
	targ->soc2->clear();
	targ->s_s2->clear();
	printf("done\n");
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

	// We should set the pid of this profiling threads

	FILE *fp = fopen("/proc/profiling_pid", "w");
	if(fp != NULL){
		int pid = getpid();
		fprintf(fp, "%d\n", pid);
		fclose(fp);
	}
	else {
		printf("can not open /proc/profiling_pid\n");
		exit(1);
	}

	// We need memory to store the profiling records

	std::map<__u64  , int> *p_soc1, *p_soc2; ;
	std::set<__u64 > *p_s_s1, *p_s_s2;
	
	__u64  base_addr ;
	__u64  huge_addr ;

	// pthread_mutex_init(mutex1, NULL);
	__u64  cur_time = 0;
	struct thread_arg targ;
	pthread_t analysis_thread;

	p_soc1 = &soc1;
	p_soc2 = &soc2;
	p_s_s1 = &s_s1;
	p_s_s2 = &s_s2;

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
							if(cur_time == 0){
								cur_time = ps->time;
							}
							else{
								if (index < SOC1){
									addr = (ps->addr)>>12 ;
									if (i == L_PM || i == R_PM){
										if(p_soc1->find(addr) != p_soc1->end()){
											(*p_soc1)[addr] = 1;
											p_s_s1->insert(addr);
										}
										else{
											(*p_soc1)[addr] +=1;
										}
									}
								}
								else{
									addr = (ps->addr)>>12;
									if (i == L_PM || i == R_PM){
										if(p_soc2->find(addr)!=p_soc2->end()){
											(*p_soc2)[addr] = 1;
											p_s_s2->insert(addr);
										}
										else{
											(*p_soc2)[addr] +=1;
										}
									}
								}
							}
							// printf( "%llu %llu, %d %d\n", (void*)(ps->addr), ps->time, index, i);
							// printf("%lld\n", ps->time - cur_time - INTERVAL);
							if( llabs(ps->time - cur_time) > INTERVAL){
								printf("%llu\n", llabs(ps->time - cur_time));
								printf("get in\n");
								cur_time = ps->time;
								// Now we start the analysis threads
								targ.soc1 = p_soc1;
								targ.soc2 = p_soc2;	
								targ.s_s1 = p_s_s1;
								targ.s_s2 = p_s_s2;

								// if (!pthread_tryjoin_np(analysis_thread, NULL)){
									// this is unusual case.
								pthread_join(analysis_thread, NULL);
								printf("join\n");
								// }
							
								if (switch_on == 0){
									printf("start switch 0\n");
									p_soc1 = &soc1_cp;
									p_soc2 = &soc2_cp;
									p_s_s1 = &s_s1_cp;
									p_s_s2 = &s_s2_cp;
								   	switch_on = 1;
								}
								else{
									printf("start switch 1 \n");
									p_soc1 = &soc1;
									p_soc2 = &soc2;
									p_s_s1 = &s_s1;
									p_s_s2 = &s_s2;
									switch_on = 0;
								}
								int ret = pthread_create(&analysis_thread, NULL, analysis_thread_func, (void*)&targ);
								if (ret != 0) {
									fprintf(stderr, "pthread_create analysis failed!\n");
									exit(1);
								}
								
								// change the profiling storage for profiling results.
							}
							// Here should be a condition that we should start the analysis.
							// if (ps->addr!=0 ){
							//    fprintf(fp, "%llu %llu, %d %d\n", (void*)(ps->addr), ps->time, index, i);
							// }
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
