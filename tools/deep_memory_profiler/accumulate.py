#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A script to accumulate values from the 'dmprof cat' command into CSV or else.
#
# Usage:
#   ./accumulate.py -f <format> -t <template-name> < input.json > output
#
# <format> is one of "csv", "json", and "tree". If "csv" or "json" is given,
# accumulate.py dumps a similar file to "dmprof csv|json". If "tree" is given,
# accumulate.py dumps a human-readable breakdown tree.
#
# <template-name> is a label in templates.json.

import datetime
import json
import logging
import optparse
import sys

from lib.ordered_dict import OrderedDict


LOGGER = logging.getLogger('dmprof-accumulate')


def visit_in_template(template, snapshot, depth):
  """Visits all categories via a given template.

  This function is not used. It's a sample function to traverse a template.
  """
  world = template[0]
  breakdown = template[1]
  rules = template[2]

  for rule, _ in snapshot[world]['breakdown'][breakdown].iteritems():
    print ('  ' * depth) + rule
    if rule in rules:
      visit_in_template(rules[rule], snapshot, depth + 1)


def accumulate(template, snapshot, units_dict, target_units):
  """Accumulates units in a JSON |snapshot| with applying a given |template|.

  Args:
      template: A template tree included in a dmprof cat JSON file.
      snapshot: A snapshot in a dmprof cat JSON file.
      units_dict: A dict of units in worlds.
      target_units: A list of unit ids which are a target of this accumulation.
  """
  world = template[0]
  breakdown = template[1]
  rules = template[2]

  remainder_units = target_units.copy()
  category_tree = OrderedDict()
  total = 0

  for rule, match in snapshot[world]['breakdown'][breakdown].iteritems():
    if 'hidden' in match and match['hidden']:
      continue
    matched_units = set(match['units']).intersection(target_units)
    subtotal = 0
    for unit_id in matched_units:
      subtotal += units_dict[world][unit_id]
    total += subtotal
    remainder_units = remainder_units.difference(matched_units)
    if rule not in rules:
      # A category matched with |rule| is a leaf of the breakdown tree.
      # It is NOT broken down more.
      category_tree[rule] = subtotal
      continue

    # A category matched with |rule| is broken down more.
    subtemplate = rules[rule]
    subworld = subtemplate[0]
    subbreakdown = subtemplate[1]

    if subworld == world:
      # Break down in the same world: consider units.
      category_tree[rule], accounted_total, subremainder_units = accumulate(
          subtemplate, snapshot, units_dict, matched_units)
      subremainder_total = 0
      if subremainder_units:
        for unit_id in subremainder_units:
          subremainder_total += units_dict[world][unit_id]
        category_tree[rule][None] = subremainder_total
      if subtotal != accounted_total + subremainder_total:
        print >> sys.stderr, (
            'WARNING: Sum of %s:%s is different from %s by %d bytes.' % (
                subworld, subbreakdown, rule,
                subtotal - (accounted_total + subremainder_total)))
    else:
      # Break down in a different world: consider only the total size.
      category_tree[rule], accounted_total, _ = accumulate(
          subtemplate, snapshot, units_dict, set(units_dict[subworld].keys()))
      if subtotal >= accounted_total:
        category_tree[rule][None] = subtotal - accounted_total
      else:
        print >> sys.stderr, (
            'WARNING: Sum of %s:%s is larger than %s by %d bytes.' % (
                subworld, subbreakdown, rule, accounted_total - subtotal))
        print >> sys.stderr, (
            'WARNING:   Assuming remainder of %s is 0.' % rule)
        category_tree[rule][None] = 0

  return category_tree, total, remainder_units


def flatten(category_tree, header=''):
  """Flattens a category tree into a flat list."""
  result = []
  for rule, sub in category_tree.iteritems():
    if not rule:
      rule = 'remaining'
    if header:
      flattened_rule = header + '>' + rule
    else:
      flattened_rule = rule
    if isinstance(sub, dict) or isinstance(sub, OrderedDict):
      result.extend(flatten(sub, flattened_rule))
    else:
      result.append((flattened_rule, sub))
  return result


def print_category_tree(category_tree, output, depth=0):
  """Prints a category tree in a human-readable format."""
  for label in category_tree:
    print >> output, ('  ' * depth),
    if (isinstance(category_tree[label], dict) or
        isinstance(category_tree[label], OrderedDict)):
      print >> output, '%s:' % label
      print_category_tree(category_tree[label], output, depth + 1)
    else:
      print >> output, '%s: %d' % (label, category_tree[label])


