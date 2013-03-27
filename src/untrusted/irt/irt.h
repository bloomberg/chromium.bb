/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_IRT_IRT_H_
#define NATIVE_CLIENT_SRC_UNTRUSTED_IRT_IRT_H_

#include <stddef.h>
#include <sys/types.h>
#include <time.h>

struct timeval;
struct timespec;
struct stat;
struct dirent;

struct PP_StartFunctions;
struct PP_ThreadFunctions;
struct NaClExceptionContext;

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * The only interface exposed directly to user code is a single function
 * of this type.  It is passed via the AT_SYSINFO field of the ELF
 * auxiliary vector on the stack at program startup.  The interfaces
 * below are accessed by calling this function with the appropriate
 * interface identifier.
 *
 * This function returns the number of bytes filled in at TABLE, which
 * is never larger than TABLESIZE.  If the interface identifier is not
 * recognized or TABLESIZE is too small, it returns zero.
 *
 * The interface of the query function avoids passing any data pointers
 * back from the IRT to user code.  Only code pointers are passed back.
 * It is an opaque implementation detail (that may change) whether those
 * point to normal untrusted code in the user address space, or whether
 * they point to special trampoline addresses supplied by trusted code.
 */
typedef size_t (*TYPE_nacl_irt_query)(const char *interface_ident,
                                      void *table, size_t tablesize);

/*
 * C libraries expose this function to reach the interface query interface.
 * If there is no IRT hook available at all, it returns zero.
 * Otherwise it behaves as described above for TYPE_nacl_irt_query.
 */
size_t nacl_interface_query(const char *interface_ident,
                            void *table, size_t tablesize);

/*
 * All functions in IRT vectors return an int, which is zero for success
 * or a (positive) errno code for errors.  Any values are delivered via
 * result parameters.  The only exceptions exit/thread_exit, which can
 * never return, and tls_get, which can never fail.
 */

#define NACL_IRT_BASIC_v0_1     "nacl-irt-basic-0.1"
struct nacl_irt_basic {
  void (*exit)(int status);
  int (*gettod)(struct timeval *tv);
  int (*clock)(clock_t *ticks);
  int (*nanosleep)(const struct timespec *req, struct timespec *rem);
  int (*sched_yield)(void);
  int (*sysconf)(int name, int *value);
};

#define NACL_IRT_FDIO_v0_1      "nacl-irt-fdio-0.1"
struct nacl_irt_fdio {
  int (*close)(int fd);
  int (*dup)(int fd, int *newfd);
  int (*dup2)(int fd, int newfd);
  int (*read)(int fd, void *buf, size_t count, size_t *nread);
  int (*write)(int fd, const void *buf, size_t count, size_t *nwrote);
  int (*seek)(int fd, off_t offset, int whence, off_t *new_offset);
  int (*fstat)(int fd, struct stat *);
  int (*getdents)(int fd, struct dirent *, size_t count, size_t *nread);
};

#define NACL_IRT_FILENAME_v0_1      "nacl-irt-filename-0.1"
struct nacl_irt_filename {
  int (*open)(const char *pathname, int oflag, mode_t cmode, int *newfd);
  int (*stat)(const char *pathname, struct stat *);
};

#define NACL_IRT_MEMORY_v0_1    "nacl-irt-memory-0.1"
struct nacl_irt_memory_v0_1 {
  int (*sysbrk)(void **newbrk);
  int (*mmap)(void **addr, size_t len, int prot, int flags, int fd, off_t off);
  int (*munmap)(void *addr, size_t len);
};

#define NACL_IRT_MEMORY_v0_2    "nacl-irt-memory-0.2"
struct nacl_irt_memory_v0_2 {
  int (*sysbrk)(void **newbrk);
  int (*mmap)(void **addr, size_t len, int prot, int flags, int fd, off_t off);
  int (*munmap)(void *addr, size_t len);
  int (*mprotect)(void *addr, size_t len, int prot);
};

#define NACL_IRT_DYNCODE_v0_1   "nacl-irt-dyncode-0.1"
struct nacl_irt_dyncode {
  int (*dyncode_create)(void *dest, const void *src, size_t size);
  int (*dyncode_modify)(void *dest, const void *src, size_t size);
  int (*dyncode_delete)(void *dest, size_t size);
};

