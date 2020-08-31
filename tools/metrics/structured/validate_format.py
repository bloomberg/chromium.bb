#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Verifies that the structured.xml file is well-formatted."""

import os
import re
import sys
from xml.dom import minidom

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import path_util

STRUCTURED_XML = path_util.GetInputFile(('tools/metrics/structured/'
                                         'structured.xml'))


def checkElementOwners(config, element_tag):
  """Check that every element in the config has at least one owner."""
  errors = []

  for node in config.getElementsByTagName(element_tag):
    name = node.getAttribute('name')
    owner_nodes = node.getElementsByTagName('owner')

    # Check <owner> tag is present for each element.
    if not owner_nodes:
      errors.append(
          "<owner> tag is required for %s '%s'." % (element_tag, name))
      continue

    for owner_node in owner_nodes:
      # Check <owner> tag actually has some content.
      if not owner_node.childNodes:
        errors.append("<owner> tag for '%s' should not be empty." % name)
      for email in owner_node.childNodes:
        # Check <owner> tag's content is an email address, not a username.
        if not re.match('^.+@(chromium\.org|google\.com)$', email.data):
          errors.append("<owner> tag for %s '%s' expects a Chromium or "
                        "Google email address, instead found '%s'." %
                        (element_tag, name, email.data.strip()))

  return errors


def checkElementsNotDuplicated(config, element_tag):
  errors = []
  elements = set()

  for node in config.getElementsByTagName(element_tag):
    name = node.getAttribute('name')
    # Check for duplicate names.
    if name in elements:
      errors.append("duplicate %s name '%s'" % (element_tag, name))
    elements.add(name)

  return errors


def checkMetricNamesWithinEventNotDuplicated(events):
  errors = []

  for node in events.getElementsByTagName('event'):
    name = node.getAttribute('name')
    metrics = set()
    for metric_node in node.getElementsByTagName('metric'):
      metric_name = metric_node.getAttribute('name')
      if metric_name in metrics:
        errors.append(
            "duplicate metric name '%s' for event '%s'" % (metric_name, name))
      metrics.add(metric_name)

  return errors


def checkEventsReferenceValidProjects(events, projects):
  errors = []

  projects = {
      project.getAttribute('name')
      for project in projects.getElementsByTagName('project')
  }

  for node in events.getElementsByTagName('event'):
    name = node.getAttribute('name')
    project = node.getAttribute('project')

    # An event's project can either be empty (not specified), or must be a
    # project listed in the projects section.
    if project and project not in projects:
      errors.append(
          "event '%s' references nonexistent project '%s'" % (name, project))

  return errors


def main():
  with open(STRUCTURED_XML, 'r') as config_file:
    document = minidom.parse(config_file)
    [config] = document.getElementsByTagName('structured-metrics')
    [events] = config.getElementsByTagName('events')
    [projects] = config.getElementsByTagName('projects')

    errors = []
    errors.extend(checkElementOwners(events, 'event'))
    errors.extend(checkElementOwners(projects, 'project'))
    errors.extend(checkElementsNotDuplicated(events, 'event'))
    errors.extend(checkElementsNotDuplicated(projects, 'project'))
    errors.extend(checkMetricNamesWithinEventNotDuplicated(events))
    errors.extend(checkEventsReferenceValidProjects(events, projects))

    if errors:
      return 'ERRORS:' + ''.join('\n  ' + e for e in errors)

if __name__ == '__main__':
  sys.exit(main())
