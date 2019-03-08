#define FUSE_USE_VERSION 29

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE

#ifdef linux
/* For pread()/pwrite()/utimensat() */
#define _XOPEN_SOURCE 700
#endif

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

static char source_dir[256];

// ----- helper functions -----

// prepend all pathes in the imcoming request with the source dir.
static void prepend_source_dir(char *new_path, const char *origin)
{
	strcat(new_path, source_dir);

	// SOURCE_DIR/ is not valid path.
	if (strcmp("/", origin) == 0)
		return;

	strcat(new_path, origin);
}

// fix mode to all users can R/W/X.
static void fix_mode(mode_t *mode)
{
	*mode |= (S_IRWXU | S_IRWXG | S_IRWXO);
}

static void *puredata_init(struct fuse_conn_info *conn)
{
	(void) conn;

	memset(source_dir, 0, 256);

	strcat(source_dir, "/tmp/from");

	return NULL;
}

static int puredata_getattr(const char *path, struct stat *stbuf)
{
	int res;
	char new_path[256] = {0};

	prepend_source_dir((char*)new_path, path);

	res = lstat(new_path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int puredata_access(const char *path, int mask)
{
	int res;
	char new_path[256] = {0};

	prepend_source_dir((char*)new_path, path);

	printf("%s, %s, %s\n", path, new_path, strcmp(new_path, "/tmp/from") == 0 ? "true" : "false");

	res = access(new_path, mask);
	if (res == -1)
		return -errno;

	return 0;
}

static int puredata_readlink(const char *path, char *buf, size_t size)
{
	int res;
	char new_path[256] = {0};

	prepend_source_dir((char*)new_path, path);

	res = readlink(new_path, buf, size - 1);
	if (res == -1)
		return -errno;

	buf[res] = '\0';
	return 0;
}


static int puredata_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
	DIR *dp;
	struct dirent *de;
	char new_path[256] = {0};

	(void) offset;
	(void) fi;

	prepend_source_dir((char*)new_path, path);

	dp = opendir(new_path);
	if (dp == NULL)
		return -errno;

	while ((de = readdir(dp)) != NULL) {
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		if (filler(buf, de->d_name, &st, 0))
			break;
	}

	closedir(dp);
	return 0;
}

static int puredata_mknod(const char *path, mode_t mode, dev_t rdev)
{
	int res;
	char new_path[256] = {0};

	fix_mode(&mode);
	prepend_source_dir((char*)new_path, path);

	/* On Linux this could just be 'mknod(path, mode, rdev)' but this
	   is more portable */
	if (S_ISREG(mode)) {
		res = open(new_path, O_CREAT | O_EXCL | O_WRONLY, mode);
		if (res >= 0)
			res = close(res);
	} else if (S_ISFIFO(mode))
		res = mkfifo(new_path, mode);
	else
		res = mknod(new_path, mode, rdev);
	if (res == -1)
		return -errno;

	return 0;
}

static int puredata_mkdir(const char *path, mode_t mode)
{
	int res;
	char new_path[256] = {0};

	fix_mode(&mode);
	prepend_source_dir((char*)new_path, path);

	res = mkdir(new_path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int puredata_unlink(const char *path)
{
	int res;
	char new_path[256] = {0};

	prepend_source_dir((char*)new_path, path);

	res = unlink(new_path);
	if (res == -1)
		return -errno;

	return 0;
}

static int puredata_rmdir(const char *path)
{
	int res;
	char new_path[256] = {0};

	prepend_source_dir((char*)new_path, path);

	res = rmdir(new_path);
	if (res == -1)
		return -errno;

	return 0;
}

static int puredata_symlink(const char *from, const char *to)
{
	int res;
	char new_from[256] = {0};
	char new_to[256] = {0};

	prepend_source_dir((char*)new_from, from);
	prepend_source_dir((char*)new_to, to);

	res = symlink(new_from, new_to);
	if (res == -1)
		return -errno;

	return 0;
}

static int puredata_rename(const char *from, const char *to)
{
	int res;
	char new_from[256] = {0};
	char new_to[256] = {0};

	prepend_source_dir((char*)new_from, from);
	prepend_source_dir((char*)new_to, to);

	res = rename(new_from, new_to);
	if (res == -1)
		return -errno;

	return 0;
}

static int puredata_link(const char *from, const char *to)
{
	int res;
	char new_from[256] = {0};
	char new_to[256] = {0};

	prepend_source_dir((char*)new_from, from);
	prepend_source_dir((char*)new_to, to);

	res = link(new_from, new_to);
	if (res == -1)
		return -errno;

	return 0;
}

static int puredata_chmod(const char *path, mode_t mode)
{
	int res;
	char new_path[256] = {0};

	fix_mode(&mode);
	prepend_source_dir((char*)new_path, path);

	res = chmod(new_path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int puredata_chown(const char *path, uid_t uid, gid_t gid)
{
	int res;
	char new_path[256] = {0};

	prepend_source_dir((char*)new_path, path);

	res = lchown(new_path, uid, gid);
	if (res == -1)
		return -errno;

	return 0;
}

static int puredata_truncate(const char *path, off_t size)
{
	int res;
	char new_path[256] = {0};

	prepend_source_dir((char*)new_path, path);

	res = truncate(new_path, size);

	if (res == -1)
		return -errno;

	return 0;
}

#ifdef HAVE_UTIMENSAT
static int puredata_utimens(const char *path, const struct timespec ts[2],
		       struct fuse_file_info *fi)
{
	(void) fi;
	int res;
	char new_path[256] = {0};

	prepend_source_dir((char*)new_path, path);

	/* don't use utime/utimes since they follow symlinks */
	res = utimensat(0, new_path, ts, AT_SYMLINK_NOFOLLOW);
	if (res == -1)
		return -errno;

	return 0;
}
#endif

static int puredata_create(const char *path, mode_t mode,
		      struct fuse_file_info *fi)
{
	int res;
	char new_path[256] = {0};

	fix_mode(&mode);
	prepend_source_dir((char*)new_path, path);

	res = open(new_path, fi->flags, mode);
	if (res == -1)
		return -errno;

	fi->fh = res;
	return 0;
}

static int puredata_open(const char *path, struct fuse_file_info *fi)
{
	int res;
	char new_path[256] = {0};

	prepend_source_dir((char*)new_path, path);

	res = open(new_path, fi->flags);
	if (res == -1)
		return -errno;

	fi->fh = res;
	return 0;
}

static int puredata_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	int fd;
	int res;
	char new_path[256] = {0};

	prepend_source_dir((char*)new_path, path);

	if(fi == NULL)
		fd = open(new_path, O_RDONLY);
	else
		fd = fi->fh;

	if (fd == -1)
		return -errno;

	res = pread(fd, buf, size, offset);
	if (res == -1)
		res = -errno;

	if(fi == NULL)
		close(fd);
	return res;
}

static int puredata_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	int fd;
	int res;
	char new_path[256] = {0};

	prepend_source_dir((char*)new_path, path);

	(void) fi;
	if(fi == NULL)
		fd = open(new_path, O_WRONLY);
	else
		fd = fi->fh;

	if (fd == -1)
		return -errno;

	res = pwrite(fd, buf, size, offset);
	if (res == -1)
		res = -errno;

	if(fi == NULL)
		close(fd);
	return res;
}

static int puredata_statfs(const char *path, struct statvfs *stbuf)
{
	int res;
	char new_path[256] = {0};

	prepend_source_dir((char*)new_path, path);

	res = statvfs(new_path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int puredata_release(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	close(fi->fh);
	return 0;
}

static int puredata_fsync(const char *path, int isdatasync,
		     struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

	(void) path;
	(void) isdatasync;
	(void) fi;
	return 0;
}

#ifdef HAVE_POSIX_FALLOCATE
static int puredata_fallocate(const char *path, int mode,
			off_t offset, off_t length, struct fuse_file_info *fi)
{
	int fd;
	int res;
	char new_path[256] = {0};

	prepend_source_dir((char*)new_path, path);

	(void) fi;

	if (mode)
		return -EOPNOTSUPP;

	if(fi == NULL)
		fd = open(new_path, O_WRONLY);
	else
		fd = fi->fh;

	if (fd == -1)
		return -errno;

	res = -posix_fallocate(fd, offset, length);

	if(fi == NULL)
		close(fd);
	return res;
}
#endif

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int puredata_setxattr(const char *path, const char *name, const char *value,
			size_t size, int flags)
{
	char new_path[256] = {0};

	prepend_source_dir((char*)new_path, path);

	int res = lsetxattr(new_path, name, value, size, flags);
	if (res == -1)
		return -errno;
	return 0;
}

static int puredata_getxattr(const char *path, const char *name, char *value,
			size_t size)
{
	char new_path[256] = {0};

	prepend_source_dir((char*)new_path, path);

	int res = lgetxattr(new_path, name, value, size);
	if (res == -1)
		return -errno;
	return res;
}

static int puredata_listxattr(const char *path, char *list, size_t size)
{
	char new_path[256] = {0};

	prepend_source_dir((char*)new_path, path);

	int res = llistxattr(new_path, list, size);
	if (res == -1)
		return -errno;
	return res;
}

static int puredata_removexattr(const char *path, const char *name)
{
	char new_path[256] = {0};

	prepend_source_dir((char*)new_path, path);

	int res = lremovexattr(new_path, name);
	if (res == -1)
		return -errno;
	return 0;
}
#endif /* HAVE_SETXATTR */

#ifdef HAVE_COPY_FILE_RANGE
static ssize_t puredata_copy_file_range(const char *path_in,
				   struct fuse_file_info *fi_in,
				   off_t offset_in, const char *path_out,
				   struct fuse_file_info *fi_out,
				   off_t offset_out, size_t len, int flags)
{
	int fd_in, fd_out;
	ssize_t res;
	char new_path_in[256] = {0};
	char new_path_out[256] = {0};

	prepend_source_dir((char*)new_path_in, path_in);
	prepend_source_dir((char*)new_path_out, path_out);

	if(fi_in == NULL)
		fd_in = open(new_path_in, O_RDONLY);
	else
		fd_in = fi_in->fh;

	if (fd_in == -1)
		return -errno;

	if(fi_out == NULL)
		fd_out = open(new_path_out, O_WRONLY);
	else
		fd_out = fi_out->fh;

	if (fd_out == -1) {
		close(fd_in);
		return -errno;
	}

	res = copy_file_range(fd_in, &offset_in, fd_out, &offset_out, len,
			      flags);
	if (res == -1)
		res = -errno;

	close(fd_in);
	close(fd_out);

	return res;
}
#endif

static struct fuse_operations puredata_oper = {
	.init           = puredata_init,
	.getattr	= puredata_getattr,
	.access		= puredata_access,
	.readlink	= puredata_readlink,
	.readdir	= puredata_readdir,
	.mknod		= puredata_mknod,
	.mkdir		= puredata_mkdir,
	.symlink	= puredata_symlink,
	.unlink		= puredata_unlink,
	.rmdir		= puredata_rmdir,
	.rename		= puredata_rename,
	.link		= puredata_link,
	.chmod		= puredata_chmod,
	.chown		= puredata_chown,
	.truncate	= puredata_truncate,
#ifdef HAVE_UTIMENSAT
	.utimens	= puredata_utimens,
#endif
	.open		= puredata_open,
	.create 	= puredata_create,
	.read		= puredata_read,
	.write		= puredata_write,
	.statfs		= puredata_statfs,
	.release	= puredata_release,
	.fsync		= puredata_fsync,
#ifdef HAVE_POSIX_FALLOCATE
	.fallocate	= puredata_fallocate,
#endif
#ifdef HAVE_SETXATTR
	.setxattr	= puredata_setxattr,
	.getxattr	= puredata_getxattr,
	.listxattr	= puredata_listxattr,
	.removexattr	= puredata_removexattr,
#endif
#ifdef HAVE_COPY_FILE_RANGE
	.copy_file_range = puredata_copy_file_range,
#endif
};

int main(int argc, char *argv[])
{
	umask(0);
	return fuse_main(argc, argv, &puredata_oper, NULL);
}