#define NACL_IRT_THREAD_v0_1   "nacl-irt-thread-0.1"
struct nacl_irt_thread {
  int (*thread_create)(void *start_user_address, void *stack, void *thread_ptr);
  void (*thread_exit)(int32_t *stack_flag);
  int (*thread_nice)(const int nice);
};

#define NACL_IRT_MUTEX_v0_1        "nacl-irt-mutex-0.1"
struct nacl_irt_mutex {
  int (*mutex_create)(int *mutex_handle);
  int (*mutex_destroy)(int mutex_handle);
  int (*mutex_lock)(int mutex_handle);
  int (*mutex_unlock)(int mutex_handle);
  int (*mutex_trylock)(int mutex_handle);
};

#define NACL_IRT_COND_v0_1      "nacl-irt-cond-0.1"
struct nacl_irt_cond {
  int (*cond_create)(int *cond_handle);
  int (*cond_destroy)(int cond_handle);
  int (*cond_signal)(int cond_handle);
  int (*cond_broadcast)(int cond_handle);
  int (*cond_wait)(int cond_handle, int mutex_handle);
  int (*cond_timed_wait_abs)(int cond_handle, int mutex_handle,
                             const struct timespec *abstime);
};

#define NACL_IRT_SEM_v0_1       "nacl-irt-sem-0.1"
struct nacl_irt_sem {
  int (*sem_create)(int *sem_handle, int32_t value);
  int (*sem_destroy)(int sem_handle);
  int (*sem_post)(int sem_handle);
  int (*sem_wait)(int sem_handle);
};

#define NACL_IRT_TLS_v0_1       "nacl-irt-tls-0.1"
struct nacl_irt_tls {
  int (*tls_init)(void *thread_ptr);
  void *(*tls_get)(void);
};

#define NACL_IRT_BLOCKHOOK_v0_1 "nacl-irt-blockhook-0.1"
struct nacl_irt_blockhook {
  int (*register_block_hooks)(void (*pre)(void), void (*post)(void));
};

#define NACL_IRT_PPAPIHOOK_v0_1 "nacl-irt-ppapihook-0.1"
struct nacl_irt_ppapihook {
  int (*ppapi_start)(const struct PP_StartFunctions *);
  void (*ppapi_register_thread_creator)(const struct PP_ThreadFunctions *);
};

#define NACL_IRT_RESOURCE_OPEN_v0_1 "nacl-irt-resource-open-0.1"
struct nacl_irt_resource_open {
  int (*open_resource)(const char *file, int *fd);
};

#define NACL_IRT_RANDOM_v0_1 "nacl-irt-random-0.1"
struct nacl_irt_random {
  int (*get_random_bytes)(void *buf, size_t count, size_t *nread);
};

#define NACL_IRT_CLOCK_v0_1 "nacl-irt-clock_get-0.1"
struct nacl_irt_clock {
  int (*clock_getres)(clockid_t clk_id, struct timespec *res);
  int (*clock_gettime)(clockid_t clk_id, struct timespec *tp);
};

/*
 * A working getpid() is not provided by NaCl inside Chromium.  We
 * only define this interface for uses of NaCl outside the Web
 * browser.  Inside Chromium, requests for this interface may fail, or
 * may return a function which always returns an error.
 */
#define NACL_IRT_DEV_GETPID_v0_1 "nacl-irt-dev-getpid-0.1"
struct nacl_irt_dev_getpid {
  int (*getpid)(int *pid);
};

/*
 * NOTE: This is a 'dev' interface which is NOT stable.
 * In the future, requests for this interface will fail.
 */
#define NACL_IRT_DEV_EXCEPTION_HANDLING_v0_1 \
  "nacl-irt-dev-exception-handling-0.1"
typedef void (*NaClExceptionHandler)(struct NaClExceptionContext *context);
struct nacl_irt_dev_exception_handling {
  int (*exception_handler)(NaClExceptionHandler handler,
                           NaClExceptionHandler *old_handler);
  int (*exception_stack)(void *stack, size_t size);
  int (*exception_clear_flag)(void);
};

#if defined(__cplusplus)
}
#endif

#endif /* NATIVE_CLIENT_SRC_UNTRUSTED_IRT_IRT_H */
