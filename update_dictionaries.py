#!/usr/bin/python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A simple script for downloading latest dictionaries."""

import glob
import os
import sys
import urllib
from zipfile import ZipFile


def main():
  if not os.getcwd().endswith("hunspell_dictionaries"):
    print "Please run this file from the hunspell_dictionaries directory"
  dictionaries = (
      ("http://downloads.sourceforge.net/project/wordlist/speller/2016.01.19/"
       "hunspell-en_US-2016.01.19.zip",
       "en_US.zip"),
      ("http://downloads.sourceforge.net/project/wordlist/speller/2016.01.19/"
       "hunspell-en_CA-2016.01.19.zip",
       "en_CA.zip"),
      ("http://downloads.sourceforge.net/project/wordlist/speller/2016.01.19/"
       "hunspell-en_GB-ise-2016.01.19.zip",
       "en_GB.zip"),
      ("http://downloads.sourceforge.net/lilak/"
       "lilak_fa-IR_2-1.zip",
       "fa_IR.zip")
  )
  for pair in dictionaries:
    urllib.urlretrieve(pair[0], pair[1])
    ZipFile(pair[1]).extractall()
    for name in glob.glob("*en_GB-ise*"):
      os.rename(name, name.replace("-ise", ""))
    os.remove(pair[1])
  return 0

if __name__ == "__main__":
  sys.exit(main())
