#!/usr/bin/python2.4
# Copyright 2009, Google Inc.
# All rights reserved.
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


"""Docbuilder for O3D and o3djs."""


import os
import os.path
import sys
import imp
import types
import glob
import subprocess
import shutil
import re


_java_exe = ''
_output_dir = ''
_third_party_dir = ''
_o3d_third_party_dir = ''
_script_path = os.path.dirname(os.path.realpath(__file__))
_js_copyright = """
/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
"""

GlobalsDict = { }


def MakePath(*file_paths):
  """Makes a path absolute given a path relative to this script."""
  return os.path.join(_script_path, *file_paths)


def MakeCommandName(name):
  """adds '.exe' if on Windows"""
  if os.name == 'nt':
    return name + '.exe'
  return name


def Execute(args):
  """Executes an external program."""
  # Comment the next line in for debugging.
  # print "Execute: ", ' '.join(args)
  if subprocess.call(args) > 0:
    raise RuntimeError('FAILED: ' + ' '.join(args))


def AppendBasePath(folder, filenames):
  """Appends a base path to a ist of files"""
  return [os.path.join(folder, filename) for filename in filenames]


def RunNixysa(idl_files, generate, output_dir, nixysa_options):
  """Executes Nixysa."""
  Execute([
    sys.executable,
    MakePath(_o3d_third_party_dir, 'nixysa', 'codegen.py'),
    '--binding-module=o3d:%s' % MakePath('..', 'plugin', 'o3d_binding.py'),
    '--generate=' + generate,
    '--force',
    '--output-dir=' + output_dir] +
    nixysa_options +
    idl_files)


def RunJSDocToolkit(js_files, ezt_output_dir, html_output_dir, prefix, mode,
                    baseURL, topURL, exports_file):
  """Executes the JSDocToolkit."""
  list_filename = MakePath(_output_dir, 'doclist.conf')
  f = open(list_filename, 'w')
  f.write('{\nD:{\n')
  f.write('prefix: "%s",\n' % prefix)
  f.write('baseURL: "%s",\n' % baseURL)
  f.write('topURL: "%s",\n' % topURL)
  f.write('mode: "%s",\n' % mode)
  f.write('htmlOutDir: "%s",\n' % html_output_dir.replace('\\', '/'))
  f.write('exportsFile: "%s",\n' % exports_file.replace('\\', '/'))
  f.write('endMarker: ""\n')
  f.write('},\n')
  f.write('_: [\n')
  for filename in js_files:
    f.write('"%s",\n' % filename.replace('\\', '/'))
  f.write(']\n}\n')
  f.close()

  files_dir = MakePath(_third_party_dir, 'jsdoctoolkit', 'files')
  Execute([
    _java_exe,
    '-Djsdoc.dir=%s' % files_dir,
    '-jar',
    MakePath(files_dir, 'jsrun.jar'),
    MakePath(files_dir, 'app', 'run.js'),
    '-v',
    '-t=%s' % MakePath('jsdoc-toolkit-templates'),
    '-d=' + ezt_output_dir,
    '-c=' + list_filename])


def DeleteOldDocs(docs_js_outpath):
  try:
    shutil.rmtree(docs_js_outpath);
  except:
    pass

def BuildJavaScriptForDocsFromIDLs(idl_files, output_dir):
  RunNixysa(idl_files, 'jsheader', output_dir, ['--properties-equal-undefined'])


def BuildJavaScriptForExternsFromIDLs(idl_files, output_dir):
  if (os.path.exists(output_dir)):
    for filename in glob.glob(os.path.join(output_dir, '*.js')):
      os.unlink(filename)
  RunNixysa(idl_files, 'jsheader', output_dir, ['--no-return-docs'])


def BuildO3DDocsFromJavaScript(js_files, ezt_output_dir, html_output_dir):
  RunJSDocToolkit(js_files, ezt_output_dir, html_output_dir,
                  'classo3d_1_1_', 'o3d', '', '', '')


