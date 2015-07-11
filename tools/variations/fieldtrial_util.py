# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import sys


def _hex(ch):
  hv = hex(ord(ch)).replace('0x', '')
  hv.zfill(2)
  return hv.upper()

# URL escapes the delimiter characters from the output. urllib.quote is not
# used because it cannot escape '.'.
def _escape(str):
  result = str
  # Must perform replace on '%' first before the others.
  for c in '%:/.,':
    result = result.replace(c, '%' + _hex(c))
  return result

# Generate a list of command-line switches to enable field trials defined in
# fieldtrial_testing_config_*.json.
def GenerateArgs(config_path):
  try:
    with open(config_path, 'r') as base_file:
      variations = json.load(base_file)
  except (IOError, ValueError):
    return []

  field_trials = []
  params = []
  for trial, groups in variations.iteritems():
    if not len(groups):
      continue
    # For now, only take the first group.
    group = groups[0]
    trial_group = [trial, group['group_name']]
    field_trials.extend(trial_group)
    param_list = []
    if 'params' in group:
      for key, value in group['params'].iteritems():
        param_list.append(key)
        param_list.append(value)
    if len(param_list):
       # Escape the variables for the command-line.
       trial_group = [_escape(x) for x in trial_group]
       param_list = [_escape(x) for x in param_list]
       param = '%s:%s' % ('.'.join(trial_group), '/'.join(param_list))
       params.append(param)
  if not len(field_trials):
    return []
  args = ['--force-fieldtrials=%s' % '/'.join(field_trials)]
  if len(params):
    args.append('--force-fieldtrial-params=%s' % ','.join(params))
  return args

def main():
  if len(sys.argv) < 3:
    print 'Usage: fieldtrial_util.py [base_config_path] [platform_config_path]'
    exit(-1)
  print GenerateArgs(sys.argv[1], sys.argv[2])

if __name__ == '__main__':
  main()
