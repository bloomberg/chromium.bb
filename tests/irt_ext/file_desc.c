/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include "native_client/src/include/nacl_assert.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/untrusted/irt/irt_dev.h"
#include "native_client/src/untrusted/irt/irt_extension.h"
#include "native_client/tests/irt_ext/file_desc.h"

/* Non-POSIX flags for open() helps with link logic. */
#ifndef O_NOFOLLOW
# define O_NOFOLLOW 0x8000
#endif

static struct file_desc_environment *g_activated_env = NULL;
static const struct inode_data g_default_inode = {
  .valid = false,
  .mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,
};

static struct inode_data g_root_dir = {
  .valid = true,
  .mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IFDIR,
};

/* Helper allocation functions. */
struct inode_data *allocate_inode(struct file_desc_environment *env) {
  for (int i = 0; i < NACL_ARRAY_SIZE(env->inode_datas); i++) {
    if (!env->inode_datas[i].valid) {
      init_inode_data(&env->inode_datas[i]);
      env->inode_datas[i].valid = true;
      return &env->inode_datas[i];
    }
  }

  return NULL;
}

int allocate_file_descriptor(struct file_desc_environment *env) {
  for (int i = 0; i < NACL_ARRAY_SIZE(env->file_descs); i++) {
    if (!env->file_descs[i].valid) {
      init_file_descriptor(&env->file_descs[i]);
      env->file_descs[i].valid = true;
      return i;
    }
  }

  return -1;
}

/* NACL_IRT_DEV_FDIO_v0_3 functions */
static int my_close(int fd) {
  if (g_activated_env &&
      fd >= 0 &&
      fd < NACL_ARRAY_SIZE(g_activated_env->file_descs) &&
      g_activated_env->file_descs[fd].valid) {
    g_activated_env->file_descs[fd].valid = false;
    return 0;
  }

  return EBADF;
}

static int my_dup(int fd, int *newfd) {
  return EBADF;
}

static int my_dup2(int fd, int newfd) {
  return EBADF;
}

static int my_read(int fd, void *buf, size_t count, size_t *nread) {
  if (g_activated_env &&
      fd >= 0 &&
      fd < NACL_ARRAY_SIZE(g_activated_env->file_descs) &&
      g_activated_env->file_descs[fd].valid &&
      g_activated_env->file_descs[fd].data != NULL) {
    struct inode_data *filedata = g_activated_env->file_descs[fd].data;
    size_t read_count = 0;
    if (count <= filedata->size && filedata->size - count >= filedata->position)
      read_count = count;
    else if (filedata->position < filedata->size)
      read_count = filedata->size - filedata->position;

    memcpy(buf, &filedata->content[filedata->position], read_count);
    filedata->position += read_count;

    *nread = read_count;
    return 0;
  }

  return EBADF;
}

static int my_write(int fd, const void *buf, size_t count, size_t *nwrote) {
  if (g_activated_env &&
      fd >= 0 &&
      fd < NACL_ARRAY_SIZE(g_activated_env->file_descs) &&
      g_activated_env->file_descs[fd].valid &&
      g_activated_env->file_descs[fd].data != NULL) {
    struct inode_data *filedata = g_activated_env->file_descs[fd].data;
    size_t write_count = 0;
    if (count <= NACL_ARRAY_SIZE(filedata->content) &&
        NACL_ARRAY_SIZE(filedata->content) - count >= filedata->position) {
      write_count = count;
    } else if (filedata->position < NACL_ARRAY_SIZE(filedata->content)) {
      write_count = NACL_ARRAY_SIZE(filedata->content) - filedata->position;
    }

    memcpy(&filedata->content[filedata->position], buf, write_count);
    filedata->position += write_count;
    if (filedata->size < filedata->position)
      filedata->size = filedata->position;

    *nwrote = write_count;
    return 0;
  }

  return EBADF;
}

