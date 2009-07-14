#!/bin/bash
#
# sel_ldr.bash
# A simple shell script for starting up sel_ldr with an enhanced
# library path, for Linux. This allows us to install libraries in
# a user directory rather than having to put them someplace like
# /usr/lib
#
LD_LIBRARY_PATH=$HOME/.mozilla/plugins:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH
if [ -f $HOME/.mozilla/plugins/sel_ldr_bin.trace ]; then
  exec $HOME/.mozilla/plugins/sel_ldr_bin.trace "$@"
else
  exec $HOME/.mozilla/plugins/sel_ldr_bin "$@"
fi
