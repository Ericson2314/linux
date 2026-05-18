// SPDX-License-Identifier: GPL-2.0

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <unistd.h>

#include "kselftest_harness.h"

#ifndef __NR_bindat
#define __NR_bindat 472
#endif

#ifndef __NR_connectat
#define __NR_connectat 473
#endif

static int sys_bindat(int dfd, int fd, const struct sockaddr *addr,
		      socklen_t addrlen, int flags)
{
	return syscall(__NR_bindat, dfd, fd, addr, addrlen, flags);
}

static int sys_connectat(int dfd, int fd, const struct sockaddr *addr,
			 socklen_t addrlen, int flags)
{
	return syscall(__NR_connectat, dfd, fd, addr, addrlen, flags);
}

FIXTURE(bindat_connectat)
{
	int server, client;
	int dirfd;
	char tmpdir[64];
};

FIXTURE_VARIANT(bindat_connectat)
{
	int type;
};

FIXTURE_VARIANT_ADD(bindat_connectat, stream)
{
	.type = SOCK_STREAM,
};

FIXTURE_VARIANT_ADD(bindat_connectat, dgram)
{
	.type = SOCK_DGRAM,
};

FIXTURE_VARIANT_ADD(bindat_connectat, seqpacket)
{
	.type = SOCK_SEQPACKET,
};

FIXTURE_SETUP(bindat_connectat)
{
	snprintf(self->tmpdir, sizeof(self->tmpdir),
		 "/tmp/bindat_test.%d", getpid());
	ASSERT_EQ(0, mkdir(self->tmpdir, 0700));
	self->dirfd = open(self->tmpdir, O_RDONLY | O_DIRECTORY);
	ASSERT_LE(0, self->dirfd);
	self->server = -1;
	self->client = -1;
}

FIXTURE_TEARDOWN(bindat_connectat)
{
	if (self->client >= 0)
		close(self->client);
	if (self->server >= 0)
		close(self->server);
	unlinkat(self->dirfd, "sock", 0);
	close(self->dirfd);
	rmdir(self->tmpdir);
}

/* bindat with a dirfd and relative path works */
TEST_F(bindat_connectat, bindat_relative)
{
	struct sockaddr_un addr = {
		.sun_family = AF_UNIX,
	};
	socklen_t addrlen;
	struct stat st;
	char path[128];

	strcpy(addr.sun_path, "sock");
	addrlen = offsetof(struct sockaddr_un, sun_path) + strlen("sock") + 1;

	self->server = socket(AF_UNIX, variant->type, 0);
	ASSERT_LE(0, self->server);

	ASSERT_EQ(0, sys_bindat(self->dirfd, self->server,
				(struct sockaddr *)&addr, addrlen, 0));

	/* Verify socket file was created in the right directory */
	snprintf(path, sizeof(path), "%s/sock", self->tmpdir);
	ASSERT_EQ(0, stat(path, &st));
	ASSERT_TRUE(S_ISSOCK(st.st_mode));
}

/* connectat with a dirfd and relative path works */
TEST_F(bindat_connectat, connectat_relative)
{
	struct sockaddr_un addr = {
		.sun_family = AF_UNIX,
	};
	socklen_t addrlen;

	strcpy(addr.sun_path, "sock");
	addrlen = offsetof(struct sockaddr_un, sun_path) + strlen("sock") + 1;

	self->server = socket(AF_UNIX, variant->type, 0);
	ASSERT_LE(0, self->server);

	ASSERT_EQ(0, sys_bindat(self->dirfd, self->server,
				(struct sockaddr *)&addr, addrlen, 0));

	if (variant->type == SOCK_STREAM || variant->type == SOCK_SEQPACKET) {
		ASSERT_EQ(0, listen(self->server, 1));
	}

	self->client = socket(AF_UNIX, variant->type, 0);
	ASSERT_LE(0, self->client);

	ASSERT_EQ(0, sys_connectat(self->dirfd, self->client,
				   (struct sockaddr *)&addr, addrlen, 0));
}

