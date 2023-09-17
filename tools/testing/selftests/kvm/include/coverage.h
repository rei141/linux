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
#include <libelf.h>
#include <fcntl.h>
#include <sys/utsname.h>
#include <gelf.h>
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
char base_path[100]; 
FILE *kvm_arch_coverage_file;
FILE *kvm_coverage_file;
unsigned long kvm_arch_base;
unsigned long kvm_base;
// #define MAX_KVM_ARCH 0xc0000
// #define MAX_KVM 0x187000
uint64_t MAX_KVM_ARCH;
uint64_t MAX_KVM;

uint8_t *kvm_arch_covered;
uint8_t *kvm_covered;

static uint64_t check_text_size(char *filepath) {
    Elf         *elf;
    Elf_Scn     *scn = NULL;
    GElf_Shdr   shdr;
    int         fd;
    size_t      shstrndx;  // Section header string table index

    // Open the file
    fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    if (elf_version(EV_CURRENT) == EV_NONE) {
        // library out of date
        exit(1);
    }

    elf = elf_begin(fd, ELF_C_READ, NULL);
    
    // Retrieve the section header string table index
    if (elf_getshdrstrndx(elf, &shstrndx) != 0) {
        perror("elf_getshdrstrndx");
        exit(1);
    }

    while ((scn = elf_nextscn(elf, scn)) != NULL) {
        if (gelf_getshdr(scn, &shdr) != &shdr) {
            // error
            exit(1);
        }

        if (shdr.sh_type == SHT_PROGBITS) {
            char *name;
            name = elf_strptr(elf, shstrndx, shdr.sh_name);  // Use shstrndx
            if (name && strcmp(name, ".text") == 0) {
                break;
            }
        }
    }

    elf_end(elf);
    close(fd);

    return (uint64_t)shdr.sh_size;
}

static void check_cpu_vendor(void) {
    FILE *cpuinfo = fopen("/proc/cpuinfo", "rb");
    char buffer[255];
    char vendor[16];
    struct utsname utbuffer;
    char filepath[128];
    FILE *fkvm_arch;
    FILE *fkvm;
    // start point of .text of kvm/kvm-intel or kvm-amd
    char kvm_arch_str[18];
    char kvm_str[18];

    if (cpuinfo == NULL) {
        perror("fopen");
        return;
    }

    if (uname(&utbuffer) != 0) {
        perror("uname");
        return;
    }

    snprintf(filepath, 128, "/usr/lib/modules/%s/kernel/arch/x86/kvm/kvm.ko", utbuffer.release);

    MAX_KVM = check_text_size(filepath);

    fkvm = fopen("/sys/module/kvm/sections/.text","r");
    if (fkvm == NULL)
        perror("fopen"), exit(1);

    int n = fread(kvm_str, sizeof(char),18,fkvm);
    if(n != 18)
        perror("fread"), exit(1);
    kvm_base = strtoul(kvm_str, NULL,0);

    if (fclose(fkvm) == EOF)
        perror("fclose"), exit(1);

    while (fgets(buffer, 255, cpuinfo)) {
        if (strncmp(buffer, "vendor_id", 9) == 0) {
            sscanf(buffer, "vendor_id : %s", vendor);

            if (strcmp(vendor, "GenuineIntel") == 0) {
                snprintf(filepath, 128, "/usr/lib/modules/%s/kernel/arch/x86/kvm/kvm-intel.ko", utbuffer.release);
                MAX_KVM_ARCH = check_text_size(filepath);
                fkvm_arch = fopen("/sys/module/kvm_intel/sections/.text","r");
                if (fkvm_arch == NULL)
                    perror("fopen"), exit(1);
            } else if (strcmp(vendor, "AuthenticAMD") == 0) {
                snprintf(filepath, 128, "/usr/lib/modules/%s/kernel/arch/x86/kvm/kvm-amd.ko", utbuffer.release);
                MAX_KVM_ARCH = check_text_size(filepath);
                fkvm_arch = fopen("/sys/module/kvm_amd/sections/.text","r");
                if (fkvm_arch == NULL)
                    perror("fopen"), exit(1);
            } else {
                printf("This is a CPU from another vendor: %s\n", vendor);
                // default value or another value
                MAX_KVM_ARCH = 0;
                return;
            }
            break;
        }
    }

    n = fread(kvm_arch_str, sizeof(char),18,fkvm_arch);
    if(n != 18)
        perror("fread"), exit(1);
    kvm_arch_base = strtoul(kvm_arch_str, NULL,0);
    if (fclose(fkvm_arch) == EOF)
        perror("fclose"), exit(1);

    fclose(cpuinfo);
}

#define COVERAGE_FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

/* this should be macro to expand __FILE__ in COVERAGE_FILENAME to targed c filename */ \
#define coverage_start() \
    check_cpu_vendor(); \
    kvm_arch_covered = malloc(sizeof(uint8_t)*MAX_KVM_ARCH); \
    kvm_covered = malloc(sizeof(uint8_t)*MAX_KVM); \
    kcov_fd = open("/sys/kernel/debug/kcov", O_RDWR); \
    if (kcov_fd == -1) \
        perror("open"), exit(1); \
    char *base_path = getenv("COVERAGE_BASE_PATH"); \
    if (!base_path) { \
        base_path = "/tmp"; \
    } \
    sprintf(coverage_file_path, "%s/COVERAGE_ARCH_%s", base_path, COVERAGE_FILENAME); \
    kvm_arch_coverage_file = fopen(coverage_file_path, "w"); \
    if (kvm_arch_coverage_file == NULL) \
        perror("fopen"), exit(1); \
    sprintf(coverage_file_path, "%s/COVERAGE_KVM_%s", base_path, COVERAGE_FILENAME); \
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
        if (cov >= 0 && cov < MAX_KVM_ARCH){ \
            if (kvm_arch_covered[cov] == 0){ \
                kvm_arch_covered[cov] = 1; \
		fprintf(kvm_arch_coverage_file,"%x\n",cov); \
            } \
        } else { \
            cov = (int)(kcov_cover[i+1]-kvm_base); \
            if (cov >= 0 && cov < MAX_KVM){ \
                if (kvm_covered[cov] == 0){ \
                    kvm_covered[cov] = 1; \
		    fprintf(kvm_coverage_file,"%x\n",cov); \
                } \
            } \
        } \
    } \
	/* Disable coverage collection for the current thread. After this call \
	* coverage can be enabled for a different thread. \
	*/ \
    /*for(int i = 0; i < MAX_KVM_ARCH;i++){ \
        if (kvm_arch_covered[i] == 1) \
		fprintf(kvm_arch_coverage_file,"%x\n",i); \
	} \
    for(int i = 0; i < MAX_KVM;i++){ \
        if (kvm_covered[i] == 1) \
		fprintf(kvm_coverage_file,"%x\n",i); \
	}*/ \
	if (ioctl(kcov_fd, KCOV_DISABLE, 0)) \
		perror("ioctl"), exit(1); \
    /* Free resources. */ \
    if (munmap(kcov_cover, COVER_SIZE * sizeof(unsigned long))) \
        perror("munmap"), exit(1); \
    if (close(kcov_fd)) \
        perror("close"), exit(1); \
    if (fclose(kvm_coverage_file) == EOF) \
        perror("fclose"), exit(1); \
    if (fclose(kvm_arch_coverage_file) == EOF) \
        perror("fclose"), exit(1);