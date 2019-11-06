#!/usr/bin/env python2
# Copyright 2019 The Dawn Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Module to create generators that render multiple Jinja2 templates for GN.

A helper module that can be used to create generator scripts (clients)
that expand one or more Jinja2 templates, without outputs usable from
GN and Ninja build-based systems. See generator_lib.gni as well.

Clients should create a Generator sub-class, then call run_generator()
with a proper derived class instance.

Clients specify a list of FileRender operations, each one of them will
output a file into a temporary output directory through Jinja2 expansion.
All temporary output files are then grouped and written to into a single JSON
file, that acts as a convenient single GN output target. Use extract_json.py
to extract the output files from the JSON tarball in another GN action.

--depfile can be used to specify an output Ninja dependency file for the
JSON tarball, to ensure it is regenerated any time one of its dependencies
changes.

Finally, --expected-output-files can be used to check the list of generated
output files.
"""

import argparse, json, os, re, sys
from collections import namedtuple

# A FileRender represents a single Jinja2 template render operation:
#
#   template: Jinja2 template name, relative to --template-dir path.
#
#   output: Output file path, relative to temporary output directory.
#
#   params_dicts: iterable of (name:string -> value:string) dictionaries.
#       All of them will be merged before being sent as Jinja2 template
#       expansion parameters.
#
# Example:
#   FileRender('api.c', 'src/project_api.c', [{'PROJECT_VERSION': '1.0.0'}])
#
FileRender = namedtuple('FileRender', ['template', 'output', 'params_dicts'])

# The interface that must be implemented by generators.
class Generator:
    def get_description(self):
        """Return generator description for --help."""
        return ""

    def add_commandline_arguments(self, parser):
        """Add generator-specific argparse arguments."""
        pass

    def get_file_renders(self, args):
        """Return the list of FileRender objects to process."""
        return []

    def get_dependencies(self, args):
        """Return a list of extra input dependencies."""
        return []

# Allow custom Jinja2 installation path through an additional python
# path from the arguments if present. This isn't done through the regular
# argparse because PreprocessingLoader uses jinja2 in the global scope before
# "main" gets to run.
#
# NOTE: If this argument appears several times, this only uses the first
#       value, while argparse would typically keep the last one!
kJinja2Path = '--jinja2-path'
jinja2_path_argv_index = sys.argv.index(kJinja2Path)
if jinja2_path_argv_index >= 0:
    # Add parent path for the import to succeed.
    path = os.path.join(sys.argv[jinja2_path_argv_index + 1], os.pardir)
    sys.path.insert(1, path)

import jinja2

# A custom Jinja2 template loader that removes the extra indentation
# of the template blocks so that the output is correctly indented
class _PreprocessingLoader(jinja2.BaseLoader):
    def __init__(self, path):
        self.path = path

    def get_source(self, environment, template):
        path = os.path.join(self.path, template)
        if not os.path.exists(path):
            raise jinja2.TemplateNotFound(template)
        mtime = os.path.getmtime(path)
        with open(path) as f:
            source = self.preprocess(f.read())
        return source, path, lambda: mtime == os.path.getmtime(path)

    blockstart = re.compile('{%-?\s*(if|elif|else|for|block|macro)[^}]*%}')
    blockend = re.compile('{%-?\s*(end(if|for|block|macro)|elif|else)[^}]*%}')

    def preprocess(self, source):
        lines = source.split('\n')

        # Compute the current indentation level of the template blocks and remove their indentation
        result = []
        indentation_level = 0

        for line in lines:
            # The capture in the regex adds one element per block start or end so we divide by two
            # there is also an extra line chunk corresponding to the line end, so we substract it.
            numends = (len(self.blockend.split(line)) - 1) // 2
            indentation_level -= numends

            result.append(self.remove_indentation(line, indentation_level))

            numstarts = (len(self.blockstart.split(line)) - 1) // 2
            indentation_level += numstarts

        return '\n'.join(result) + '\n'

    def remove_indentation(self, line, n):
        for _ in range(n):
            if line.startswith(' '):
                line = line[4:]
            elif line.startswith('\t'):
                line = line[1:]
            else:
                assert(line.strip() == '')
        return line

_FileOutput = namedtuple('FileOutput', ['name', 'content'])

def _do_renders(renders, template_dir):
    loader = _PreprocessingLoader(template_dir)
    env = jinja2.Environment(loader=loader, lstrip_blocks=True, trim_blocks=True, line_comment_prefix='//*')

    def do_assert(expr):
        assert expr
        return ''

    def debug(text):
        print(text)

    base_params = {
        'enumerate': enumerate,
        'format': format,
        'len': len,
        'debug': debug,
        'assert': do_assert,
    }

    outputs = []
    for render in renders:
        params = {}
        params.update(base_params)
        for param_dict in render.params_dicts:
            params.update(param_dict)
        content = env.get_template(render.template).render(**params)
        outputs.append(_FileOutput(render.output, content))

    return outputs

# Compute the list of imported, non-system Python modules.
# It assumes that any path outside of the root directory is system.
def _compute_python_dependencies(root_dir = None):
    if not root_dir:
        # Assume this script is under generator/ by default.
        root_dir = os.path.join(os.path.dirname(__file__), os.pardir)
    root_dir = os.path.abspath(root_dir)

    module_paths = (module.__file__ for module in sys.modules.values()
                                    if module and hasattr(module, '__file__'))

    paths = set()
    for path in module_paths:
        path = os.path.abspath(path)

        if not path.startswith(root_dir):
            continue

        if (path.endswith('.pyc')
                or (path.endswith('c') and not os.path.splitext(path)[1])):
            path = path[:-1]

        paths.add(path)

    return paths

def run_generator(generator):
    parser = argparse.ArgumentParser(
        description = generator.get_description(),
        formatter_class = argparse.ArgumentDefaultsHelpFormatter
    )

    generator.add_commandline_arguments(parser);
    parser.add_argument('-t', '--template-dir', default='templates', type=str, help='Directory with template files.')
    parser.add_argument(kJinja2Path, default=None, type=str, help='Additional python path to set before loading Jinja2')
    parser.add_argument('--output-json-tarball', default=None, type=str, help='Name of the "JSON tarball" to create (tar is too annoying to use in python).')
    parser.add_argument('--depfile', default=None, type=str, help='Name of the Ninja depfile to create for the JSON tarball')
    parser.add_argument('--expected-outputs-file', default=None, type=str, help="File to compare outputs with and fail if it doesn't match")
    parser.add_argument('--root-dir', default=None, type=str, help='Optional source root directory for Python dependency computations')

    args = parser.parse_args()

    renders = generator.get_file_renders(args);

    # The caller wants to assert that the outputs are what it expects.
    # Load the file and compare with our renders.
    if args.expected_outputs_file != None:
        with open(args.expected_outputs_file) as f:
            expected = set([line.strip() for line in f.readlines()])

        actual = {render.output for render in renders}

        if actual != expected:
            print("Wrong expected outputs, caller expected:\n    " + repr(sorted(expected)))
            print("Actual output:\n    " + repr(sorted(actual)))
            return 1

    outputs = _do_renders(renders, args.template_dir)

    # Output the tarball and its depfile
    if args.output_json_tarball != None:
        json_root = {}
        for output in outputs:
            json_root[output.name] = output.content

        with open(args.output_json_tarball, 'w') as f:
            f.write(json.dumps(json_root))

    # Output a list of all dependencies for the tarball for Ninja.
    if args.depfile != None:
        dependencies = generator.get_dependencies(args)
        dependencies += [args.template_dir + os.path.sep + render.template for render in renders]
        dependencies += _compute_python_dependencies(args.root_dir)

        with open(args.depfile, 'w') as f:
            f.write(args.output_json_tarball + ": " + " ".join(dependencies))
