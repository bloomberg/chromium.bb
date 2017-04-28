# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import ast
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
third_party_dir = os.path.normpath(os.path.join(module_path, os.pardir, os.pardir, os.pardir, os.pardir))
# jinja2 is in chromium's third_party directory.
# Insert at 1 so at front to override system libraries, and
# after path[0] == invoking script dir
sys.path.insert(1, third_party_dir)
import jinja2

from name_utilities import method_name

def _json5_loads(lines):
    # Use json5.loads when json5 is available. Currently we use simple
    # regexs to convert well-formed JSON5 to PYL format.
    # Strip away comments and quote unquoted keys.
    re_comment = re.compile(r"^\s*//.*$|//+ .*$", re.MULTILINE)
    re_map_keys = re.compile(r"^\s*([$A-Za-z_][\w]*)\s*:", re.MULTILINE)
    pyl = re.sub(re_map_keys, r"'\1':", re.sub(re_comment, "", lines))
    # Convert map values of true/false to Python version True/False.
    re_true = re.compile(r":\s*true\b")
    re_false = re.compile(r":\s*false\b")
    pyl = re.sub(re_true, ":True", re.sub(re_false, ":False", pyl))
    return ast.literal_eval(pyl)


def to_singular(text):
    return text[:-1] if text[-1] == "s" else text


def to_lower_case(name):
    return name[:1].lower() + name[1:]


def agent_config(agent_name, field):
    return config["observers"].get(agent_name, {}).get(field)


def agent_name_to_class(agent_name):
    return agent_config(agent_name, "class") or "Inspector%sAgent" % agent_name


def agent_name_to_include(agent_name):
    include_path = agent_config(agent_name, "include_path") or config["settings"]["include_path"]
    return os.path.join(include_path, agent_name_to_class(agent_name) + ".h")


def initialize_jinja_env(cache_dir):
    jinja_env = jinja2.Environment(
        loader=jinja2.FileSystemLoader(os.path.join(module_path, "templates")),
        # Bytecode cache is not concurrency-safe unless pre-cached:
        # if pre-cached this is read-only, but writing creates a race condition.
        bytecode_cache=jinja2.FileSystemBytecodeCache(cache_dir),
        keep_trailing_newline=True,  # newline-terminate generated files
        lstrip_blocks=True,  # so can indent control flow tags
        trim_blocks=True)
    jinja_env.filters.update({
        "to_lower_case": to_lower_case,
        "to_singular": to_singular,
        "agent_name_to_class": agent_name_to_class,
        "agent_name_to_include": agent_name_to_include})
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
        self.forward_declarations = []
        self.declarations = []
        for line in map(str.strip, source.split("\n")):
            line = re.sub(r"\s{2,}", " ", line).strip()  # Collapse whitespace
            if len(line) == 0:
                continue
            elif line.startswith("class ") or line.startswith("struct "):
                self.forward_declarations.append(line)
            else:
                self.declarations.append(Method(line))
        self.forward_declarations.sort()


class Method(object):
    def __init__(self, source):
        match = re.match(r"(?:(\w+\*?)\s+)?(\w+)\s*\((.*)\)\s*;", source)
        if not match:
            sys.stderr.write("Cannot parse %s\n" % source)
            sys.exit(1)

        self.name = match.group(2)
        self.is_scoped = not match.group(1)
        if not self.is_scoped and match.group(1) != "void":
            raise Exception("Instant probe must return void: %s" % self.name)

        # Splitting parameters by a comma, assuming that attribute lists contain no more than one attribute.
        self.params = map(Parameter, map(str.strip, match.group(3).split(",")))


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

        if self.type[-1] == "*" and "char" not in self.type:
            self.member_type = "Member<%s>" % self.type[:-1]
        else:
            self.member_type = self.type


def build_param_name(param_type):
    return "param" + re.match(r"(const |RefPtr<)?(\w*)", param_type).group(2)


def load_config(file_name):
    default_config = {
        "settings": {},
        "observers": {}
    }
    if not file_name:
        return default_config
    with open(file_name) as config_file:
        return _json5_loads(config_file.read()) or default_config


def build_observers():
    all_pidl_probes = set()
    for f in files:
        probes = set([probe.name for probe in f.declarations])
        if all_pidl_probes & probes:
            raise Exception("Multiple probe declarations: %s" % all_pidl_probes & probes)
        all_pidl_probes |= probes

    all_observers = set()
    observers_by_probe = {}
    unused_probes = set(all_pidl_probes)
    for observer_name in config["observers"]:
        all_observers.add(observer_name)
        observer = config["observers"][observer_name]
        for probe in observer["probes"]:
            unused_probes.discard(probe)
            if probe not in all_pidl_probes:
                raise Exception('Probe %s is not declared in PIDL file' % probe)
            observers_by_probe.setdefault(probe, set()).add(observer_name)
    if unused_probes:
        raise Exception("Unused probes: %s" % unused_probes)

    for f in files:
        for probe in f.declarations:
            probe.agents = observers_by_probe[probe.name]
    return all_observers


cmdline_parser = optparse.OptionParser()
cmdline_parser.add_option("--output_dir")
cmdline_parser.add_option("--config")

try:
    arg_options, arg_values = cmdline_parser.parse_args()
    if len(arg_values) != 1:
        raise Exception("Exactly one plain argument expected (found %s)" % len(arg_values))
    input_path = arg_values[0]
    output_dirpath = arg_options.output_dir
    if not output_dirpath:
        raise Exception("Output directory must be specified")
    config_file_name = arg_options.config
except Exception:
    # Work with python 2 and 3 http://docs.python.org/py3k/howto/pyporting.html
    exc = sys.exc_info()[1]
    sys.stderr.write("Failed to parse command-line arguments: %s\n\n" % exc)
    sys.stderr.write("Usage: <script> [options] <probes.pidl>\n")
    sys.stderr.write("Options:\n")
    sys.stderr.write("\t--config <config_file.json5>\n")
    sys.stderr.write("\t--output_dir <output_dir>\n")
    exit(1)

config = load_config(config_file_name)
jinja_env = initialize_jinja_env(output_dirpath)
base_name = os.path.splitext(os.path.basename(input_path))[0]

fin = open(input_path, "r")
files = load_model_from_idl(fin.read())
fin.close()

template_context = {
    "files": files,
    "agents": build_observers(),
    "config": config,
    "method_name": method_name,
    "name": base_name,
    "input_file": os.path.basename(input_path)
}
cpp_template = jinja_env.get_template("/InstrumentingProbesImpl.cpp.tmpl")
cpp_file = open(output_dirpath + "/" + base_name + "Impl.cpp", "w")
cpp_file.write(cpp_template.render(template_context))
cpp_file.close()

sink_h_template = jinja_env.get_template("/ProbeSink.h.tmpl")
sink_h_file = open(output_dirpath + "/" + to_singular(base_name) + "Sink.h", "w")
sink_h_file.write(sink_h_template.render(template_context))
sink_h_file.close()

for f in files:
    template_context["file"] = f
    h_template = jinja_env.get_template("/InstrumentingProbesInl.h.tmpl")
    h_file = open(output_dirpath + "/" + f.header_name + ".h", "w")
    h_file.write(h_template.render(template_context))
    h_file.close()
