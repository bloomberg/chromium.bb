# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""A utility module for making standalone scripts to start servers.

Scripts in tools/ can use this module to start servers that are normally used
for layout tests, outside of the layout test runner.
"""

import logging
import optparse

from blinkpy.common.host import Host
from blinkpy.web_tests.port.factory import configuration_options


class RawTextHelpFormatter(optparse.IndentedHelpFormatter):
    def format_description(self, description):
        return description


def main(server_constructor, input_fn=None, argv=None, description=None, **kwargs):
    input_fn = input_fn or raw_input

    parser = optparse.OptionParser(description=description, formatter=RawTextHelpFormatter())
    parser.add_option('--output-dir', type=str, default=None,
                      help='output directory, for log files etc.')
    parser.add_option('-v', '--verbose', action='store_true',
                      help='print more information, including port numbers')
    for opt in configuration_options():
        parser.add_option(opt)
    options, _ = parser.parse_args(argv)

    logging.basicConfig()
    logger = logging.getLogger()
    logger.setLevel(logging.DEBUG if options.verbose else logging.INFO)

    host = Host()
    port_obj = host.port_factory.get(options=options)
    if not options.output_dir:
        options.output_dir = port_obj.default_results_directory()

    # Create the output directory if it doesn't already exist.
    port_obj.host.filesystem.maybe_make_directory(options.output_dir)

    server = server_constructor(port_obj, options.output_dir, **kwargs)
    server.start()
    try:
        _ = input_fn('Hit any key to stop the server and exit.')
    except (KeyboardInterrupt, EOFError):
        pass

    server.stop()
