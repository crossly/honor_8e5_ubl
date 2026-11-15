# honor_ubl 3.2.0：98260 提权后无法刷写的修复

## 原因定位

3.1.9 中两条链的身份与命令路径不同：

- 98260 使用 fake-cred rootd、`pc98260-su` 和 `root-command.sh`；rootd 把命令子进程切到 `u:r:system_server:s0`。
- 64650 使用 `bfexec`，通过已有 KSU 授权接口执行 `setresuid/setresgid`、fsuid/fsgid 修正和 shell 命令。

98260 的载荷源码明确记录了 ROM 会在短时间内重新恢复 SELinux Enforcing。旧 rootd 在建立后没有继续处理这种状态变化，命令子进程又使用不适合该工具原始块设备和应用脚本的 `system_server` 域。应用在提权成功时只检查 UID/GID，没有验证 CAP_SYS_ADMIN 或块设备 RW-open，因此“可以读取备份”不能证明后续可以刷写。

该结论来自源码路径对照。没有收到实际 `execute.log` 或 AVC 日志，因此不把其他可能的只读硬件、分区映射或镜像错误混作同一个原因。3.1.9 已有的 `blockdev --setrw` 和备份/刷写流程不需要改变。

## 修复内容

### rootd

`adaptation/aph136g142/src/rootd.c` 新增：

1. 读取 `/sys/fs/selinux/enforce`，在需要时通过该接口写入 `0` 并读回确认。
2. rootd 运行期间每 200 ms 检查一次，持续保持 Permissive；不恢复 Enforcing。重启后该临时提权自然失效。
3. 命令子进程使用 `u:r:init:s0`，不再使用 `u:r:system_server:s0`。
4. SELinux 或上下文准备失败时输出 `ROOT98260_CHILD_POLICY_FAILED` 并退出，不执行该命令。
5. watcher 检测其父进程是否仍是 rootd；rootd 结束后 watcher 退出。

不增加 KSU 管理器或新的 KSU handoff。

### 应用准入

`app/src/main/cpp/block_write_probe.c` 检查：

- 当前有效 UID 必须为 0；
- 当前进程必须有 CAP_SYS_ADMIN；
- 依次寻找 ABL、init_boot 或 userdata 的真实块设备，以 `O_RDWR|O_CLOEXEC` 打开后立即关闭。

成功输出 `WRITE_OPEN_OK path=...`。没有写入、截断、分区修改或 `blockdev --setrw` 调用。若失败，应用不再把该通道登记为可用于后续刷写的 root。

`Root98260Adapter`、`NewKernel98260Adapter`、`G14298260Adapter` 均在 rootd 建立或复用时执行探测。旧版 rootd 缺少新策略时，错误提示要求重启后重新提权，避免旧 rootd 上重复驱动漏洞。

## 重新编译的执行文件

共用原有 `uaf_plant_61258.c`、`stubs.c` 和修改后的 `rootd.c`，使用 NDK r29 静态编译：

| profile | `exploit-android-key-ptr` SHA-256 |
| --- | --- |
| 61258（图片中的 g3a7a） | `5b867d47132e0367004a00d05e1df13cdac3b1e7f30a6a8183cb47eed751e1cd` |
| 6138 | `ae4fcf42cb455c33d873b4dbe6c538613d92d88cf736144474a7ad3c023eef45` |
| aph112 | `89cbe0803de1be327c43f400821669c90b8bf67458dd56830ab0fce287cb5f44` |
| aph136g142 | `089052bf4b4bc8a8b178193ae86f51d824f9cd33cb07d3f8a9267e6dea823823` |

重建命令：

```powershell
powershell -ExecutionPolicy Bypass -File adaptation/rootd-write-fix/build-rootd-write-fix.ps1 `
  -NdkHome D:\AndroidSDK\ndk\android-ndk-r29 -Install
```

构建脚本会写回四个 profile 的 APK assets，并更新各目录 `SHA256SUMS.txt` 中的 exploit 哈希；其他文件保持原值。

## 回归与验证边界

新增 `RootdWriteAccessTest`，检查 rootd 的 Permissive 维护、init 上下文、失败边界、watcher 顺序，以及探测器的 CAP 和只打开不写约束。

标准构建：

```powershell
.\gradlew.bat :app:assembleRelease :app:test
```

本次没有连接可执行 98260 的目标设备。源码修复、静态编译和主机/应用回归不等于真实块设备写入已经验证。需要在目标设备重启后重新建立 98260 rootd，先确认 `WRITE_OPEN_OK`，再执行原有备份和刷写流程；不要用旧 rootd 的结果代替新版本验证。

`block_write_probe` 的 SHA-256 为 `14f48d9b754068bed9d0ce6dc9ea1c86d8be1db3470882635e1a27fb15e7379e`。3.2.0 的 `versionCode=120`；3.2.1 项目测试共 535 项，失败 0、错误 0、跳过 7。APK 内四个载荷和探测工具的哈希均已与编译输出核对。
