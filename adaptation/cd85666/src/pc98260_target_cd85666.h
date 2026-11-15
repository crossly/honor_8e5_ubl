#ifndef PC98260_TARGET_CD85666_H
#define PC98260_TARGET_CD85666_H

#include <stdint.h>

#define TARGET_NAME          "honor-cd85666"
#define TARGET_KERNEL        "6.12.23-android16-5-gcd85666d7884-abogki452537391-4k"
#define TARGET_KERNEL_SHA256 "c3cf2a0422b5df55cf834b60b12f614db524c77dc1b79ced56a122b59712a1a9"

#define LINK_SELINUX_STATE   UINT64_C(0xffffffc082656570)
#define LINK_INIT_USER_NS    UINT64_C(0xffffffc0823f1360)
#define LINK_ROOT_USER       UINT64_C(0xffffffc0823f15d8)
#define LINK_INIT_GROUPS     UINT64_C(0xffffffc0823f2aa0)
#define LINK_INIT_UCOUNTS    UINT64_C(0xffffffc0823f2f70)

#define LINK_ALLOC_TIMER     UINT64_C(0xffffffc080cb8b60)
#define LINK_ALLOC_CRED      UINT64_C(0xffffffc080df5358)
#define LINK_ALLOC_TASK      UINT64_C(0xffffffc080d03ae4)

#define GEO_TASK_CRED        0x900
#define GEO_TASK_REAL_CRED   0x8f8
#define GEO_TASK_COMM        0x910
#define GEO_KITIMER_IT       0x78
#define GEO_KITIMER_SIZE     0x100
#define GEO_KITIMER_STRIDE   0x100
#define GEO_CRED_SIZE        184
#define GEO_CRED_ALLOC_SIZE  192
#define GEO_CRED_UID         0x8
#define GEO_CRED_GID         0xc
#define GEO_CRED_SUID        0x10
#define GEO_CRED_SGID        0x14
#define GEO_CRED_EUID        0x18
#define GEO_CRED_EGID        0x1c
#define GEO_CRED_FSUID       0x20
#define GEO_CRED_FSGID       0x24
#define GEO_CRED_CAP_INH     0x30
#define GEO_CRED_CAP_PERM    0x38
#define GEO_CRED_CAP_EFF     0x40
#define GEO_CRED_CAP_BSET    0x48
#define GEO_CRED_CAP_AMB     0x50
#define GEO_CRED_SECURITY    0x80
#define GEO_CRED_USER        0x88
#define GEO_CRED_USER_NS     0x90
#define GEO_SELINUX_ENFORCE  0
#define GEO_SELINUX_INIT     1
#define GEO_SELINUX_WRITE    (-7)
#define GEO_SELINUX_SIZE     0x80

#define GEO_TASK_OBJSIZE             5696
#define GEO_TASK_OBJ_PER_SLAB        5
#define GEO_TASK_SLAB_PAGES          8
#define GEO_POSIX_TIMER_OBJSIZE      256
#define GEO_POSIX_TIMER_OBJ_PER_SLAB 32
#define GEO_POSIX_TIMER_SLAB_ORDER   1
#define GEO_PAGE_SIZE                4096
#define GEO_FRAG_PAGE_SIZE           8192

#define GEOM_PAGE_OFFSET     UINT64_C(0xffffff8000000000)
#define GEOM_VA_BITS         39
#define GEOM_MEMSTART_PFN    UINT64_C(0x80000)
#define GEOM_MEMSTART_ADDR   UINT64_C(0xffffffc0817e5cd8)

#define CAP_FULL             UINT64_C(0x1ffffffffff)

#define BNE_LINK              UINT64_C(0xffffffc080cffbb0)
#define BNE_RELEASE_TASK_OFF  0xa48
#define BNE_DOWN_READ_OFF     0x168
#define BNE_TIMERS_EXIT_OFF   0x2a0
#define BNE_EXIT_ITIMERS_OFF  0x2b0

#endif
