# Copyright 2014 Google Inc. All rights reserved.
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

from typ import main, spawn_main


if __name__ == '__main__':
    if sys.platform == 'win32':  # pragma: win32
        sys.exit(spawn_main(sys.argv[1:], sys.stdout, sys.stderr))
    else:  # pragma: no win32
        sys.exit(main())