/* AT_FDCWD behaves like regular bind/connect */
TEST_F(bindat_connectat, at_fdcwd)
{
	struct sockaddr_un addr = {
		.sun_family = AF_UNIX,
	};
	socklen_t addrlen;
	char path[128];

	snprintf(path, sizeof(path), "%s/sock", self->tmpdir);
	strcpy(addr.sun_path, path);
	addrlen = offsetof(struct sockaddr_un, sun_path) + strlen(path) + 1;

	self->server = socket(AF_UNIX, variant->type, 0);
	ASSERT_LE(0, self->server);

	ASSERT_EQ(0, sys_bindat(AT_FDCWD, self->server,
				(struct sockaddr *)&addr, addrlen, 0));

	if (variant->type == SOCK_STREAM || variant->type == SOCK_SEQPACKET) {
		ASSERT_EQ(0, listen(self->server, 1));
	}

	self->client = socket(AF_UNIX, variant->type, 0);
	ASSERT_LE(0, self->client);

	ASSERT_EQ(0, sys_connectat(AT_FDCWD, self->client,
				   (struct sockaddr *)&addr, addrlen, 0));
}

/* Non-zero flags are rejected */
TEST_F(bindat_connectat, bad_flags)
{
	struct sockaddr_un addr = {
		.sun_family = AF_UNIX,
		.sun_path = "sock",
	};
	socklen_t addrlen;

	addrlen = offsetof(struct sockaddr_un, sun_path) + strlen("sock") + 1;

	self->server = socket(AF_UNIX, variant->type, 0);
	ASSERT_LE(0, self->server);

	ASSERT_EQ(-1, sys_bindat(self->dirfd, self->server,
				 (struct sockaddr *)&addr, addrlen, 1));
	ASSERT_EQ(EINVAL, errno);

	ASSERT_EQ(-1, sys_connectat(self->dirfd, self->server,
				    (struct sockaddr *)&addr, addrlen, 1));
	ASSERT_EQ(EINVAL, errno);
}

/* Non-AF_UNIX socket with dfd != AT_FDCWD returns EOPNOTSUPP */
TEST_F(bindat_connectat, non_unix_eopnotsupp)
{
	struct sockaddr_in addr = {
		.sin_family = AF_INET,
	};
	int fd;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		SKIP(return, "AF_INET socket not available");

	ASSERT_EQ(-1, sys_bindat(self->dirfd, fd,
				 (struct sockaddr *)&addr, sizeof(addr), 0));
	ASSERT_EQ(EOPNOTSUPP, errno);

	ASSERT_EQ(-1, sys_connectat(self->dirfd, fd,
				    (struct sockaddr *)&addr, sizeof(addr), 0));
	ASSERT_EQ(EOPNOTSUPP, errno);

	close(fd);
}

/* Abstract sockets work with bindat (dfd is ignored for abstract) */
TEST_F(bindat_connectat, abstract_socket)
{
	struct sockaddr_un addr = {
		.sun_family = AF_UNIX,
	};
	socklen_t addrlen;
	char abstract_name[20];

	snprintf(abstract_name, sizeof(abstract_name), "bindat%d", getpid());
	addr.sun_path[0] = '\0';
	memcpy(addr.sun_path + 1, abstract_name, strlen(abstract_name));
	addrlen = offsetof(struct sockaddr_un, sun_path) + 1 + strlen(abstract_name);

	self->server = socket(AF_UNIX, variant->type, 0);
	ASSERT_LE(0, self->server);

	ASSERT_EQ(0, sys_bindat(self->dirfd, self->server,
				(struct sockaddr *)&addr, addrlen, 0));

	if (variant->type == SOCK_STREAM || variant->type == SOCK_SEQPACKET) {
		ASSERT_EQ(0, listen(self->server, 1));
	}

	self->client = socket(AF_UNIX, variant->type, 0);
	ASSERT_LE(0, self->client);

	ASSERT_EQ(0, sys_connectat(self->dirfd, self->client,
				   (struct sockaddr *)&addr, addrlen, 0));
}

/* bindat with a bad dirfd returns ENOENT (path resolution fails) */
TEST_F(bindat_connectat, bad_dirfd)
{
	struct sockaddr_un addr = {
		.sun_family = AF_UNIX,
		.sun_path = "sock",
	};
	socklen_t addrlen;

	addrlen = offsetof(struct sockaddr_un, sun_path) + strlen("sock") + 1;

	self->server = socket(AF_UNIX, variant->type, 0);
	ASSERT_LE(0, self->server);

	ASSERT_EQ(-1, sys_bindat(9999, self->server,
				 (struct sockaddr *)&addr, addrlen, 0));
	ASSERT_EQ(EBADF, errno);
}

TEST_HARNESS_MAIN
