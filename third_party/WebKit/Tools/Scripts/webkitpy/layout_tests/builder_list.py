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

"""Represents a set of builder bots running layout tests.

This class is used to hold a list of builder bots running layout tests and their
corresponding port names and TestExpectations specifiers.

The actual constants are in webkitpy.common.config.builders.
"""


class BuilderList(object):

    def __init__(self, builders_dict):
        """The given dictionary maps builder names to dicts with the keys:

            port_name: A fully qualified port name.
            specifiers: TestExpectations specifiers for that config. Valid values are found in
                  TestExpectationsParser._configuration_tokens_list.
            TODO(qyearsley): Remove rebaseline_override_dir if it's not used.
            rebaseline_override_dir (optional): Directory to put baselines in instead of where
                  you would normally put them. This is useful when we don't have bots that cover
                  particular configurations; so, e.g., you might support mac-mountainlion but not
                  have a mac-mountainlion bot yet, so you'd want to put the mac-lion results into
                  platform/mac temporarily.

        Possible refactoring note: Potentially, it might make sense to use
        webkitpy.common.buildbot.Builder and add port_name and specifiers
        properties to that class.
        """
        self._exact_matches = builders_dict
        self._ports_without_builders = []

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
