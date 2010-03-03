/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Native Client threads library
 */

#include <limits.h>
#include <assert.h>
#include <malloc.h>
#include <stdint.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/unistd.h>

#include "native_client/src/include/nacl_base.h"

#include "native_client/src/untrusted/nacl/tls.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

#include "native_client/src/untrusted/pthread/nc_hash.h"
#include "native_client/src/untrusted/pthread/pthread.h"
#include "native_client/src/untrusted/pthread/pthread_types.h"

#define FUN_TO_VOID_PTR(a) ((void*)((uintptr_t) a))

#if NACL_ARCH(NACL_TARGET_ARCH) == NACL_x86
# if __x86_64__
typedef uint64_t return_addr_t;
# elif __i386__
typedef uint32_t return_addr_t;
# else
#  error "What kind of x86 processor are we on?!?"
# endif
#elif NACL_ARCH(NACL_TARGET_ARCH) == NACL_arm
typedef uint32_t return_addr_t;
#else
# error "What target architecture are we running the NaCl module on?"
#endif

static const int kStackAlignment = 32;

/* Thread management global variables */
const int __nc_kMaxCachedMemoryBlocks = 50;

/* array of TDB pointers */
struct NaClHashTable __nc_thread_threads;

int32_t __nc_thread_id_counter;
int32_t __nc_thread_counter_reached_32_bit;

/* mutex used to synchronize thread management code */
pthread_mutex_t  __nc_thread_management_lock;

/* condition variable that gets signaled when all the threads
 * except the main thread have terminated
 */
pthread_cond_t __nc_last_thread_cond;

/* number of threads currently running in this NaCl module */
int __nc_running_threads_counter;

/*
 * Array for TSD keys status management. */
tsd_status_t __nc_thread_specific_storage[PTHREAD_KEYS_MAX];

/* we have two queues of memory blocks - one for each type */
STAILQ_HEAD(tailhead, entry) __nc_thread_memory_blocks[2];
/* We need a counter for each queue to keep track of number of blocks */
int __nc_memory_block_counter[2];


#define NODE_TO_PAYLOAD(TlsNode) \
  ((char*)(TlsNode) + sizeof(nc_thread_memory_block_t))

/* Internal functions */

#if NACL_ARCH(NACL_TARGET_ARCH) == NACL_x86

#ifdef __x86_64__
static inline nc_thread_descriptor_t *nc_get_tdb() {
  return NACL_SYSCALL(tls_get)();
}
#else
static inline nc_thread_descriptor_t *nc_get_tdb() {
  nc_thread_descriptor_t *tdb = NULL;
  __asm__ __volatile__ ("mov %%gs:0, %0"
                      : "=r" (tdb)
                      : );
  return tdb;
}
#endif

#elif NACL_ARCH(NACL_TARGET_ARCH) == NACL_arm
static nc_thread_descriptor_t *nc_get_tdb() {
  /*
   * NOTE: if this turns out to be a performance issue, the code can be
   *       replaced with a read and mask from r9.
   *       c.f. src/untrusted/stubs/crt1_arm.S::__aeabi_read_tp
   */
  return NACL_SYSCALL(tls_get)();
}
#else
#error "unknown platform"
#endif


static int nc_allocate_thread_id_mu(nc_basic_thread_data_t *basic_data) {
  /* assuming the global lock is locked */
  uint32_t i = 0;
  if (!__nc_thread_counter_reached_32_bit) {
    /* just allocate the next id */
    basic_data->thread_id = __nc_thread_id_counter;
    if (MAX_THREAD_ID == __nc_thread_id_counter) {
      /* no more higher ids, next threads will have to search for a free id */
      __nc_thread_counter_reached_32_bit = 1;
    } else {
      ++__nc_thread_id_counter;
    }
  } else {
    /*
     * the counter is useless at this point, because some older threads
     * may be still running
     */
    int found = 0;
    for (i = 0; i <= MAX_THREAD_ID; ++i) {
      if (!HASH_ID_EXISTS(&__nc_thread_threads, i)) {
        basic_data->thread_id = i;
        found = 1;
        break;
      }
    }
    if (!found) {
      /* all the IDs are in use?! */
      return -1;
    }
  }

  HASH_TABLE_INSERT(&__nc_thread_threads, basic_data, hash_entry);
  return 0;
}