static int my_seek(int fd, nacl_irt_off_t offset, int whence,
                  nacl_irt_off_t *new_offset) {
  if (g_activated_env &&
      fd >= 0 &&
      fd < NACL_ARRAY_SIZE(g_activated_env->file_descs) &&
      g_activated_env->file_descs[fd].valid &&
      g_activated_env->file_descs[fd].data != NULL) {
    struct inode_data *filedata = g_activated_env->file_descs[fd].data;
    off_t try_position = -1;
    switch (whence) {
      case SEEK_SET:
        try_position = offset;
        break;
      case SEEK_CUR:
        try_position = filedata->position + offset;
        break;
      case SEEK_END:
        try_position = filedata->size + offset;
        break;
      default:
        return EINVAL;
    }

    if (try_position < 0)
      return EINVAL;

    filedata->position = try_position;
    *new_offset = try_position;
    return 0;
  }

  return EBADF;
}

static int my_fstat(int fd, struct stat *buf) {
  return EBADF;
}

static int my_getdents(int fd, struct dirent *ents, size_t count,
                       size_t *nread) {
  return EBADF;
}

static int my_fchdir(int fd) {
  return EBADF;
}

static int my_fchmod(int fd, mode_t mode) {
  return EBADF;
}

static int my_fsync(int fd) {
  return EBADF;
}

static int my_fdatasync(int fd) {
  return EBADF;
}

static int my_ftruncate(int fd, nacl_irt_off_t length) {
  return EBADF;
}

static int my_isatty(int fd, int *result) {
  if (g_activated_env &&
      fd >= 0 &&
      fd < NACL_ARRAY_SIZE(g_activated_env->file_descs) &&
      g_activated_env->file_descs[fd].valid &&
      g_activated_env->file_descs[fd].data != NULL) {
    *result = (fd >= 0 && fd < 3);
    return 0;
  }

  return EBADF;
}

/* NACL_IRT_DEV_FILENAME_v0_3 functions */
static int my_open(const char *pathname, int oflag, mode_t cmode, int *newfd) {
  if (g_activated_env) {
    struct inode_data *parent_dir = NULL;
    struct inode_data *path_item = find_inode_path(g_activated_env,
                                                   pathname, &parent_dir);
    const char *filename = strrchr(pathname, '/');
    if (filename == NULL)
      filename = pathname;
    else
      filename = filename + 1;

    /* If parent directory is invalid, must not have been a directory. */
    if (parent_dir == NULL) {
      return ENOTDIR;
    }

    bool write_access = false;
    switch (oflag & O_ACCMODE) {
      case O_WRONLY:
      case O_RDWR:
        /* Both write and readwrite access has special checks later. */
        write_access = true;

      case O_RDONLY:
        break;
      default:
        return EINVAL;
    }

    /* If requested write/read write access, path cannot be a directory. */
    if (write_access && path_item && S_ISDIR(path_item->mode))
      return EISDIR;

    if (oflag & O_CREAT) {
      if (path_item == NULL) {
        if (0 == (parent_dir->mode & S_IWUSR))
          return EACCES;

        path_item = allocate_inode(g_activated_env);
        if (path_item == NULL) {
          return ENOSPC;
        }
        path_item->mode = cmode;
        path_item->parent_dir = parent_dir;

        size_t file_len = strlen(filename);
        if (file_len >= NACL_ARRAY_SIZE(path_item->name))
          return ENAMETOOLONG;

        memcpy(path_item->name, filename, file_len + 1);
        path_item->valid = true;
      } else if (oflag & O_EXCL) {
        return EEXIST;
      }
    }

    if (path_item && (oflag & O_NOFOLLOW) == 0) {
      int follow_count = 0;
      while (path_item->link) {
        path_item = path_item->link;

        follow_count++;
        if (follow_count > MAX_INODES) {
          return ELOOP;
        }
      }
    }

    if (write_access && (oflag & O_TRUNC) && path_item) {
      path_item->position = 0;
      path_item->size = 0;
    }

    int try_fd = allocate_file_descriptor(g_activated_env);
    if (try_fd == -1) {
      return ENFILE;
    }

    g_activated_env->file_descs[try_fd].oflag = oflag;
    g_activated_env->file_descs[try_fd].data = path_item;
    *newfd = try_fd;
    return 0;
  }

  return ENOSYS;
}

static int my_stat(const char *pathname, struct stat *buf) {
  return ENOSYS;
}

