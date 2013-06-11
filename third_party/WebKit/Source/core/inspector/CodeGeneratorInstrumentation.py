#!/usr/bin/env python
# Copyright (c) 2013 Google Inc. All rights reserved.
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

import optparse
import re
import string
import sys

template_h = string.Template("""// Code generated from InspectorInstrumentation.idl

#ifndef ${file_name}_h
#define ${file_name}_h

${includes}

namespace WebCore {

namespace InspectorInstrumentation {

$methods
} // namespace InspectorInstrumentation

} // namespace WebCore

#endif // !defined(${file_name}_h)
""")

template_inline = string.Template("""
inline void ${name}(${params_public})
{   ${fast_return}
    if (${condition})
        ${name}Impl(${params_impl});
}
""")

template_inline_forward = string.Template("""
inline void ${name}(${params_public})
{   ${fast_return}
    ${name}Impl(${params_impl});
}
""")

template_inline_accepts_cookie = string.Template("""
inline void ${name}(${params_public})
{   ${fast_return}
    if (${condition}.isValid())
        ${name}Impl(${params_impl});
}
""")

template_inline_returns_cookie = string.Template("""
inline InspectorInstrumentationCookie ${name}(${params_public})
{   ${fast_return}
    if (${condition})
        return ${name}Impl(${params_impl});
    return InspectorInstrumentationCookie();
}
""")


template_cpp = string.Template("""// Code generated from InspectorInstrumentation.idl

#include "config.h"

${includes}

namespace WebCore {

namespace InspectorInstrumentation {
$methods

} // namespace InspectorInstrumentation

} // namespace WebCore
""")

template_outofline = string.Template("""
${return_type} ${name}Impl(${params_impl})
{${impl_lines}
}""")

template_agent_call = string.Template("""
    if (${agent_class}* agent = ${agent_fetch})
        agent->${name}(${params_agent});""")

template_agent_call_timeline_returns_cookie = string.Template("""
    int timelineAgentId = 0;
    if (InspectorTimelineAgent* agent = agents->inspectorTimelineAgent()) {
        if (agent->${name}(${params_agent}))
            timelineAgentId = agent->id();
    }""")


def match_and_consume(pattern, source):
    match = re.match(pattern, source)
    if match:
        return match, source[len(match.group(0)):].strip()
    return None, source


def load_model_from_idl(source):
    source = re.sub("//.*", "", source)  # Remove line comments
    source = re.sub("/\*(.|\n)*?\*/", "", source, re.MULTILINE)  # Remove block comments
    source = re.sub("\]\s*?\n\s*", "] ", source)  # Merge the method annotation with the next line
    source = source.strip()

    model = []

    while len(source):
        match, source = match_and_consume("interface\s(\w*)\s?\{([^\{]*)\}", source)
        if not match:
            sys.stderr.write("Cannot parse %s\n" % source[:100])
            sys.exit(1)
        model.append(File(match.group(1), match.group(2)))

    return model


class File:
    def __init__(self, name, source):
        self.name = name
        self.header_name = self.name + "Inl"
        self.includes = [include_inspector_header("InspectorInstrumentation")]
        self.declarations = []
        for line in map(str.strip, source.split("\n")):
            line = re.sub("\s{2,}", " ", line).strip()  # Collapse whitespace
            if len(line) == 0:
                continue
            if line[0] == "#":
                self.includes.append(line)
            else:
                self.declarations.append(Method(line))
        self.includes.sort()

    def generate(self, cpp_lines, used_agents):
        header_lines = []
        for declaration in self.declarations:
            for agent in set(declaration.agents):
                used_agents.add(agent)
            declaration.generate_header(header_lines)
            declaration.generate_cpp(cpp_lines)

        return template_h.substitute(None,
                                     file_name=self.header_name,
                                     includes="\n".join(self.includes),
                                     methods="\n".join(header_lines))


