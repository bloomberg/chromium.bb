# Copyright (c) 2014 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Conditions for scons to gn

Contains all conditions we iterate over (OS, CPU), as well as helper
convertion functions.
"""

FULLARCH = {
  'arm' : 'arm',
  'x86' : 'x86-32',
  'x64' : 'x86-64'
}

class Conditions(object):
  def __init__(self, seta, setb):
    self._set_a = seta
    self._set_b = setb
    self._all = ['%s_%s' % (a, b) for a in seta for b in setb]
    self._active_condition = self._all[0]

  def get(self, key, condition, default=False):
    os, arch = condition.split('_')
    if key == "TARGET_FULLARCH":
      return FULLARCH[arch]

  def All(self):
    return self._all

  def SetA(self):
    return self._set_a

  def SetB(self):
    return self._set_b

  def ActiveCondition(self):
    return self._active_condition

  def SetActiveCondition(self, cond):
    if cond not in self._all:
      raise RuntimeError('Unknown condition: ' + cond)
    self._active_condition = cond


class TrustedConditions(Conditions):
  def __init__(self):
    OSES = ['AND', 'CHR', 'IOS',  'LIN', 'MAC', 'WIN']
    ARCH = ['arm', 'x86', 'x64']
    Conditions.__init__(self, OSES, ARCH)

  def Bit(self, name):
    os, arch = self._active_condition.split('_')
    osname = name[:3].upper()

    if osname in self.SetA():
      return osname == os

    if name == 'target_arm':
      return arch == 'arm'

    if name == 'target_x86':
      return arch == 'x86' or arch == 'x64'

    if name == 'target_x86_32':
      return arch == 'x86'

    if name == 'target_x86_64':
      return arch == 'x64'

    print 'Unknown bit: ' + name
    return False


class UntrustedConditions(Conditions):
  def __init__(self):
    LIBS = ['newlib', 'glibc', 'bionic']
    ARCH = ['arm', 'x86', 'x64', 'pnacl']
    Conditions.__init__(self, LIBS, ARCH)

  def get(self, key, condition, default=False):
    os, arch = self._active_condition.split('_')
    if key == "TARGET_FULLARCH":
      return FULLARCH[arch]

  def Bit(self, name):
    libc, arch = self._active_condition.split('_')

    if name == 'bitcode':
      return arch == 'arm'

    print 'Unknown bit: ' + name
    return False


BOGUS = """
ALL = ['%s_%s' % (os, cpu) for os in OSES for cpu in CPUS]

CPU_TO_BIT_MAP = {}
BIT_TO_CPU_MAP = {}

for idx, cpu in enumerate(CPUS):
  CPU_TO_BIT_MAP[cpu] = 1 << idx
  BIT_TO_CPU_MAP[1 << idx] = cpu

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
"""
