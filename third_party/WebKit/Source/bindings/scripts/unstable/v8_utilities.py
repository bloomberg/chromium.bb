# Copyright (C) 2013 Google Inc. All rights reserved.
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

"""Functions shared by various parts of the code generator.

FIXME: Not currently used in build.
This is a rewrite of the Perl IDL compiler in Python, but is not complete.
Once it is complete, we will switch all IDL files over to Python at once.
Until then, please work on the Perl IDL compiler.
For details, see bug http://crbug.com/239771
"""

# FIXME: eliminate this file if possible

import v8_types

ACRONYMS = ['CSS', 'HTML', 'IME', 'JS', 'SVG', 'URL', 'WOFF', 'XML', 'XSLT']


# FIXME: these could be merged as implemented_as_cpp_name
def cpp_class_name(interface):
    return interface.extended_attributes.get('ImplementedAs', interface.name)


def cpp_method_name(attribute_or_operation):
    return attribute_or_operation.extended_attributes.get('ImplementedAs', attribute_or_operation.name)


def generate_conditional_string(interface_or_attribute_or_operation):
    if 'Conditional' not in interface_or_attribute_or_operation.extended_attributes:
        return ''
    conditional = interface_or_attribute_or_operation.extended_attributes['Conditional']
    for operator in ['&', '|']:
        if operator in conditional:
            conditions = set(conditional.split(operator))
            operator_separator = ' %s%s ' % (operator, operator)
            return operator_separator.join(['ENABLE(%s)' % expression for expression in sorted(conditions)])
    return 'ENABLE(%s)' % conditional


def runtime_enable_function_name(definition_or_member):
    """Return the name of the RuntimeEnabledFeatures function.

    The returned function checks if a method/attribute is enabled.
    Given extended attribute EnabledAtRuntime=FeatureName, return:
        RuntimeEnabledFeatures::{featureName}Enabled
    Note that the initial character or acronym is uncapitalized.
    """
    feature_name = definition_or_member.extended_attributes['EnabledAtRuntime']
    return 'RuntimeEnabledFeatures::%sEnabled' % uncapitalize(feature_name)


def uncapitalize(name):
    """Uncapitalize first letter or initial acronym (* with some exceptions).

    E.g., 'SetURL' becomes 'setURL', but 'URLFoo' becomes 'urlFoo'.
    Used in method names; exceptions differ from capitalize().
    """
    for acronym in ACRONYMS:
        if name.startswith(acronym):
            name.replace(acronym, acronym.lower())
            return name
    return name[0].lower() + name[1:]


def v8_class_name(interface):
    return v8_types.v8_type(interface.name)