def BuildO3DClassHierarchy(html_output_dir):
  # TODO(gman): We need to make mutliple graphs. One for Params, one for
  #     ParamMatrix4, one for RenderNode, one for everythng else.
  dot_path = MakePath(_third_party_dir, 'graphviz', 'files', 'bin',
                      MakeCommandName('dot'))
  if os.path.exists(dot_path):
    Execute([
      dot_path,
      '-Tcmapx', '-o' + MakePath(html_output_dir, 'class_hierarchy.map'),
      '-Tpng', '-o' + MakePath(html_output_dir, 'class_hierarchy.png'),
      MakePath(html_output_dir, 'class_hierarchy.dot')])


def BuildO3DJSDocs(js_files, ezt_output_dir, html_output_dir, exports_file):
  # The backslashes below on 'jsdocs/' and '../' must stay.
  RunJSDocToolkit(js_files, ezt_output_dir, html_output_dir, 'js_1_0_', 'o3djs',
                  'jsdocs/', '../', exports_file)


def BuildO3DExternsFile(js_files_dir, extra_externs_file, externs_file):
  outfile = open(externs_file, 'w')
  filenames = (glob.glob(os.path.join(js_files_dir, '*.js')) +
               [extra_externs_file])
  for filename in filenames:
    print "-----", filename
    infile = open(filename, 'r')
    lines = infile.readlines()
    infile.close()
    filtered = []
    skipping = False
    # strip out @o3dparameter stuff
    for line in lines:
      if skipping:
        if line.startswith(' * @') or line.startswith(' */'):
          skipping = False
      if not skipping:
        if line.startswith(' * @o3dparameter'):
          skipping = True
      if not skipping:
        filtered.append(line)
    outfile.write(''.join(filtered))
  outfile.close()


def BuildCompiledO3DJS(o3djs_files,
                       externs_path,
                       o3d_externs_js_path,
                       compiled_o3djs_outpath):
  Execute([
    _java_exe,
    '-jar',
    MakePath('..', '..', 'o3d-internal', 'jscomp', 'JSCompiler_deploy.jar'),
    '--property_renaming', 'OFF',
    '--variable_renaming', 'LOCAL',
    '--strict',
    '--externs=%s' % externs_path,
    ('--externs=%s' % o3d_externs_js_path),
    ('--js_output_file=%s' % compiled_o3djs_outpath)] +
    ['-js=%s' % (x, ) for x in o3djs_files]);

  # strip out goog.exportSymbol and move o3djs.require to end
  file = open(compiled_o3djs_outpath, 'r')
  contents = file.read()
  file.close()
  contents = re.sub(r'goog.exportSymbol\([^\)]*\);', '', contents)
  requires = set(re.findall(r'o3djs.require\([^\)]*\);', contents))
  contents = re.sub(r'o3djs.require\([^\)]*\);', '', contents)
  file = open(compiled_o3djs_outpath, 'w')
  file.write(_js_copyright)
  file.write(contents)
  file.write('\n')
  file.write('\n'.join(requires))
  file.close()


def CopyStaticFiles(o3d_docs_ezt_outpath, o3d_docs_html_outpath):
  files = ['stylesheet.css',
           'prettify.css',
           'prettify.js',
           'tabs.css',
           'tab_l.gif',
           'tab_r.gif',
           'tab_b.gif']
  for file in files:
    shutil.copyfile(MakePath('jsdoc-toolkit-templates', 'static', file),
                    MakePath(os.path.join(o3d_docs_ezt_outpath, file)))
    shutil.copyfile(MakePath('jsdoc-toolkit-templates', 'static', file),
                    MakePath(os.path.join(o3d_docs_html_outpath, file)))


