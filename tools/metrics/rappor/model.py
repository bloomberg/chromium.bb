# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# """Model objects for rappor.xml contents."""

import logging
import operator
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import models

# Model definitions for rappor.xml content
_SUMMARY_TYPE = models.TextNodeType('summary')

_NOISE_VALUES_TYPE = models.ObjectNodeType(
    'noise-values',
    attributes=[
        ('fake-prob', float),
        ('fake-one-prob', float),
        ('one-coin-prob', float),
        ('zero-coin-prob', float),
    ])

_NOISE_LEVEL_TYPE = models.ObjectNodeType(
    'noise-level',
    extra_newlines=(1, 1, 1),
    attributes=[('name', unicode)],
    children=[
        models.ChildType('summary', _SUMMARY_TYPE, False),
        models.ChildType('values', _NOISE_VALUES_TYPE, False),
    ])

_NOISE_LEVELS_TYPE = models.ObjectNodeType(
    'noise-levels',
    extra_newlines=(1, 1, 1),
    indent=False,
    children=[
        models.ChildType('levels', _NOISE_LEVEL_TYPE, True),
    ])

_PARAMETERS_TYPE = models.ObjectNodeType(
    'parameters',
    attributes=[
        ('num-cohorts', int),
        ('bytes', int),
        ('hash-functions', int),
        ('reporting-level', unicode),
        ('noise-level', unicode),
    ])

_RAPPOR_PARAMETERS_TYPE = models.ObjectNodeType(
    'rappor-parameters',
    extra_newlines=(1, 1, 1),
    attributes=[('name', unicode)],
    children=[
        models.ChildType('summary', _SUMMARY_TYPE, False),
        models.ChildType('parameters', _PARAMETERS_TYPE, False),
    ])

_RAPPOR_PARAMETERS_TYPES_TYPE = models.ObjectNodeType(
    'rappor-parameter-types',
    extra_newlines=(1, 1, 1),
    indent=False,
    children=[
        models.ChildType('types', _RAPPOR_PARAMETERS_TYPE, True),
    ])

_OWNER_TYPE = models.TextNodeType('owner', single_line=True)

_STRING_FIELD_TYPE = models.ObjectNodeType(
    'string-field',
    extra_newlines=(1, 1, 0),
    attributes=[('name', unicode)],
    children=[
        models.ChildType('summary', _SUMMARY_TYPE, False),
    ])

_FLAG_TYPE = models.TextNodeType('flag', single_line=True)

_FLAGS_FIELD_TYPE = models.ObjectNodeType(
    'flags-field',
    extra_newlines=(1, 1, 0),
    attributes=[('name', unicode), ('noise-level', unicode)],
    children=[
        models.ChildType('flags', _FLAG_TYPE, True),
        models.ChildType('summary', _SUMMARY_TYPE, False),
    ])

_UINT64_FIELD_TYPE = models.ObjectNodeType(
    'uint64-field',
    extra_newlines=(1, 1, 0),
    attributes=[('name', unicode), ('noise-level', unicode)],
    children=[
        models.ChildType('summary', _SUMMARY_TYPE, False),
    ])

_RAPPOR_METRIC_TYPE = models.ObjectNodeType(
    'rappor-metric',
    extra_newlines=(1, 1, 1),
    attributes=[('name', unicode), ('type', unicode)],
    children=[
        models.ChildType('owners', _OWNER_TYPE, True),
        models.ChildType('summary', _SUMMARY_TYPE, False),
        models.ChildType('strings', _STRING_FIELD_TYPE, True),
        models.ChildType('flags', _FLAGS_FIELD_TYPE, True),
        models.ChildType('uint64', _UINT64_FIELD_TYPE, True),
    ])

_RAPPOR_METRICS_TYPE = models.ObjectNodeType(
    'rappor-metrics',
    extra_newlines=(1, 1, 1),
    indent=False,
    children=[
        models.ChildType('metrics', _RAPPOR_METRIC_TYPE, True),
    ])

_RAPPOR_CONFIGURATION_TYPE = models.ObjectNodeType(
    'rappor-configuration',
    extra_newlines=(1, 1, 1),
    indent=False,
    children=[
        models.ChildType('noiseLevels', _NOISE_LEVELS_TYPE, False),
        models.ChildType(
            'parameterTypes', _RAPPOR_PARAMETERS_TYPES_TYPE, False),
        models.ChildType('metrics', _RAPPOR_METRICS_TYPE, False),
    ])

RAPPOR_XML_TYPE = models.DocumentType(_RAPPOR_CONFIGURATION_TYPE)


METRIC_DIMENSION_TYPES = [
    'strings',
    'flags',
    'uint64',
]


def GetTypeNames(config):
  return set(p['name'] for p in config['parameterTypes']['types'])


def IsSimpleStringMetric(metric):
  """Checks if the given metric is a simple string metric.

  Args:
    metric: A metric object, as extracted from _RAPPOR_METRIC_TYPE
  Returns:
    True iff the metric is a simple string metric.
  """
  return all(not metric[dim_type] for dim_type in METRIC_DIMENSION_TYPES)


def HasMissingOwners(metrics):
  """Checks that all of the metrics have owners.

  Args:
    metrics: A list of rappor metric description objects.

  Returns:
    True iff some metrics are missing owners.
  """
  missing_owners = [metric for metric in metrics if not metric['owners']]
  for metric in missing_owners:
    logging.error('Rappor metric "%s" is missing an owner.', metric['name'])
  return bool(missing_owners)


def HasInvalidTypes(type_names, metrics):
  """Checks that all of the metrics have valid types.

  Args:
    type_names: The set of valid type names.
    metrics: A list of rappor metric description objects.

  Returns:
    True iff some metrics have invalid types.
  """
  invalid_types = [metric for metric in metrics
                   if metric['type'] not in type_names]
  for metric in invalid_types:
    logging.error('Rappor metric "%s" has invalid type "%s"',
                  metric['name'], metric['type'])
  return bool(invalid_types)


def HasErrors(config):
  """Checks that rappor.xml passes some basic validation checks.

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
  """Performs cleanup on description contents, such as sorting metrics.

  Args:
    config: The parsed rappor.xml contents.
  """
  config['parameterTypes']['types'].sort(key=operator.itemgetter('name'))
  config['metrics']['metrics'].sort(key=operator.itemgetter('name'))


def UpdateXML(original_xml):
  """Parses the original xml and return a pretty printed version.

  Args:
    original_xml: A string containing the original xml file contents.

  Returns:
    A pretty-printed xml string, or None if the config contains errors.
  """
  comments, config = RAPPOR_XML_TYPE.Parse(original_xml)

  if HasErrors(config):
    return None

  Cleanup(config)

  return RAPPOR_XML_TYPE.PrettyPrint(comments, config)