static nc_basic_thread_data_t *nc_find_tdb_mu(uint32_t id) {
  /* assuming the global lock is locked */
  return HASH_FIND_ID(&__nc_thread_threads,
                      id,
                      nc_basic_thread_data,
                      hash_entry);
}

void nc_thread_starter(nc_thread_function func,
                       void *state) {
  nc_thread_descriptor_t *tdb =  NULL;
  void *retval;
  tdb = nc_get_tdb();
  __newlib_thread_init();
  retval = tdb->start_func(tdb->state);
  /* if the function returns, terminate the thread */
  tdb->exiting_without_returning = 0;
  pthread_exit(retval);
  /* NOTREACHED */
  /* TODO(gregoryd) - add assert */
}

static nc_thread_memory_block_t* nc_allocate_memory_block_mu(
    nc_thread_memory_block_type_t type,
    int required_size) {
  struct tailhead *head;
  nc_thread_memory_block_t *node;
  /* assume the lock is held!!! */
  if (type >= MAX_MEMORY_TYPE)
    return NULL;
  head = &__nc_thread_memory_blocks[type];

  /* We need to know the size even if we find a free node - to memset it to 0 */
  switch (type) {
     case THREAD_STACK_MEMORY:
       required_size = required_size + kStackAlignment - 1;
       break;
     case TLS_AND_TDB_MEMORY:
       break;
     case MAX_MEMORY_TYPE:
     default:
       return NULL;
  }

  if (!STAILQ_EMPTY(head)) {
    /* try to get one from queue */
    nc_thread_memory_block_t *node = STAILQ_FIRST(head);

    /*
     * On average the memory blocks will be marked as not used in the same order
     * as they are added to the queue, therefore there is no need to check the
     * next queue entries if the first one is still in use.
     */
    if (0 == node->is_used) {
      STAILQ_REMOVE_HEAD(head, entries);
      --__nc_memory_block_counter[type];
      while (__nc_memory_block_counter[type] > __nc_kMaxCachedMemoryBlocks) {
        /*
         * We have too many blocks in the queue - try to release some.
         * The maximum number of memory blocks to keep in the queue
         * is almost arbitrary and can be tuned.
         * The main limitation is that if we keep too many
         * blocks in the queue, the NaCl app will run out of memory,
         * since the default thread stack size is 512K.
         * TODO(gregoryd): we might give up reusing stack entries once we
         * support variable stack size.
         */
        nc_thread_memory_block_t *tmp = STAILQ_FIRST(head);
        if (0 == tmp->is_used) {
          STAILQ_REMOVE_HEAD(head, entries);
          --__nc_memory_block_counter[type];
          free(tmp);
        } else {
          /*
           * Stop once we find a block that is still in use,
           * since probably there is no point to continue
           */
          break;
        }
      }
      memset(node, 0,sizeof(*node));
      node->size = required_size;
      node->is_used = 1;
      return node;
    }
  }
  /* no available blocks of the required type - allocate one */
  node = malloc(MEMORY_BLOCK_ALLOCATION_SIZE(required_size));
  memset(node, 0, sizeof(*node));
  node->size = required_size;
  node->is_used = 1;
  return node;
}

static void nc_free_memory_block_mu(nc_thread_memory_block_type_t type,
                                    nc_thread_memory_block_t* node) {
  /* assume the lock is held !!! */
  struct tailhead *head = &__nc_thread_memory_blocks[type];
  STAILQ_INSERT_TAIL(head, node, entries);
  ++__nc_memory_block_counter[type];
}

static void nc_release_basic_data_mu(nc_basic_thread_data_t *basic_data) {
  if (NACL_PTHREAD_ILLEGAL_THREAD_ID != basic_data->thread_id) {
    HASH_REMOVE(&__nc_thread_threads,
                basic_data->thread_id,
                nc_basic_thread_data,
                hash_entry);
  }
  /* join_condvar can be initialized only if tls_node exists */
  pthread_cond_destroy(&basic_data->join_condvar);
  free(basic_data);
}

static void nc_release_tls_node(nc_thread_memory_block_t *block) {
  if (block) {
    nc_thread_descriptor_t* tdb =
      (nc_thread_descriptor_t *) __nacl_tls_tdb_start(NODE_TO_PAYLOAD(block));

    tdb->basic_data->tdb = NULL;
    block->is_used = 0;
    nc_free_memory_block_mu(TLS_AND_TDB_MEMORY, block);
  }
}

