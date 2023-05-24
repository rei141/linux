// for NestedKVMFuzzer

#include <fcntl.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// for COVERAGE_FILENAME

#include <string.h>

#define KCOV_INIT_TRACE _IOR('c', 1, unsigned long)
#define KCOV_ENABLE _IO('c', 100)
#define KCOV_DISABLE _IO('c', 101)
#define COVER_SIZE (64 << 16)

#define KCOV_TRACE_PC 0
#define KCOV_TRACE_CMP 1

int kcov_fd;
unsigned long *kcov_cover, kcov_n;
char coverage_file_path[200];
FILE *kvm_intel_coverage_file;
FILE *kvm_coverage_file;
unsigned long kvm_arch_base;
unsigned long kvm_base;
unsigned long max_kvm_arch;
// #define MAX_KVM_INTEL 0xc0000
// #define MAX_KVM 0x187000
#define MAX_KVM_INTEL 0xc7000
#define MAX_KVM_AMD 0x5f000
#define MAX_KVM 0x1af000
uint8_t *kvm_arch_coverd;
uint8_t *kvm_coverd;
uint8_t is_intel;
uint8_t invalid;

#define COVERAGE_FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

/* this should be macro to expand __FILE__ in COVERAGE_FILENAME to targed c filename */ \
#define coverage_start() \
    FILE * fkvm_arch = fopen("/sys/module/kvm_intel/sections/.text","r"); \
    if (fkvm_arch == NULL) { \
        fkvm_arch = fopen("/sys/module/kvm_amd/sections/.text", "r"); \
        if (fkvm_arch == NULL) { \
            perror("fopen"), exit(1); \
        } \
        else { \
            max_kvm_arch = MAX_KVM_AMD; \
        } \
    } \
    else { \
        max_kvm_arch = MAX_KVM_INTEL; \
    } \
    FILE * fkvm = fopen("/sys/module/kvm/sections/.text","r"); \
    if (fkvm == NULL) \
        perror("fopen"), exit(1); \
    char kvm_intel_str[18]; \
    char kvm_str[18]; \
    int n = fread(kvm_intel_str, sizeof(char),18,fkvm_arch); \
    if(n != 18) \
        perror("fread"), exit(1); \
    kvm_arch_base = strtoul(kvm_intel_str, NULL,0); \
    n = fread(kvm_str, sizeof(char),18,fkvm); \
    if(n != 18) \
        perror("fread"), exit(1); \
    kvm_base = strtoul(kvm_str, NULL,0); \
    if (fclose(fkvm_arch) == EOF) \
        perror("fclose"), exit(1); \
    if (fclose(fkvm) == EOF) \
        perror("fclose"), exit(1); \
    kvm_arch_coverd = malloc(sizeof(uint8_t)*max_kvm_arch); \
    if (kvm_arch_coverd == NULL) \
        perror("malloc"), exit(1); \
    kvm_coverd = malloc(sizeof(uint8_t)*MAX_KVM); \
    if (kvm_coverd == NULL) \
        perror("malloc"), exit(1); \
    kcov_fd = open("/sys/kernel/debug/kcov", O_RDWR); \
    if (kcov_fd == -1) \
        perror("open"), exit(1); \
    sprintf(coverage_file_path, "/home/ishii/work/linux/selftest_coverage/arch_%s", COVERAGE_FILENAME); \
    kvm_intel_coverage_file = fopen(coverage_file_path, "w"); \
    if (kvm_intel_coverage_file == NULL) \
        perror("fopen"), exit(1); \
    sprintf(coverage_file_path, "/home/ishii/work/linux/selftest_coverage/kvm_%s", COVERAGE_FILENAME); \
    kvm_coverage_file = fopen(coverage_file_path, "w"); \
    if (kvm_coverage_file == NULL) \
        perror("fopen"), exit(1); \
    /* Setup trace mode and trace size. */ \
    if (ioctl(kcov_fd, KCOV_INIT_TRACE, COVER_SIZE)) \
        perror("ioctl"), exit(1); \
    /* Mmap buffer shared between kernel- and user-space. */ \
    kcov_cover = (unsigned long *)mmap(NULL, COVER_SIZE * sizeof(unsigned long), \
                                    PROT_READ | PROT_WRITE, MAP_SHARED, kcov_fd, 0); \
    if ((void *)kcov_cover == MAP_FAILED) \
        perror("mmap"), exit(1); \
	/* Enable coverage collection on the current thread. */ \
	if (ioctl(kcov_fd, KCOV_ENABLE, KCOV_TRACE_PC)) \
		perror("ioctl"), exit(1); \
	/* Reset coverage from the tail of the ioctl() call. */ \
	__atomic_store_n(&kcov_cover[0], 0, __ATOMIC_RELAXED);


/* this doesn't have to be macro,
but I make it a macro to match the format with coverage_start() */
#define coverage_end() \
	kcov_n = __atomic_load_n(&kcov_cover[0], __ATOMIC_RELAXED); \
    for (int i = 0; i < kcov_n; i++) { \
        int cov = (int)(kcov_cover[i+1]-kvm_arch_base); \
        if (cov >= 0 && cov < max_kvm_arch){ \
            if (kvm_arch_coverd[cov] == 0){ \
                kvm_arch_coverd[cov] = 1; \
		fprintf(kvm_intel_coverage_file,"%x\n",cov); \
            } \
        } else { \
            cov = (int)(kcov_cover[i+1]-kvm_base); \
            if (cov >= 0 && cov < MAX_KVM){ \
                if (kvm_coverd[cov] == 0){ \
                    kvm_coverd[cov] = 1; \
		    fprintf(kvm_coverage_file,"%x\n",cov); \
                } \
            } \
        } \
    } \
	if (ioctl(kcov_fd, KCOV_DISABLE, 0)) \
		perror("ioctl"), exit(1); \
    /* Free resources. */ \
    if (munmap(kcov_cover, COVER_SIZE * sizeof(unsigned long))) \
        perror("munmap"), exit(1); \
    if (close(kcov_fd)) \
        perror("close"), exit(1); \
    if (fclose(kvm_coverage_file) == EOF) \
        perror("fclose"), exit(1); \
    if (fclose(kvm_intel_coverage_file) == EOF) \
        perror("fclose"), exit(1);