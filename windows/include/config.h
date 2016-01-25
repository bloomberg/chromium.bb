/* liblouis Braille Translation and Back-Translation  Library

   Copyright (C) 2014 ViewPlus Technologies, Inc. www.viewplus.com
   and the liblouis team. http://liblouis.org
   All rights reserved

   This file is free software; you can redistribute it and/or modify it
   under the terms of the Lesser or Library GNU General Public License 
   as published by the
   Free Software Foundation; either version 3, or (at your option) any
   later version.

   This file is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
   Library GNU General Public License for more details.

   You should have received a copy of the Library GNU General Public 
   License along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
   */

#define PACKAGE_VERSION "liblouis-2.6.5"

/* The Visual C++ C Runtime deprecates several standard library functions in
   preference for _s variants that are specific to Visual C++. This removes
   those deprecation warnings. */
#define _CRT_SECURE_NO_WARNINGS

/* The Visual C++ C Runtime deprecates standard POSIX APIs that conflict with
   reserved ISO C names (like strdup) in favour of non-portable conforming
   variants that start with an '_'. This removes those deprecation warnings. */
#define _CRT_NONSTDC_NO_DEPRECATE
