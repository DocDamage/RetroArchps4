#ifndef ORBISFILE_H__
#define ORBISFILE_H__

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

#ifndef ORBISFILE_MAX_DIR_HANDLES
#define ORBISFILE_MAX_DIR_HANDLES 256
#endif

/* Compatibility shim for toolchains that do not provide liborbisFile. */
static DIR *orbisfile_dir_handles[ORBISFILE_MAX_DIR_HANDLES];

static inline int orbisFileInit(void)
{
   return 0;
}

static inline int orbisOpen(const char *path, int flags, int mode)
{
   return open(path, flags, mode);
}

static inline int orbisClose(int fd)
{
   return close(fd);
}

static inline ssize_t orbisRead(int fd, void *buf, size_t len)
{
   return read(fd, buf, len);
}

static inline ssize_t orbisWrite(int fd, const void *buf, size_t len)
{
   return write(fd, buf, len);
}

static inline off_t orbisLseek(int fd, off_t offset, int whence)
{
   return lseek(fd, offset, whence);
}

static inline int orbisMkdir(const char *path, mode_t mode)
{
   return mkdir(path, mode);
}

static inline int orbisDopen(const char *path)
{
   int i;
   DIR *dir = opendir(path);

   if (!dir)
      return -1;

   for (i = 0; i < ORBISFILE_MAX_DIR_HANDLES; i++)
   {
      if (!orbisfile_dir_handles[i])
      {
         orbisfile_dir_handles[i] = dir;
         return i;
      }
   }

   closedir(dir);
   errno = EMFILE;
   return -1;
}

static inline int orbisDread(int dfd, struct dirent *entry)
{
   struct dirent *next = NULL;

   if (dfd < 0 || dfd >= ORBISFILE_MAX_DIR_HANDLES || !orbisfile_dir_handles[dfd])
   {
      errno = EBADF;
      return -1;
   }

   errno = 0;
   next  = readdir(orbisfile_dir_handles[dfd]);
   if (!next)
      return (errno == 0) ? 0 : -1;

   if (entry)
      memcpy(entry, next, sizeof(*entry));

   return 1;
}

static inline int orbisDclose(int dfd)
{
   int ret;
   DIR *dir;

   if (dfd < 0 || dfd >= ORBISFILE_MAX_DIR_HANDLES || !orbisfile_dir_handles[dfd])
   {
      errno = EBADF;
      return -1;
   }

   dir = orbisfile_dir_handles[dfd];
   orbisfile_dir_handles[dfd] = NULL;
   ret = closedir(dir);

   return ret;
}

#endif