class Method:
    def __init__(self, source):
        match = re.match("(\[[\w|,|=|\s]*\])?\s?(\w*) (\w*)\((.*)\)\s?;", source)
        if not match:
            sys.stderr.write("Cannot parse %s\n" % source)
            sys.exit(1)

        self.options = []
        if match.group(1):
            options_str = re.sub("\s", "", match.group(1)[1:-1])
            if len(options_str) != 0:
                self.options = options_str.split(",")

        self.return_type = match.group(2)

        self.name = match.group(3)

        # Splitting parameters by a comma, assuming that attribute lists contain no more than one attribute.
        self.params = map(Parameter, map(str.strip, match.group(4).split(",")))

        self.accepts_cookie = len(self.params) and self.params[0].type == "const InspectorInstrumentationCookie&"
        self.returns_cookie = self.return_type == "InspectorInstrumentationCookie"

        self.params_impl = self.params
        if not self.accepts_cookie and not "Inline=Forward" in self.options:
            if not "Keep" in self.params_impl[0].options:
                self.params_impl = self.params_impl[1:]
            self.params_impl = [Parameter("InstrumentingAgents* agents")] + self.params_impl
            self.agents_selector_class = re.match("(\w*)", self.params[0].type).group(1)

        self.agents = filter(lambda option: not "=" in option, self.options)

    def generate_header(self, header_lines):
        if "Inline=Custom" in self.options:
            return

        header_lines.append("%s %sImpl(%s);" % (
            self.return_type, self.name, ", ".join(map(Parameter.to_str_class, self.params_impl))))

        if "Inline=FastReturn" in self.options or "Inline=Forward" in self.options:
            fast_return = "\n    FAST_RETURN_IF_NO_FRONTENDS(%s());" % self.return_type
        else:
            fast_return = ""

        if self.accepts_cookie:
            condition = self.params_impl[0].name
            template = template_inline_accepts_cookie
        elif "Inline=Forward" in self.options:
            condition = ""
            template = template_inline_forward
        else:
            condition = "InstrumentingAgents* agents = instrumentingAgentsFor%s(%s)" % (
                self.agents_selector_class, self.params[0].name)

            if self.returns_cookie:
                template = template_inline_returns_cookie
            elif self.return_type == "void":
                template = template_inline
            else:
                sys.stderr.write("Unsupported return type %s" % self.return_type)
                sys.exit(1)

        header_lines.append(template.substitute(
            None,
            name=self.name,
            fast_return=fast_return,
            params_public=", ".join(map(Parameter.to_str_full, self.params)),
            params_impl=", ".join(map(Parameter.to_str_name, self.params_impl)),
            condition=condition))

    def generate_cpp(self, cpp_lines):
        if len(self.agents) == 0:
            return

        body_lines = map(self.generate_agent_call, self.agents)

        if self.returns_cookie:
            if "Timeline" in self.agents:
                timeline_agent_id = "timelineAgentId"
            else:
                timeline_agent_id = "0"
            body_lines.append("\n    return InspectorInstrumentationCookie(agents, %s);" % timeline_agent_id)

        cpp_lines.append(template_outofline.substitute(
            None,
            return_type=self.return_type,
            name=self.name,
            params_impl=", ".join(map(Parameter.to_str_class_and_name, self.params_impl)),
            impl_lines="".join(body_lines)))

    def generate_agent_call(self, agent):
        agent_class = agent_class_name(agent)
        agent_getter = agent_class[0].lower() + agent_class[1:]

        leading_param_name = self.params_impl[0].name
        if not self.accepts_cookie:
            agent_fetch = "%s->%s()" % (leading_param_name, agent_getter)
        elif agent == "Timeline":
            agent_fetch = "retrieveTimelineAgent(%s)" % leading_param_name
        else:
            agent_fetch = "%s.instrumentingAgents()->%s()" % (leading_param_name, agent_getter)

        if agent == "Timeline" and self.returns_cookie:
            template = template_agent_call_timeline_returns_cookie
        else:
            template = template_agent_call

        return template.substitute(
            None,
            name=self.name,
            agent_class=agent_class,
            agent_fetch=agent_fetch,
            params_agent=", ".join(map(Parameter.to_str_name, self.params_impl)[1:]))