static int my_mkdir(const char *pathname, mode_t mode) {
  if (g_activated_env) {
    struct inode_data *parent_dir = NULL;
    struct inode_data *path_item = find_inode_path(g_activated_env,
                                                   pathname, &parent_dir);
    struct inode_data *new_dir = NULL;
    const char *dirname = strrchr(pathname, '/');
    if (dirname == NULL)
      dirname = pathname;
    else
      dirname = dirname + 1;

    /* If parent directory is invalid, must not have been a directory. */
    if (parent_dir == NULL)
      return ENOTDIR;

    /* Sub-directories are not allowed. */
    if (parent_dir != &g_root_dir)
      return ENOSPC;

    if (path_item)
      return EEXIST;

    new_dir = allocate_inode(g_activated_env);
    if (new_dir == NULL) {
      return ENOSPC;
    }

    new_dir->mode = mode | S_IFDIR;
    new_dir->parent_dir = parent_dir;
    new_dir->name[NACL_ARRAY_SIZE(new_dir->name)-1] = '\0';
    strncpy(new_dir->name, dirname, NACL_ARRAY_SIZE(new_dir->name));
    if (new_dir->name[NACL_ARRAY_SIZE(new_dir->name)-1] != '\0') {
      return ENAMETOOLONG;
    }
    new_dir->valid = true;

    return 0;
  }

  return ENOSYS;
}

static int my_rmdir(const char *pathname) {
  if (g_activated_env) {
    struct inode_data *parent_dir = NULL;
    struct inode_data *path_item = find_inode_path(g_activated_env,
                                                   pathname, &parent_dir);

    if (path_item == NULL || !S_ISDIR(path_item->mode))
      return ENOTDIR;

    /* Make sure the directory is empty. */
    for (int i = 0; i < NACL_ARRAY_SIZE(g_activated_env->inode_datas); i++) {
      if (path_item->valid &&
          path_item == g_activated_env->inode_datas[i].parent_dir) {
        return ENOTEMPTY;
      }
    }

    path_item->valid = false;
    return 0;
  }

  return ENOSYS;
}

static int my_chdir(const char *pathname) {
  if (g_activated_env) {
    struct inode_data *parent_dir = NULL;
    struct inode_data *path_item = find_inode_path(g_activated_env,
                                                   pathname, &parent_dir);

    if (path_item == NULL || !S_ISDIR(path_item->mode))
      return ENOTDIR;

    g_activated_env->current_dir = path_item;
    return 0;
  }

  return ENOSYS;
}

static int my_getcwd(char *pathname, size_t len) {
  if (g_activated_env) {
    const struct inode_data *current_dir = g_activated_env->current_dir;

    if (len == 0) {
      return pathname ? EINVAL : 0;
    }

    if (current_dir == &g_root_dir) {
      if (len < 2)
        return ENAMETOOLONG;
      pathname[0] = '/';
      pathname[1] = '\0';
    } else {
      int total_len = snprintf(pathname, len, "/%s", current_dir->name);
      if (total_len >= len)
        return ENAMETOOLONG;
    }

    return 0;
  }

  return ENOSYS;
}

static int my_unlink(const char *pathname) {
  return ENOSYS;
}

static int my_truncate(const char *pathname, nacl_irt_off_t length) {
  return ENOSYS;
}

static int my_lstat(const char *pathname, struct stat *buf) {
  return ENOSYS;
}

static int my_link(const char *oldpath, const char *newpath) {
  return ENOSYS;
}

static int my_rename(const char *oldpath, const char *newpath) {
  return ENOSYS;
}

static int my_symlink(const char *oldpath, const char *newpath) {
  /* For the purposes of this simple module, link and symlink are the same. */
  return my_link(oldpath, newpath);
}

static int my_chmod(const char *path, mode_t mode) {
  return ENOSYS;
}

static int my_access(const char *path, int amode) {
  return ENOSYS;
}

static int my_readlink(const char *path, char *buf, size_t count,
                       size_t *nread) {
  return ENOSYS;
}

static int my_utimes(const char *filename, const struct timeval *times) {
  return ENOSYS;
}

