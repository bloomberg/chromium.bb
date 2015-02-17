#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import sys
import os

# Import the metrics/common module for pretty print xml.
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import models
import presubmit_util


# Model definitions for rappor.xml content
_SUMMARY_TYPE = models.TextNodeType('summary')

_PARAMETERS_TYPE = models.ObjectNodeType('parameters',
    int_attributes=[
      'num-cohorts',
      'bytes',
      'hash-functions',
    ],
    float_attributes=[
      'fake-prob',
      'fake-one-prob',
      'one-coin-prob',
      'zero-coin-prob',
    ],
    string_attributes=[
      'reporting-level'
    ])

_RAPPOR_PARAMETERS_TYPE = models.ObjectNodeType('rappor-parameters',
    extra_newlines=(1, 1, 1),
    string_attributes=['name'],
    children=[
      models.ChildType('summary', _SUMMARY_TYPE, False),
      models.ChildType('parameters', _PARAMETERS_TYPE, False),
    ])

_RAPPOR_PARAMETERS_TYPES_TYPE = models.ObjectNodeType('rappor-parameter-types',
    extra_newlines=(1, 1, 1),
    dont_indent=True,
    children=[
      models.ChildType('types', _RAPPOR_PARAMETERS_TYPE, True),
    ])

_OWNER_TYPE = models.TextNodeType('owner', single_line=True)

_RAPPOR_METRIC_TYPE = models.ObjectNodeType('rappor-metric',
    extra_newlines=(1, 1, 1),
    string_attributes=['name', 'type'],
    children=[
      models.ChildType('owners', _OWNER_TYPE, True),
      models.ChildType('summary', _SUMMARY_TYPE, False),
    ])

_RAPPOR_METRICS_TYPE = models.ObjectNodeType('rappor-metrics',
    extra_newlines=(1, 1, 1),
    dont_indent=True,
    children=[
      models.ChildType('metrics', _RAPPOR_METRIC_TYPE, True),
    ])

_RAPPOR_CONFIGURATION_TYPE = models.ObjectNodeType('rappor-configuration',
    extra_newlines=(1, 1, 1),
    dont_indent=True,
    children=[
      models.ChildType('parameterTypes', _RAPPOR_PARAMETERS_TYPES_TYPE, False),
      models.ChildType('metrics', _RAPPOR_METRICS_TYPE, False),
    ])

RAPPOR_XML_TYPE = models.DocumentType(_RAPPOR_CONFIGURATION_TYPE)


def GetTypeNames(config):
  return set(p['name'] for p in config['parameterTypes']['types'])


def HasMissingOwners(metrics):
  """Check that all of the metrics have owners.

  Args:
    metrics: A list of rappor metric description objects.

  Returns:
    True iff some metrics are missing owners.
  """
  missing_owners = [m for m in metrics if not m['owners']]
  for metric in missing_owners:
    logging.error('Rappor metric "%s" is missing an owner.', metric['name'])
    print metric
  return bool(missing_owners)


def HasInvalidTypes(type_names, metrics):
  """Check that all of the metrics have valid types.

  Args:
    type_names: The set of valid type names.
    metrics: A list of rappor metric description objects.

  Returns:
    True iff some metrics have invalid types.
  """
  invalid_types = [m for m in metrics if m['type'] not in type_names]
  for metric in invalid_types:
    logging.error('Rappor metric "%s" has invalid type "%s"',
        metric['name'], metric['type'])
  return bool(invalid_types)


def HasErrors(config):
  """Check that rappor.xml passes some basic validation checks.

  Args:
    config: The parsed rappor.xml contents.

  Returns:
    True iff there are validation errors.
  """
  metrics = config['metrics']['metrics']
  type_names = GetTypeNames(config)
  return (HasMissingOwners(metrics) or
          HasInvalidTypes(type_names, metrics))


def Cleanup(config):
  """Preform cleanup on description contents, such as sorting metrics.

  Args:
    config: The parsed rappor.xml contents.
  """
  types = config['parameterTypes']['types']
  types.sort(key=lambda x: x['name'])
  metrics = config['metrics']['metrics']
  metrics.sort(key=lambda x: x['name'])


def UpdateXML(original_xml):
  """Parse the original xml and return a pretty printed version.

  Args:
    original_xml: A string containing the original xml file contents.

  Returns:
    A Pretty printed xml string.
  """
  comments, config = RAPPOR_XML_TYPE.Parse(original_xml)

  if HasErrors(config):
    return None

  Cleanup(config)

  return RAPPOR_XML_TYPE.PrettyPrint(comments, config)


def main(argv):
  presubmit_util.DoPresubmitMain(argv, 'rappor.xml', 'rappor.old.xml',
                                 'pretty_print.py', UpdateXML)


if '__main__' == __name__:
  sys.exit(main(sys.argv))
