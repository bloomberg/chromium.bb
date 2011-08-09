/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Native Client startup wrapper
 * (NOTE: this code used to live in the various platform specific
 * assembler files.)
 */

/* avoid including header files in order to make this independent of anything */
/* @IGNORE_LINES_FOR_CODE_HYGIENE[10] */
extern void __libc_init_array();
extern void __libc_fini_array();
extern int main(int argc, char *argv[], char *envp[]);
extern void exit(int result);
extern void __pthread_initialize();
extern void atexit(void (*funptr)());

typedef void (*FUN_PTR)();

/* @IGNORE_LINES_FOR_CODE_HYGIENE[1] */
extern char** environ;

/*
 *  __nacl_startup is called from crt1XXX, it ultimately calls main().
 */
void __nacl_startup(int argc, char *argv[], char *envp[]) {
  int result;
  /*
   * Remember envp for use by getenv, etc.
   */
  environ = envp;
  /*
   * Install the fini section for use at exit.  The C++ static object
   * destructors are invoked from here.
   */
  atexit(__libc_fini_array);
  /*
   * Initialize the pthreads library.  We need to do at least a minimal
   * amount of initialization (e.g., set up gs) to allow thread local
   * storage references to work.  The default binding of the symbol
   * is weak, replaced by the real pthread library initialization when
   * present.
   */
  __pthread_initialize();
  /*
   * Execute the init section before starting main.  The C++ static
   * object constructors are invoked from here.
   * The code can be found in:
   * newlib/libc/misc/init.c
   * It calls (in this order):
   * * all function pointers in[__preinit_array_start,  __preinit_array_end[
   *   ( the .preinit_array section)
   * * _init (the .init section beginning)
   * * all function pointers in [__init_array_start,  __init_array_end[
   *   ( the .init_array section)
   *
   * NOTE: there are three version of registering code to be run before main
   *       * emit code into .init (not with pnacl)
   *       * add a pointer to .init_array/.preinit_array section
   *       * add a pointer to .ctors section (not with pnacl)
   */
  __libc_init_array();
  result = main(argc, argv, envp);
  /*
   * exit will also call atexit()
   */
  exit(result);
}

/* @IGNORE_LINES_FOR_CODE_HYGIENE[1] */
#if defined(__pnacl__)
/* pnacl handles init/fini through the .init_array and .fini_array sections
 * rather than .init/.fini.  Stub out the functions called from newlib's
 * startup routine in libc/misc/init.c.
 */
void _init() { }
void _fini() { }

/* __dso_handle is zero on the main executable. */
void *__dso_handle = 0;

/* HACK HACK HACK */
/* The real structure is defined in llvm-gcc-4.2/gcc/unwind-dw2-fde.h
   this is something that is at least twice as big.
*/
struct object {
  void *p[16];
};

/* NOTE: __register_frameXXX() are provided by libgcc_eh.a, code can be found
 * here: llvm-gcc-4.2/gcc/unwind-dw2-fde.c
 * traditionally gcc uses weak linkage magic to making linking this library
 * in optional.
 * To simplify our TC we will always link this in.
 */

/* @IGNORE_LINES_FOR_CODE_HYGIENE[2] */
extern void __deregister_frame_info (const void *);
extern void __register_frame_info(void *begin, struct object *ob);
/* pnacl exception handling support.
 * The linker script brackets the .eh_frame section by two proprietary
 * pnacl sections: .eh_frame_prolog and .eh_frame_epilog.
 */
#define SECTION_ATTR(sec)\
  __attribute__ ((used, section(sec), aligned(sizeof(FUN_PTR))))
#define ADD_FUN_PTR_TO_SEC(name, sec, ptr)       \
  static const FUN_PTR name SECTION_ATTR(sec)  = { ptr }
#define EH_FRAME_END_MARKER ((FUN_PTR) 0)

/* Exception handling frames are aggregated into a single section called
 * .eh_frame.  The runtime system needs to (1) have a symbol for the beginning
 * of this section, and needs to (2) mark the end of the section by a NULL.
 * To eliminate the need for binary "bookend" objects to accomplish this pair
 * of goals, we rely on support in the native linker script.  In particular,
 * the portion of the linker script that concatenates the .eh_frame sections
 * now prepends the .eh_frame_prolog and appends the .eh_frame_epilog sections
 * to produce the resulting section.
 */
ADD_FUN_PTR_TO_SEC(__EH_FRAME_BEGIN__[0], ".eh_frame_prolog",);
ADD_FUN_PTR_TO_SEC(__EH_FRAME_END__[1],
                   ".eh_frame_epilog",
                   EH_FRAME_END_MARKER);

/* Registration and deregistration of exception handling tables are done
 * by .init_array and .fini_array elements added here.
 */
/* __attribute__((constructor)) places a call to the function in the
 * .init_array section in PNaCl.  The function pointers in .init_array
 * are then invoked in order (frame_dummy is invoked first) before main.
 */
static void frame_dummy (void) __attribute__ ((constructor));
static void frame_dummy (void) {
  static struct object object;
  /*
   * NOTE: the volatile hack below prevents an undesirable llvm optimization.
   * Without it llvm assumes that the zero length array, __EH_FRAME_BEGIN__,
   * consists of at least one zero pointer. This makes
   * __register_frame_info() into a nop.
   * This hack can be avoided if we move the bracketing eh_frame
   * into the linkerscript.
   */
  volatile FUN_PTR start_of_eh_frame_section = (FUN_PTR) __EH_FRAME_BEGIN__;
  __register_frame_info (start_of_eh_frame_section, &object);
}

/* __attribute__((destructor)) places a call to the function in the
 * .fini_array section in PNaCl.  The function pointers in .fini_array
 * are then invoked in inverse order (__do_global_dtors_aux is invoked last)
 * at exit.
 */
static void __do_global_dtors_aux (void) __attribute__ ((destructor));
static void __do_global_dtors_aux (void) {
  volatile FUN_PTR start_of_eh_frame_section = (FUN_PTR) __EH_FRAME_BEGIN__;
  __deregister_frame_info (start_of_eh_frame_section);
}

#endif  /* defined(__pnacl__) */
