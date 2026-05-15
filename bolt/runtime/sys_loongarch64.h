#ifndef LLVM_TOOLS_LLVM_BOLT_SYS_LOONGARCH
#define LLVM_TOOLS_LLVM_BOLT_SYS_LOONGARCH

// Save all 32 GPRs while keeping 16B stack alignment.
#define SAVE_ALL                                                               \
  "addi.d $sp, $sp, -256      \n"                                              \
  "st.d   $sp,   $sp, (8*3)   \n"                                              \
  "st.d   $zero, $sp, (8*0)   \n"                                              \
  "st.d   $ra,   $sp, (8*1)   \n"                                              \
  "st.d   $tp,   $sp, (8*2)   \n"                                              \
  "st.d   $a0,   $sp, (8*4)   \n"                                              \
  "st.d   $a1,   $sp, (8*5)   \n"                                              \
  "st.d   $a2,   $sp, (8*6)   \n"                                              \
  "st.d   $a3,   $sp, (8*7)   \n"                                              \
  "st.d   $a4,   $sp, (8*8)   \n"                                              \
  "st.d   $a5,   $sp, (8*9)   \n"                                              \
  "st.d   $a6,   $sp, (8*10)  \n"                                              \
  "st.d   $a7,   $sp, (8*11)  \n"                                              \
  "st.d   $t0,   $sp, (8*12)  \n"                                              \
  "st.d   $t1,   $sp, (8*13)  \n"                                              \
  "st.d   $t2,   $sp, (8*14)  \n"                                              \
  "st.d   $t3,   $sp, (8*15)  \n"                                              \
  "st.d   $t4,   $sp, (8*16)  \n"                                              \
  "st.d   $t5,   $sp, (8*17)  \n"                                              \
  "st.d   $t6,   $sp, (8*18)  \n"                                              \
  "st.d   $t7,   $sp, (8*19)  \n"                                              \
  "st.d   $t8,   $sp, (8*20)  \n"                                              \
  "st.d   $r21,  $sp, (8*21)  \n"                                              \
  "st.d   $fp,   $sp, (8*22)  \n"                                              \
  "st.d   $s0,   $sp, (8*23)  \n"                                              \
  "st.d   $s1,   $sp, (8*24)  \n"                                              \
  "st.d   $s2,   $sp, (8*25)  \n"                                              \
  "st.d   $s3,   $sp, (8*26)  \n"                                              \
  "st.d   $s4,   $sp, (8*27)  \n"                                              \
  "st.d   $s5,   $sp, (8*28)  \n"                                              \
  "st.d   $s6,   $sp, (8*29)  \n"                                              \
  "st.d   $s7,   $sp, (8*30)  \n"                                              \
  "st.d   $s8,   $sp, (8*31)  \n"
// Mirrors SAVE_ALL
#define RESTORE_ALL                                                            \
  "ld.d   $s8,   $sp, (8*31)  \n"                                              \
  "ld.d   $s7,   $sp, (8*30)  \n"                                              \
  "ld.d   $s6,   $sp, (8*29)  \n"                                              \
  "ld.d   $s5,   $sp, (8*28)  \n"                                              \
  "ld.d   $s4,   $sp, (8*27)  \n"                                              \
  "ld.d   $s3,   $sp, (8*26)  \n"                                              \
  "ld.d   $s2,   $sp, (8*25)  \n"                                              \
  "ld.d   $s1,   $sp, (8*24)  \n"                                              \
  "ld.d   $s0,   $sp, (8*23)  \n"                                              \
  "ld.d   $fp,   $sp, (8*22)  \n"                                              \
  "ld.d   $r21,  $sp, (8*21)  \n"                                              \
  "ld.d   $t8,   $sp, (8*20)  \n"                                              \
  "ld.d   $t7,   $sp, (8*19)  \n"                                              \
  "ld.d   $t6,   $sp, (8*18)  \n"                                              \
  "ld.d   $t5,   $sp, (8*17)  \n"                                              \
  "ld.d   $t4,   $sp, (8*16)  \n"                                              \
  "ld.d   $t3,   $sp, (8*15)  \n"                                              \
  "ld.d   $t2,   $sp, (8*14)  \n"                                              \
  "ld.d   $t1,   $sp, (8*13)  \n"                                              \
  "ld.d   $t0,   $sp, (8*12)  \n"                                              \
  "ld.d   $a7,   $sp, (8*11)  \n"                                              \
  "ld.d   $a6,   $sp, (8*10)  \n"                                              \
  "ld.d   $a5,   $sp, (8*9)   \n"                                              \
  "ld.d   $a4,   $sp, (8*8)   \n"                                              \
  "ld.d   $a3,   $sp, (8*7)   \n"                                              \
  "ld.d   $a2,   $sp, (8*6)   \n"                                              \
  "ld.d   $a1,   $sp, (8*5)   \n"                                              \
  "ld.d   $a0,   $sp, (8*4)   \n"                                              \
  "ld.d   $tp,   $sp, (8*2)   \n"                                              \
  "ld.d   $ra,   $sp, (8*1)   \n"                                              \
  "ld.d   $zero, $sp, (8*0)   \n"                                              \
  "ld.d   $sp,   $sp, (8*3)   \n"                                              \
  "addi.d $sp, $sp, 256       \n"

