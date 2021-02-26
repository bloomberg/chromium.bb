# Copyright (c) 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import owners


class OwnersClient(object):
  """Interact with OWNERS files in a repository.

  This class allows you to interact with OWNERS files in a repository both the
  Gerrit Code-Owners plugin REST API, and the owners database implemented by
  Depot Tools in owners.py:

   - List all the owners for a change.
   - Check if a change has been approved.
   - Check if the OWNERS configuration in a change is valid.

  All code should use this class to interact with OWNERS files instead of the
  owners database in owners.py
  """
  def __init__(self, host):
    self._host = host

  def ListOwnersForFile(self, project, branch, path):
    """List all owners for a file."""
    raise Exception('Not implemented')

  def IsChangeApproved(self, change_id):
    """Check if the latest patch set for a change has been approved."""
    raise Exception('Not implemented')

  def IsOwnerConfigurationValid(self, change_id, patch):
    """Check if the owners configuration in a change is valid."""
    raise Exception('Not implemented')


class DepotToolsClient(OwnersClient):
  """Implement OwnersClient using owners.py Database."""
  def __init__(self, host, root):
    super(DepotToolsClient, self).__init__(host)
    self._root = root
    self._db = owners.Database(root, open, os.path)

  def ListOwnersForFile(self, _project, _branch, path):
    return sorted(self._db.all_possible_owners([arg], None))
