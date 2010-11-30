/*
 * Copyright 2008 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl Simple/secure ELF loader (NaCl SEL).
 */
#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_arm || NACL_SANDBOX_FIXED_AT_ZERO == 1
/* Required for our use of mallopt -- see below. */
#include <malloc.h>
#endif

#include "native_client/src/shared/gio/gio.h"
#include "native_client/src/shared/imc/nacl_imc_c.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

#ifdef NACL_BREAKPAD
#include "native_client/src/trusted/nacl_breakpad/nacl_breakpad.h"
#endif
#include "native_client/src/trusted/service_runtime/env_cleanser.h"
#include "native_client/src/trusted/service_runtime/nacl_app.h"
#include "native_client/src/trusted/service_runtime/nacl_all_modules.h"
#include "native_client/src/trusted/service_runtime/nacl_config_dangerous.h"
#include "native_client/src/trusted/service_runtime/nacl_debug.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_common.h"
#include "native_client/src/trusted/service_runtime/outer_sandbox.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_qualify.h"

/* may redefine main() to install a hook */
#if defined(HAVE_SDL)
#include <SDL.h>
#endif

static void VmentryPrinter(void           *state,
                    struct NaClVmmapEntry *vmep) {
  UNREFERENCED_PARAMETER(state);
  printf("page num 0x%06x\n", (uint32_t)vmep->page_num);
  printf("num pages %d\n", (uint32_t)vmep->npages);
  printf("prot bits %x\n", vmep->prot);
  fflush(stdout);
}

static void PrintVmmap(struct NaClApp  *nap) {
  printf("In PrintVmmap\n");
  fflush(stdout);
  NaClXMutexLock(&nap->mu);
  NaClVmmapVisit(&nap->mem_map, VmentryPrinter, (void *) 0);

  NaClXMutexUnlock(&nap->mu);
}


struct redir {
  struct redir  *next;
  int           nacl_desc;
  enum {
    HOST_DESC,
    IMC_DESC
  }             tag;
  union {
    struct {
      int d;
      int mode;
    }                         host;
    NaClHandle                handle;
    struct NaClSocketAddress  addr;
  } u;
};

int ImportModeMap(char opt) {
  switch (opt) {
    case 'h':
      return O_RDWR;
    case 'r':
      return O_RDONLY;
    case 'w':
      return O_WRONLY;
  }
  fprintf(stderr, ("option %c not understood as a host descriptor"
                   " import mode\n"),
          opt);
  exit(1);
  /* NOTREACHED */
}

int NaClDup2(int d_old, int d_new) {
  if (d_old == d_new) {
    return 0;
  }
  if (DUP2(d_old, d_new) == -1) {
    fprintf(stderr, "sel_ldr: dup2 for log file failed\n");
    exit(1);
  }
  return 1;
}


static void PrintUsage() {
  /* NOTE: this is broken up into multiple statements to work around
           the constant string size limit */
  fprintf(stderr,
          "Usage: sel_ldr [-h d:D] [-r d:D] [-w d:D] [-i d:D]\n"
          "               [-f nacl_file]\n"
          "               [-l log_file]\n"
          "               [-X d] [-acFgImMRsQv] -- [nacl_file] [args]\n"
          "\n");
  fprintf(stderr,
          " -h\n"
          " -r\n"
          " -w associate a host POSIX descriptor D with app desc d\n"
          "    that was opened in O_RDWR, O_RDONLY, and O_WRONLY modes\n"
          "    respectively\n"
          " -i associates an IMC handle D with app desc d\n"
          " -f file to load; if omitted, 1st arg after \"--\" is loaded\n"
          " -v increases verbosity\n"
          " -X create a bound socket and export the address via an\n"
          "    IMC message to a corresponding NaCl app descriptor\n"
          "    (use -1 to create the bound socket / address descriptor\n"
          "    pair, but that no export via IMC should occur)\n"
          " [NB: -m and -M only applies to the SDL builds\n ]\n");
  fprintf(stderr,
          " -m enforce that certain syscalls can only be made from\n"
          "    the main NaCl app thread, so that we're enforcing (bug)\n"
          "    compatibility with standalone windows apps that must\n"
          "    make certain (e.g., graphics) calls from the main\n"
          "    thread (default)\n"
          " -M allow syscalls to be made from any NaCl app thread,\n"
          "    since these (windows-library-using) syscalls are\n"
          "    actually done via a work queue using the sel_ldr\n"
          "    main thread anyway\n"
          " -R an RPC supplies the NaCl module.\n"
          "    No nacl_file argument is expected, and the -f flag cannot be\n"
          "    used with this flag.\n"
          "\n"
          " (testing flags)\n"
          " -a allow file access! dangerous!\n"
          " -c ignore validator! dangerous!\n"
          " -F fuzz testing; quit after loading NaCl app\n"
          " -g enable gdb debug stub.\n"
          " -I disable ELF ABI version number check (safe)\n"
          " -l <file>  write log output to the given file\n"
          " -s safely stub out non-validating instructions\n"
          " -Q disable platform qualification (dangerous!)\n"
          );  /* easier to add new flags/lines */
}

