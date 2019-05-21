# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Draw wayland protocols with Graphviz.

Provides a GvPrinter class which is used to output one (or more) wayland
protocols as a graph, showing the relationships between interfaces, their
messages, and how those messages create new interfaces.
"""

from __future__ import absolute_import
from __future__ import print_function
import protocol_util


class GvPrinter(object):
  """Base class for printing graphviz graphs.

  The base printer class, has several generic utilities for printing different
  graph representations of the protocols.

  Users of this class will call the "draw(protocols)" method to print a
  graphviz-syntax graph.
  """

  def __init__(self):
    self.nodes = {}

  def force_iface(self, arg):
    i_name = arg.attrib.get('interface', 'unknown')
    if (i_name, None) not in self.nodes:
      self.out('"%s" [label="%s" shape=parallelogram]' % (i_name, i_name))
      self.nodes[(i_name, None)] = i_name
    return (i_name, None)

  def out(self, text):
    print(text)

  def draw_nodes(self, protocol):
    pass

  def draw_edges(self, protocol):
    pass

  def draw(self, protocols):
    """Draw the graph.

    Args:
      protocols: the list of xml.etree.ElementTree.Element protocols which you
        want to draw as a graph. See third_party/wayland/src/protocol/ for more
        information.
    """
    self.out('digraph g {')
    self.out('pack=false;')
    self.out('start="random";')
    self.out('overlap=false;')
    self.out('splines=true;')
    self.out('graph [rankdir = "LR"];')
    for protocol in protocols:
      self.draw_nodes(protocol)
    for protocol in protocols:
      self.draw_edges(protocol)
    self.out('}')


class InterfacesPrinter(GvPrinter):
  """Print protocol interfaces as a graphviz graph.

  A specialization of the graph printer which draws interfaces and their
  methods as a class diagram, where edges indicate relationships between the
  methods.
  """

  def __init__(self, draw_enums=False):
    super(InterfacesPrinter, self).__init__()
    self.draw_enums = draw_enums

  def draw_edge(self, src, snk):
    for n in [src, snk]:
      if n not in self.nodes:
        raise Exception('Unable to find node "%s" when drawing (%s, %s)' %
                        (n, src, snk))
    self.out(self.nodes[src] + ' -> ' + self.nodes[snk] + ' [];')

  def draw_edges(self, protocol):
    """Draw edges between interfaces and methods.

    Draws all the edges:
     - From an enum to a method that uses that enum as an argument.
     - From an interface to a method that uses that interface as an argument.
     - From a method to an interface that invoking that method will create.

    Args:
      protocol: a wayland <protocol> as an xml.etree.ElementTree.Element.
    """
    for interface in protocol.findall('interface'):
      i_name = interface.attrib['name']
      for message in protocol_util.grab_interface_messages(interface):
        m_name = message.attrib['name']
        for arg in message.findall('arg'):
          if self.draw_enums and 'enum' in arg.attrib:
            self.draw_edge((i_name, arg.attrib['enum']), (i_name, m_name))
            continue
          ty = arg.attrib['type']
          if ty == 'new_id':
            self.draw_edge((i_name, m_name), self.force_iface(arg))
          elif ty == 'object':
            self.draw_edge(self.force_iface(arg), (i_name, m_name))

  def draw_nodes(self, protocol):
    """Draw record nodes for each interface.

    Draws a record-style node for each interface. The header is the interface
    name and each subsequent entry is an event or request which that interface
    defines.

    As a side-effect, this method populates the |nodes| map.

    Args:
      protocol: a wayland <protocol> as an xml.etree.ElementTree.Element.
    """
    for interface in protocol.findall('interface'):
      i_name = interface.attrib['name']
      i_label = '<i>'+i_name
      if self.draw_enums:
        for enum in interface.findall('enum'):
          e_name = enum.attrib['name']
          node_name = i_name + '_' + e_name
          fields = [
              e.attrib['name'] + ' = ' + e.attrib['value']
              for e in enum.findall('entry')
          ]
          self.out('"%s" [label="%s" shape=record]' %
                   (node_name, '|'.join(['<e>'+e_name]+fields)))
          self.nodes[(i_name, e_name)] = node_name+':e'
      for count, message in enumerate(
          protocol_util.grab_interface_messages(interface)):
        m_name = message.attrib['name']
        is_request = message.tag == 'request'
        m_label = ('<m%d>'%count) + (m_name + ' -\\>' if is_request
                                     else '-\\> ' + m_name)
        i_label += '|' + m_label
        self.nodes[(i_name, m_name)] = i_name + (':m%d'%count)
      self.out('"%s" [label="%s" shape=record]' % (i_name, i_label))
      self.nodes[(i_name, None)] = i_name+':i'


class DepsPrinter(GvPrinter):
  """Print message dependencies as a graphviz graph.

  A specialization of the graph printer for showind dependencies between
  methods. Nodes in this graph are either interfaces or methods, and edges
  indicate a relationship of "defines" (when the interface is the method's
  'this' object), "uses" (when the interface is an argument), or "creates"
  (when the result of the call is the creation of an object).
  """

  def __init__(self, draw_all=False):
    super(DepsPrinter, self).__init__()
    self.draw_all = draw_all

  def should_draw_message(self, message):
    return self.draw_all or protocol_util.is_constructor(message)

  def draw_nodes(self, protocol):
    """Draws a node for each message/interface.

    Draws a node for every interface and all of its methods. As a side effect,
    calling this method will populate the |nodes| map.

    Args:
      protocol: a wayland <protocol> as an xml.etree.ElementTree.Element.
    """
    self.nodes[(None, None)] = '_unknown_'
    self.out('"_unknown_" [label="?" shape=diamond]')
    for i in protocol.findall('interface'):
      i_name = i.attrib['name']
      self.nodes[(i_name, None)] = i_name
      self.out('"%s" [shape=box]' % (i_name))
      for m in protocol_util.grab_interface_messages(i):
        if self.should_draw_message(m):
          m_name = m.attrib['name']
          m_node = i_name + ':' + m_name
          m_shape = 'hexagon' if m.tag == 'event' else 'ellipse'
          self.nodes[(i_name, m_name)] = m_node
          self.out('"%s" [label="%s" shape=%s]' %
                   (m_node, m_name, m_shape))

  def get_obj(self, arg):
    if 'interface' in arg.attrib:
      return self.nodes[self.force_iface(arg)]
    return self.nodes[(None, None)]

  def draw_edges(self, protocol):
    """Draw edges showing message arguments.

    Draws edges showing the relationship between the messages and the
    interfaces. Bold edges signify "defines" or "creates" relationships, while
    dotted edges signify "uses" relationships.

    Args:
      protocol: a wayland <protocol> as an xml.etree.ElementTree.Element.
    """
    for i in protocol.findall('interface'):
      i_name = i.attrib['name']
      for m in protocol_util.grab_interface_messages(i):
        if self.should_draw_message(m):
          m_name = m.attrib['name']
          self.out('"%s" -> "%s" [style=bold]' %
                   (self.nodes[(i_name, None)], self.nodes[(i_name, m_name)]))
          for pa in m.findall('arg'):
            if pa.attrib['type'] == 'object':
              self.out('"%s" -> "%s" [style=dotted]' %
                       (self.get_obj(pa), self.nodes[(i_name, m_name)]))
          for pa in m.findall('arg'):
            if pa.attrib['type'] == 'new_id':
              self.out('"%s" -> "%s" [style=bold]' %
                       (self.nodes[(i_name, m_name)], self.get_obj(pa)))

