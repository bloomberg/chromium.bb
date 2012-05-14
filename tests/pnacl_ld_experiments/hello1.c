/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* This is highly experimental code to test shared/dynamic images
 * with pnacl.
 * Currently, only x86-32 is tested.
 */

/* ====================================================================== */
#define USE_TLS 1
#define USE_LIBC 1
#define CALL_LDSO 1
#define CALL_SIMPLESO 1
#define USE_PTHREAD 1
#define NUM_THREADS 5
/* ====================================================================== */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <bits/nacl_syscalls.h>
#if USE_PTHREAD == 1
#include <pthread.h>
#endif
/* ====================================================================== */
#define NACL_INSTR_BLOCK_SHIFT         5
#define NACL_PAGESHIFT                12
#define NACL_SYSCALL_START_ADDR       (16 << NACL_PAGESHIFT)
#define NACL_SYSCALL_ADDR(syscall_number)                               \
  (NACL_SYSCALL_START_ADDR + (syscall_number << NACL_INSTR_BLOCK_SHIFT))

#define NACL_SYSCALL(s) ((TYPE_nacl_ ## s) NACL_SYSCALL_ADDR(NACL_sys_ ## s))

/* ====================================================================== */
/* prototypes for nacl syscalls */

typedef int (*TYPE_nacl_write) (int desc, void const *buf, int count);
typedef void (*TYPE_nacl_null) (void);
typedef void (*TYPE_nacl_exit) (int status);

/* ====================================================================== */
/* prototypes for functions inside ld.so */
#define REGPARM(n) __attribute__((regparm(n)))

extern REGPARM(2) void _dl_get_tls_static_info(int *static_tls_size,
                                               int *static_tls_align);
extern void _dl_debug_state();
extern void *_dl_allocate_tls (void *mem);

/* this may be x86 specific but we do not have ld.so for arm anyway */
extern char *__nacl_read_tp();
/* from src/untrusted/nacl/tls_params.h */
ptrdiff_t __nacl_tp_tdb_offset(size_t tdb_size) asm("llvm.nacl.tp.tdb.offset");

/* prototypes for functions inside libsimple.so */
extern int fortytwo();

/* prototypes for functions inside mylibpthread.so */
extern int __pthread_initialize(void);

/* prototypes for functions inside mylibc.so */
extern void __newlib_thread_init(void);


/* This does not seem to have any effect - left for reference */
#define EXPORT __attribute__ ((visibility("default"),externally_visible))
/* ====================================================================== */
/* poor man's io */
/* ====================================================================== */
size_t mystrlen(const char* s) {
   unsigned int count = 0;
   while (*s++) ++count;
   return count;
}


void myhextochar(int n, char buffer[9]) {
  int i;
  buffer[8] = 0;

  for (i = 0; i < 8; ++i) {
    int nibble = 0xf & (n >> (4 * (7 - i)));
    if (nibble <= 9) {
      buffer[i] = nibble + '0';
    } else {
      buffer[i] = nibble - 10 + 'A';
     }
  }
}

ssize_t mywrite(int fd, const void* buf, size_t n)  {
  return NACL_SYSCALL(write)(fd, buf, n);
}

void myprint(const char *s) {
  mywrite(1, s, mystrlen(s));
}

void myprinthex(int n) {
  char buffer[9];
  myhextochar(n, buffer);
  myprint(buffer);
}

#if USE_LIBC == 0
void exit(int r) {
  NACL_SYSCALL(exit)(r);
  while (1) *(volatile int *) 0;
}
#endif

/* ====================================================================== */
/* stub functions */
/* ====================================================================== */
void __deregister_frame_info(const void *begin) {
   myprint("STUB __deregister_frame_info\n");
}

void __register_frame_info(void *begin, void *ob) {
  myprint("STUB __register_frame_info\n");
}

/* This is usually provided by
 * src/untrusted/nacl/pthread_initialize_minimal.c
 */
int __pthread_initialize_minimal(size_t tdb_size) {
  /* ld.so does some of the work for us, e.g.
   *  allocating and initialzing the tls area
   * but we still need put it in a
   * state which our libpthread expects.
   * We also do not call  __newlib_thread_init here
   */
   char *tp = __nacl_read_tp();
   void *tdb = tp + __nacl_tp_tdb_offset(tdb_size);
   /*
    * This should only be done for x86 - but this is all we support now anyway
    */
   *(void **) tdb = tdb;

   /* I do not think we need this as it has been setup already: confirm */
   /* nacl_tls_init(tdb); */
   myprint("tdb_size: ");
   myprinthex(tdb_size);
   myprint("\n");

   myprint("tp: ");
   myprinthex((int)tp);
   myprint("\n");

   myprint("tdb: ");
   myprinthex((int)tdb);
   myprint("\n");

   return 0;
}

/* This is usually provided by
 * src/untrusted/nacl/tls.c
 */
size_t __nacl_tls_combined_size(size_t tdb_size) {
  myprint("STUB:  __nacl_tls_combined_size\n");
  /* generous size for now to kick the tires */
  /* switch to using the results from _dl_get_tls_static_info */
  return 10 * 1024;
}

static size_t aligned_size(size_t size, size_t alignment) {
   return (size + alignment - 1) & -alignment;
 }

/* This is usually provided by
 * src/untrusted/nacl/tls.c
 */