/* Implements this, atomically:
 *  old = *ptr;
 *  if (*ptr == old_value)
 *    *ptr = new_value;
 *  return old;
 */
static AtomicInt32 CompareAndSwap(volatile AtomicInt32* ptr,
                                  AtomicInt32 old_value,
                                  AtomicInt32 new_value) {
  /* GCC intrinsic; see:
   * http://gcc.gnu.org/onlinedocs/gcc/Atomic-Builtins.html.
   * The x86-{32,64} compilers generate inline code.  The ARM
   * implementation is external: instrincs/crt1_arm.S.
   */
  return __sync_val_compare_and_swap(ptr, old_value, new_value);
}

void nc_spinlock_lock(volatile int *lock) {
  uint32_t val;
  do {
    val = CompareAndSwap(lock, 0, 1);
  } while (val != 0);
}

void nc_spinlock_unlock(volatile int *lock) {
  *lock = 0;
}

uint32_t __nacl_tdb_id_function(nc_hash_entry_t *entry) {
  nc_basic_thread_data_t *basic_data =
      HASH_ENTRY_TO_ENTRY_TYPE(entry, nc_basic_thread_data, hash_entry);
  return basic_data->thread_id;
}

/* Initialize a newly allocated TDB to some default values */
static int nc_tdb_init(nc_thread_descriptor_t *tdb,
                       nc_basic_thread_data_t * basic_data) {
  tdb->tls_base = tdb;
  tdb->basic_data = basic_data;
  basic_data->tdb = tdb;
  /* Put an illegal value, should be set when the ID is allocated */
  tdb->basic_data->thread_id = NACL_PTHREAD_ILLEGAL_THREAD_ID;
  tdb->basic_data->retval = 0;
  tdb->basic_data->status = THREAD_RUNNING;
  tdb->basic_data->hash_entry.id_function = __nacl_tdb_id_function;

  tdb->joinable = PTHREAD_CREATE_JOINABLE;
  tdb->join_waiting = 0;

  tdb->thread_specific_data = NULL;
  tdb->tls_node = NULL;
  tdb->stack_node = NULL;

  tdb->start_func = NULL;
  tdb->state = NULL;
  /*
   * A call to pthread_exit without returning from the thread function
   * should be recognized
   */
  tdb->exiting_without_returning = 1;

  /* Imitate PTHREAD_COND_INITIALIZER - we cannot use it directly here,
   * since this is not variable initialization.
   */
  nc_pthread_condvar_ctor(&basic_data->join_condvar);
  return 0;
}


/* Will be called from the library startup code,
 * which always happens on the application's main thread
 */
int __pthread_initialize() {
  int retval = 0;
  nc_thread_descriptor_t *tdb;
  nc_basic_thread_data_t *basic_data;
  /* allocate TLS+TDB area */
  /* We allocate the basic data immediately after TDB */
  __pthread_initialize_minimal(sizeof(nc_thread_descriptor_t) +
                               sizeof(nc_basic_thread_data_t));

  /* At this point GS is already initialized */
  tdb = nc_get_tdb();
  basic_data = (nc_basic_thread_data_t *)(tdb + 1);

  HASH_INIT(&__nc_thread_threads);

  retval = pthread_mutex_init(&__nc_thread_management_lock, NULL);
  if (retval) {
    /* If the initialization fails we just fail the whole process */
    /*
     * TODO(gregoryd): check that the return value
     * is actually checked somewhere
     */

    return retval;
  }

  retval = pthread_cond_init(&__nc_last_thread_cond, NULL);
  if (retval) {
    return retval;
  }
  STAILQ_INIT(&__nc_thread_memory_blocks[0]);
  STAILQ_INIT(&__nc_thread_memory_blocks[1]);

  __nc_running_threads_counter = 1; /* the main thread */

  /* Initialize the main thread TDB */
  nc_tdb_init(tdb, basic_data);
  /* ID 0 is reserved for the main thread */
  basic_data->thread_id = 0;
  __nc_thread_id_counter = 1;
  HASH_TABLE_INSERT(&__nc_thread_threads, basic_data, hash_entry);
  return retval;
}



/* pthread functions */

