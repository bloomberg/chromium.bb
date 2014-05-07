# Copyright (c) 2014 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import defaultdict
import hashlib
import json
import pprint
import sys

from conditions import *
from properties import ConvertIfSingle, ParsePropertyTable
from nodes import *

"""The workhorse of scons to gn.

the Environment holds the state of a scons 'env' objects, while the object
tracker stores the matrix of conditions and values.

The object tracket is responsible for building the Node tree.
"""

def HashCPUData(cpus, hits):
  m = hashlib.md5()
  for hit in hits:
    if hit:
      m.update(str(hit))
  m.update(str(cpus))
  return m.hexdigest()


def CPUSubset(configs, os):
  cpus = [x[4:] for x in configs if x[:3] == os]
  return CPUsToBits(cpus)


#
# TransformToPerOS
#
# Transform dict<CONFIG_HASH = [NAMES] to a dictionary of
# [OS,CPU] = [NODES]
#
def TransformToPerOS(data):
  out = {}
  for os in OSES:
    out[os] = defaultdict(list)

  for name in data:
    config = data[name]['configs']
    for os in OSES:
      cpu_set = int(CPUSubset(config, os))
      if cpu_set:
        out[os][cpu_set].append(name)
  return out

#
# MergeHashes
#
def MergeToDataCPU(original, os_avail, cpu_avail):
  merge_os = defaultdict(list)
  merge_cpus = {}
  merge_data = {}

  # Build table of DATA[OS][CPU_SET] = List of NODES
  data = TransformToPerOS(original)

  # For every item in the data table, generate key based on CPUs and NODES
  # matching a configuration, and build tables indexed by that key.
  for os in data:
    for cpus,hits in data[os].iteritems():
      key = HashCPUData(cpus, hits)
      merge_os[key].append(os)
      merge_cpus[key] = cpus
      merge_data[key] = hits

  # Merge all subsets
  keys = merge_os.keys()
  for i, i_key in enumerate(keys):
    # For each key, get the PLATFORMS, CPUS, and NODES
    i_oses = merge_os[i_key]
    i_cpus = merge_cpus[i_key]
    i_hits = merge_data[i_key]

    for j, j_key in enumerate(keys):
      # For each other non-matching key, get PLATFORMS, CPUS, and NODES
      if i == j:
        continue
      j_oses = merge_os[j_key]
      j_cpus = merge_cpus[j_key]
      j_hits = merge_data[j_key]

      # If the set of supported CPUS matches
      if i_cpus == j_cpus:
        i_set = set(i_hits)
        j_set = set(j_hits)
        # If the nodes in J are a subset of nodes in I
        # then move those nodes from I to J and add the OS
        if j_set.issubset(i_set):
          for hit in j_hits:
            merge_data[i_key].remove(hit)
          for os in i_oses:
            if os not in j_oses:
              merge_os[j_key].append(os)

  out_list = []
  keys = merge_os.keys()
  for i in range(len(keys)):
    hash_cpu_data = keys[i]
    os_use = merge_os[hash_cpu_data]
    cpu_bits = merge_cpus[hash_cpu_data]
    cpu_use = BitsToCPUs(cpu_bits)
    names = merge_data[hash_cpu_data]

    node = (names, os_use, cpu_use)
    out_list.append(node)

  def SortByEffect(x, y):
    # Sort by bigest combition first (# platforms * # cpu architectures)
    xval = len(x[1]) * len(x[2])
    yval = len(y[1]) * len(y[2])
    return  yval - xval

  return sorted(out_list, cmp=SortByEffect)



