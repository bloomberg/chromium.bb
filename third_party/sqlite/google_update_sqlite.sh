#!/bin/bash

# NOTICE: sqlite is no longer kept in CVS. It is now stored in fossil, a VCS
# built on top of sqlite (yay recursion). This script is kept as a reference
# for how to manually do the merge.

head -n 5 "$0"
exit 1

# A simple script to make it easier to merge in newer versions of sqlite.
# It may not work perfectly, in which case, it at least serves as an outline
# of the procedure to follow.

if [ "$1" = "" ]; then
  echo "Usage: $0 <Date to pull from CVS> [<merge tool>]"
  echo "Example: $0 '2007/01/24 09:54:56'"
  exit 1
fi

if [ ! -f VERSION_DATE ]; then
  echo "You must run this script in the sqlite directory, i.e.:"
  echo "\$ ./google_update_sqlite.sh"
  exit 1
fi

if [ "$2" = "" ]; then
  MERGE="kdiff3 -m"
fi

BASE_DATE=`cat VERSION_DATE`
NEW_DATE="$1"

cd ..
echo "_____ Logging in to sqlite.org CVS (log in as anonymous)..."
cvs -d :pserver:anonymous@www.sqlite.org:/sqlite login
cvs -d :pserver:anonymous@www.sqlite.org:/sqlite checkout -P -D "$BASE_DATE" -d sqlite-base sqlite
cvs -d :pserver:anonymous@www.sqlite.org:/sqlite checkout -P -D "$NEW_DATE" -d sqlite-latest sqlite

echo "_____ Removing CVS directories..."
find sqlite-base -type d -name CVS -execdir rm -rf {} + -prune
find sqlite-latest -type d -name CVS -execdir rm -rf {} + -prune

echo "_____ Running merge tool..."
$MERGE sqlite-base sqlite-latest sqlite

cd sqlite

echo "_____ Updating VERSION_DATE to be $NEW_DATE ..."
echo $NEW_DATE > VERSION_DATE

echo "_____ Processing generated files..."
./google_generate_preprocessed.sh
