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

import sys

from typ.tests import host_test
from typ.fakes.host_fake import FakeHost


class TestFakeHost(host_test.TestHost):

    def host(self):
        return FakeHost()

    def test_add_to_path(self):
        # TODO: FakeHost uses the real sys.path, and then gets
        # confused becayse host.abspath() doesn't work right for
        # windows-style paths.
        if sys.platform != 'win32':
            super(TestFakeHost, self).test_add_to_path()

    def test_call(self):
        h = self.host()
        ret, out, err = h.call(['echo', 'hello, world'])
        self.assertEqual(ret, 0)
        self.assertEqual(out, '')
        self.assertEqual(err, '')
        self.assertEqual(h.cmds, [['echo', 'hello, world']])

    def test_for_mp(self):
        h = self.host()
        self.assertNotEqual(h.for_mp(), None)