class Parameter:
    def __init__(self, source):
        self.options = []
        match, source = match_and_consume("\[(\w*)\]", source)
        if match:
            self.options.append(match.group(1))

        parts = map(str.strip, source.split("="))
        if len(parts) == 1:
            self.default_value = None
        else:
            self.default_value = parts[1]

        param_decl = parts[0]

        if re.match("(const|unsigned long) ", param_decl):
            min_type_tokens = 2
        else:
            min_type_tokens = 1

        if len(param_decl.split(" ")) > min_type_tokens:
            parts = param_decl.split(" ")
            self.type = " ".join(parts[:-1])
            self.name = parts[-1]
        else:
            self.type = param_decl
            self.name = generate_param_name(self.type)

    def to_str_full(self):
        if self.default_value is None:
            return self.to_str_class_and_name()
        return "%s %s = %s" % (self.type, self.name, self.default_value)

    def to_str_class_and_name(self):
        return "%s %s" % (self.type, self.name)

    def to_str_class(self):
        return self.type

    def to_str_name(self):
        return self.name


def generate_param_name(param_type):
    base_name = re.match("(const |PassRefPtr<)?(\w*)", param_type).group(2)
    return "param" + base_name


def agent_class_name(agent):
    custom_agent_names = ["Inspector", "PageDebugger", "PageRuntime", "WorkerRuntime"]
    if agent in custom_agent_names:
        return "%sAgent" % agent
    return "Inspector%sAgent" % agent


def include_inspector_header(name):
    return "#include \"core/inspector/%s.h\"" % name


def generate(input_path, output_h_dir, output_cpp_dir):
    fin = open(input_path, "r")
    files = load_model_from_idl(fin.read())
    fin.close()

    cpp_includes = []
    cpp_lines = []
    used_agents = set()
    for f in files:
        file_name = f.header_name + ".h"
        cpp_includes.append("#include \"%s\"" % file_name)

        fout = open(output_h_dir + "/" + file_name, "w")
        fout.write(f.generate(cpp_lines, used_agents))
        fout.close()

    for agent in used_agents:
        cpp_includes.append(include_inspector_header(agent_class_name(agent)))
    cpp_includes.append(include_inspector_header("InstrumentingAgents"))
    cpp_includes.sort()

    fout = open(output_cpp_dir + "/InspectorInstrumentationImpl.cpp", "w")
    fout.write(template_cpp.substitute(None,
                                       includes="\n".join(cpp_includes),
                                       methods="\n".join(cpp_lines)))
    fout.close()


cmdline_parser = optparse.OptionParser()
cmdline_parser.add_option("--output_h_dir")
cmdline_parser.add_option("--output_cpp_dir")

try:
    arg_options, arg_values = cmdline_parser.parse_args()
    if (len(arg_values) != 1):
        raise Exception("Exactly one plain argument expected (found %s)" % len(arg_values))
    input_path = arg_values[0]
    output_header_dirpath = arg_options.output_h_dir
    output_cpp_dirpath = arg_options.output_cpp_dir
    if not output_header_dirpath:
        raise Exception("Output .h directory must be specified")
    if not output_cpp_dirpath:
        raise Exception("Output .cpp directory must be specified")
except Exception:
    # Work with python 2 and 3 http://docs.python.org/py3k/howto/pyporting.html
    exc = sys.exc_info()[1]
    sys.stderr.write("Failed to parse command-line arguments: %s\n\n" % exc)
    sys.stderr.write("Usage: <script> InspectorInstrumentation.idl --output_h_dir <output_header_dir> --output_cpp_dir <output_cpp_dir>\n")
    exit(1)

generate(input_path, output_header_dirpath, output_cpp_dirpath)
