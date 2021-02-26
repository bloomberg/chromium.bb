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
      ("https://sourceforge.net/projects/wordlist/files/speller/2019.10.06/"
       "hunspell-en_US-2019.10.06.zip",
       "en_US.zip"),
      ("https://sourceforge.net/projects/wordlist/files/speller/2019.10.06/"
       "hunspell-en_CA-2019.10.06.zip",
       "en_CA.zip"),
      ("https://sourceforge.net/projects/wordlist/files/speller/2019.10.06/"
       "hunspell-en_GB-ise-2019.10.06.zip",
       "en_GB.zip"),
      ("https://sourceforge.net/projects/wordlist/files/speller/2019.10.06/"
       "hunspell-en_GB-ize-2019.10.06.zip",
       "en_GB_oxendict.zip"),
      ("https://sourceforge.net/projects/wordlist/files/speller/2019.10.06/"
       "hunspell-en_AU-2019.10.06.zip",
       "en_AU.zip"),
      ("https://github.com/b00f/lilak/releases/latest/download/"
       "fa-IR.zip",
       "fa_IR.zip"),
      # NOTE: need to remove IGNORE from uk_UA.aff
      ("https://github.com/brown-uk/dict_uk/releases/latest/download/"
       "hunspell-uk_UA.zip",
       "uk_UA.zip"),
  )
  for pair in dictionaries:
    url = pair[0]
    file_name = pair[1]

    urllib.urlretrieve(url, file_name)
    ZipFile(file_name).extractall()
    for name in glob.glob("*en_GB-ise*"):
      os.rename(name, name.replace("-ise", ""))
    for name in glob.glob("*en_GB-ize*"):
      os.rename(name, name.replace("-ize", "_oxendict"))
    for name in glob.glob("*fa-IR.*"):
      os.rename(name, name.replace("-", "_"))
    os.remove(file_name)
  return 0

if __name__ == "__main__":
  sys.exit(main())
