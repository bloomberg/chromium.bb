/* Copied and simplified from gcc's crtstuff.c.

   Specialized bits of code needed to support construction and
   destruction of file-scope objects in C++ code.
   Copyright (C) 1991, 1994, 1995, 1996, 1997, 1998,
   1999, 2000, 2001, 2002, 2003, 2004, 2005 Free Software Foundation, Inc.
   Contributed by Ron Guilmette (rfg@monkeys.com).

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

In addition to the permissions in the GNU General Public License, the
Free Software Foundation gives you unlimited permission to link the
compiled version of this file into combinations with other programs,
and to distribute those combinations without any restriction coming
from the use of this file.  (The General Public License restrictions
do apply in other respects; for example, they cover modification of
the file, and distribution when not linked into a combine
executable.)

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to the Free
Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301, USA.  */

/* TODO: switch to the implementation in http://llvm.org/PR5323 so
   that we have more consistent licensing. */

typedef void (*func_ptr) (void);

static func_ptr __CTOR_END__[1]
  __attribute__((used, section(".ctors"), aligned(sizeof(func_ptr))))
  = { (func_ptr) 0 };

static func_ptr __DTOR_END__[1]
  __attribute__((__used__, section(".dtors"), aligned(sizeof(func_ptr))))
  = { (func_ptr) 0 };

static void __attribute__((used))
__do_global_ctors_aux (void)
{
  func_ptr *p;
  for (p = __CTOR_END__ - 1; *p != (func_ptr) -1; p--)
    (*p) ();
}
