# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class UkmXmlValidation(object):
  """Validations for the content of ukm.xml."""

  def __init__(self, ukm_config):
    """Attributes:

    config: A XML minidom Element representing the root node of the UKM config
        tree.
    """
    self.config = ukm_config

  def checkEventsHaveOwners(self):
    """Check that every event in the config has at least one owner."""
    errors = []

    for event_node in self.config.getElementsByTagName('event'):
      event_name = event_node.getAttribute('name')
      owner_nodes = event_node.getElementsByTagName('owner')

      # Check <owner> tag is present for each event.
      if not owner_nodes:
        errors.append("<owner> tag is required for event '%s'." % event_name)
        continue

      for owner_node in owner_nodes:
        # Check <owner> tag actually has some content.
        if not owner_node.childNodes:
          errors.append(
              "<owner> tag for event '%s' should not be empty." % event_name)
          continue

        for email in owner_node.childNodes:
          # Check <owner> tag's content is an email address, not a username.
          if not ('@chromium.org' in email.data or '@google.com' in email.data):
            errors.append("<owner> tag for event '%s' expects a Chromium or "
                          "Google email address." % event_name)

    isSuccess = not errors

    return (isSuccess, errors)
