#!/usr/bin/python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import urllib
from zipfile import ZipFile

def main():
  if not os.getcwd().endswith("hunspell_dictionaries"):
    print "Please run this file from the hunspell_dictionaries directory"
    return 1
  urllib.urlretrieve(
    "http://downloads.sourceforge.net/wordlist/hunspell-en_US-2015.05.18.zip",
    "en_US.zip")
  urllib.urlretrieve(
    "http://downloads.sourceforge.net/wordlist/hunspell-en_CA-2015.05.18.zip",
    "en_CA.zip")
  ZipFile("en_US.zip").extractall()
  ZipFile("en_CA.zip").extractall()
  os.remove("en_US.zip")
  os.remove("en_CA.zip")
  return 0

if __name__ == '__main__':
  sys.exit(main())