void *__nacl_tls_data_bss_initialize_from_template(void *combined_area,
                                                   size_t tdb_size) {
  myprint("STUB:  __nacl_tls_data_bss_initialize_from_template\n");
  /* just point to somewhere in the middle with a generous alignment */
  int tdb = aligned_size((int) combined_area + 5 * 1024, 1024);
  myprint("tdb is: ");
  myprinthex((int) tdb);
  myprint("\n");
  void *tp = _dl_allocate_tls((void *) tdb);
  myprint("tp is: ");
  myprinthex((int) tp);
  myprint("\n");
  return tp;
}


/* ====================================================================== */
/*
 * __local_lock_acquire, etc.  only work when pthread has been initialized.
 * We provide stubs here which will be called by newlib if the
 * pthread implementations are not available
 */

#if USE_PTHREAD == 0
void __local_lock_acquire() {
  myprint("STUB __local_lock_acquire\n");
}

void __local_lock_acquire_recursive() {
  myprint("STUB __local_lock_acquire_recursive\n");
}

void __local_lock_close_recursive() {
  myprint("STUB __local_lock_close_recursive\n");
}

void __local_lock_init_recursive() {
  myprint("STUB __local_lock_init_recursive\n");
}

void __local_lock_release() {
  myprint("STUB __local_lock_release\n");
}

void __local_lock_release_recursive() {
  myprint("STUB __local_lock_release_recursive\n");
}
#endif

/* ====================================================================== */
#if USE_PTHREAD == 1
pthread_mutex_t my_mutex = PTHREAD_MUTEX_INITIALIZER;

void *my_thread(void *arg) {

  pthread_mutex_lock(&my_mutex);
  myprint("entering thread: ");
  myprinthex((int)arg);
  myprint("\n");

  myprint("exiting thread: ");
  myprinthex((int)arg);
  myprint("\n");
  pthread_mutex_unlock(&my_mutex);
  /* probably overkill */
  pthread_exit(arg);
  return arg;
}
#endif

/* ====================================================================== */
#if USE_TLS == 1
__thread int tdata1 = 0x73537353;
__thread int tdata2 = 0xfaceface;
#endif

int main(int argc, char** argv, char** envp) {
  myprint("raw printing: hello world\n");

  /* ======================================== */
  /* tls initialization test
   * NOTE: tls access works even without pthread init
   *
   * TODO(robertm): add some tls testing to .so's
   */
#if USE_TLS == 1
  myprinthex(tdata1);
  myprint(" expecting  0x73537353\n");

  myprinthex(tdata2);
  myprint(" expecting 0xfaceface\n");
#endif

  /* ======================================== */
  /*
   * libc calls  various pthread function which we have
   * conditionally stubbed out above.
   * they can only be used after __pthread_initialize
   * has been called which enables certain irt functions.
   */
#if USE_PTHREAD == 1
  /* Note: this calls __pthread_initialize_minimal */
  __pthread_initialize();
#endif
  /* ======================================== */
  /* this is usually called by either
   * src/untrusted/nacl/pthread_initialize_minimal.c
   * but we do not want any of the tls setup which goes with it
   */
#if USE_LIBC == 1
  __newlib_thread_init();
  puts("libc call\n");
  void *addr = (void *) &main;
  double d = 3.1415;
  printf("another libc call %p\n", addr);
  printf("another libc call %e\n", d);

  time_t rawtime;
  time(&rawtime);
  printf("The current local time is: %s\n", ctime(&rawtime));
#endif

  /* ======================================== */
  /* call into another .so (and back) */
#if CALL_SIMPLESO == 1
  int x = fortytwo();
  myprinthex(x);
  myprint(" expecting 42\n");
#endif

  /* ======================================== */
  /* trivial call into ld.so */
#if CALL_LDSO == 1
  _dl_debug_state();
#endif

  /* ======================================== */
  /* non-trivial call into ld.so */
#if CALL_LDSO == 1
  int static_tls_size;
  int static_tls_align;
  _dl_get_tls_static_info (&static_tls_size, &static_tls_align);
  myprint("tls size: ");
  myprinthex(static_tls_size);
  myprint("\n");

  myprint("tls align: ");
  myprinthex(static_tls_align);
  myprint("\n");
#endif

#if USE_PTHREAD == 1
  pthread_mutex_lock(&my_mutex);
  int i;
  pthread_t tid[NUM_THREADS];
  for (i = 0; i < NUM_THREADS; ++i) {
    myprint("starting thread\n");
    int res = pthread_create(&tid[i], NULL, my_thread, (void*) i);
    myprint("started thread: ");
    myprinthex(i);
    myprint("   res: ");
    myprinthex(res);
    myprint("\n");
  }

  myprint("\n\n");
  pthread_mutex_unlock(&my_mutex);


  for (i = 0; i < NUM_THREADS; ++i) {
    void *status;
    int res = pthread_join(tid[i], &status);
    pthread_mutex_lock(&my_mutex);
    myprint("joining thread: ");
    myprinthex(i);
    myprint("   res: ");
    myprinthex(res);
    myprint("   status: ");
    myprinthex((int) status);
    myprint("\n");
    pthread_mutex_unlock(&my_mutex);
  }
#endif

  myprint("exit\n");
  return 0;
}
