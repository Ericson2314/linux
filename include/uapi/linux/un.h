/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _LINUX_UN_H
#define _LINUX_UN_H

#include <linux/socket.h>

#define UNIX_PATH_MAX	108

struct sockaddr_un {
	__kernel_sa_family_t sun_family; /* AF_UNIX */
	char sun_path[UNIX_PATH_MAX];	/* pathname */
};

struct sockaddr_un2 {
	__kernel_sa_family_t sun2_family; /* AF_UNIX2 */
	__s32 sun2_dfd;			/* directory fd, or AT_FDCWD */
	const char *sun2_path;		/* pointer to null-terminated path */
	__u32 sun2_flags;		/* reserved, must be 0 */
};

#define SIOCUNIXFILE (SIOCPROTOPRIVATE + 0) /* open a socket file with O_PATH */

#endif /* _LINUX_UN_H */
