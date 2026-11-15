# cd85666 内核适配（honor_ubl 3.2.1）

## 输入与实现范围

输入为用户提供的 `boot (2).img`。该镜像是新的内核构建，新增独立 profile `cd85666`，按完整 `uname -r` 选择，不按机型放行。

```text
kernel release  6.12.23-android16-5-gcd85666d7884-abogki452537391-4k
boot header     v4，kernel_size=39,815,680
boot SHA-256    e5eeda2cff592f73fa7cb0fc6e33301775d36f8d9f36918b737df0b3aec2f9b1
Image SHA-256   c3cf2a0422b5df55cf834b60b12f614db524c77dc1b79ced56a122b59712a1a9
BTF             6,984,895 bytes
IKCONFIG        220,730 bytes
kallsyms        126,268 recovered symbols
```

保留 3.2.0 的 OTA、备份、分区、刷写、双提权链以及 enforcing 处理。98260 使用 rootd 和 `root-command.sh`，64650 使用 `bfexec`，不增加 KSU 管理器或新的 KSU handoff。

## 98260 链接地址

以下地址均为链接期地址，运行时 slide 由 exploit 在当前 boot 上重新测量。

| 常量 | cd85666 |
| --- | --- |
| `LINK_SELINUX_STATE` | `0xffffffc082656570` |
| `LINK_INIT_USER_NS` | `0xffffffc0823f1360` |
| `LINK_ROOT_USER` | `0xffffffc0823f15d8` |
| `LINK_INIT_GROUPS` | `0xffffffc0823f2aa0` |
| `LINK_INIT_UCOUNTS` | `0xffffffc0823f2f70` |
| `LINK_ALLOC_TIMER` | `0xffffffc080cb8b60` |
| `LINK_ALLOC_CRED` | `0xffffffc080df5358` |
| `LINK_ALLOC_TASK` | `0xffffffc080d03ae4` |
| `GEOM_MEMSTART_ADDR` | `0xffffffc0817e5cd8` |
| `BNE_LINK` | `0xffffffc080cffbb0` |

## 函数内偏移与反汇编证据

| 位置 | 偏移 | 证据 |
| --- | ---: | --- |
| `do_timer_create` 分配返回点 | `+0xac` | `bl kmem_cache_alloc_noprof`，cache 为 `posix_timers_cache` |
| `prepare_creds` 分配返回点 | `+0x2c` | `bl kmem_cache_alloc_noprof`，cache 为 `cred_jar` |
| `copy_process` task 分配返回点 | `+0x158` | `bl kmem_cache_alloc_node_noprof`，参数来自 `task_struct_cachep` |
| `begin_new_exec -> release_task` | `+0xa48` | 调用目标匹配 |
| `begin_new_exec -> down_read_killable` | `+0x168` | 调用目标匹配 |
| `begin_new_exec -> posix_cpu_timers_exit` | `+0x2a0` | 调用目标匹配 |
| `begin_new_exec -> exit_itimers` | `+0x2b0` | 调用目标匹配 |

`copy_process` 的目标返回点是 `0xffffffc080d03ae4`，不能复用旧模板的 `+0x190`。`task_struct_cachep` 地址为 `0xffffffc082607450`，与该调用前装载参数一致。profiling 的 `_alloc_tag` 调用已排除。

## BTF 布局

| 字段 | 值 |
| --- | ---: |
| `sizeof(task_struct)` | `0x1440` |
| `task_struct.real_cred` / `cred` / `comm` | `0x8f8` / `0x900` / `0x910` |
| `task_struct.pid` | `0x708` |
| `sizeof(struct cred)` | `0xb8` |
| `cred.uid` | `0x08` |
| `cred.cap_effective` / `cap_bset` / `cap_ambient` | `0x40` / `0x48` / `0x50` |
| `cred.security` / `user_ns` | `0x80` / `0x90` |
| `sizeof(k_itimer)` | `0x100` |
| `k_itimer.it.cpu.node` | `0x78` |
| `k_itimer.rcu` | `0xf0` |
| `sizeof(selinux_state)` | `0x80` |
| `selinux_state.enforcing` / `initialized` | `0x0` / `0x1` |
| SELinux 写目标 | `selinux_state - 7` |

