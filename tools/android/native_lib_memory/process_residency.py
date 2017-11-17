#!/usr/bin/python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""From a native code residency dump created by log_residency.cc, generates a
visual timeline.
"""

import argparse
import logging
from matplotlib import collections as mc
from matplotlib import pylab as plt
import numpy as np


def CreateArgumentParser():
  """Creates and returns an argument parser."""
  parser = argparse.ArgumentParser(
      description='Reads and shows native library residency data.')
  parser.add_argument('--dump', type=str, required=True, help='Residency dump')
  parser.add_argument('--output', type=str, required=True,
                      help='Output filename')
  return parser


def ParseDump(filename):
  """Parses a residency dump, as generated from lightweight_cygprofile.cc.

  Args:
    filename: (str) dump filename.

  Returns:
    {timestamp (int): data ([bool])}
  """
  result = {}
  with open(filename, 'r') as f:
    for line in f:
      line = line.strip()
      timestamp, data = line.split(' ')
      data_array = [x == '1' for x in data]
      result[int(timestamp)] = data_array
  return result


def PlotResidency(data, output_filename):
  """Creates a graph of residency.

  Args:
    data: (dict) As returned by ParseDump().
    output_filename: (str) Output filename.
  """
  fig, ax = plt.subplots(figsize=(20, 10))
  timestamps = sorted(data.keys())
  x_max = len(data.values()[0]) * 4096
  for t in timestamps:
    offset_ms = (t - timestamps[0]) / 1e6
    incore = [i * 4096 for (i, x) in enumerate(data[t]) if x]
    outcore = [i * 4096 for (i, x) in enumerate(data[t]) if not x]
    percentage = 100. * len(incore) / (len(incore) + len(outcore))
    plt.text(x_max, offset_ms, '%.1f%%' % percentage)
    for (d, color) in ((incore, (.2, .6, .05, 1)), (outcore, (1, 0, 0, 1))):
      segments = [[(x, offset_ms), (x + 4096, offset_ms)] for x in d]
      colors = np.array([color] * len(segments))
      lc = mc.LineCollection(segments, colors=colors, linewidths=8)
      ax.add_collection(lc)

  plt.title('Code residency vs time since startup.')
  plt.xlabel('Code page offset (bytes)')
  plt.ylabel('Time since startup (ms)')
  plt.ylim(-.5e3, ymax=(timestamps[-1] - timestamps[0]) / 1e6 + .5e3)
  plt.xlim(xmin=0, xmax=x_max)
  plt.savefig(output_filename, bbox_inches='tight', dpi=300)


def main():
  parser = CreateArgumentParser()
  args = parser.parse_args()
  logging.basicConfig(level=logging.INFO)
  logging.info('Parsing the data')
  data = ParseDump(args.dump)
  logging.info('Plotting the results')
  PlotResidency(data, args.output)


if __name__ == '__main__':
  main()