int pthread_create(pthread_t *thread_id,
                   pthread_attr_t *attr,
                   void *(*start_routine) (void *),
                   void *arg) {
  int retval = -1;
  void *esp;
  /* declare the variables outside of the while scope */
  nc_thread_memory_block_t *stack_node = NULL;
  char *thread_stack = NULL;
  nc_thread_descriptor_t *new_tdb = NULL;
  nc_basic_thread_data_t *new_basic_data = NULL;
  nc_thread_memory_block_t *tls_node = NULL;
  size_t stacksize = PTHREAD_STACK_MIN;

  /* TODO(gregoryd) - right now a single lock is used, try to optimize? */
  pthread_mutex_lock(&__nc_thread_management_lock);

  do {
    /* Allocate the tls block */

    tls_node = nc_allocate_memory_block_mu(
        TLS_AND_TDB_MEMORY,
         __nacl_tls_combined_size(sizeof(nc_thread_descriptor_t), 1));
    if (NULL == tls_node)
      break;

    __nacl_tls_data_bss_initialize_from_template(NODE_TO_PAYLOAD(tls_node));

    new_tdb = (nc_thread_descriptor_t *)
              __nacl_tls_tdb_start(NODE_TO_PAYLOAD(tls_node));
    /* TODO(gregoryd): consider creating a pool of basic_data structs,
     * similar to stack and TLS+TDB (probably when adding the support for
     * variable stack size).
     */
    new_basic_data = malloc(sizeof(*new_basic_data));
    if (NULL == new_basic_data)
      break;

    nc_tdb_init(new_tdb, new_basic_data);
    new_tdb->tls_node = tls_node;

    retval = nc_allocate_thread_id_mu(new_basic_data);
    if (0 != retval)
      break;

    /* all the required members of the tdb must be initialized before
     * the thread is started and actually before the global lock is released,
     * since another thread can call pthread_join() or pthread_detach()
     */
    new_tdb->start_func = start_routine;
    new_tdb->state = arg;
    if (attr != NULL) {
      new_tdb->joinable = attr->joinable;
      stacksize = attr->stacksize;
    }

    /* Allocate the stack for the thread */
    stack_node = nc_allocate_memory_block_mu(THREAD_STACK_MEMORY, stacksize);
    if (NULL == stack_node) {
      retval = -1;
      break;
    }
    thread_stack = NODE_TO_PAYLOAD(stack_node);
    thread_stack = __nacl_thread_stack_align(thread_stack);
    new_tdb->stack_node = stack_node;

  } while (0);

  pthread_mutex_unlock(&__nc_thread_management_lock);

  if (0 != retval) {
    goto ret; /* error */
  }

  /*
   * Calculate the stack location - it should be 12 mod 16 aligned.
   * We subtract sizeof(return_addr_t) since thread_stack is 0 mod 16
   * aligned and the stack size is a multiple of 16.
   */
  esp = (void*) (thread_stack + stacksize - sizeof(return_addr_t));

  /*
   * Put 0 on the stack as a return address - it is needed to satisfy
   * the alignment requirement when we call nacl's exit_thread syscall
   * when the thread terminates.
   */
  *(return_addr_t *) esp = 0;

  /* start the thread */
  retval = NACL_SYSCALL(thread_create)(FUN_TO_VOID_PTR(nc_thread_starter),
                                       esp,
                                       new_tdb,
                                       sizeof(nc_thread_descriptor_t));
  if (0 != retval) {
    goto ret;
  }
  *thread_id = new_basic_data->thread_id;
  pthread_mutex_lock(&__nc_thread_management_lock);
  /* TODO(gregoryd) : replace with atomic increment? */
  ++__nc_running_threads_counter;
  pthread_mutex_unlock(&__nc_thread_management_lock);
  assert(0 == retval);

ret:
  if (retval) {
    /* failed to create a thread */
    pthread_mutex_lock(&__nc_thread_management_lock);

    nc_release_tls_node(tls_node);
    if (new_basic_data) {
      nc_release_basic_data_mu(new_basic_data);
    }
    if (stack_node) {
      stack_node->is_used = 0;
      nc_free_memory_block_mu(THREAD_STACK_MEMORY, stack_node);
    }

    pthread_mutex_unlock(&__nc_thread_management_lock);
    *thread_id = -1;
  }

  return retval;
}

int __pthread_shutdown() {
  pthread_mutex_lock(&__nc_thread_management_lock);

  while (1 != __nc_running_threads_counter) {
    pthread_cond_wait(&__nc_last_thread_cond, &__nc_thread_management_lock);
  }

  pthread_mutex_unlock(&__nc_thread_management_lock);
  return 0;
}