class Environment(object):
  def __init__(self, tracker, condition):
    self.add_attributes = defaultdict()
    self.del_attributes = defaultdict()
    self.tracker = tracker
    self.condition = condition

  def Bit(self, name):
    os, arch = self.condition.split('_')
    osname = name[:3].upper()

    if osname in OSES:
      return osname == os

    if name == 'target_arm':
      return self.condition[-3]

    if name == 'target_x86':
      return arch == 'x86' or arch == 'x64'

    if name == 'target_x86_32':
      return arch == 'x86'

    if name == 'target_x86_64':
      return arch == 'x64'

    print 'Unknown bit: ' + name
    return False

  def Clone(self):
    env = Environment(self.tracker, self.condition)
    env.add_attributes = dict(self.add_attributes)
    env.del_attributes = dict(self.del_attributes)
    return env

  def Append(self, **kwargs):
    table = ParsePropertyTable(kwargs)
    for k,v in table.iteritems():
      self.add_attributes[k] = self.add_attributes.get(k,[]) + v

  def FilterOut(self, **kwargs):
    table = ParsePropertyTable(kwargs)
    for k,v in table.iteritems():
      self.del_attributes[k] = self.del_attributes.get(k,[]) + v

  def AddObject(self, name, sources, objtype, **kwargs):
    add_props = { 'sources': sources }

    table = ParsePropertyTable(kwargs)
    for k,v in table.iteritems():
      add_props[k] = add_props.get(k, []) + v

    # For all GN property:value pairs in the environment
    for k,v in self.add_attributes.iteritems():
      add_props[k] = add_props.get(k, []) + v

    self.tracker.AddObject(name, objtype, add_props, self.del_attributes)

  def ComponentLibrary(self, name, sources, **kwargs):
    self.AddObject(name, sources, 'library', **kwargs)
    return name

  def DualLibrary(self, name, sources, **kwargs):
    self.AddObject(name, sources, 'dual', **kwargs)
    return name

  def ComponentProgram(self, name, sources, **kwargs):
    self.AddObject(name, sources, 'executable', **kwargs)
    return name

  def CommandTest(self, name, command, size='small', direct_emulation=True,
                extra_deps=[], posix_path=False, capture_output=True,
                capture_stderr=True, wrapper_program_prefix=None,
                scale_timeout=None, **kwargs):

    ARGS = kwargs
    ARGS['direct_emulation']=direct_emulation
    ARGS['extra_deps']=extra_deps
    ARGS['posix_path']=posix_path
    ARGS['capture_output']=capture_output
    ARGS['capture_stderr']=capture_stderr
    ARGS['wrapper_program_prefix']=wrapper_program_prefix
    ARGS['scale_timeout'] = scale_timeout
    self.AddObject(name, [], 'test', **ARGS)
    return name

  def AddNodeToTestSuite(self, node, suites, name, **kwargs):
    self.tracker.AddObject('Add %s as %s to %s.' %
                           (node, name, ' and '.join(suites)),
                           'note')
    return name


class ObjectTracker(object):
  def __init__(self, name):
    self.object_map = {}
    self.top = TopNode(name)
    self.BuildObjectMap(name)
    self.BuildConditions()

  def Dump(self, fileobj):
    self.top.Dump(fileobj, 0)

  def ExecCondition(self, name):
    global_map = {
      'Import': Import
    }
    local_map = {
      'env' : Environment(self, self.active_condition)
    }
    execfile(name, global_map, local_map)

  def AddObject(self, name, obj_type, add_props={}, del_props={}):
    default_obj = {
      'type' : obj_type,
      'configs' : [],
      'properties' : {}
    }

    obj = self.object_map.get(name, default_obj)
    if obj['type'] != obj_type:
      raise RuntimeError('Mismatch type for %s.' % name)
    obj['configs'].append(self.active_condition)

    for prop_name, prop_values in add_props.iteritems():
      prop = obj['properties'].get(prop_name, {})

      # for each value in the property
      if not prop_values:
        continue

      for value in prop_values:
        value_map = prop.get(value, {'configs' : []})
        value_map['configs'].append(self.active_condition)
        prop[value] = value_map
      obj['properties'][prop_name] = prop
    self.object_map[name] = obj

  def BuildObjectMap(self, name):
    for active_condition in ALL:
      # Execute with an empty dict for this condition
      self.active_condition = active_condition
      self.ExecCondition(name)

  def BuildConditions(self, os_avail=OSES, cpu_avail=CPUS):
    # First we parse the TOP objects to get top conditions
    data = self.object_map
    for names, os_use, cpu_use in MergeToDataCPU(data, os_avail, cpu_avail):

      # Create a condition node for this sub-group
      cond = ConditionNode(os_use, os_avail, cpu_use, cpu_avail)
      self.top.AddChild(cond)

      # For each top child in this condition group create the object
      for name in names:
        child_data = data[name]
        obj_type = child_data['type']
        node = ObjectNode(name, obj_type)
        cond.AddChild(node)

        # Now add properties to that object
        for prop in child_data['properties']:
          prop_data = child_data['properties'][prop]

          merged = MergeToDataCPU(prop_data, os_use, cpu_use)
          for names, prop_os, prop_cpu in merged:
            # Create a condition for this property set
            prop_cond = ConditionNode(prop_os, os_use, prop_cpu, cpu_use)
            node.AddChild(prop_cond)

            # Add property set to condition
            prop_node = PropertyNode(prop)
            prop_cond.AddChild(prop_node)
            for value in names:
              prop_node.AddChild(ValueNode(value))
    self.top.Examine(OrganizeProperties())


def Import(name):
  if name != 'env':
    print 'Warning: Tried to IMPORT: ' + name


def ParseSource(name):
  tracker = ObjectTracker(name);
