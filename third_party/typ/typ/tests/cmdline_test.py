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

import os
import tempfile

from typ import spawn_main
from typ import test_case
from typ import VERSION


class TestSpawnMain(test_case.MainTestCase):
    def call(self, host, argv, stdin, env):
        out = err = None
        out_str = err_str = ''
        try:
            out = tempfile.NamedTemporaryFile(delete=False)
            err = tempfile.NamedTemporaryFile(delete=False)
            ret = spawn_main(argv, stdout=out, stderr=err)
            out.close()
            out_str = open(out.name).read()
            err.close()
            err_str = open(err.name).read()
        finally:
            if out:
                out.close()
                os.remove(out.name)
            if err:
                err.close()
                os.remove(err.name)
        return ret, out_str, err_str

    def test_version(self):
        self.check(['--version'], ret=0, out=VERSION + '\n')
