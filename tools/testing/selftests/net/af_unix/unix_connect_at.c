// SPDX-License-Identifier: GPL-2.0

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "kselftest_harness.h"

#ifndef AF_UNIX2
#define AF_UNIX2 46
#endif

struct sockaddr_un2 {
	sa_family_t sun2_family;
	int sun2_dfd;
	const char *sun2_path;
	unsigned int sun2_flags;
};

FIXTURE(af_unix2)
{
	int server, client;
	int dirfd;
	char tmpdir[64];
};

FIXTURE_VARIANT(af_unix2)
{
	int type;
};

FIXTURE_VARIANT_ADD(af_unix2, stream)
{
	.type = SOCK_STREAM,
};

FIXTURE_VARIANT_ADD(af_unix2, dgram)
{
	.type = SOCK_DGRAM,
};

FIXTURE_VARIANT_ADD(af_unix2, seqpacket)
{
	.type = SOCK_SEQPACKET,
};

FIXTURE_SETUP(af_unix2)
{
	snprintf(self->tmpdir, sizeof(self->tmpdir),
		 "/tmp/af_unix2_test.%d", getpid());
	ASSERT_EQ(0, mkdir(self->tmpdir, 0700));
	self->dirfd = open(self->tmpdir, O_RDONLY | O_DIRECTORY);
	ASSERT_LE(0, self->dirfd);
	self->server = -1;
	self->client = -1;
}

FIXTURE_TEARDOWN(af_unix2)
{
	if (self->client >= 0)
		close(self->client);
	if (self->server >= 0)
		close(self->server);
	unlinkat(self->dirfd, "sock", 0);
	close(self->dirfd);
	rmdir(self->tmpdir);
}

/* bind with AF_UNIX2, dirfd, and relative path works */
TEST_F(af_unix2, bind_relative)
{
	struct sockaddr_un2 addr = {
		.sun2_family = AF_UNIX2,
		.sun2_dfd = self->dirfd,
		.sun2_path = "sock",
		.sun2_flags = 0,
	};
	struct stat st;
	char path[128];

	self->server = socket(AF_UNIX, variant->type, 0);
	ASSERT_LE(0, self->server);

	ASSERT_EQ(0, bind(self->server, (struct sockaddr *)&addr,
			  sizeof(addr)));

	/* Verify socket file was created in the right directory */
	snprintf(path, sizeof(path), "%s/sock", self->tmpdir);
	ASSERT_EQ(0, stat(path, &st));
	ASSERT_TRUE(S_ISSOCK(st.st_mode));
}

/* connect with AF_UNIX2 and dirfd works */
TEST_F(af_unix2, connect_relative)
{
	struct sockaddr_un2 addr = {
		.sun2_family = AF_UNIX2,
		.sun2_dfd = self->dirfd,
		.sun2_path = "sock",
		.sun2_flags = 0,
	};

	self->server = socket(AF_UNIX, variant->type, 0);
	ASSERT_LE(0, self->server);

	ASSERT_EQ(0, bind(self->server, (struct sockaddr *)&addr,
			  sizeof(addr)));

	if (variant->type == SOCK_STREAM || variant->type == SOCK_SEQPACKET) {
		ASSERT_EQ(0, listen(self->server, 1));
	}

	self->client = socket(AF_UNIX, variant->type, 0);
	ASSERT_LE(0, self->client);

	ASSERT_EQ(0, connect(self->client, (struct sockaddr *)&addr,
			     sizeof(addr)));
}

/* AT_FDCWD with AF_UNIX2 behaves like regular bind/connect */
TEST_F(af_unix2, at_fdcwd)
{
	char path[128];
	struct sockaddr_un2 addr = {
		.sun2_family = AF_UNIX2,
		.sun2_dfd = AT_FDCWD,
		.sun2_flags = 0,
	};

	snprintf(path, sizeof(path), "%s/sock", self->tmpdir);
	addr.sun2_path = path;

	self->server = socket(AF_UNIX, variant->type, 0);
	ASSERT_LE(0, self->server);

	ASSERT_EQ(0, bind(self->server, (struct sockaddr *)&addr,
			  sizeof(addr)));

	if (variant->type == SOCK_STREAM || variant->type == SOCK_SEQPACKET) {
		ASSERT_EQ(0, listen(self->server, 1));
	}

	self->client = socket(AF_UNIX, variant->type, 0);
	ASSERT_LE(0, self->client);

	ASSERT_EQ(0, connect(self->client, (struct sockaddr *)&addr,
			     sizeof(addr)));
}

/* Non-zero flags are rejected */
TEST_F(af_unix2, bad_flags)
{
	struct sockaddr_un2 addr = {
		.sun2_family = AF_UNIX2,
		.sun2_dfd = self->dirfd,
		.sun2_path = "sock",
		.sun2_flags = 1,
	};

	self->server = socket(AF_UNIX, variant->type, 0);
	ASSERT_LE(0, self->server);

	ASSERT_EQ(-1, bind(self->server, (struct sockaddr *)&addr,
			   sizeof(addr)));
	ASSERT_EQ(EINVAL, errno);

	ASSERT_EQ(-1, connect(self->server, (struct sockaddr *)&addr,
			      sizeof(addr)));
	ASSERT_EQ(EINVAL, errno);
}

/* Wrong addrlen is rejected */
TEST_F(af_unix2, bad_addrlen)
{
	struct sockaddr_un2 addr = {
		.sun2_family = AF_UNIX2,
		.sun2_dfd = self->dirfd,
		.sun2_path = "sock",
		.sun2_flags = 0,
	};

	self->server = socket(AF_UNIX, variant->type, 0);
	ASSERT_LE(0, self->server);

	ASSERT_EQ(-1, bind(self->server, (struct sockaddr *)&addr,
			   sizeof(addr) - 1));
	ASSERT_EQ(EINVAL, errno);
}

/* bind with a bad dirfd fails */
TEST_F(af_unix2, bad_dirfd)
{
	struct sockaddr_un2 addr = {
		.sun2_family = AF_UNIX2,
		.sun2_dfd = 9999,
		.sun2_path = "sock",
		.sun2_flags = 0,
	};

	self->server = socket(AF_UNIX, variant->type, 0);
	ASSERT_LE(0, self->server);

	ASSERT_EQ(-1, bind(self->server, (struct sockaddr *)&addr,
			   sizeof(addr)));
	ASSERT_EQ(EBADF, errno);
}

TEST_HARNESS_MAIN
