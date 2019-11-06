/* Subprocesses with pipes.
   Copyright (C) 2002, 2004, 2005 Free Software Foundation, Inc.

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

/* Written by Paul Eggert <eggert@twinsun.com>
   and Florian Krohm <florian@edamail.fishkill.ibm.com>.  */

#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

void init_subpipe (void);
pid_t create_subpipe (char const * const *, int[2]);
void end_of_output_subpipe (pid_t, int[2]);
void reap_subpipe (pid_t, char const *);
#ifdef _WIN32
# include <io.h>
# include <fcntl.h>
# define pipe(a) _pipe((a), 0, O_NOINHERIT)
#endif
