#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

import spec
import spec_val
import test_format


class SpecValTestRunner(test_format.TestRunner):

  SECTION_NAME = 'spec'

  def GetSectionContent(self, options, hex_content):
      validator_cls = {32: spec_val.Validator32}[options.bits]

      data = ''.join(test_format.ParseHex(hex_content))
      data += '\x90' * (-len(data) % spec.BUNDLE_SIZE)

      val = validator_cls(data)
      val.Validate()

      if val.messages == []:
        return 'SAFE\n'

      return ''.join(
          '%x: %s\n' % (offset, message) for offset, message in val.messages)


def main(argv):
  SpecValTestRunner().Run(argv)


if __name__ == '__main__':
  main(sys.argv[1:])
