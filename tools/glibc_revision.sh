#!/bin/bash
REVISION=0
for i in glibc_revision.sh REVISIONS Makefile ; do
  NREV="$(svn log "$(dirname "$0")/$i" 2>/dev/null | head -n 2 | tail -n 1 |
                                                sed -e s'+r\([0-9]*\) .*+\1+')"
  if [[ "$NREV" -gt "$REVISION" ]]; then
    REVISION="$NREV"
  fi
done
echo "$REVISION"
