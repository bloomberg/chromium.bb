# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import os
import sys
import types

from grit.format import interface
from grit.format.policy_templates import policy_template_generator
from grit.format.policy_templates import writer_configuration
from grit.node import structure
from grit.node import message
from grit.node import misc


class TemplateFormatter(interface.ItemFormatter):
  '''Creates a template file corresponding to an <output> node of the grit
  tree.

  More precisely, processes the whole grit tree for a given <output> node whose
  type is 'adm'. TODO(gfeher) add new types here
  The result of processing is a policy template file with the
  given type and language of the <output> node. A new instance of this class
  is created by grit.misc.GritNode for each <output> node. This class does
  the interfacing with grit, but the actual template-generating work is done in
  policy_template_generator.PolicyTemplateGenerator.
  '''

  def __init__(self, writer_name):
    '''Initializes this formatter to output messages with a given writer.

    Args:
      writer_name: A string identifying the TemplateWriter subclass used
        for generating the output. If writer name is 'adm', then the class
        from module 'writers.adm_writer' will be used.
    '''
    super(type(self), self).__init__()
    writer_module_name = \
        'grit.format.policy_templates.writers.' + writer_name + '_writer'
    __import__(writer_module_name)
    # The module that contains the writer class:
    self._writer_module = sys.modules[writer_module_name]

  def Format(self, item, lang='en', begin_item=True, output_dir='.'):
    '''Generates a template corresponding to an <output> node in the grd file.

    Args:
      item: the <grit> root node of the grit tree.
      lang: the language of outputted text, e.g.: 'en'
      begin_item: True or False, depending on if this function was called at
                  the beginning or at the end of the item.
      output_dir: The output directory, currently unused here.

    Returns:
      The text of the template file.
    '''
    if not begin_item:
      return ''

    self._lang = lang
    self._config = writer_configuration.GetConfigurationForBuild(item.defines)
    self._policy_data = None
    self._messages = {}
    self._ParseGritNodes(item)
    return self._GetOutput()

  def _GetOutput(self):
    '''Generates a template file using the instance variables initialized
    in Format() using the writer specified in __init__().

    Returns:
      The text of the policy template based on the parameters passed
      to __init__() and Format().
    '''
    policy_generator = policy_template_generator.PolicyTemplateGenerator(
        self._config,
        self._policy_data)
    writer = self._writer_module.GetWriter(self._config)
    str = policy_generator.GetTemplateText(writer)
    return str

  def _ParseGritNodes(self, item):
    '''Collects the necessary information from the grit tree:
    the message strings and the policy definitions.

    Args:
      item: The grit node parsed currently.
    '''
    nodes = []
    if (isinstance(item, misc.IfNode) and not item.IsConditionSatisfied()):
      return
    if (isinstance(item, structure.StructureNode) and
        item.attrs['type'] == 'policy_template_metafile'):
      assert self._policy_data == None
      json_text = item.gatherer.Translate(
          self._lang,
          pseudo_if_not_available=item.PseudoIsAllowed(),
          fallback_to_english=item.ShouldFallbackToEnglish())
      self._policy_data = eval(json_text)
    for child in item.children:
      self._ParseGritNodes(child)
