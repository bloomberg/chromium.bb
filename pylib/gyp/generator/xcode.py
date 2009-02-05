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

  def AddTarget(self, name, type, configurations):
    _types = {
      'shared_library': 'com.apple.product-type.library.dynamic',
      'static_library': 'com.apple.product-type.library.static',
      'executable':     'com.apple.product-type.tool',
    }

    # Set up the configurations for the target according to the list of names
    # supplied.
    xccl = gyp.xcodeproj_file.XCConfigurationList({'buildConfigurations': []})
    for configuration in configurations:
      xcbc = gyp.xcodeproj_file.XCBuildConfiguration({'name': configuration})
      xccl.AppendProperty('buildConfigurations', xcbc)
    xccl.SetProperty('defaultConfigurationName', configurations[0])

    target = gyp.xcodeproj_file.PBXNativeTarget(
        {
          'buildConfigurationList': xccl,
          'name':                   name,
          'productType':            _types[type]
        },
        parent=self.project)
    self.project.AppendProperty('targets', target)
    return target

  def Finalize(self):
    # Collect a list of all of the build configuration names used by the
    # various targets in the file.  It is very heavily advised to keep each
    # target in an entire project (even across multiple project files) using
    # the same set of configuration names.
    configurations = []
    for xct in self.project.GetProperty('targets'):
      xccl = xct.GetProperty('buildConfigurationList')
      xcbcs = xccl.GetProperty('buildConfigurations')
      for xcbc in xcbcs:
        name = xcbc.GetProperty('name')
        if name not in configurations:
          configurations.append(name)

    # Replace the XCConfigurationList attached to the PBXProject object with
    # a new one specifying all of the configuration names used by the various
    # targets.
    xccl = gyp.xcodeproj_file.XCConfigurationList({'buildConfigurations': []})
    for configuration in configurations:
      xcbc = gyp.xcodeproj_file.XCBuildConfiguration({'name': configuration})
      xccl.AppendProperty('buildConfigurations', xcbc)
    xccl.SetProperty('defaultConfigurationName', configurations[0])
    self.project.SetProperty('buildConfigurationList', xccl)

    # Sort the groups nicely.
    self.project.SortGroups()

    # TODO(mark): Sort the targets based on how they appeared in the input.

    # Give everything an ID.
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
    configuration_names = []
    for configuration in spec['configurations']:
      configuration_names.append(configuration['configuration_name'])
    pbxp = xcode_projects[build_file].project
    xct = xcode_projects[build_file].AddTarget(target, spec['type'],
                                               configuration_names)
    xcode_targets[qualified_target] = xct

    for source in spec['sources']:
      # TODO(mark): Perhaps this can be made a little bit fancier.
      source_extensions = ['c', 'cc', 'cpp', 'm', 'mm', 's']
      basename = os.path.basename(source)
      dot = basename.rfind('.')
      added = False
      if dot != -1:
        extension = basename[dot + 1:]
        if extension in source_extensions:
          xct.SourcesPhase().AddFile(source)
          added = True
      if not added:
        # Files that aren't added to a sources build phase can still go into
        # the project's source group.
        pbxp.SourceGroup().AddOrGetFileByPath(source, True)

    # Excluded files can also go into the project's source group.
    if 'sources_excluded' in spec:
      for source in spec['sources_excluded']:
        pbxp.SourceGroup().AddOrGetFileByPath(source, True)

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
        xct.AddDependency(xcode_targets[dependency])

    if 'libraries' in spec:
      for library in spec['libraries']:
        xct.FrameworksPhase().AddFile(library)

    for configuration in spec['configurations']:
      configuration_name = configuration['configuration_name']
      xcbc = xct.ConfigurationNamed(configuration_name)
      if 'xcode_framework_dirs' in configuration:
        for include_dir in configuration['xcode_framework_dirs']:
          xcbc.AppendBuildSetting('FRAMEWORK_SEARCH_PATHS', include_dir)
      if 'include_dirs' in configuration:
        for include_dir in configuration['include_dirs']:
          xcbc.AppendBuildSetting('HEADER_SEARCH_PATHS', include_dir)
      if 'defines' in configuration:
        for define in configuration['defines']:
          xcbc.AppendBuildSetting('GCC_PREPROCESSOR_DEFINITIONS', define)
      if 'xcode_settings' in configuration:
        for xck, xcv in configuration['xcode_settings'].iteritems():
          xcbc.SetBuildSetting(xck, xcv)

  for build_file, build_file_dict in data.iteritems():
    xcode_projects[build_file].Finalize()

  for build_file, build_file_dict in data.iteritems():
    xcode_projects[build_file].Write()
