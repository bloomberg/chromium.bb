#! /usr/bin/env python
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from editor import LogEditor
from log import Log
from optparse import OptionParser
import sys


usage = """Touchpad Tool Usage Examples:

Viewing logs:
$ %prog filename.log (from file)
$ %prog 172.22.75.0 (from device ip address)
$ %prog http://feedback.google.com/... (from feedback report url)

Edit log and save result into output.log:
$ %prog log -o output.log

Download log and save without editing:
$ %prog log -d -o output.log"""


def main(argv):
  parser = OptionParser(usage=usage)
  parser.add_option("-o",
                    dest="out", default=None,
                    help="set target filename for storing results",
                    metavar="output.log")
  parser.add_option("-d", "--download",
                    action="store_true", dest="download", default=False,
                    help="download file only, don't edit.")
  parser.add_option("-n", "--new",
                    dest="new", action="store_true", default=False,
                    help="Create new device logs before downloading. "+
                         "[Default: False]")
  parser.add_option("-p", "--persistent",
                    dest="persistent", action="store_true", default=False,
                    help="Keep server alive until killed in the terminal "+
                         "via CTRL-C [Default: False]")
  parser.add_option("-s", "--serve",
                    dest="serve", action="store_true", default=False,
                    help="Serve a standalone TPView.")
  (options, args) = parser.parse_args()

  editor = LogEditor(persistent=options.persistent)

  if options.serve:
    editor.Serve()
    return

  if len(args) != 1:
    parser.print_help()
    exit(-1)

  log = Log(args[0], options)

  if options.download:
    if not options.out:
      print "--download requires -o to be set."
      exit(-1)
    log.SaveAs(options.out)
  else:
    if options.out is None:
      editor.View(log)
    else:
      log = editor.Edit(log)
      log.SaveAs(options.out)

if __name__ == "__main__":
  main(sys.argv)
