/* liblouis Braille Translation and Back-Translation Library

   Based on BRLTTY, copyright (C) 1999-2006 by
   The BRLTTY Team

   Copyright (C) 2004, 2005, 2006, 2009
   ViewPlus Technologies, Inc. www.viewplus.com and
   JJB Software, Inc. www.jjb-software.com

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

   Maintained by John J. Boyer john.boyer@jjb-software.com
   */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "louis.h"

int
main (int argc, char **argv)
{
  const TranslationTableHeader *table;
  if (argc != 2)
    {
      fprintf (stderr, "Usage: lou_checktable tablename\n");
      exit (1);
    }
  if (!(table = lou_getTable (argv[1])))
    {
      lou_free ();
      return 1;
    }
  fprintf (stderr, "No errors found.\n");
  lou_free ();
  return 0;
}
