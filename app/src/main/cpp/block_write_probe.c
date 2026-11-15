#include <errno.h>
#include <fcntl.h>
#include <linux/capability.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

static int has_sys_admin(void)
{
	struct __user_cap_header_struct header = {
		.version = _LINUX_CAPABILITY_VERSION_3,
		.pid = 0,
	};
	struct __user_cap_data_struct data[2] = { {0}, {0} };
	if (syscall(SYS_capget, &header, data) != 0) {
		printf("WRITE_PROBE_CAPGET_ERR=%d\n", errno);
		return 0;
	}
	return (data[0].effective & (1U << CAP_SYS_ADMIN)) != 0;
}

int main(void)
{
	static const char *paths[] = {
		"/dev/block/by-name/abl_a",
		"/dev/block/by-name/abl_b",
		"/dev/block/by-name/init_boot_a",
		"/dev/block/by-name/init_boot_b",
		"/dev/block/by-name/userdata",
		"/dev/block/mapper/userdata",
	};
	if (geteuid() != 0 || !has_sys_admin()) {
		printf("WRITE_PROBE_ID_OR_CAP_FAIL uid=%d\n", geteuid());
		return 1;
	}
	for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
		struct stat st;
		if (stat(paths[i], &st) != 0 || !S_ISBLK(st.st_mode)) continue;
		int fd = open(paths[i], O_RDWR | O_CLOEXEC);
		if (fd < 0) {
			printf("WRITE_OPEN_ERR=%s path=%s\n", strerror(errno), paths[i]);
			continue;
		}
		close(fd);
		printf("WRITE_OPEN_OK path=%s\n", paths[i]);
		return 0;
	}
	printf("WRITE_OPEN_NO_BLOCK_DEVICE\n");
	return 1;
}
