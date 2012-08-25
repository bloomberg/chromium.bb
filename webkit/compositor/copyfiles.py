# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import shutil
import os
import re

prefixes = ["../../third_party/WebKit/Source/WebCore/platform/chromium/support",
            "../../third_party/WebKit/Source/WebKit/chromium/src",
            "../../third_party/WebKit/Source/WebCore/platform"]

def Copy(name):
  src = name
  dst = name
  fullsrc = ""
  for prefix in prefixes:
    candidate = "%s/%s" % (prefix, src)
    if os.path.exists(candidate):
      fullsrc = candidate
      break
  assert fullsrc != ""
  shutil.copyfile(fullsrc, dst)
  print "copying from %s to %s" % (fullsrc, dst)
  return dst

def Readfile(gypfile):
  cc_gyp = open(gypfile, "r")
  obj = eval(cc_gyp.read())
  cc_gyp.close()
  return obj

def FixCopyrightHeaderText(text, year):
  header_template = """// Copyright %s The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
"""

  while text[0].find(" */") == -1:
    text = text[1:]
  text = text[1:]

  return (header_template % year) + "".join(text)

def FixCopyrightHeader(filepath):
  with open(filepath, "r") as f:
    text = f.readlines()

  pattern = ".*Copyright \(C\) (20[01][0-9])"
  m = re.match(pattern, text[0])
  if m == None:
    m = re.match(pattern, text[1])
  assert m
  year = m.group(1)

  fixed_text = FixCopyrightHeaderText(text, year)
  with open(filepath, "w") as f:
    f.write(fixed_text)

def Main():
  files = Readfile("compositor.gyp")['variables']['webkit_compositor_sources']
  for f in files:
    dst =Copy(f)
    FixCopyrightHeader(dst)

if __name__ == '__main__':
  import sys
  os.chdir(os.path.dirname(__file__))
  sys.exit(Main())
