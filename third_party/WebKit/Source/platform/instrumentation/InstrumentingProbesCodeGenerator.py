# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import os.path
import re
import sys

# Path handling for libraries and templates
# Paths have to be normalized because Jinja uses the exact template path to
# determine the hash used in the cache filename, and we need a pre-caching step
# to be concurrency-safe. Use absolute path because __file__ is absolute if
# module is imported, and relative if executed directly.
# If paths differ between pre-caching and individual file compilation, the cache
# is regenerated, which causes a race condition and breaks concurrent build,
# since some compile processes will try to read the partially written cache.
module_path, module_filename = os.path.split(os.path.realpath(__file__))
templates_dir = module_path
third_party_dir = os.path.normpath(os.path.join(
    module_path, os.pardir, os.pardir, os.pardir, os.pardir))
# jinja2 is in chromium's third_party directory.
# Insert at 1 so at front to override system libraries, and
# after path[0] == invoking script dir
sys.path.insert(1, third_party_dir)
import jinja2


def to_lower_case(name):
    return name[:1].lower() + name[1:]


def agent_name_to_class(agent_name):
    if agent_name == "Performance":
        return "PerformanceMonitor"
    elif agent_name == "TraceEvents":
        return "InspectorTraceEvents"
    elif agent_name == "PlatformTraceEvents":
        return "PlatformTraceEventsAgent"
    else:
        return "Inspector%sAgent" % agent_name


def initialize_jinja_env(cache_dir):
    jinja_env = jinja2.Environment(
        loader=jinja2.FileSystemLoader(templates_dir),
        # Bytecode cache is not concurrency-safe unless pre-cached:
        # if pre-cached this is read-only, but writing creates a race condition.
        bytecode_cache=jinja2.FileSystemBytecodeCache(cache_dir),
        keep_trailing_newline=True,  # newline-terminate generated files
        lstrip_blocks=True,  # so can indent control flow tags
        trim_blocks=True)
    jinja_env.filters.update({
        "to_lower_case": to_lower_case,
        "agent_name_to_class": agent_name_to_class})
    jinja_env.add_extension('jinja2.ext.loopcontrols')
    return jinja_env


def match_and_consume(pattern, source):
    match = re.match(pattern, source)
    if match:
        return match, source[len(match.group(0)):].strip()
    return None, source


def load_model_from_idl(source):
    source = re.sub(r"//.*", "", source)  # Remove line comments
    source = re.sub(r"/\*(.|\n)*?\*/", "", source, re.MULTILINE)  # Remove block comments
    source = re.sub(r"\]\s*?\n\s*", "] ", source)  # Merge the method annotation with the next line
    source = source.strip()
    model = []
    while len(source):
        match, source = match_and_consume(r"interface\s(\w*)\s?\{([^\{]*)\}", source)
        if not match:
            sys.stderr.write("Cannot parse %s\n" % source[:100])
            sys.exit(1)
        model.append(File(match.group(1), match.group(2)))
    return model


class File(object):
    def __init__(self, name, source):
        self.name = name
        self.header_name = self.name + "Inl"
        self.includes = [include_inspector_header(base_name)]
        self.forward_declarations = []
        self.declarations = []
        self.defines = []
        for line in map(str.strip, source.split("\n")):
            line = re.sub(r"\s{2,}", " ", line).strip()  # Collapse whitespace
            if len(line) == 0:
                continue
            if line.startswith("#define"):
                self.defines.append(line)
            elif line.startswith("#include"):
                self.includes.append(line)
            elif line.startswith("class ") or line.startswith("struct "):
                self.forward_declarations.append(line)
            else:
                self.declarations.append(Method(line))
        self.includes.sort()
        self.forward_declarations.sort()


def include_header(name):
    return "#include \"%s.h\"" % name


def include_inspector_header(name):
    if name == "PerformanceMonitor":
        return include_header("core/frame/" + name)
    if name == "PlatformInstrumentation":
        return include_header("platform/instrumentation/" + name)
    return include_header("core/inspector/" + name)


