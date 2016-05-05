# Copyright (C) 2011 Google Inc. All rights reserved.
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

import re

"""Represents builder bots running layout tests.

This class is used to keep a list of all builder bots running layout tests on
the Chromium waterfall. There are other waterfalls that run layout tests but
this list is the one we care about in the context of TestExpectatoins. The
builders are hard coded in the constructor but can be overridden for unit tests.

"""
class Builders(object):

    def __init__(self):
        """ In this dictionary, each item stores:

            * port_name -- a fully qualified port name
            * rebaseline_override_dir -- (optional) directory to put baselines in instead of where
                  you would normally put them. This is useful when we don't have bots that cover
                  particular configurations; so, e.g., you might support mac-mountainlion but not
                  have a mac-mountainlion bot yet, so you'd want to put the mac-lion results into
                  platform/mac temporarily.
            * specifiers -- TestExpectation specifiers for that config. Valid values are found in
                 TestExpectationsParser._configuration_tokens_list
        """
        self._exact_matches = {
            "WebKit Win7": {"port_name": "win-win7", "specifiers": ['Win7', 'Release']},
            "WebKit Win7 (dbg)": {"port_name": "win-win7", "specifiers": ['Win7', 'Debug']},
            "WebKit Win10": {"port_name": "win-win10", "specifiers": ['Win10', 'Release']},
            # FIXME: Rename this to 'WebKit Linux Precise'
            "WebKit Linux": {"port_name": "linux-precise", "specifiers": ['Precise', 'Release']},
            "WebKit Linux Trusty": {"port_name": "linux-trusty", "specifiers": ['Trusty', 'Release']},
            "WebKit Linux (dbg)": {"port_name": "linux-precise", "specifiers": ['Precise', 'Debug']},
            "WebKit Mac10.9": {"port_name": "mac-mac10.9", "specifiers": ['Mac10.9', 'Release']},
            "WebKit Mac10.10": {"port_name": "mac-mac10.10", "specifiers": ['Mac10.10', 'Release']},
            "WebKit Mac10.11": {"port_name": "mac-mac10.11", "specifiers": ['10.11', 'Release']},
            "WebKit Mac10.11 (dbg)": {"port_name": "mac-mac10.11", "specifiers": ['10.11', 'Debug']},
            "WebKit Mac10.11 (retina)": {"port_name": "mac-retina", "specifiers": ['Retina', 'Release']},
            "WebKit Android (Nexus4)": {"port_name": "android", "specifiers": ['Android', 'Release']},
        }

        self._ports_without_builders = [
        ]

    def builder_path_from_name(self, builder_name):
        return re.sub(r'[\s().]', '_', builder_name)

    def all_builder_names(self):
        return sorted(set(self._exact_matches.keys()))

    def all_port_names(self):
        return sorted(set(map(lambda x: x["port_name"], self._exact_matches.values()) + self._ports_without_builders))

    def rebaseline_override_dir(self, builder_name):
        return self._exact_matches[builder_name].get("rebaseline_override_dir", None)

    def port_name_for_builder_name(self, builder_name):
        return self._exact_matches[builder_name]["port_name"]

    def specifiers_for_builder(self, builder_name):
        return self._exact_matches[builder_name]["specifiers"]

    def builder_name_for_port_name(self, target_port_name):
        debug_builder_name = None
        for builder_name, builder_info in self._exact_matches.items():
            if builder_info['port_name'] == target_port_name:
                if 'dbg' in builder_name:
                    debug_builder_name = builder_name
                else:
                    return builder_name
        return debug_builder_name

    def builder_path_for_port_name(self, port_name):
        self.builder_path_from_name(self.builder_name_for_port_name(port_name))
