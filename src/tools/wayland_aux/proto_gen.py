# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates a protobuf for fuzzing purposes.

Use the generate() function to print a protobuf file, which allows libfuzzer to
fuzz the given protocols..
"""

from __future__ import absolute_import
from __future__ import print_function
import protocol_util


class Generator(object):
  """Base class for generating something from a list of protocols.

  Provides several utilities for generating files of things from a list of
  protocols.
  """

  def out(self, text):
    print(text)

  def generate(self, protocols):
    pass


proto_type_conversions = {
    'object': 'uint32',
    'int': 'int32',
    'uint': 'uint32',
    'string': 'string',
    'fd': 'uint32',
}


class ProtoGenerator(Generator):
  """Generate a libfuzzable protobuf.

  Creates a protobuf for use when fuzzing wayland. This protobuf defines all the
  client-side messages which the fuzzer might call.
  """

  def named(self, *args):
    return '_'.join((e.attrib['name'] for e in args))

  def get_type(self, ty):
    if ty in proto_type_conversions:
      return proto_type_conversions[ty]
    raise Exception('unknown conversion for type: ' + ty)

  def generate_action(self, name, args):
    self.out('message %s {' % name)
    for (idx, (a_type, a_name)) in enumerate([('uint32', 'receiver')] + args):
      self.out('required %s %s = %d;'%(a_type, a_name, idx+1))
    self.out('}')

  def generate_req_action(self, ptc, ifc, msg):
    """Generate an action protobuf message.

    Args:
      ptc: the <protocol> xml element.
      ifc: the <interface> xml element.
      msg: the <request> xml element (given that <event>s don't need messages).
    """
    args = []
    name = self.named(ptc, ifc, msg)
    if name == 'wayland_wl_registry_bind':
      args = [('global', 'global'), ('uint32', 'name'), ('uint32', 'version')]
    else:
      args = [(self.get_type(arg.attrib['type']), arg.attrib['name'])
              for arg in msg.findall('arg')
              if arg.attrib['type'] != 'new_id']
    if protocol_util.is_constructor(msg):
      c_type = protocol_util.get_constructed(msg)
      self.out('// Constructs %s' % (c_type if c_type else '???'))
    self.generate_action(name, args)

  def generate(self, protocols):
    self.out('syntax = "proto2";')
    self.out('package = exo.wayland_fuzzer;')

    # Make the globals enum for identifying globals to bind.
    self.out('enum global {')
    self.out('GLOBAL_UNSPECIFIED=0;')
    for (idx, (pro, ifc)) in enumerate(protocol_util.get_globals(protocols)):
      self.out('%s = %d;'%(self.named(pro, ifc), idx+1))
    self.out('}')

    # List all the possible actions.
    self.out('message actions {')
    self.out('repeated action acts = 1;')
    self.out('}')
    self.out('message action {')
    self.out('oneof act {')
    actions = ['meta_dispatch', 'meta_roundtrip'] + [
        self.named(p, i, m)
        for (p, i, m) in protocol_util.all_messages(protocols)
        if protocol_util.is_request(m)]
    for (idx, act) in enumerate(actions):
      self.out('%s act_%s = %d;' % (act, act, idx+1))
    self.out('}')
    self.out('}')
    self.generate_action('meta_dispatch', [])
    self.generate_action('meta_roundtrip', [])
    for (p, i, m) in protocol_util.all_messages(protocols):
      if protocol_util.is_request(m):
        self.generate_req_action(p, i, m)


def generate(protocols):
  """Make a protobuf for fuzzing the |protocols|.

  Args:
    protocols: a list of xml.etree.ElementTree.Element where each element is a
      wayland <protocol> node. This list will be converted to a protobuf file
      that can be used for fuzzing.
  """
  ProtoGenerator().generate(protocols)