void pthread_exit (void* retval) {
  /* get all we need from the tdb before releasing it */
  nc_thread_descriptor_t    *tdb = nc_get_tdb();
  nc_thread_memory_block_t  *stack_node = tdb->stack_node;
  int32_t                   *is_used = &stack_node->is_used;
  nc_basic_thread_data_t    *basic_data = tdb->basic_data;
  pthread_t                 thread_id = basic_data->thread_id;
  int                       joinable = tdb->joinable;
  unsigned int              generation;

  /* call the destruction functions for TSD */
  if (tdb->exiting_without_returning) {
    int destruction_iterations;
    for (destruction_iterations = 0;
        destruction_iterations < PTHREAD_DESTRUCTOR_ITERATIONS;
        destruction_iterations++) {
      int i = 0;
      int nothing_to_do = 1;
      for (i = 0; i < PTHREAD_KEYS_MAX; i++) {
        void (*destructor)(void*) =
            __nc_thread_specific_storage[i].destruction_callback;
        if ((KEY_ALLOCATED(generation =
                __nc_thread_specific_storage[i].generation)) &&
            (tdb->thread_specific_data != NULL) &&
            (tdb->thread_specific_data[i].generation == generation) &&
            (tdb->thread_specific_data[i].ptr != NULL) &&
            (NULL != destructor)) {
          void *tmp = tdb->thread_specific_data[i].ptr;
          tdb->thread_specific_data[i].ptr = NULL;
          destructor(tmp);
          nothing_to_do = 0;
        }
      }
      if (nothing_to_do) {
        break;
      }
    }
  }


  if (0 == thread_id) {
    /* This is the main thread - wait for other threads to complete */
    __pthread_shutdown();
    /* TODO(gregoryd): should we call exit() or __nacl_exit_thread? */
    NACL_SYSCALL(thread_exit)(NULL);
  }

  pthread_mutex_lock(&__nc_thread_management_lock);

  basic_data->retval = retval;

  if (joinable) {
    /* If somebody is waiting for this thread, signal */
    basic_data->status = THREAD_TERMINATED;
    pthread_cond_signal(&basic_data->join_condvar);
  }
  /*
   * We can release TLS+TDB - thread id and its return value are still
   * kept in basic_data
   */
  nc_release_tls_node(tdb->tls_node);

  if (!joinable) {
    nc_release_basic_data_mu(basic_data);
  }

  /* now add the stack to the list but keep it marked as used */
  nc_free_memory_block_mu(THREAD_STACK_MEMORY, stack_node);

  --__nc_running_threads_counter;
  /* TODO(gregoryd): is this synchronization enough? Main thread's
   * pthread_exit can still complete before the last thread
   */

  if (1 == __nc_running_threads_counter) {
    pthread_cond_signal(&__nc_last_thread_cond);
  }

  pthread_mutex_unlock(&__nc_thread_management_lock);
  NACL_SYSCALL(thread_exit)(is_used);
}

int pthread_join(pthread_t thread_id, void **thread_return) {
  int retval = 0;
  nc_basic_thread_data_t *basic_data;
  if (pthread_self() == thread_id) {
    return EDEADLK;
  }

  pthread_mutex_lock(&__nc_thread_management_lock);
  basic_data = nc_find_tdb_mu(thread_id);
  if (NULL == basic_data) {
    retval = ESRCH;
    goto ret;
  }

  if (basic_data->tdb != NULL) {
    /* The thread is still running */
    nc_thread_descriptor_t *joined_tdb = basic_data->tdb;
    if (!joined_tdb->joinable || joined_tdb->join_waiting) {
      /* the thread is detached or another thread is waiting to join */
      retval = EINVAL;
      goto ret;
    }
    joined_tdb->join_waiting = 1;
    /* wait till the thread terminates */
    while (THREAD_TERMINATED != basic_data->status) {
      pthread_cond_wait(&basic_data->join_condvar,
                        &__nc_thread_management_lock);
    }
  }
  /* The thread has already terminated */
  /* save the return value */
  if (thread_return != NULL) {
    *thread_return = basic_data->retval;
  }

  /* release the resources */
  nc_release_basic_data_mu(basic_data);
  retval = 0;

ret:
  pthread_mutex_unlock(&__nc_thread_management_lock);

  return retval;

}