`CAP_FULL=0x1ffffffffff`。task cache 回退值为 5696 bytes、5 objects/slab、8 pages；运行时读取 `/proc/slabinfo` 并覆盖回退值。POSIX timer 为 256 bytes、32 objects/slab、order-1、8192-byte fragment page。

`GEOM_MEMSTART_PFN=0x80000` 为静态推断值，首次上机需复核，不能把其他启动周期的 slide 写入 profile。

## 64560 锚点

```text
worker_thread           0xffffffc080b2f74c
rcu_gp_fqs_loop         0xffffffc080d21be4
rcu_gp_kthread          0xffffffc080d20f08
folio_wait_bit_common   0xffffffc08010c158
ufshcd_issue_dev_cmd    0xffffffc080f25c6c
icc_set_bw              0xffffffc080b009cc
prepare_creds           0xffffffc080df532c
selinux_state           0xffffffc082656570
```

`syms_cd85666.txt` 共 226 项，全部解析，静态 `ko_patch` 输出 `PATCHED=226 MISSING=0`，vermagic 与目标内核一致。零 slide 仅用于静态校验。

## enforcing 与刷写准入

98260 使用 3.2.0 已修复的 `rootd.c`，不是旧版 `system_server` 域实现：

1. rootd 启动 watcher，每 200 ms 检查并维持 SELinux Permissive；ROM 再次恢复 Enforcing 时会继续处理。
2. 每次命令执行前再次检查 Permissive，命令子进程切换到 `u:r:init:s0`。
3. 上下文或 SELinux 准备失败时输出 `ROOT98260_CHILD_POLICY_FAILED` 并退出，不执行该命令。
4. rootd 建立或复用后，通过 `root-command.sh` 调用 `block_write_probe`。探测器要求 UID 0、CAP_SYS_ADMIN，并对实际 ABL/init_boot/userdata 块设备执行 `O_RDWR|O_CLOEXEC` 打开后立即关闭。
5. 只有上述探测成功，应用才登记后续备份/刷写使用的 root 通道。探测不写分区、不截断、不调用 `blockdev --setrw`；原有分区调整与刷写流程继续使用各自的写入和回读校验。

该方案用于避免 enforcing 在提权后或刷写期间再次拒绝命令。真实设备上的成功仍需用 `WRITE_OPEN_OK`、进程上下文和实际写入回读共同证明，不能只根据 UID 0 或静态构建宣称刷写必然成功。

## 内置执行文件

使用与此前 APH 适配相同的漏洞链和 NDK r29 静态编译；`rootd.c` 已包含上述 enforcing 修复。

| 文件 | SHA-256 |
| --- | --- |
| `exploit-android-key-ptr` | `294611f81f911fbd3f6e74f533b97dc20b4bdf88f05c4812f57bb2ed823883d3` |
| `pc98260-su` | `2ad971c4eb50a7002e53671b8f7f107f5c7ad81bbdb26b4da2d250c0ec947321` |
| `kaslr_slide_cd85666` | `9b88017d8bf076f49ec1df84cc7a44bed179ffa569bff63ec1a14f31eadbabc8` |

嵌入位置：`app/src/main/assets/root_extra/cd85666/`。构建脚本：

```powershell
powershell -File adaptation/cd85666/build-cd85666.ps1 `
  -NdkHome D:\AndroidSDK\ndk\android-ndk-r29 -Install
```

`NewKernelRootAdapterBase` 通过 `NEW_ROOT_KERNEL_CD856` 检查精确内核。`Cd85698260Adapter` 与 `Cd85664650Adapter` 实现部署、提权、命令执行与清理，不使用 `pending(...)` 占位实现。应用版本为 `versionName=3.2.1`、`versionCode=121`。

## 验证边界

已完成 boot/kernel 提取、BTF 核对、反汇编、静态编译、符号检查及 3.2.0 enforcing 接口接入。没有连接与 cd85666 完全匹配的设备，因此未宣称真实 UAF、rootd 或块设备写入已经成功。首次上机应先执行只读 preflight，再验证 rootd 的 UID/上下文/Permissive 和 `WRITE_OPEN_OK`，最后才进入备份和刷写流程。切换提权链前按原流程重启。