#define SYSCALL_CLOBBERS                                                       \
  "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7", "t8", "memory"

// Anonymous namespace covering everything but our library entry point
namespace {

// Get the difference between runtime address of .text section and
// static address in section header table.
uint64_t getTextBaseAddress() {
  uint64_t DynAddr;
  uint64_t StaticAddr;
  __asm__ volatile("pcalau12i %0, %%pc_hi20(__hot_end)\n\t"
                   "addi.d    %0, %0, %%pc_lo12(__hot_end)\n\t"
                   "lu12i.w   %1, %%abs_hi20(__hot_end)\n\t"
                   "ori       %1, %1, %%abs_lo12(__hot_end)\n\t"
                   "lu32i.d   %1, %%abs64_lo20(__hot_end)\n\t"
                   "lu52i.d   %1, %1, %%abs64_hi12(__hot_end)\n\t"
                   : "=r"(DynAddr), "=r"(StaticAddr));
  return DynAddr - StaticAddr;
}

uint64_t __read(uint64_t fd, const void *buf, uint64_t count) {
  register uint64_t a0 __asm__("$a0") = fd;
  register const void *a1 __asm__("$a1") = buf;
  register uint64_t a2 __asm__("$a2") = count;
  register uint64_t a7 __asm__("$a7") = 63;
  __asm__ __volatile__("syscall 0\n\t"
                       : "+r"(a0)
                       : "r"(a1), "r"(a2), "r"(a7)
                       : SYSCALL_CLOBBERS);
  return a0;
}

uint64_t __write(uint64_t fd, const void *buf, uint64_t count) {
  register uint64_t a0 __asm__("$a0") = fd;
  register const void *a1 __asm__("$a1") = buf;
  register uint64_t a2 __asm__("$a2") = count;
  register uint64_t a7 __asm__("$a7") = 64;
  __asm__ __volatile__("syscall 0\n\t"
                       : "+r"(a0)
                       : "r"(a1), "r"(a2), "r"(a7)
                       : SYSCALL_CLOBBERS);
  return a0;
}

void *__mmap(uint64_t addr, uint64_t size, uint64_t prot, uint64_t flags,
             uint64_t fd, uint64_t offset) {
  register void *a0 __asm__("$a0") = (void *)addr;
  register uint64_t a1 __asm__("$a1") = size;
  register uint64_t a2 __asm__("$a2") = prot;
  register uint64_t a3 __asm__("$a3") = flags;
  register uint64_t a4 __asm__("$a4") = fd;
  register uint64_t a5 __asm__("$a5") = offset;
  register uint64_t a7 __asm__("$a7") = 222;
  __asm__ __volatile__("syscall 0\n\t"
                       : "+r"(a0)
                       : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a7)
                       : SYSCALL_CLOBBERS);
  return a0;
}

uint64_t __munmap(void *addr, uint64_t size) {
  register void *a0 __asm__("$a0") = addr;
  register uint64_t a1 __asm__("$a1") = size;
  register uint64_t a7 __asm__("$a7") = 215;
  __asm__ __volatile__("syscall 0\n\t"
                       : "+r"(a0), "+r"(a1)
                       : "r"(a7)
                       : SYSCALL_CLOBBERS);
  return (uint64_t)a0;
}

uint64_t __exit(uint64_t code) {
  register uint64_t a0 __asm__("$a0") = code;
  register uint64_t a7 __asm__("$a7") = 93;
  __asm__ __volatile__("syscall 0\n\t" : "+r"(a0) : "r"(a7) : SYSCALL_CLOBBERS);
  return a0;
}