def main(argv):
  """Builds the O3D API docs and externs and the o3djs docs."""
  global _java_exe
  _java_exe = argv[0]
  global _third_party_dir
  _third_party_dir = argv[1]
  global _o3d_third_party_dir
  _o3d_third_party_dir = os.path.normpath(
    os.path.join(os.path.dirname(__file__), '..', 'third_party'))

  # Fix up the python path of subprocesses by setting PYTHONPATH.
  pythonpath = os.pathsep.join([MakePath(_o3d_third_party_dir, 'gflags', 'python'),
                                MakePath(_o3d_third_party_dir, 'ply')])

  orig_pythonpath = os.environ.get('PYTHONPATH')
  if orig_pythonpath:
    pythonpath = os.pathsep.join([pythonpath, orig_pythonpath])

  os.environ['PYTHONPATH'] = pythonpath

  js_list_filename = MakePath('..', 'samples', 'o3djs', 'js_list.manifest')
  idl_list_filename = MakePath('..', 'plugin', 'idl_list.manifest')
  js_list_basepath = os.path.dirname(js_list_filename)
  idl_list_basepath = os.path.dirname(idl_list_filename)

  global _output_dir
  _output_dir = argv[2]
  docs_outpath = os.path.join(_output_dir, 'documentation')
  docs_js_outpath = MakePath(docs_outpath, 'apijs')
  externs_js_outpath = MakePath(_output_dir, 'externs')
  o3d_docs_ezt_outpath = MakePath(docs_outpath, 'reference')
  o3d_docs_html_outpath = MakePath(docs_outpath, 'local_html')
  o3djs_docs_ezt_outpath = MakePath(docs_outpath, 'reference', 'jsdocs')
  o3djs_docs_html_outpath = MakePath(docs_outpath, 'local_html', 'jsdocs')
  o3d_externs_path = MakePath(_output_dir, 'o3d-externs.js')
  o3djs_exports_path = MakePath(_output_dir, 'o3d-exports.js')
  compiled_o3djs_outpath = MakePath(docs_outpath, 'base.js')
  externs_path = MakePath('externs', 'externs.js')
  o3d_extra_externs_path = MakePath('externs', 'o3d-extra-externs.js')

  js_list = eval(open(js_list_filename, "r").read())
  idl_list = eval(open(idl_list_filename, "r").read())

  idl_files = AppendBasePath(idl_list_basepath, idl_list)
  o3djs_files = AppendBasePath(js_list_basepath, js_list)

  # we need to put base.js first?
  o3djs_files = (
      filter(lambda x: x.endswith('base.js'), o3djs_files) +
      filter(lambda x: not x.endswith('base.js'), o3djs_files))

  docs_js_files = [os.path.join(
                       docs_js_outpath,
                       os.path.splitext(os.path.basename(f))[0] + '.js')
                   for f in idl_list]

  DeleteOldDocs(MakePath(docs_outpath))
  BuildJavaScriptForDocsFromIDLs(idl_files, docs_js_outpath)
  BuildO3DDocsFromJavaScript([o3d_extra_externs_path] + docs_js_files,
                             o3d_docs_ezt_outpath, o3d_docs_html_outpath)
  BuildO3DClassHierarchy(o3d_docs_html_outpath)
  BuildJavaScriptForExternsFromIDLs(idl_files, externs_js_outpath)
  BuildO3DExternsFile(externs_js_outpath,
                      o3d_extra_externs_path,
                      o3d_externs_path)
  BuildO3DJSDocs(o3djs_files + [o3d_externs_path], o3djs_docs_ezt_outpath,
                 o3djs_docs_html_outpath, o3djs_exports_path)
  CopyStaticFiles(o3d_docs_ezt_outpath, o3d_docs_html_outpath)
  BuildCompiledO3DJS(o3djs_files + [o3djs_exports_path],
                     externs_path,
                     o3d_externs_path,
                     compiled_o3djs_outpath)


if __name__ == '__main__':
  main(sys.argv[1:])
