# sed script for DJGPP specific editing of config.hin

# Copyright (C) 2005, 2006, 2007, 2008 Free Software Foundation, Inc.

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.


$ a\
\
\
/* DJGPP specific defines.  */\
\
#include <unistd.h>\
#define TAB_EXT     ((pathconf(NULL, _PC_NAME_MAX) > 12) ? ".tab" : "_tab")\
#define OUTPUT_EXT  ((pathconf(NULL, _PC_NAME_MAX) > 12) ? ".output" : ".out")\
\
#define DEFAULT_TMPDIR  "/dev/env/DJDIR/tmp"
