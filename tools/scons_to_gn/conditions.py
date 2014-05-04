# Copyright (c) 2014 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Conditions for scons to gn

Contains all conditions we iterate over (OS, CPU), as well as helper
convertion functions.
"""


OSES = ['AND', 'CHR', 'IOS',  'LIN', 'MAC', 'WIN']
CPUS = ['arm', 'x86', 'x64']
ALL = ['%s_%s' % (os, cpu) for os in OSES for cpu in CPUS]

CPU_TO_BIT_MAP = {}
BIT_TO_CPU_MAP = {}

for idx, cpu in enumerate(CPUS):
  CPU_TO_BIT_MAP[cpu] = 1 << idx
  BIT_TO_CPU_MAP[1 << idx] = cpu

REMAP = {
  'arm' : 'arm',
  'AND' : 'android',
  'CHR' : 'chrome',
  'LIN' : 'linux',
  'MAC' : 'mac',
  'IOS' : 'ios',
  'WIN' : 'win',
  'x86' : 'x86',
  'x64' : 'x64',
}

def CPUsToBits(cpus):
  out = 0;
  for cpu in cpus:
    out += CPU_TO_BIT_MAP[cpu]
  return out


def BitsToCPUs(cpus):
  out = []
  for i in [1, 2, 4]:
    if cpus & i:
      out.append(BIT_TO_CPU_MAP[i])
  return out