def flatten_all_category_trees(category_trees):
  flattened_labels = set()
  flattened_table = []
  for category_tree in category_trees:
    flattened = OrderedDict()
    for label, subtotal in flatten(category_tree):
      flattened_labels.add(label)
      flattened[label] = subtotal
    flattened_table.append(flattened)
  return flattened_labels, flattened_table


def output_csv(output, category_trees, data, first_time, output_exponent):
  flattened_labels, flattened_table = flatten_all_category_trees(category_trees)

  sorted_flattened_labels = sorted(flattened_labels)
  print >> output, ','.join(['second'] + sorted_flattened_labels)
  for index, row in enumerate(flattened_table):
    values = [str(data['snapshots'][index]['time'] - first_time)]
    for label in sorted_flattened_labels:
      if label in row:
        divisor = 1
        if output_exponent.upper() == 'K':
          divisor = 1024.0
        elif output_exponent.upper() == 'M':
          divisor = 1024.0 * 1024.0
        values.append(str(row[label] / divisor))
      else:
        values.append('0')
    print >> output, ','.join(values)


def output_json(output, category_trees, data, first_time, template_label):
  flattened_labels, flattened_table = flatten_all_category_trees(category_trees)

  json_snapshots = []
  for index, row in enumerate(flattened_table):
    row_with_meta = row.copy()
    row_with_meta['second'] = data['snapshots'][index]['time'] - first_time
    row_with_meta['dump_time'] = datetime.datetime.fromtimestamp(
        data['snapshots'][index]['time']).strftime('%Y-%m-%d %H:%M:%S')
    json_snapshots.append(row_with_meta)
  json_root = {
      'version': 'JSON_DEEP_2',
      'policies': {
          template_label: {
              'legends': sorted(flattened_labels),
              'snapshots': json_snapshots
              }
          }
      }
  json.dump(json_root, output, indent=2, sort_keys=True)


def output_tree(output, category_trees):
  for index, category_tree in enumerate(category_trees):
    print >> output, '< Snapshot #%d >' % index
    print_category_tree(category_tree, output, 1)
    print >> output, ''


def do_main(cat_input, output, template_label, output_format, output_exponent):
  """Does the main work: accumulate for every snapshot and print a result."""
  if output_format not in ['csv', 'json', 'tree']:
    raise NotImplementedError('The output format \"%s\" is not implemented.' %
                              output_format)

  if output_exponent.upper() not in ['B', 'K', 'M']:
    raise NotImplementedError('The exponent \"%s\" is not implemented.' %
                              output_exponent)

  data = json.loads(cat_input.read(), object_pairs_hook=OrderedDict)

  templates = data['templates']
  if not template_label:
    template_label = data['default_template']
  if template_label not in templates:
    LOGGER.error('A template \'%s\' is not found.' % template_label)
    return
  template = templates[template_label]

  category_trees = []
  first_time = None

  for snapshot in data['snapshots']:
    if not first_time:
      first_time = snapshot['time']

    units = {}
    for world_name in snapshot['worlds']:
      world_units = {}
      for unit_id, sizes in snapshot['worlds'][world_name]['units'].iteritems():
        world_units[int(unit_id)] = sizes[0]
      units[world_name] = world_units

    category_tree, _, _ = accumulate(
        template, snapshot['worlds'], units, set(units[template[0]].keys()))
    category_trees.append(category_tree)

  if output_format == 'csv':
    output_csv(output, category_trees, data, first_time, output_exponent)
  elif output_format == 'json':
    output_json(output, category_trees, data, first_time, template_label)
  elif output_format == 'tree':
    output_tree(output, category_trees)


def main():
  LOGGER.setLevel(logging.DEBUG)
  handler = logging.StreamHandler()
  handler.setLevel(logging.INFO)
  formatter = logging.Formatter('%(message)s')
  handler.setFormatter(formatter)
  LOGGER.addHandler(handler)

  parser = optparse.OptionParser()
  parser.add_option('-t', '--template', dest='template',
                    metavar='TEMPLATE',
                    help='Apply TEMPLATE to list up.')
  parser.add_option('-f', '--format', dest='format', default='csv',
                    help='Specify the output format: csv, json or tree.')
  parser.add_option('-e', '--exponent', dest='exponent', default='M',
                    help='Specify B (bytes), K (kilobytes) or M (megabytes).')

  options, _ = parser.parse_args(sys.argv)
  do_main(sys.stdin, sys.stdout,
          options.template, options.format, options.exponent)


if __name__ == '__main__':
  sys.exit(main())
