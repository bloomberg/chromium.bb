# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from find_invalid_css_variables import FindInvalidCSSVariables
import unittest


class FindInvalidCSSVariablesTest(unittest.TestCase):
    def testUnspecified(self):
        def GitResult(command):
            return '''--test-not-specified
--test-only-rgb-used-rgb
--test-toolbar'''

        json_string = '''
{
  options: {
    CSS: {
      prefix: 'test'
    }
  },
  colors: {
    toolbar: "rgb(255, 255, 255)",
    only_rgb_used: "rgb(255, 255, 255)",
  }
}
        '''

        result = FindInvalidCSSVariables(json_string,
                                         'test',
                                         git_runner=GitResult)
        unused = set()
        self.assertEqual(result['unused'], unused)
        unspecified = set(['--test-not-specified'])
        self.assertEqual(result['unspecified'], unspecified)

    def testUnused(self):
        def GitResult(command):
            return '''--test-toolbar'''

        json_string = '''
{
  options: {
    CSS: {
      prefix: 'test'
    }
  },
  colors: {
    toolbar: "rgb(255, 255, 255)",
    unused: "rgb(255, 255, 255)",
  }
}
        '''

        result = FindInvalidCSSVariables(json_string,
                                         'test',
                                         git_runner=GitResult)
        unused = set(['--test-unused'])
        self.assertEqual(result['unused'], unused)
        unspecified = set()
        self.assertEqual(result['unspecified'], unspecified)

    def testNoPrefix(self):
        def GitResult(command):
            return ''

        json_string = '''
{
  colors: {
    toolbar: "rgb(255, 255, 255)",
  }
}
        '''
        self.assertRaises(KeyError,
                          FindInvalidCSSVariables,
                          json_string,
                          'test',
                          git_runner=GitResult)


if __name__ == '__main__':
    unittest.main()
