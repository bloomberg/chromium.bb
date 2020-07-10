/* System-dependent definitions for Bison.

   Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007 Free
   Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifndef BISON_SYSTEM_H
#define BISON_SYSTEM_H

/* flex 2.5.31 gratutiously defines macros like INT8_MIN.  But this
   runs afoul of pre-C99 compilers that have <inttypes.h> or
   <stdint.h>, which are included below if available.  It also runs
   afoul of pre-C99 compilers that define these macros in <limits.h>.  */
#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901
# undef INT8_MIN
# undef INT16_MIN
# undef INT32_MIN
# undef INT8_MAX
# undef INT16_MAX
# undef UINT8_MAX
# undef INT32_MAX
# undef UINT16_MAX
# undef UINT32_MAX
#endif

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#include <unistd.h>
#include <inttypes.h>

#ifndef UINTPTR_MAX
/* This isn't perfect, but it's good enough for Bison, which needs
   only to hash pointers.  */
typedef size_t uintptr_t;
#endif


/*---------.
| Gnulib.  |
`---------*/

#include <unlocked-io.h>
#include <verify.h>
#include <xalloc.h>


/*-----------------.
| GCC extensions.  |
`-----------------*/

/* Use this to suppress gcc's `...may be used before initialized'
   warnings.  */
#ifdef lint
# define IF_LINT(Code) Code
#else
# define IF_LINT(Code) /* empty */
#endif

#ifndef __attribute__
/* This feature is available in gcc versions 2.5 and later.  */
# if (! defined __GNUC__ || __GNUC__ < 2 \
      || (__GNUC__ == 2 && __GNUC_MINOR__ < 5) || __STRICT_ANSI__)
#  define __attribute__(Spec) /* empty */
# endif
#endif

/* The __-protected variants of `format' and `printf' attributes
   are accepted by gcc versions 2.6.4 (effectively 2.7) and later.  */
#if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 7)
# define __format__ format
# define __printf__ printf
#endif

#ifndef ATTRIBUTE_NORETURN
# define ATTRIBUTE_NORETURN __attribute__ ((__noreturn__))
#endif

#ifndef ATTRIBUTE_UNUSED
# define ATTRIBUTE_UNUSED __attribute__ ((__unused__))
#endif

#define FUNCTION_PRINT() fprintf (stderr, "%s: ", __func__)

/*------.
| NLS.  |
`------*/

#include <locale.h>

#include <gettext.h>
#define _(Msgid)  gettext (Msgid)
#define N_(Msgid) (Msgid)


/*-----------.
| Booleans.  |
`-----------*/

#include <stdbool.h>



/*-------------.
| Assertions.  |
`-------------*/

/* <assert.h>'s assertions are too heavyweight, and can be disabled
   too easily, so use aver rather than assert.  */
static inline void
aver (bool assertion)
{
  if (! assertion)
    abort ();
}


/*-----------.
| Obstacks.  |
`-----------*/

#define obstack_chunk_alloc xmalloc
#define obstack_chunk_free  free
#include <obstack.h>

#define obstack_sgrow(Obs, Str) \
  obstack_grow (Obs, Str, strlen (Str))

#define obstack_fgrow1(Obs, Format, Arg1)	\
do {						\
  char buf[4096];				\
  sprintf (buf, Format, Arg1);			\
  obstack_grow (Obs, buf, strlen (buf));	\
} while (0)

#define obstack_fgrow2(Obs, Format, Arg1, Arg2)	\
do {						\
  char buf[4096];				\
  sprintf (buf, Format, Arg1, Arg2);		\
  obstack_grow (Obs, buf, strlen (buf));	\
} while (0)

#define obstack_fgrow3(Obs, Format, Arg1, Arg2, Arg3)	\
do {							\
  char buf[4096];					\
  sprintf (buf, Format, Arg1, Arg2, Arg3);		\
  obstack_grow (Obs, buf, strlen (buf));		\
} while (0)

#define obstack_fgrow4(Obs, Format, Arg1, Arg2, Arg3, Arg4)	\
do {								\
  char buf[4096];						\
  sprintf (buf, Format, Arg1, Arg2, Arg3, Arg4);		\
  obstack_grow (Obs, buf, strlen (buf));			\
} while (0)



/*-----------------------------------------.
| Extensions to use for the output files.  |
`-----------------------------------------*/

#ifndef OUTPUT_EXT
# define OUTPUT_EXT ".output"
#endif

#ifndef TAB_EXT
# define TAB_EXT ".tab"
#endif

#ifndef DEFAULT_TMPDIR
# define DEFAULT_TMPDIR "/tmp"
#endif



/*---------------------.
| Free a linked list.  |
`---------------------*/

#define LIST_FREE(Type, List)			\
do {						\
  Type *_node, *_next;				\
  for (_node = List; _node; _node = _next)	\
    {						\
      _next = _node->next;			\
      free (_node);				\
    }						\
} while (0)


/*---------------------------------------------.
| Debugging memory allocation (must be last).  |
`---------------------------------------------*/

# if WITH_DMALLOC
#  define DMALLOC_FUNC_CHECK
#  include <dmalloc.h>
# endif /* WITH_DMALLOC */

#endif  /* ! BISON_SYSTEM_H */