int pthread_detach(pthread_t thread_id) {
  int retval = 0;
  nc_basic_thread_data_t *basic_data;
  nc_thread_descriptor_t *detached_tdb;
  /* TODO(gregoryd) - can be optimized using InterlockedExchange
   * once it's available */
  pthread_mutex_lock(&__nc_thread_management_lock);
  basic_data = nc_find_tdb_mu(thread_id);
  if (NULL == basic_data) {
    /* no such thread */
    retval = ESRCH;
    goto ret;
  }
  detached_tdb = basic_data->tdb;

  if (NULL == detached_tdb) {
    /* The thread has already terminated */
    nc_release_basic_data_mu(basic_data);
  } else {
    if (!detached_tdb->join_waiting) {
      if (detached_tdb->joinable) {
        detached_tdb->joinable = 0;
      } else {
        /* already detached */
        retval = EINVAL;
      }
    } else {
      /* another thread is already waiting to join - do nothing */
    }
  }
ret:
  pthread_mutex_unlock(&__nc_thread_management_lock);
  return retval;
}

int nacl_thread_nice(const int nice) {
  return NACL_SYSCALL(thread_nice)(nice);
}

int pthread_kill(pthread_t thread_id,
                 int sig) {
  /* This function is currently unimplemented. */
  errno = EINVAL;
  return -1;
}

pthread_t pthread_self() {
  /* get the tdb pointer from gs and use it to return the thread handle*/
  nc_thread_descriptor_t *tdb = nc_get_tdb();
  return tdb->basic_data->thread_id;
}

int pthread_equal (pthread_t thread1, pthread_t thread2) {
  return (thread1 == thread2);
}

/* Thread-specific data functions */

int pthread_key_create (pthread_key_t *key,
                        void (*destr_function) (void *)) {
  int i;
  unsigned int generation;
  for (i=0; i<PTHREAD_KEYS_MAX; ++i) {
    if (!KEY_ALLOCATED(generation =
            __nc_thread_specific_storage[i].generation) &&
        /* A key is not reusable after UINT_MAX-1 generations -
         * the last even generation */
        (__nc_thread_specific_storage[i].generation != UINT_MAX - 1) &&
        (generation == CompareAndSwap(
            (AtomicInt32*) &__nc_thread_specific_storage[i].generation,
            generation,
            generation + 1))) {
      *key = i;
      __nc_thread_specific_storage[i].destruction_callback = destr_function;
      return 0;
    }
  }

  return EAGAIN;
}

int pthread_key_delete (pthread_key_t key) {
  unsigned int generation;
  if ((key > PTHREAD_KEYS_MAX - 1) ||
      !KEY_ALLOCATED(generation =
          __nc_thread_specific_storage[key].generation)) {
    return EINVAL;
  }

  if (generation !=
      CompareAndSwap(
          (AtomicInt32*) &__nc_thread_specific_storage[key].generation,
          generation,
          generation + 1)) {
    /* Somebody incremented the generation counter before we did, i.e. the key
     * has already been deleted.
     */
    return EINVAL;
  }

  __nc_thread_specific_storage[key].destruction_callback = NULL;
  return 0;
}

int pthread_setspecific (pthread_key_t key,
                         const void *pointer) {
  nc_thread_descriptor_t *tdb = nc_get_tdb();
  unsigned int generation;

  if ((key > PTHREAD_KEYS_MAX - 1) ||
      !KEY_ALLOCATED(generation =
          __nc_thread_specific_storage[key].generation)) {
      return EINVAL;
  }

  /* Allocate the memory for this thread's TSD - we actually need it */
  if (NULL == tdb->thread_specific_data) {
    tdb->thread_specific_data = calloc(PTHREAD_KEYS_MAX, sizeof(tsd_t));
    if (tdb->thread_specific_data == NULL) {
      return EINVAL;
    }
  }

  tdb->thread_specific_data[key].generation = generation;
  tdb->thread_specific_data[key].ptr = (void*) pointer;
  return 0;
}

void *pthread_getspecific (pthread_key_t key) {
  nc_thread_descriptor_t *tdb = nc_get_tdb();
  if (!tdb->thread_specific_data ||
      (key > PTHREAD_KEYS_MAX - 1) ||
      (tdb->thread_specific_data[key].generation !=
          __nc_thread_specific_storage[key].generation)) {
    return NULL;
  }
  return tdb->thread_specific_data[key].ptr;
}

