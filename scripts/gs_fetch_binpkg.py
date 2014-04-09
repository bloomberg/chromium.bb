# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Download a binpkg from Google Storage.

This is needed for two reasons:
  1) In the case where a binpkg is left over in the packages dir,
     portage doesn't handle retries well and reports an error.
  2) gsutil retries when a download is interrupted, but it doesn't
     handle the case where we are unable to resume a transfer and the
     transfer needs to be restarted from scratch. Ensuring that the
     file is deleted between each retry helps handle that eventuality.
"""

import shutil

from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import gs
from chromite.lib import osutils
from chromite.lib import retry_util


def GetParser():
  """Creates the argparse parser."""
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('uri', help='Google Storage URI to download')
  parser.add_argument('filename', help='Location to store the file.')
  return parser

def Copy(ctx, uri, filename):
  """Run the copy using a temp file."""
  temp_path = '%s.tmp' % filename
  osutils.SafeUnlink(temp_path)
  try:
    ctx.Copy(uri, temp_path, parallel=False, log_output=True)
    shutil.move(temp_path, filename)
  finally:
    osutils.SafeUnlink(temp_path)

def main(argv):
  parser = GetParser()
  options = parser.parse_args(argv)
  ctx = gs.GSContext(retries=0)
  try:
    retry_util.RetryCommand(Copy, ctx.DEFAULT_RETRIES, ctx, options.uri,
                            options.filename, sleep=ctx.DEFAULT_SLEEP_TIME)
  except (gs.GSContextException, cros_build_lib.RunCommandError) as ex:
    # Hide the stack trace using Die.
    cros_build_lib.Die('%s', ex)
