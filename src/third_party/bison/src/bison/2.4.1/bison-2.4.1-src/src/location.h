/* Locations for Bison
   Copyright (C) 2002, 2004, 2005, 2006, 2007 Free Software Foundation, Inc.

   This file is part of Bison, the GNU Compiler Compiler.

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

#ifndef LOCATION_H_
# define LOCATION_H_

# include "uniqstr.h"

/* A boundary between two characters.  */
typedef struct
{
  /* The name of the file that contains the boundary.  */
  uniqstr file;

  /* The (origin-1) line that contains the boundary.
     If this is INT_MAX, the line number has overflowed.  */
  int line;

  /* The (origin-1) column just after the boundary.  This is neither a
     byte count, nor a character count; it is a column count.
     If this is INT_MAX, the column number has overflowed.  */
  int column;

} boundary;

/* Set the position of \a a. */
static inline void
boundary_set (boundary *b, const char *f, int l, int c)
{
  b->file = f;
  b->line = l;
  b->column = c;
}

/* Return nonzero if A and B are equal boundaries.  */
static inline bool
equal_boundaries (boundary a, boundary b)
{
  return (a.column == b.column
	  && a.line == b.line
	  && UNIQSTR_EQ (a.file, b.file));
}

/* A location, that is, a region of source code.  */
typedef struct
{
  /* Boundary just before the location starts.  */
  boundary start;

  /* Boundary just after the location ends.  */
  boundary end;

} location;

#define YYLTYPE location

#define EMPTY_LOCATION_INIT {{NULL, 0, 0}, {NULL, 0, 0}}
extern location const empty_location;

/* Set *LOC and adjust scanner cursor to account for token TOKEN of
   size SIZE.  */
void location_compute (location *loc,
		       boundary *cur, char const *token, size_t size);

void location_print (FILE *out, location loc);

/* LOC_STR must be formatted as `file:line.column', it will be modified.  */
void boundary_set_from_string (boundary *bound, char *loc_str);

#endif /* ! defined LOCATION_H_ */