int pthread_attr_init (pthread_attr_t *attr) {
  if (NULL == attr) {
    return EINVAL;
  }
  attr->joinable = PTHREAD_CREATE_JOINABLE;
  attr->stacksize = PTHREAD_STACK_MIN;
  return 0;
}

int pthread_attr_destroy (pthread_attr_t *attr) {
  if (NULL == attr) {
    return EINVAL;
  }
  /* nothing to destroy */
  return 0;
}

int pthread_attr_setdetachstate (pthread_attr_t *attr,
                                 int detachstate) {
  if (NULL == attr) {
    return EINVAL;
  }
  attr->joinable = detachstate;
  return 0;
}

int pthread_attr_getdetachstate (pthread_attr_t *attr,
                                 int *detachstate) {
  if (NULL == attr) {
    return EINVAL;
  }
  return attr->joinable;
}

int pthread_attr_setstacksize(pthread_attr_t *attr,
                              size_t stacksize) {
  if (NULL == attr) {
    return EINVAL;
  }
  if (PTHREAD_STACK_MIN < stacksize) {
    attr->stacksize = stacksize;
  } else {
    attr->stacksize = PTHREAD_STACK_MIN;
  }
  return 0;
}

int pthread_attr_getstacksize(pthread_attr_t *attr,
                              size_t *stacksize) {
  if (NULL == attr) {
    return EINVAL;
  }
  return attr->stacksize;
}

void __local_lock_init(_LOCK_T* lock);
void __local_lock_init_recursive(_LOCK_T* lock);
void __local_lock_close(_LOCK_T* lock);
void __local_lock_close_recursive(_LOCK_T* lock);
void __local_lock_acquire(_LOCK_T* lock);
void __local_lock_acquire_recursive(_LOCK_T* lock);
int __local_lock_try_acquire(_LOCK_T* lock);
int __local_lock_try_acquire_recursive(_LOCK_T* lock);
void __local_lock_release(_LOCK_T* lock);
void __local_lock_release_recursive(_LOCK_T* lock);

void __local_lock_init(_LOCK_T* lock) {
  if (lock != NULL) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_FAST_NP);
    pthread_mutex_init((pthread_mutex_t*)lock, &attr);
  }
}

void __local_lock_init_recursive(_LOCK_T* lock) {
  if (lock != NULL) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
    pthread_mutex_init((pthread_mutex_t*)lock, &attr);
  }
}

void __local_lock_close(_LOCK_T* lock) {
  if (lock != NULL) {
    pthread_mutex_destroy((pthread_mutex_t*)lock);
  }
}

void __local_lock_close_recursive(_LOCK_T* lock) {
  __local_lock_close(lock);
}

void __local_lock_acquire(_LOCK_T* lock) {
  if (0 == __nc_thread_id_counter) {
    /*
     * pthread library is not initialized yet - there is only one thread.
     * Calling pthread_mutex_lock will cause an access violation because it
     * will attempt to access the TDB which is not initialized yet
     */
    return;
  }
  if (lock != NULL) {
    pthread_mutex_lock((pthread_mutex_t*)lock);
  }
}

void __local_lock_acquire_recursive(_LOCK_T* lock) {
  __local_lock_acquire(lock);
}

int __local_lock_try_acquire(_LOCK_T* lock) {
  if (0 == __nc_thread_id_counter) {
    /*
    * pthread library is not initialized yet - there is only one thread.
    * Calling pthread_mutex_lock will cause an access violation because it
    * will attempt to access the TDB which is not initialized yet
    */
    return 0;
  }

  if (lock != NULL) {
    return pthread_mutex_trylock((pthread_mutex_t*)lock);
  } else {
    return EINVAL;
  }
}

int __local_lock_try_acquire_recursive(_LOCK_T* lock) {
  return __local_lock_try_acquire(lock);
}

void __local_lock_release(_LOCK_T* lock) {
  if (0 == __nc_thread_id_counter) {
    /*
    * pthread library is not initialized yet - there is only one thread.
    * Calling pthread_mutex_lock will cause an access violation because it
    * will attempt to access the TDB which is not initialized yet
    * NOTE: there is no race condition here because the value of the counter
    * cannot change while the lock is held - the startup process is
    * single-threaded.
    */
    return;
  }

  if (lock != NULL) {
    pthread_mutex_unlock((pthread_mutex_t*)lock);
  }
}

void __local_lock_release_recursive(_LOCK_T* lock) {
  __local_lock_release(lock);
}
