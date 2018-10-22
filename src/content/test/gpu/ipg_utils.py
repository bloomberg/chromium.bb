# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script implements a few IntelPowerGadget related helper functions.

This script only works on Windows with Intel CPU. Intel Power Gadget needs to
be installed on the machine before this script works. The software can be
downloaded from:
  https://software.intel.com/en-us/articles/intel-power-gadget-20

An easy way to use the APIs are:
1) Launch your program.
2) Call RunIPG() with no args. It will automatically locate the IPG installed
   on the machine.
3) Call AnalyzeIPGLogFile() with no args. It will analyze the default IPG log
   file, which is PowerLog.csv at current dir; then it will print out the power
   usage summary. If you want to skip a few seconds of the power log data, say,
   5 seconds, call AnalyzeIPGLogFile(skip_in_sec=5).
"""

import logging
import os
import subprocess
import datetime

def LocateIPG():
  ipg_dir = os.getenv('IPG_Dir')
  if not ipg_dir:
    logging.warning("No env IPG_Dir")
    return None
  gadget_path = os.path.join(ipg_dir, "PowerLog3.0.exe")
  logging.debug("Try to locale Intel Power Gadget at " + gadget_path)
  if os.path.isfile(gadget_path):
    logging.debug("Intel Power Gadget Found")
    return gadget_path
  return None

def GenerateIPGLogFilename(prefix='PowerLog', dir=None, current_run=1,
                           total_runs=1, timestamp=False):
  # If all args take default value, it is the IPG's default log path.
  dir = dir or os.getcwd()
  dir = os.path.abspath(dir)
  if total_runs > 1:
    prefix = "%s_%d_%d" % (prefix, current_run, total_runs)
  if timestamp:
    now = datetime.datetime.now()
    prefix = "%s_%s" % (prefix, now.strftime('%Y%m%d%H%M%S'))
  return os.path.join(dir, prefix + '.csv')

def RunIPG(duration_in_s=60, resolution_in_ms=100, logfile=None):
  intel_power_gadget_path = LocateIPG()
  if not intel_power_gadget_path:
    logging.warning("Can't locate Intel Power Gadget")
    return
  command = ('"%s" -duration %d -resolution %d' %
             (intel_power_gadget_path, duration_in_s, resolution_in_ms))
  if not logfile:
    # It is not necessary but allows to print out the log path for debugging.
    logfile = GenerateIPGLogFilename();
  command = command + (' -file %s' %logfile)
  logging.debug("Running: " + command)
  try:
    output = subprocess.check_output(command)
    logging.debug("Running: DONE")
    logging.debug(output)
  except subprocess.CalledProcessError as err:
    logging.warning(err)

def AnalyzeIPGLogFile(logfile=None, skip_in_sec=0):
  if not logfile:
    logfile = GenerateIPGLogFilename()
  if not os.path.isfile(logfile):
    logging.warning("Can't locate logfile at " + logfile)
    return {}
  first_line = True
  samples = 0
  cols = 0
  indices = []
  labels = []
  sums = []
  col_time = None
  for line in open(logfile):
    tokens = line.split(',')
    if first_line:
      first_line = False
      cols = len(tokens)
      for ii in range(0, cols):
        if tokens[ii].startswith('Elapsed Time'):
          col_time = ii;
        elif tokens[ii].endswith('(Watt)'):
          indices.append(ii)
          labels.append(tokens[ii][:-len('(Watt)')])
          sums.append(0.0)
      assert col_time
      assert cols > 0
      assert len(indices) > 0
      continue
    if len(tokens) != cols:
      continue
    if skip_in_sec > 0 and float(tokens[col_time]) < skip_in_sec:
      continue
    samples += 1
    for ii in range(0, len(indices)):
      index = indices[ii]
      sums[ii] += float(tokens[index])
  results = {'samples': samples}
  if samples > 0:
    for ii in range(0, len(indices)):
      results[labels[ii]] = sums[ii] / samples
  return results
