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

#ifndef InspectorInstrumentationInl_h
#define InspectorInstrumentationInl_h

namespace WebCore {

namespace InspectorInstrumentation {

$impl_declarations
$inline_methods
} // namespace InspectorInstrumentation

} // namespace WebCore

#endif // !defined(InspectorInstrumentationInl_h)
""")

template_inline = string.Template("""
inline void ${name}(${params_public})
{   ${fast_return}
    if (InstrumentingAgents* instrumentingAgents = ${agents_getter})
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
    if (${cookie}.isValid())
        ${name}Impl(${params_impl});
}
""")

template_inline_returns_cookie = string.Template("""
inline InspectorInstrumentationCookie ${name}(${params_public})
{   ${fast_return}
    if (InstrumentingAgents* instrumentingAgents = ${agents_getter})
        return ${name}Impl(${params_impl});
    return InspectorInstrumentationCookie();
}
""")


template_cpp = string.Template("""// Code generated from InspectorInstrumentation.idl

#include "config.h"
#include "core/inspector/InspectorInstrumentation.h"

#include "core/inspector/InspectorAgent.h"
#include "core/inspector/InspectorApplicationCacheAgent.h"
#include "core/inspector/InspectorCSSAgent.h"
#include "core/inspector/InspectorCanvasAgent.h"
#include "core/inspector/InspectorConsoleAgent.h"
#include "core/inspector/InspectorConsoleInstrumentation.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorDOMDebuggerAgent.h"
#include "core/inspector/InspectorDOMStorageAgent.h"
#include "core/inspector/InspectorDatabaseAgent.h"
#include "core/inspector/InspectorDatabaseInstrumentation.h"
#include "core/inspector/InspectorDebuggerAgent.h"
#include "core/inspector/InspectorHeapProfilerAgent.h"
#include "core/inspector/InspectorLayerTreeAgent.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/inspector/InspectorProfilerAgent.h"
#include "core/inspector/InspectorResourceAgent.h"
#include "core/inspector/InspectorTimelineAgent.h"
#include "core/inspector/InspectorWorkerAgent.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/inspector/PageDebuggerAgent.h"
#include "core/inspector/PageRuntimeAgent.h"
#include "core/inspector/WorkerRuntimeAgent.h"

