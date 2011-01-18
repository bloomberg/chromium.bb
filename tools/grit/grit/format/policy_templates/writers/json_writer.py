# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


from xml.dom import minidom
from grit.format.policy_templates.writers import template_writer


def GetWriter(config):
  '''Factory method for creating JsonWriter objects.
  See the constructor of TemplateWriter for description of
  arguments.
  '''
  return JsonWriter(['linux'], config)


class JsonWriter(template_writer.TemplateWriter):
  '''Class for generating policy files in JSON format (for Linux). The
  generated files will define all the supported policies with example values
  set for them. This class is used by PolicyTemplateGenerator to write .json
  files.
  '''

  def PreprocessPolicies(self, policy_list):
    return self.FlattenGroupsAndSortPolicies(policy_list)

  def WritePolicy(self, policy):
    example_value = policy['example_value']
    if policy['type'] == 'string':
      example_value_str = '"' + example_value + '"'
    elif policy['type'] == 'int':
      example_value_str = str(example_value)
    elif policy['type'] == 'list':
      if example_value == []:
        example_value_str = '[]'
      else:
        example_value_str = '["%s"]' % '", "'.join(example_value)
    elif policy['type'] == 'main':
      if example_value == True:
        example_value_str = 'true'
      else:
        example_value_str = 'false'
    elif policy['type'] == 'string-enum':
      example_value_str = '"%s"' % example_value;
    elif policy['type'] == 'int-enum':
      example_value_str = str(example_value)
    else:
      raise Exception('unknown policy type %s:' % policy['type'])

    # Add comme to the end of the previous line.
    if not self._first_written:
      self._out[-1] += ','

    line = '  "%s": %s' % (policy['name'], example_value_str)
    self._out.append(line)

    self._first_written = False

  def BeginTemplate(self):
    self._out.append('{')

  def EndTemplate(self):
    self._out.append('}')

  def Init(self):
    self._out = []
    # The following boolean member is true until the first policy is written.
    self._first_written = True

  def GetTemplateText(self):
    return '\n'.join(self._out)
