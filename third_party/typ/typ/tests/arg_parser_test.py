# Copyright 2014 Dirk Pranke. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import optparse
import unittest

from typ import ArgumentParser


class ArgumentParserTest(unittest.TestCase):

    def test_optparse_options(self):
        parser = optparse.OptionParser()
        ArgumentParser.add_option_group(parser, 'foo',
                                        discovery=True,
                                        running=True,
                                        reporting=True,
                                        skip='[-d]')
        options, _ = parser.parse_args(['-j', '1'])
        self.assertEqual(options.jobs, 1)