/*
 * Note that we cannot use the 3 arg declaration for main, since SDL
 * preprocessor defines main to SDLmain and then provides a
 * replacement main that performs other startup initialization.  The
 * SDL startup code only supplies 2 args to the renamed SDLmain, and
 * if we used the 3 arg version here we'd pick up some garbage off the
 * stack for envp.  Instead, we make envp be a local variable, and
 * initialize it from the eviron global variable.
 *
 * NB: do NOT return from main until our dependency on SDL is removed.
 * This is because the OSX version of SDL ignores main's return value
 * and always exits with an exit status of 0, breaking tests.
 */
int main(int  argc,
         char **argv) {
  char const *const             *envp;
  int                           opt;
  char                          *rest;
  struct redir                  *entry;
  struct redir                  *redir_queue;
  struct redir                  **redir_qend;


  struct NaClApp                state;
  char                          *nacl_file = NULL;
  int                           rpc_supplies_nexe = 0;
  int                           main_thread_only = 1;
  int                           export_addr_to = -2;
  enum NaClAbiCheckOption       check_abi = NACL_ABI_CHECK_OPTION_CHECK;

  struct NaClApp                *nap;

  struct GioFile                gout;
  NaClErrorCode                 errcode;
  struct GioMemoryFileSnapshot  gf;


  int                           ret_code;
  /* NOTE: because of windows dll issue this cannot be moved to the top level */
  /* @IGNORE_LINES_FOR_CODE_HYGIENE[1] */
  extern char                   **environ;

  char                          *log_file = NULL;
  struct GioFile                *log_gio;
  int                           log_desc;
  int                           verbosity = 0;
  int                           fuzzing_quit_after_load = 0;
  int                           debug_mode_bypass_acl_checks = 0;
  int                           debug_mode_ignore_validator = 0;
  int                           stub_out_mode = 0;
  int                           skip_qualification = 0;
  int                           start_broken = 0;

  struct NaClEnvCleanser        filtered_env;

  const char* sandbox_fd_string;

#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_arm || NACL_SANDBOX_FIXED_AT_ZERO == 1
  /*
   * Set malloc not to use mmap even for large allocations.  This is currently
   * necessary when we must use a specific area of RAM for the sandbox.
   *
   * During startup, before the sandbox is set up, the sel_ldr allocates a chunk
   * of memory to store the untrusted code.  Normally such an allocation would
   * go into the sel_ldr's heap area, but the allocation is typically large --
   * at least hundreds of KiB.  The default malloc configuration on Linux (at
   * least) switches to mmap for such allocations, and mmap will select
   * essentially any unoccupied section of the address space.  The result: the
   * nexe is allocated in the region we use for the sandbox, we protect the
   * address space, and then the memcpy into the sandbox (of course) fails.
   *
   * This is at best a temporary fix.  The proper fix is to reserve the
   * sandbox region early enough that this isn't a problem.  Possible methods
   * are discussed in this bug:
   *   http://code.google.com/p/nativeclient/issues/detail?id=232
   */
  mallopt(M_MMAP_MAX, 0);
#endif

  /* SDL does not pass 3 args to re-defined main */
  envp = (char const *const *) environ;

  ret_code = 1;
  redir_queue = NULL;
  redir_qend = &redir_queue;

  /*
   * Set an exception handler so Windows won't show a message box if
   * an exception occurs
   */
  WINDOWS_EXCEPTION_TRY;

  NaClAllModulesInit();

  verbosity = NaClLogGetVerbosity();

  fflush((FILE *) NULL);

  if (!GioFileRefCtor(&gout, stdout)) {
    fprintf(stderr, "Could not create general standard output channel\n");
    exit(1);
  }

  while ((opt = getopt(argc, argv, "abcf:Fgh:i:Il:mMQr:Rsvw:X:")) != -1) {
    switch (opt) {
      case 'c':
        fprintf(stderr, "DEBUG MODE ENABLED (ignore validator)\n");
        debug_mode_ignore_validator = 1;
        break;
      case 'a':
        fprintf(stderr, "DEBUG MODE ENABLED (bypass acl)\n");
        debug_mode_bypass_acl_checks = 1;
        break;
      case 'b':
        start_broken = 1;
        break;
      case 'f':
        nacl_file = optarg;
        break;
      case 'F':
        fuzzing_quit_after_load = 1;
        break;
      case 'g':
        NaClDebugSetAllow(1);
        break;
      case 'h':
      case 'r':
      case 'w':
        /* import host descriptor */
        entry = malloc(sizeof *entry);
        if (NULL == entry) {
          fprintf(stderr, "No memory for redirection queue\n");
          exit(1);
        }
        entry->next = NULL;
        entry->nacl_desc = strtol(optarg, &rest, 0);
        entry->tag = HOST_DESC;
        entry->u.host.d = strtol(rest+1, (char **) 0, 0);
        entry->u.host.mode = ImportModeMap(opt);
        *redir_qend = entry;
        redir_qend = &entry->next;
        break;
      case 'i':
        /* import IMC handle */
        entry = malloc(sizeof *entry);
        if (NULL == entry) {
          fprintf(stderr, "No memory for redirection queue\n");
          exit(1);
        }
        entry->next = NULL;
        entry->nacl_desc = strtol(optarg, &rest, 0);
        entry->tag = IMC_DESC;
        entry->u.handle = (NaClHandle) strtol(rest+1, (char **) 0, 0);
        *redir_qend = entry;
        redir_qend = &entry->next;
        break;
      case 'I':
        check_abi = NACL_ABI_CHECK_OPTION_SKIP;
        break;
     case 'l':
        log_file = optarg;
        break;
      case 'm':
        main_thread_only = 1;
        break;
      case 'M':
        main_thread_only = 0;
        break;
      case 'R':
        rpc_supplies_nexe = 1;
        break;
      /* case 'r':  with 'h' and 'w' above */
      case 'v':
        ++verbosity;
        NaClLogIncrVerbosity();
        break;
      /* case 'w':  with 'h' and 'r' above */
      case 'X':
        export_addr_to = strtol(optarg, (char **) 0, 0);
        break;
      case 'Q':
        fprintf(stderr, "PLATFORM QUALIFICATION DISABLED BY -Q - "
            "Native Client's sandbox will be unreliable!\n");
        skip_qualification = 1;
        break;
      case 's':
        stub_out_mode = 1;
        break;
      default:
       fprintf(stderr, "ERROR: unknown option: [%c]\n\n", opt);
       PrintUsage();
       exit(-1);
    }
  }

  if (verbosity) {
    int         ix;
    char const  *separator = "";

    fprintf(stderr, "sel_ldr argument list:\n");
    for (ix = 0; ix < argc; ++ix) {
      fprintf(stderr, "%s%s", separator, argv[ix]);
      separator = " ";
    }
    putc('\n', stderr);
  }

  /* Check if we should start broken */
  if (start_broken) NaClDebugSetStartBroken(1);

  if (NACL_DANGEROUS_STUFF_ENABLED) {
    fprintf(stderr,
            "WARNING WARNING WARNING WARNING"
            " WARNING WARNING WARNING WARNING\n");
    fprintf(stderr,
            "WARNING\n");
    fprintf(stderr,
            "WARNING  Using a dangerous/debug configuration.\n");
    fprintf(stderr,
            "WARNING\n");
    fprintf(stderr,
            "WARNING WARNING WARNING WARNING"
            " WARNING WARNING WARNING WARNING\n");
  }

  if (debug_mode_bypass_acl_checks) {
    NaClInsecurelyBypassAllAclChecks();
  }

#if NACL_DANGEROUS_DEBUG_MODE_DISABLE_INNER_SANDBOX
  if (debug_mode_bypass_acl_checks == 0 &&
      debug_mode_ignore_validator == 0) {
    fprintf(stderr,
            "ERROR: dangerous debug version of sel_ldr can only "
            "be invoked with -a/-c options");
    exit(-1);
  }
#endif
  /*
   * change stdout/stderr to log file now, so that subsequent error
   * messages will go there.  unfortunately, error messages that
   * result from getopt processing -- usually out-of-memory, which
   * shouldn't happen -- won't show up.
   */
  if (NULL != log_file) {
    NaClLogSetFile(log_file);
  }

  /*
   * NB: the following cast is okay since we only ever permit GioFile
   * objects to be used -- NaClLogModuleInit and NaClLogSetFile both
   * can only assign the log output to a file.  If neither were
   * called, logging goes to stderr.
   */
  log_gio = (struct GioFile *) NaClLogGetGio();
  /*
   * By default, the logging module logs to stderr, or descriptor 2.
   * If NaClLogSetFile was performed above, then log_desc will have
   * the non-default value.
   */
  log_desc = fileno(log_gio->iop);

  if (rpc_supplies_nexe) {
    if (NULL != nacl_file) {
      fprintf(stderr,
              "sel_ldr: mutually exclusive flags -f and -R both used\n");
      exit(1);
    }
    /* post: NULL == nacl_file */
    if (export_addr_to < 0) {
      fprintf(stderr,
              "sel_ldr: -R requires -X to set up secure command channel\n");
      exit(1);
    }
  } else {
    if (NULL == nacl_file && optind < argc) {
      nacl_file = argv[optind];
      ++optind;
    }
    if (NULL == nacl_file) {
      fprintf(stderr, "No nacl file specified\n");
      exit(1);
    }
    /* post: NULL != nacl_file */
  }
  /*
   * post condition established by the above code (in Hoare logic
   * terminology):
   *
   * NULL == nacl_file iff rpc_supplies_nexe
   *
   * so hence forth, testing !rpc_supplies_nexe suffices for
   * establishing NULL != nacl_file.
   */
  CHECK((NULL == nacl_file) == rpc_supplies_nexe);

  /* to be passed to NaClMain, eventually... */
  argv[--optind] = (char *) "NaClMain";

  if (!NaClAppCtor(&state)) {
    fprintf(stderr, "Error while constructing app state\n");
    goto done_file_dtor;
  }

  state.restrict_to_main_thread = main_thread_only;
  state.ignore_validator_result = debug_mode_ignore_validator;
  state.validator_stub_out_mode = stub_out_mode;

  nap = &state;
  errcode = LOAD_OK;

  /*
   * in order to report load error to the browser plugin through the
   * secure command channel, we do not immediate jump to cleanup code
   * on error.  rather, we continue processing (assuming earlier
   * errors do not make it inappropriate) until the secure command
   * channel is set up, and then bail out.
   */

  /*
   * Ensure the platform qualification checks pass.
   */
  if (!skip_qualification) {
    NaClErrorCode pq_error = NaClRunSelQualificationTests();
    if (LOAD_OK != pq_error) {
      errcode = pq_error;
      nap->module_load_status = pq_error;
      fprintf(stderr, "Error while loading \"%s\": %s\n",
              NULL != nacl_file ? nacl_file
                                : "(no file, to-be-supplied-via-RPC)",
              NaClErrorString(errcode));
    }
  }

  if (!rpc_supplies_nexe) {
    if (0 == GioMemoryFileSnapshotCtor(&gf, nacl_file)) {
      perror("sel_main");
      fprintf(stderr, "Cannot open \"%s\".\n", nacl_file);
      errcode = LOAD_OPEN_ERROR;
    }

    if (LOAD_OK == errcode) {
      errcode = NaClAppLoadFile((struct Gio *) &gf, nap, check_abi);
      if (LOAD_OK != errcode) {
        fprintf(stderr, "Error while loading \"%s\": %s\n",
                nacl_file,
                NaClErrorString(errcode));
        fprintf(stderr,
                ("Using the wrong type of nexe (nacl-x86-32"
                 " on an x86-64 or vice versa)\n"
                 "or a corrupt nexe file may be"
                 " responsible for this error.\n"));
      }

      NaClXMutexLock(&nap->mu);
      nap->module_load_status = errcode;
      NaClXCondVarBroadcast(&nap->cv);
      NaClXMutexUnlock(&nap->mu);
    }

    if (fuzzing_quit_after_load) {
      exit(0);
    }
  }

  /*
   * Execute additional I/O redirections.  NB: since the NaClApp
   * takes ownership of host / IMC socket descriptors, all but
   * the first run will not get access if the NaClApp closes
   * them.  Currently a normal NaClApp process exit does not
   * close descriptors, since the underlying host OS will do so
   * as part of service runtime exit.
   */
  NaClLog(4, "Processing I/O redirection/inheritance from environment\n");
  for (entry = redir_queue; NULL != entry; entry = entry->next) {
    switch (entry->tag) {
      case HOST_DESC:
        NaClAddHostDescriptor(nap, entry->u.host.d,
                              entry->u.host.mode, entry->nacl_desc);
        break;
      case IMC_DESC:
        NaClAddImcHandle(nap, entry->u.handle, entry->nacl_desc);
        break;
    }
  }

  /*
   * If export_addr_to is set to a non-negative integer, we create a
   * bound socket and socket address pair and bind the former to
   * descriptor 3 and the latter to descriptor 4.  The socket address
   * is written out to the export_addr_to descriptor.
   *
   * The service runtime also accepts a connection on the bound socket
   * and spawns a secure command channel thread to service it.
   *
   * If export_addr_to is -1, we only create the bound socket and
   * socket address pair, and we do not export to an IMC socket.  This
   * use case is typically only used in testing, where we only "dump"
   * the socket address to stdout or similar channel.
   */
  if (-2 < export_addr_to) {
    NaClCreateServiceSocket(nap);
    if (0 <= export_addr_to) {
      NaClSendServiceAddressTo(nap, export_addr_to);
      /*
       * NB: spawns a thread that uses the command channel.  we do
       * this after NaClAppLoadFile so that NaClApp object is more
       * fully populated.  Hereafter any changes to nap should be done
       * while holding locks.
       */
      NaClSecureCommandChannel(nap);
    }
  }
  /*
   * May have created a thread, so need to synchronize uses of nap
   * contents henceforth.
   */

  if (rpc_supplies_nexe) {
    errcode = NaClWaitForLoadModuleStatus(nap);
  } else {
    /**************************************************************************
     * TODO(bsy): This else block should be made unconditional and
     * invoked after the LoadModule RPC completes, eliminating the
     * essentially dulicated code in latter part of NaClLoadModuleRpc.
     * This cannot be done until we have full saucer separation
     * technology, since Chrome currently uses sel_main_chrome.c and
     * relies on the functionality of the duplicated code.
     *************************************************************************/
    if (LOAD_OK == errcode) {
      if (verbosity) {
        gprintf((struct Gio *) &gout, "printing NaClApp details\n");
        NaClAppPrintDetails(nap, (struct Gio *) &gout);
      }

      /*
       * Finish setting up the NaCl App.  This includes dup'ing
       * descriptors 0-2 and making them available to the NaCl App.
       *
       * If a log file were specified at the command-line and the
       * logging module was set to use a log file instead of standard
       * error, then we redirect standard output of the NaCl module to
       * that log file too.  Standard input is inherited, and could
       * result in a NaCl module competing for input from the terminal;
       * for graphical / browser plugin environments, this never is
       * allowed to happen, and having this is useful for debugging, and
       * for potential standalone text-mode applications of NaCl.
       *
       * TODO(bsy): consider whether inheriting stdin should occur only
       * in debug mode.
       */
      errcode = NaClAppPrepareToLaunch(nap,
                                       0,
                                       (2 == log_desc) ? 1 : log_desc,
                                       log_desc);
      if (LOAD_OK != errcode) {
        nap->module_load_status = errcode;
        fprintf(stderr, "NaClAppPrepareToLaunch returned %d", errcode);
      }
    }

    if (-1 == (*((struct Gio *) &gf)->vtbl->Close)((struct Gio *) &gf)) {
      fprintf(stderr, "Error while closing \"%s\".\n", argv[optind]);
    }
    (*((struct Gio *) &gf)->vtbl->Dtor)((struct Gio *) &gf);

    /* Give debuggers a well known point at which xlate_base is known.  */
    NaClGdbHook(&state);
  }

  /*
   * Print out a marker for scripts to use to mark the start of app
   * output.
   */
  NaClLog(1, "NACL: Application output follows\n");

  /*
   * Chroot() ourselves.  Based on agl's chrome implementation.
   *
   * TODO(mseaborn): This enables a SUID-based Linux outer sandbox,
   * but it is not used now.  When we do have an outer sandbox for
   * standalone sel_ldr, we should enable it earlier on, and merge it
   * with NaClEnableOuterSandbox().
   */
  sandbox_fd_string = getenv(NACL_SANDBOX_CHROOT_FD);
  if (NULL != sandbox_fd_string) {
    static const char kChrootMe = 'C';
    static const char kChrootSuccess = 'O';

    char* endptr;
    char reply;
    int fd;
    long fd_long;
    errno = 0;  /* To distinguish between success/failure after call */
    fd_long = strtol(sandbox_fd_string, &endptr, 10);

    fprintf(stdout, "Chrooting the NaCl module\n");
    if ((ERANGE == errno && (LONG_MAX == fd_long || LONG_MIN == fd_long)) ||
        (0 != errno && 0 == fd_long)) {
      perror("strtol");
      exit(1);
    }
    if (endptr == sandbox_fd_string) {
      fprintf(stderr, "Could not initialize sandbox fd: No digits found\n");
      exit(1);
    }
    if (*endptr) {
      fprintf(stderr, "Could not initialize sandbox fd: Extra digits\n");
      exit(1);
    }
    fd = fd_long;

    /*
     * TODO(neha): When we're more merged with chrome, use HANDLE_EINTR()
     */
    if (write(fd, &kChrootMe, 1) != 1) {
      fprintf(stderr, "Cound not signal sandbox to chroot()\n");
      exit(1);
    }

    /*
     * TODO(neha): When we're more merged with chrome, use HANDLE_EINTR()
     */
    if (read(fd, &reply, 1) != 1) {
      fprintf(stderr, "Could not get response to chroot() from sandbox\n");
      exit(1);
    }
    if (kChrootSuccess != reply) {
      fprintf(stderr, "%s\n", &reply);
      fprintf(stderr, "Reply not correct\n");
      exit(1);
    }
  }

  /*
   * Make sure all the file buffers are flushed before entering
   * the application code.
   */
  fflush((FILE *) NULL);

  if (NULL != nap->secure_channel && LOAD_OK == errcode) {
    /*
     * wait for start_module RPC call on secure channel thread.
     */
    errcode = NaClWaitForStartModuleCommand(nap);
  }

  /*
   * error reporting done; can quit now if there was an error earlier.
   */
  if (LOAD_OK != errcode) {
    NaClLog(4,
            "Not running app code since errcode is %s (%d)\n",
            NaClErrorString(errcode),
            errcode);
    goto done;
  }

  NaClEnvCleanserCtor(&filtered_env);
  if (!NaClEnvCleanserInit(&filtered_env, envp)) {
    fprintf(stderr, "Filtering environment variables failed\n");
    NaClEnvCleanserDtor(&filtered_env);
    goto done;
  }
  /*
   * only nap->ehdrs.e_entry is usable, no symbol table is
   * available.
   */
  if (!NaClCreateMainThread(nap,
                            argc - optind,
                            argv + optind,
                            NaClEnvCleanserEnvironment(&filtered_env))) {
    fprintf(stderr, "creating main thread failed\n");
    NaClEnvCleanserDtor(&filtered_env);
    goto done;
  }
  NaClEnvCleanserDtor(&filtered_env);

  ret_code = NaClWaitForMainThreadToExit(nap);

  /*
   * exit_group or equiv kills any still running threads while module
   * addr space is still valid.  otherwise we'd have to kill threads
   * before we clean up the address space.
   */
  _exit(ret_code);

 done:
  fflush(stdout);

  if (verbosity) {
    gprintf((struct Gio *) &gout, "exiting -- printing NaClApp details\n");
    NaClAppPrintDetails(nap, (struct Gio *) &gout);

    printf("Dumping vmmap.\n"); fflush(stdout);
    PrintVmmap(nap);
    fflush(stdout);
  }

 done_file_dtor:
  if (verbosity > 0) {
    printf("Done.\n");
  }
  fflush(stdout);

  NaClAllModulesFini();

  WINDOWS_EXCEPTION_CATCH;

  _exit(ret_code);
}