uint64_t __open(const char *pathname, uint64_t flags, uint64_t mode) {
  register int a0 __asm__("$a0") = -100;
  register const char *a1 __asm__("$a1") = pathname;
  register uint64_t a2 __asm__("$a2") = flags;
  register uint64_t a3 __asm__("$a3") = mode;
  register uint64_t a7 __asm__("$a7") = 56;
  __asm__ __volatile__("syscall 0\n\t"
                       : "+r"(a0)
                       : "r"(a1), "r"(a2), "r"(a3), "r"(a7)
                       : SYSCALL_CLOBBERS);
  return a0;
}

long __getdents64(unsigned int fd, dirent64 *dirp, size_t count) {
  register unsigned int a0 __asm__("$a0") = fd;
  register dirent64 *a1 __asm__("$a1") = dirp;
  register size_t a2 __asm__("$a2") = count;
  register uint64_t a7 __asm__("$a7") = 61;
  __asm__ __volatile__("syscall 0\n\t"
                       : "+r"(a0), "+r"(a1)
                       : "r"(a2), "r"(a7)
                       : SYSCALL_CLOBBERS);
  return a0;
}

uint64_t __readlink(const char *pathname, char *buf, size_t bufsize) {
  register int a0 __asm__("$a0") = -100;
  register const char *a1 __asm__("$a1") = pathname;
  register char *a2 __asm__("$a2") = buf;
  register size_t a3 __asm__("$a3") = bufsize;
  register uint64_t a7 __asm__("$a7") = 78;
  __asm__ __volatile__("syscall 0\n\t"
                       : "+r"(a0), "+r"(a1)
                       : "r"(a2), "r"(a3), "r"(a7)
                       : SYSCALL_CLOBBERS);
  return a0;
}

uint64_t __lseek(uint64_t fd, uint64_t pos, uint64_t whence) {
  register uint64_t a0 __asm__("$a0") = fd;
  register uint64_t a1 __asm__("$a1") = pos;
  register uint64_t a2 __asm__("$a2") = whence;
  register uint64_t a7 __asm__("$a7") = 62;
  __asm__ __volatile__("syscall 0\n\t"
                       : "+r"(a0), "+r"(a1)
                       : "r"(a2), "r"(a7)
                       : SYSCALL_CLOBBERS);
  return a0;
}

int __ftruncate(uint64_t fd, uint64_t length) {
  register uint64_t a0 __asm__("$a0") = fd;
  register uint64_t a1 __asm__("$a1") = length;
  register uint64_t a7 __asm__("$a7") = 46;
  __asm__ __volatile__("syscall 0\n\t"
                       : "+r"(a0), "+r"(a1)
                       : "r"(a7)
                       : SYSCALL_CLOBBERS);
  return (int)a0;
}

int __close(uint64_t fd) {
  register uint64_t a0 __asm__("$a0") = fd;
  register uint64_t a7 __asm__("$a7") = 57;
  __asm__ __volatile__("syscall 0\n\t" : "+r"(a0) : "r"(a7) : SYSCALL_CLOBBERS);
  return (int)a0;
}

int __madvise(void *addr, size_t length, int advice) {
  register void *a0 __asm__("$a0") = addr;
  register size_t a1 __asm__("$a1") = length;
  register int a2 __asm__("$a2") = advice;
  register uint64_t a7 __asm__("$a7") = 233;
  __asm__ __volatile__("syscall 0\n\t"
                       : "+r"(a0), "+r"(a1)
                       : "r"(a2), "r"(a7)
                       : SYSCALL_CLOBBERS);
  return (int)(int64_t)a0;
}

int __uname(struct UtsNameTy *buf) {
  register UtsNameTy *a0 __asm__("$a0") = buf;
  register uint64_t a7 __asm__("$a7") = 160;
  __asm__ __volatile__("syscall 0\n\t" : "+r"(a0) : "r"(a7) : SYSCALL_CLOBBERS);
  return (int)(int64_t)a0;
}

uint64_t __nanosleep(const timespec *req, timespec *rem) {
  register const timespec *a0 __asm__("$a0") = req;
  register timespec *a1 __asm__("$a1") = rem;
  register uint64_t a7 __asm__("$a7") = 101;
  __asm__ __volatile__("syscall 0\n\t"
                       : "+r"(a0), "+r"(a1)
                       : "r"(a7)
                       : SYSCALL_CLOBBERS);
  return (uint64_t)(int64_t)a0;
}