/* Module file_desc functions. */
void init_file_desc_module(void) {
  struct nacl_irt_dev_fdio fdio = {
    my_close,
    my_dup,
    my_dup2,
    my_read,
    my_write,
    my_seek,
    my_fstat,
    my_getdents,
    my_fchdir,
    my_fchmod,
    my_fsync,
    my_fdatasync,
    my_ftruncate,
    my_isatty,
  };

  size_t size = nacl_interface_ext_supply(NACL_IRT_DEV_FDIO_v0_3, &fdio,
                                          sizeof(fdio));
  ASSERT_EQ(size, sizeof(fdio));

  /* nacl_irt_fdio shares the same prefix as nacl_irt_dev_fdio. */
  size = nacl_interface_ext_supply(NACL_IRT_FDIO_v0_1, &fdio,
                                   sizeof(struct nacl_irt_fdio));
  ASSERT_EQ(size, sizeof(struct nacl_irt_fdio));

  struct nacl_irt_dev_filename fname = {
    my_open,
    my_stat,
    my_mkdir,
    my_rmdir,
    my_chdir,
    my_getcwd,
    my_unlink,
    my_truncate,
    my_lstat,
    my_link,
    my_rename,
    my_symlink,
    my_chmod,
    my_access,
    my_readlink,
    my_utimes,
  };

  size = nacl_interface_ext_supply(NACL_IRT_DEV_FILENAME_v0_3, &fname,
                                   sizeof(fname));
  ASSERT_EQ(size, sizeof(fname));
}

void init_inode_data(struct inode_data *inode_data) {
  *inode_data = g_default_inode;
}

void init_file_descriptor(struct file_descriptor *file_desc) {
  file_desc->valid = false;
  file_desc->data = NULL;
}

void init_file_desc_environment(struct file_desc_environment *env) {
  env->current_dir = &g_root_dir;
  for (int i = 0; i < NACL_ARRAY_SIZE(env->inode_datas); i++) {
    init_inode_data(&env->inode_datas[i]);
  }
  for (int i = 0; i < NACL_ARRAY_SIZE(env->file_descs); i++) {
    init_file_descriptor(&env->file_descs[i]);
  }

  /* Initialize STDIN, STDOUT, STDERR. */
  for (int i = 0; i < 3; i++) {
    env->file_descs[i].valid = true;
    env->file_descs[i].data = allocate_inode(env);
  }
}

struct inode_data *find_inode_name(struct file_desc_environment *env,
                                   struct inode_data *parent_dir,
                                   const char *name, size_t name_len) {
  for (int i = 0; i < NACL_ARRAY_SIZE(env->inode_datas); i++) {
    if (env->inode_datas[i].valid &&
        parent_dir == env->inode_datas[i].parent_dir &&
        strncmp(env->inode_datas[i].name, name, name_len) == 0 &&
        env->inode_datas[i].name[name_len] == '\0') {
      return &env->inode_datas[i];
    }
  }

  return NULL;
}

struct inode_data *find_inode_path(struct file_desc_environment *env,
                                   const char *path,
                                   struct inode_data **parent_dir) {
  const char *path_iter = path;
  const char *slash_loc;
  struct inode_data *current_dir = env->current_dir;
  if (*path_iter == '/') {
    current_dir = &g_root_dir;
    path_iter++;
  }

  /* See if there is a directory in the path. */
  if (current_dir == &g_root_dir) {
    slash_loc = strchr(path_iter, '/');
    if (slash_loc != NULL) {
      const size_t dir_len = slash_loc - path_iter;
      struct inode_data *dir_item = find_inode_name(env, current_dir,
                                                    path_iter, dir_len);
      if (dir_item == NULL || !S_ISDIR(dir_item->mode)) {
        *parent_dir = NULL;
        return NULL;
      }

      current_dir = dir_item;
      path_iter = slash_loc + 1;
    }
  }

  /* See if there is another slash, sub-directories are not allowed. */
  slash_loc = strchr(path_iter, '/');
  if (slash_loc != NULL) {
    /* sub-directories are not allowed. */
    *parent_dir = NULL;
    return NULL;
  }

  /* When last character is a slash, found path is the current directory. */
  if (*path_iter == '\0') {
    *parent_dir = current_dir->parent_dir;
    return current_dir;
  }

  /* path_iter now points to last item in the path. */
  *parent_dir = current_dir;
  return find_inode_name(env, current_dir, path_iter, strlen(path_iter));
}

void activate_file_desc_env(struct file_desc_environment *env) {
  g_activated_env = env;
}

void deactivate_file_desc_env(void) {
  g_activated_env = NULL;
}