namespace WebCore {

namespace InspectorInstrumentation {
$out_of_line_methods

} // namespace InspectorInstrumentation

} // namespace WebCore
""")

template_outofline = string.Template("""
void ${name}Impl(${params_impl})
{${agent_calls}
}""")

template_agent_call = string.Template("""
    if (${agent_class}* ${agent}Agent = ${agent_fetch})
        ${agent}Agent->${name}(${params_agent});""")

template_timeline_agent_call = string.Template("""
    int timelineAgentId = 0;
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents->inspectorTimelineAgent()) {
        if (timelineAgent->${name}(${params_agent}))
            timelineAgentId = timelineAgent->id();
    }""")

template_outofline_returns_cookie = string.Template("""
${return_type} ${name}Impl(${params_impl})
{${agent_calls}
    return InspectorInstrumentationCookie(instrumentingAgents, ${timeline_agent_id});
}""")


def load_model_from_idl(source):
    source = re.sub("//.*\n", "", source)    # Remove line comments
    source = re.sub("\n", " ", source)       # Remove newlines
    source = re.sub("/\*.*\*/", "", source)  # Remove block comments
    source = re.sub("\s\s+", " ", source)    # Collapse whitespace
    source = source.strip()

    match = re.match("interface\s\w*\s?\{(.*)\}", source)
    if not match:
        sys.stderr.write("Cannot parse the file")
        sys.exit(1)
    lines = match.group(1)

    methods = []
    for line in map(str.strip, lines.split(";")):
        if len(line) == 0:
            continue
        methods.append(Method(line))
    return methods


class Method:
    def __init__(self, source):
        match = re.match("(\[[\w|,|=|\s]*\])?\s?(\w*) (\w*)\((.*)\)", source)
        if not match:
            sys.stderr.write("Cannot parse %s\n" % source)
            sys.exit(1)

        self.method_options = []
        if match.group(1):
            method_options_str = re.sub("\s", "", match.group(1)[1:-1])
            if len(method_options_str) != 0:
                self.method_options = method_options_str.split(",")

        self.return_type = match.group(2)

        self.name = match.group(3)

        param_string = match.group(4)

        self.param_options = []
        options_match = re.match("\[(.*)\]\s?(.*)", param_string)
        if options_match:
            param_string = options_match.group(2)
            self.param_options = map(str.strip, options_match.group(1).split(","))

        self.params = map(Parameter, map(str.strip, param_string.split(",")))


class Parameter:
    def __init__(self, source):
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


# This function is only needed to minimize the diff with the handwritten code.
# In fact it is sufficient to return a globally unique string.
def generate_param_name(param_type):
    base_name = re.match("(const |RefPtr<|PassRefPtr<)?(\w*)", param_type).group(2)

    custom_param_types = {
        "CharacterData": "characterData",
        "CSSSelector": "pseudoState",
        "DocumentStyleSheetCollection": "styleSheetCollection",
        "EventPath": "eventPath",
        "FormData": "formData",
        "InspectorCSSOMWrappers": "inspectorCSSOMWrappers",
        "InstrumentingAgents": "instrumentingAgents",
        "NamedFlow": "namedFlow",
        "RenderObject": "renderer",
        "RenderLayer": "renderLayer",
        "ResourceLoader": "resourceLoader",
        "PseudoElement": "pseudoElement",
        "ScriptState": "scriptState"}
    if base_name in custom_param_types:
        return custom_param_types[base_name]

    match = re.match("(.*)([A-Z][a-z]+)", base_name)
    if match:
        return match.group(2).lower()  # CamelCaseWord -> word

    if base_name.lower() == base_name:
        return base_name + "_"
    return base_name.lower()


# This function is only needed to minimize the diff with the handwritten code.
# In fact is is sufficient to hardcode a constant string (e.g. "agent") into the template.
def agent_variable_name(agent):
    if re.match("DOM", agent):
        return re.sub("DOM", "dom", agent)
    if agent.upper() == agent:
        return agent.lower()
    return agent[0].lower() + agent[1:]


def agent_class_name(agent):
    custom_agent_names = ["Inspector", "PageDebugger", "PageRuntime", "WorkerRuntime"]
    if agent in custom_agent_names:
        return "%sAgent" % agent
    return "Inspector%sAgent" % agent


def agent_getter_name(agent):
    name = agent_class_name(agent)
    return name[0].lower() + name[1:]


def generate(input_path, output_h_dir, output_cpp_dir):
    fin = open(input_path, "r")
    declarations = load_model_from_idl(fin.read())
    fin.close()

    impl_declarations = []
    inline_methods = []
    out_of_line_methods = []

    for declaration in declarations:
        param_string_public = ", ".join(map(Parameter.to_str_full, declaration.params))

        param_list_impl_parsed = declaration.params[:]

        accepts_cookie = (declaration.params[0].type == "const InspectorInstrumentationCookie&")
        if not accepts_cookie and not "Inline=Forward" in declaration.method_options:
            if not "Keep" in declaration.param_options:
                param_list_impl_parsed = param_list_impl_parsed[1:]
            param_list_impl_parsed = [Parameter("InstrumentingAgents*")] + param_list_impl_parsed

        generate_inline = not "Inline=Custom" in declaration.method_options
        if generate_inline:
            impl_declarations.append("%s %sImpl(%s);" % (
                declaration.return_type, declaration.name, ", ".join(map(Parameter.to_str_class, param_list_impl_parsed))))

        leading_impl_param_name = param_list_impl_parsed[0].name
        param_string_impl_full = ", ".join(map(Parameter.to_str_class_and_name, param_list_impl_parsed))

        param_list_impl_names_only = map(Parameter.to_str_name, param_list_impl_parsed)
        param_string_impl_names_only = ", ".join(param_list_impl_names_only)
        param_string_agent = ", ".join(param_list_impl_names_only[1:])

        def is_agent_name(name):
            return not "=" in name

        agents = filter(is_agent_name, declaration.method_options)

        if "Inline=FastReturn" in declaration.method_options or "Inline=Forward" in declaration.method_options:
            fast_return = "\n    FAST_RETURN_IF_NO_FRONTENDS(%s());" % declaration.return_type
        else:
            fast_return = ""

        if accepts_cookie:
            if generate_inline:
                inline_methods.append(
                    template_inline_accepts_cookie.substitute(
                        None,
                        name=declaration.name,
                        fast_return=fast_return,
                        params_public=param_string_public,
                        params_impl=param_string_impl_names_only,
                        cookie=leading_impl_param_name))
            if len(agents):
                agent_calls = []
                for agent in agents:
                    if agent == "Timeline":
                        agent_fetch = "retrieveTimelineAgent(%s)" % leading_impl_param_name
                    else:
                        agent_fetch = "%s.instrumentingAgents()->%s()" % (leading_impl_param_name, agent_getter_name(agent))
                    agent_calls.append(
                        template_agent_call.substitute(
                            None,
                            name=declaration.name,
                            agent_fetch=agent_fetch,
                            params_agent=param_string_agent,
                            agent_class=agent_class_name(agent),
                            agent=agent_variable_name(agent)))
                out_of_line_methods.append(
                    template_outofline.substitute(
                        None,
                        name=declaration.name,
                        params_impl=param_string_impl_full,
                        agent_calls="".join(agent_calls)))
        else:
            leading_public_param = declaration.params[0]
            selector_class = re.match("(\w*)", leading_public_param.type).group(1)
            agents_getter = "instrumentingAgentsFor%s(%s)" % (selector_class, leading_public_param.name)
            if declaration.return_type == "void":
                if generate_inline:
                    if "Inline=Forward" in declaration.method_options:
                        inline_methods.append(
                            template_inline_forward.substitute(
                                None,
                                name=declaration.name,
                                fast_return=fast_return,
                                params_public=param_string_public,
                                params_impl=param_string_impl_names_only))
                    else:
                        inline_methods.append(
                            template_inline.substitute(
                                None,
                                name=declaration.name,
                                fast_return=fast_return,
                                params_public=param_string_public,
                                params_impl=param_string_impl_names_only,
                                agents_getter=agents_getter))
                if len(agents):
                    agent_calls = []
                    for agent in agents:
                        agent_fetch = "%s->%s()" % (leading_impl_param_name, agent_getter_name(agent))
                        agent_call = template_agent_call.substitute(
                            None,
                            name=declaration.name,
                            agent_fetch=agent_fetch,
                            params_agent=param_string_agent,
                            agent_class=agent_class_name(agent),
                            agent=agent_variable_name(agent))
                        agent_calls.append(agent_call)
                    out_of_line_methods.append(
                        template_outofline.substitute(
                            None,
                            name=declaration.name,
                            params_impl=param_string_impl_full,
                            agent_calls="".join(agent_calls)))
            elif declaration.return_type == "InspectorInstrumentationCookie":
                if generate_inline:
                    inline_methods.append(
                        template_inline_returns_cookie.substitute(
                            None,
                            name=declaration.name,
                            fast_return=fast_return,
                            params_public=param_string_public,
                            params_impl=param_string_impl_names_only,
                            agents_getter=agents_getter))

                if len(agents):
                    timeline_agent_id = "0"
                    agent_calls = []
                    for agent in agents:
                        if agent == "Timeline":
                            agent_call = template_timeline_agent_call.substitute(
                                None,
                                name=declaration.name,
                                params_agent=param_string_agent)
                            timeline_agent_id = "timelineAgentId"
                        else:
                            agent_fetch = "%s->%s()" % (leading_impl_param_name, agent_getter_name(agent))
                            agent_call = template_agent_call.substitute(
                                None,
                                name=declaration.name,
                                agent_fetch=agent_fetch,
                                params_agent=param_string_agent,
                                agent_class=agent_class_name(agent),
                                agent=agent_variable_name(agent))
                        agent_calls.append(agent_call)

                    out_of_line_methods.append(
                        template_outofline_returns_cookie.substitute(
                            None,
                            return_type=declaration.return_type,
                            name=declaration.name,
                            params_impl=param_string_impl_full,
                            agent_calls="".join(agent_calls),
                            timeline_agent_id=timeline_agent_id))
            else:
                sys.stderr.write("Unsupported return type %s" % declaration.return_type)
                sys.exit(1)

    fout = open(output_h_dir + "/InspectorInstrumentationInl.h", "w")
    fout.write(template_h.substitute(None,
                                     impl_declarations="\n".join(impl_declarations),
                                     inline_methods="".join(inline_methods)))
    fout.close()

    fout = open(output_cpp_dir + "/InspectorInstrumentationImpl.cpp", "w")
    fout.write(template_cpp.substitute(None,
                                       out_of_line_methods="\n".join(out_of_line_methods)))
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