class Method(object):
    def __init__(self, source):
        match = re.match(r"(\[[\w|,|=|\s]*\])?\s?(\w*\*?) (\w*)\((.*)\)\s?;", source)
        if not match:
            sys.stderr.write("Cannot parse %s\n" % source)
            sys.exit(1)

        self.options = []
        if match.group(1):
            options_str = re.sub(r"\s", "", match.group(1)[1:-1])
            if len(options_str) != 0:
                self.options = options_str.split(",")

        self.return_type = match.group(2)
        self.name = match.group(3)
        self.is_scoped = self.return_type == ""

        # Splitting parameters by a comma, assuming that attribute lists contain no more than one attribute.
        self.params = map(Parameter, map(str.strip, match.group(4).split(",")))

        self.returns_value = self.return_type != "" and self.return_type != "void"
        if self.return_type == "bool":
            self.default_return_value = "false"
        elif self.returns_value:
            sys.stderr.write("Can only return bool: %s\n" % self.name)
            sys.exit(1)

        self.agents = [option for option in self.options if "=" not in option]

        if self.returns_value and len(self.agents) > 1:
            sys.stderr.write("Can only return value from a single agent: %s\n" % self.name)
            sys.exit(1)


class Parameter(object):
    def __init__(self, source):
        self.options = []
        match, source = match_and_consume(r"\[(\w*)\]", source)
        if match:
            self.options.append(match.group(1))

        parts = map(str.strip, source.split("="))
        self.default_value = parts[1] if len(parts) != 1 else None

        param_decl = parts[0]
        min_type_tokens = 2 if re.match("(const|unsigned long) ", param_decl) else 1

        if len(param_decl.split(" ")) > min_type_tokens:
            parts = param_decl.split(" ")
            self.type = " ".join(parts[:-1])
            self.name = parts[-1]
        else:
            self.type = param_decl
            self.name = build_param_name(self.type)

        self.value = self.name
        self.is_prp = re.match(r"PassRefPtr<", param_decl) is not None
        if self.is_prp:
            self.name = "prp" + self.name[0].upper() + self.name[1:]
            self.inner_type = re.match(r"PassRefPtr<(.+)>", param_decl).group(1)

        if self.type[-1] == "*" and "char" not in self.type:
            self.member_type = "Member<%s>" % self.type[:-1]
        else:
            self.member_type = self.type


def build_param_name(param_type):
    base_name = re.match(r"(const |PassRefPtr<)?(\w*)", param_type).group(2)
    return "param" + base_name


cmdline_parser = optparse.OptionParser()
cmdline_parser.add_option("--output_dir")
cmdline_parser.add_option("--template_dir")

try:
    arg_options, arg_values = cmdline_parser.parse_args()
    if len(arg_values) != 1:
        raise Exception("Exactly one plain argument expected (found %s)" % len(arg_values))
    input_path = arg_values[0]
    output_dirpath = arg_options.output_dir
    if not output_dirpath:
        raise Exception("Output directory must be specified")
except Exception:
    # Work with python 2 and 3 http://docs.python.org/py3k/howto/pyporting.html
    exc = sys.exc_info()[1]
    sys.stderr.write("Failed to parse command-line arguments: %s\n\n" % exc)
    sys.stderr.write("Usage: <script> --output_dir <output_dir> InstrumentingProbes.idl\n")
    exit(1)

jinja_env = initialize_jinja_env(output_dirpath)
all_agents = set()
all_defines = []
base_name = os.path.splitext(os.path.basename(input_path))[0]

fin = open(input_path, "r")
files = load_model_from_idl(fin.read())
fin.close()

for f in files:
    for declaration in f.declarations:
        for agent in declaration.agents:
            all_agents.add(agent)
    all_defines += f.defines

template_context = {
    "files": files,
    "agents": all_agents,
    "defines": all_defines,
    "name": base_name,
    "input_file": os.path.basename(input_path)
}
cpp_template = jinja_env.get_template("/InstrumentingProbesImpl_cpp.template")
cpp_file = open(output_dirpath + "/" + base_name + "Impl.cpp", "w")
cpp_file.write(cpp_template.render(template_context))
cpp_file.close()

agents_h_template = jinja_env.get_template("/InstrumentingAgents_h.template")
agents_h_file = open(output_dirpath + "/" + base_name + "Agents.h", "w")
agents_h_file.write(agents_h_template.render(template_context))
agents_h_file.close()

for f in files:
    template_context["file"] = f
    h_template = jinja_env.get_template("/InstrumentingProbesImpl_h.template")
    h_file = open(output_dirpath + "/" + f.header_name + ".h", "w")
    h_file.write(h_template.render(template_context))
    h_file.close()
