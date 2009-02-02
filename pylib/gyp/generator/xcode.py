#!/usr/bin/python

import gyp.common
import gyp.xcodeproj_file
import errno
import os


generator_default_variables = {
  'OS': 'mac',
}


class XcodeProject(object):
  def __init__(self, path):
    self.path = path
    self.project = gyp.xcodeproj_file.PBXProject(path=path)
    self.project_file = \
        gyp.xcodeproj_file.XCProjectFile({'rootObject': self.project})

  def AddTarget(self, name, type):
    _types = { 'static_library': 'com.apple.product-type.library.static',
               'executable':     'com.apple.product-type.tool',
             }
    target = gyp.xcodeproj_file.PBXNativeTarget({'name':        name,
                                                 'productType': _types[type]},
                                parent=self.project)
    self.project.AppendProperty('targets', target)
    return target

  def Finalize(self):
    self.project_file.ComputeIDs()

  def Write(self):
    pbxproj_path = self.path + '/project.pbxproj'

    try:
      os.mkdir(self.path)
    except OSError, e:
      if e.errno != errno.EEXIST:
        raise

    output_file = open(pbxproj_path, 'w')
    self.project_file.Print(output_file)
    output_file.close()


def GenerateOutput(target_list, target_dicts, data):
  xcode_projects = {}
  for build_file, build_file_dict in data.iteritems():
    if build_file[-4:] != '.gyp':
      # TODO(mark): Pick an exception class
      raise 'Build file name must end in .gyp'
    build_file_stem = build_file[:-4]
    # TODO(mark): To keep gyp-generated xcodeproj bundles from colliding with
    # checked-in versions, temporarily put _gyp into the ones created here.
    xcode_projects[build_file] = XcodeProject(build_file_stem +
                                              '_gyp.xcodeproj')

  xcode_targets = {}
  for qualified_target in target_list:
    [build_file, target] = \
        gyp.common.BuildFileAndTarget('', qualified_target)[0:2]
    spec = target_dicts[qualified_target]
    xcode_targets[qualified_target] = \
        xcode_projects[build_file].AddTarget(target, spec['type'])
    # TODO(mark): I hate the header map.  Even so, this must not be hard-coded
    # here.  It should be pushed into the configuration from some input file
    # somewhere, or it should be a part of an xcconfig.
    xcode_targets[qualified_target].SetBuildSetting('USE_HEADERMAP', 'NO')
    for source in spec['sources']:
      # TODO(mark): Only add files with known extensions to the sources phase.
      # This will be made fancier.
      source_extensions = ['c', 'cc', 'cpp', 'm', 'mm', 's']
      basename = os.path.basename(source)
      dot = basename.rfind('.')
      if dot != -1:
        extension = basename[dot + 1:]
        if extension in source_extensions:
          xcode_targets[qualified_target].SourcesPhase().AddFile(source)
    if 'xcode_framework_dirs' in spec:
      for include_dir in spec['xcode_framework_dirs']:
        xcode_targets[qualified_target].AppendBuildSetting( \
            'FRAMEWORK_SEARCH_PATHS', include_dir)
    if 'include_dirs' in spec:
      for include_dir in spec['include_dirs']:
        xcode_targets[qualified_target].AppendBuildSetting( \
            'HEADER_SEARCH_PATHS', include_dir)
    if 'defines' in spec:
      for define in spec['defines']:
        xcode_targets[qualified_target].AppendBuildSetting( \
            'GCC_PREPROCESSOR_DEFINITIONS', define)

    # Add dependencies before libraries, because adding a dependency may imply
    # adding a library.  It's preferable to keep dependencies listed first
    # during a link phase so that they can override symbols that would
    # otherwise be provided by libraries, which will usually include system
    # libraries.  On some systems, ld is finicky and even requires the
    # libraries to be ordered in such a way that unresolved symbols in
    # earlier-listed libraries may only be resolved by later-listed libraries.
    # The Mac linker doesn't work that way, but other platforms do, and so
    # their linker invocations need to be constructed in this way.  There's
    # no compelling reason for Xcode's linker invocations to differ.

    if 'dependencies' in spec:
      for dependency in spec['dependencies']:
        dependency = gyp.common.QualifiedTarget(build_file, dependency)
        xcode_targets[qualified_target].AddDependency(xcode_targets[dependency])

    if 'libraries' in spec:
      for library in spec['libraries']:
        xcode_targets[qualified_target].FrameworksPhase().AddFile(library)

  for build_file, build_file_dict in data.iteritems():
    xcode_projects[build_file].Finalize()

  for build_file, build_file_dict in data.iteritems():
    xcode_projects[build_file].Write()
