#!/usr/bin/python

import gyp
import gyp.xcodeproj_file as xf
import errno
import os


class XcodeProject(object):
  def __init__(self, path):
    self.path = path
    self.project = xf.PBXProject(path=path)
    self.project_file = xf.XCProjectFile({'rootObject': self.project})

  def AddTarget(self, name, type):
    _types = { 'static_library': 'com.apple.product-type.library.static',
               'executable':     'com.apple.product-type.tool',
             }
    target = xf.PBXNativeTarget({'name': name, 'productType': _types[type]},
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


def GenerateOutput(targets, data):
  xcode_projects = {}
  for build_file, build_file_dict in data.iteritems():
    if build_file[-4:] != '.gyp':
      # TODO(mark): Pick an exception class
      raise 'Build file name must end in .gyp'
    build_file_stem = build_file[:-4]
    xcode_projects[build_file] = XcodeProject(build_file_stem + '.xcodeproj')

  xcode_targets = {}
  for qualified_target in targets:
    [build_file, target] = gyp.BuildFileAndTarget('', qualified_target)[0:2]
    spec = data[build_file]['targets'][target]
    xcode_targets[qualified_target] = \
        xcode_projects[build_file].AddTarget(target, spec['type'])
    # TODO(mark): This needs to go into a .build file.
    xcode_targets[qualified_target].SetBuildSetting('USE_HEADERMAP', 'NO')
    for source in spec['sources']:
      xcode_targets[qualified_target].SourcesPhase().AddFile(source)
    if 'computed_libraries' in spec:
      for library in spec['computed_libraries']:
        xcode_targets[qualified_target].FrameworksPhase().AddFile(library)
    if 'include_dirs' in spec:
      for include_dir in spec['include_dirs']:
        xcode_targets[qualified_target].AppendBuildSetting( \
            'HEADER_SEARCH_PATHS', include_dir)
    if 'defines' in spec:
      for define in spec['defines']:
        xcode_targets[qualified_target].AppendBuildSetting( \
            'GCC_PREPROCESSOR_DEFINITIONS', define)
    if 'computed_dependencies' in spec:
      for dependency in spec['computed_dependencies']:
        dependency = gyp.QualifiedTarget(build_file, dependency)
        xcode_targets[qualified_target].AddDependency(xcode_targets[dependency])

  for build_file, build_file_dict in data.iteritems():
    xcode_projects[build_file].Finalize()

  for build_file, build_file_dict in data.iteritems():
    xcode_projects[build_file].Write()
