# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Common tools for unit-testing writers.'''


import os
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(sys.argv[0]), '../../../..'))

import tempfile
import unittest
import StringIO

from grit import grd_reader
from grit import util
from grit.tool import build


class DummyOutput(object):
  def __init__(self, type, language, file = 'hello.gif'):
    self.type = type
    self.language = language
    self.file = file
  def GetType(self):
    return self.type
  def GetLanguage(self):
    return self.language
  def GetOutputFilename(self):
    return self.file


class WriterUnittestCommon(unittest.TestCase):
  '''Common class for unittesting writers.'''

  def prepareTest(self, policy_json, grd_text):
    '''Parses a grit tree along with a data structure of policies.

    Args:
      policy_json: The policy data structure in JSON format.
      grd_text: The grit tree in text form.
    '''
    tmp_file_name = 'test.json'
    tmp_dir_name = tempfile.gettempdir()
    json_file_path = tmp_dir_name + '/' + tmp_file_name
    f = open(json_file_path, 'w')
    f.write(policy_json.strip())
    f.close()
    grd = grd_reader.Parse(
        StringIO.StringIO(grd_text % json_file_path), dir=tmp_dir_name)
    grd.RunGatherers(recursive=True)
    os.unlink(json_file_path)
    return grd

  def CompareResult(self, grd, env_lang, env_defs, out_type, out_lang,
                    expected_output):
    '''Generates an output of the writer and compares it with the expected
    result. Fails if they differ.

    Args:
      grd: The root of the grit tree.
      env_lang: The environment language.
      env_defs: Environment definitions.
      out_type: Type of the output node for which output will be generated.
      out_lang: Language of the output node for which output will be generated.
      expected_output: The expected output of the writer.
    '''
    grd.SetOutputContext(env_lang, env_defs)
    buf = StringIO.StringIO()
    build.RcBuilder.ProcessNode(grd, DummyOutput(out_type, out_lang), buf)
    output = buf.getvalue()
    self.assertEquals(output.strip(), expected_output.strip())
