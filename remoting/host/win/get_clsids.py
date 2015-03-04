# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Each CLSID is a hash of the current version string salted with an
# arbitrary GUID. This ensures that the newly installed COM classes will
# be used during/after upgrade even if there are old instances running
# already.
# The IDs are not random to avoid rebuilding host when it's not
# necessary.

import uuid
import sys

if len(sys.argv) != 4:
  print """Expecting 3 args:
<daemon_controller_guid> <rdp_desktop_session_guid> <version>"""
  sys.exit(1)

daemon_controller_guid = sys.argv[1]
rdp_desktop_session_guid = sys.argv[2]
version_full = sys.argv[3]

# Output a GN list of 2 strings.
print '["' + \
    str(uuid.uuid5(uuid.UUID(daemon_controller_guid), version_full)) + '", '
print '"' + \
    str(uuid.uuid5(uuid.UUID(rdp_desktop_session_guid), version_full)) + '"]'