int64_t __fork() {
  register uint64_t a0 __asm__("$a0") = 0x1200011;
  register uint64_t a1 __asm__("$a1") = 0;
  register uint64_t a2 __asm__("$a2") = 0;
  register uint64_t a3 __asm__("$a3") = 0;
  register uint64_t a4 __asm__("$a4") = 0;
  register uint64_t a7 __asm__("$a7") = 220;
  __asm__ __volatile__("syscall 0\n\t"
                       : "+r"(a0), "+r"(a1)
                       : "r"(a2), "r"(a3), "r"(a4), "r"(a7)
                       : SYSCALL_CLOBBERS);
  return a0;
}

int __mprotect(void *addr, size_t len, int prot) {
  register void *a0 __asm__("$a0") = addr;
  register size_t a1 __asm__("$a1") = len;
  register int a2 __asm__("$a2") = prot;
  register uint64_t a7 __asm__("$a7") = 226;
  __asm__ __volatile__("syscall 0\n\t"
                       : "+r"(a0), "+r"(a1)
                       : "r"(a2), "r"(a7)
                       : SYSCALL_CLOBBERS);
  return (int)(int64_t)a0;
}

uint64_t __getpid() {
  register uint64_t a0 __asm__("$a0");
  register uint64_t a7 __asm__("$a7") = 172;
  __asm__ __volatile__("syscall 0\n\t" : "=r"(a0) : "r"(a7) : SYSCALL_CLOBBERS);
  return a0;
}

uint64_t __getppid() {
  register uint64_t a0 __asm__("$a0");
  register uint64_t a7 __asm__("$a7") = 173;
  __asm__ __volatile__("syscall 0\n\t" : "=r"(a0) : "r"(a7) : SYSCALL_CLOBBERS);
  return a0;
}

int __setpgid(uint64_t pid, uint64_t pgid) {
  register uint64_t a0 __asm__("$a0") = pid;
  register uint64_t a1 __asm__("$a1") = pgid;
  register uint64_t a7 __asm__("$a7") = 154;
  __asm__ __volatile__("syscall 0\n\t"
                       : "+r"(a0), "+r"(a1)
                       : "r"(a7)
                       : SYSCALL_CLOBBERS);
  return (int)a0;
}

uint64_t __getpgid(uint64_t pid) {
  register uint64_t a0 __asm__("$a0") = pid;
  register uint64_t a7 __asm__("$a7") = 155;
  __asm__ __volatile__("syscall 0\n\t" : "+r"(a0) : "r"(a7) : SYSCALL_CLOBBERS);
  return a0;
}

int __kill(uint64_t pid, int sig) {
  register uint64_t a0 __asm__("$a0") = pid;
  register int a1 __asm__("$a1") = sig;
  register uint64_t a7 __asm__("$a7") = 129;
  __asm__ __volatile__("syscall 0\n\t"
                       : "+r"(a0), "+r"(a1)
                       : "r"(a7)
                       : SYSCALL_CLOBBERS);
  return (int)a0;
}

int __fsync(int fd) {
  register int a0 __asm__("$a0") = fd;
  register uint64_t a7 __asm__("$a7") = 82;
  __asm__ __volatile__("syscall 0\n\t" : "+r"(a0) : "r"(a7) : SYSCALL_CLOBBERS);
  return a0;
}

uint64_t __sigprocmask(int how, const void *set, void *oldset) {
  register int a0 __asm__("$a0") = how;
  register const void *a1 __asm__("$a1") = set;
  register void *a2 __asm__("$a2") = oldset;
  register long a3 __asm__("$a3") = 8;
  register uint64_t a7 __asm__("$a7") = 135;
  __asm__ __volatile__("syscall 0\n\t"
                       : "+r"(a0), "+r"(a1)
                       : "r"(a2), "r"(a3), "r"(a7)
                       : SYSCALL_CLOBBERS);
  return a0;
}

int __prctl(int option, unsigned long arg2, unsigned long arg3,
            unsigned long arg4, unsigned long arg5) {
  register int a0 __asm__("$a0") = option;
  register unsigned long a1 __asm__("$a1") = arg2;
  register unsigned long a2 __asm__("$a2") = arg3;
  register unsigned long a3 __asm__("$a3") = arg4;
  register unsigned long a4 __asm__("$a4") = arg5;
  register uint64_t a7 __asm__("$a7") = 167;
  __asm__ __volatile__("syscall 0\n\t"
                       : "+r"(a0), "+r"(a1)
                       : "r"(a2), "r"(a3), "r"(a4), "r"(a7)
                       : SYSCALL_CLOBBERS);
  return a0;
}
} // anonymous namespace

#endif
