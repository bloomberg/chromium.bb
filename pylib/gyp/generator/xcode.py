#!/usr/bin/python

import filecmp
import gyp.common
import gyp.xcodeproj_file
import errno
import os
import re
import tempfile


generator_default_variables = {
  'EXECUTABLE_PREFIX': '',
  'EXECUTABLE_SUFFIX': '',
  # INTERMEDIATE_DIR is a place for targets to build up intermediate products.
  # It is specific to each build environment.  It is only guaranteed to exist
  # and be constant within the context of a target.  Some build environments
  # may allow their intermediate directory or equivalent to be shared on a
  # wider scale, but this is not guaranteed.
  'INTERMEDIATE_DIR': '$(DERIVED_FILE_DIR)',
  'OS': 'mac',
  'PRODUCT_DIR': '$(BUILT_PRODUCTS_DIR)',
}


class XcodeProject(object):
  def __init__(self, gyp_path, path):
    self.gyp_path = gyp_path
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

    if type != 'none':
      target = gyp.xcodeproj_file.PBXNativeTarget(
          {
            'buildConfigurationList': xccl,
            'name':                   name,
            'productType':            _types[type]
          },
          parent=self.project)
    else:
      target = gyp.xcodeproj_file.PBXAggregateTarget(
          {
            'buildConfigurationList': xccl,
            'name':                   name,
          },
          parent=self.project)
    self.project.AppendProperty('targets', target)
    return target

  def Finalize1(self, xcode_targets, build_file_dict):
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

    # Sort the targets based on how they appeared in the input.
    # TODO(mark): Like a lot of other things here, this assumes internal
    # knowledge of PBXProject - in this case, of its "targets" property.
    targets = []
    for target in build_file_dict['targets']:
      target_name = target['target_name']
      qualified_target = gyp.common.QualifiedTarget(self.gyp_path, target_name)
      xcode_target = xcode_targets[qualified_target]
      # Make sure that the target being added to the sorted list is already in
      # the unsorted list.
      assert xcode_target in self.project._properties['targets']
      targets.append(xcode_targets[qualified_target])

    # Make sure that the list of targets being replaced is the same length as
    # the one replacing it.
    assert len(self.project._properties['targets']) == len(targets)

    self.project._properties['targets'] = targets

    # Sort the groups nicely.  Do this after sorting the targets, because the
    # Products group is sorted based on the order of the targets.
    self.project.SortGroups()

    # Create an "All" target if there's more than one target in this project
    # file.  Put the "All" target it first so that people opening up the
    # project for the first time will build everything by default.
    if len(self.project._properties['targets']) > 1:
      xccl = gyp.xcodeproj_file.XCConfigurationList({'buildConfigurations': []})
      for configuration in configurations:
        xcbc = gyp.xcodeproj_file.XCBuildConfiguration({'name': configuration})
        xccl.AppendProperty('buildConfigurations', xcbc)
      xccl.SetProperty('defaultConfigurationName', configurations[0])

      all_target = gyp.xcodeproj_file.PBXAggregateTarget(
          {
            'buildConfigurationList': xccl,
            'name':                   'All',
          },
          parent=self.project)

      for target in self.project._properties['targets']:
        all_target.AddDependency(target)

      # TODO(mark): This is evil because it relies on internal knowledge of
      # PBXProject._properties.  It's important to get the "All" target first,
      # though.
      self.project._properties['targets'].insert(0, all_target)

  def Finalize2(self):
    # Finalize2 needs to happen in a separate step because the process of
    # updating references to other projects depends on the ordering of targets
    # within remote project files.  Finalize1 is responsible for sorting duty,
    # and once all project files are sorted, Finalize2 can come in and update
    # these references.

    # Update all references to other projects, to make sure that the lists of
    # remote products are complete.  Otherwise, Xcode will fill them in when
    # it opens the project file, which will result in unnecessary diffs.
    # TODO(mark): This is evil because it relies on internal knowledge of
    # PBXProject._other_pbxprojects.
    for other_pbxproject in self.project._other_pbxprojects.keys():
      self.project.AddOrGetProjectReference(other_pbxproject)

    self.project.SortRemoteProductReferences()

    # Give everything an ID.
    self.project_file.ComputeIDs()

  def Write(self):
    created_dir = False
    try:
      os.mkdir(self.path)
      created_dir = True
    except OSError, e:
      if e.errno != errno.EEXIST:
        raise

    # Write the project file to a temporary location first.  Xcode watches for
    # changes to the project file and presents a UI sheet offering to reload
    # the project when it does change.  However, in some cases, especially when
    # multiple projects are open or when Xcode is busy, things don't work so
    # seamlessly.  Sometimes, Xcode is able to detect that a project file has
    # changed but can't unload it because something else is referencing it.
    # To mitigate this problem, and to avoid even having Xcode present the UI
    # sheet when an open project is rewritten for inconsequential changes, the
    # project file is written to a temporary file in the xcodeproj directory
    # first.  The new temporary file is then compared to the existing project
    # file, if any.  If they differ, the new file replaces the old; otherwise,
    # the new project file is simply deleted.  Xcode properly detects a file
    # being renamed over an open project file as a change and so it remains
    # able to present the "project file changed" sheet under this system.
    # Writing to a temporary file first also avoids the possible problem of
    # Xcode rereading an incomplete project file.
    (output_fd, new_pbxproj_path) = \
        tempfile.mkstemp(suffix='.tmp', prefix='project.pbxproj.gyp.',
                         dir=self.path)

    try:
      output_file = os.fdopen(output_fd, 'w')

      self.project_file.Print(output_file)
      output_file.close()

      pbxproj_path = os.path.join(self.path, 'project.pbxproj')

      same = False
      try:
        same = filecmp.cmp(pbxproj_path, new_pbxproj_path, False)
      except OSError, e:
        if e.errno != errno.ENOENT:
          raise

      if same:
        # The new file is identical to the old one, just get rid of the new
        # one.
        os.unlink(new_pbxproj_path)
      else:
        # The new file is different from the old one, or there is no old one.
        # Rename the new file to the permanent name.
        #
        # tempfile.mkstemp uses an overly restrictive mode, resulting in a
        # file that can only be read by the owner, regardless of the umask.
        # There's no reason to not respect the umask here, which means that
        # an extra hoop is required to fetch it and reset the new file's mode.
        #
        # No way to get the umask without setting a new one?  Set a safe one
        # and then set it back to the old value.
        umask = os.umask(077)
        os.umask(umask)

        os.chmod(new_pbxproj_path, 0666 & ~umask)

        os.rename(new_pbxproj_path, pbxproj_path)

    except Exception:
      # Don't leave turds behind.  In fact, if this code was responsible for
      # creating the xcodeproj directory, get rid of that too.
      os.unlink(new_pbxproj_path)
      if created_dir:
        os.rmdir(self.path)
      raise


def GenerateOutput(target_list, target_dicts, data):
  xcode_projects = {}
  for build_file, build_file_dict in data.iteritems():
    if build_file[-4:] != '.gyp':
      continue
    build_file_stem = build_file[:-4]
    # TODO(mark): To keep gyp-generated xcodeproj bundles from colliding with
    # checked-in versions, temporarily put _gyp into the ones created here.
    xcode_projects[build_file] = \
        XcodeProject(build_file, build_file_stem + '_gyp.xcodeproj')

  xcode_targets = {}
  for qualified_target in target_list:
    [build_file, target] = \
        gyp.common.BuildFileAndTarget('', qualified_target)[0:2]
    spec = target_dicts[qualified_target]
    configuration_names = [spec['default_configuration']]
    for configuration_name in sorted(spec['configurations'].keys()):
      if configuration_name not in configuration_names:
        configuration_names.append(configuration_name)
    pbxp = xcode_projects[build_file].project
    xct = xcode_projects[build_file].AddTarget(target, spec['type'],
                                               configuration_names)
    xcode_targets[qualified_target] = xct

    prebuild_index = 0
    if 'actions' in spec:
      for action in spec['actions']:
        # Convert Xcode-type variable references to sh-compatible environment
        # variable references.  Be sure the script runs in exec, and that if
        # exec fails, the script exits signalling an error.
        script = "exec " + re.sub('\$\((.*?)\)', '${\\1}', action['action']) + \
                 "\nexit 1\n"
        ssbp = gyp.xcodeproj_file.PBXShellScriptBuildPhase({
              'inputPaths': action['inputs'],
              'name': 'Action "' + action['action_name'] + '"',
              'outputPaths': action['outputs'],
              'shellScript': script,
              'showEnvVarsInLog': 0,
            })

        # TODO(mark): this assumes too much knowledge of the internals of
        # xcodeproj_file; some of these smarts should move into xcodeproj_file
        # itself.
        xct._properties['buildPhases'].insert(prebuild_index, ssbp)
        prebuild_index = prebuild_index + 1

    rules_by_ext = {}
    if 'rules' in spec:
      for rule in spec['rules']:
        rules_by_ext[rule['extension']] = rule

    rules_in_target = {}

    for source in spec['sources']:
      # TODO(mark): Perhaps this can be made a little bit fancier.
      source_extensions = ['c', 'cc', 'cpp', 'm', 'mm', 's', 'y']
      basename = os.path.basename(source)
      dot = basename.rfind('.')
      added = False
      if dot != -1:
        extension = basename[dot + 1:]
        if extension in rules_by_ext:
          if not extension in rules_in_target:
            rule = rules_by_ext[extension]
            outputs = []
            for output in rule['outputs']:
              outputs.append(output.replace('*', '$(INPUT_FILE_BASE)'))
            # Convert Xcode-type variable references to sh-compatible
            # environment variable references.  Be sure the script runs in
            # exec, and that if exec fails, the script exits signalling an
            # error.
            action = 'exec ' + \
                re.sub('\$\((.*?)\)', '${\\1}', rule['action']). \
                replace('*', '${SCRIPT_INPUT_FILE}') + \
                '\nexit 1\n'
            pbxbr = gyp.xcodeproj_file.PBXBuildRule({
                  'compilerSpec': 'com.apple.compilers.proxy.script',
                  'filePatterns': '*.' + rule['extension'],
                  'fileType':     'pattern.proxy',
                  'outputFiles':  outputs,
                  'script':       action,
                })
            xct.AppendProperty('buildRules', pbxbr)
          outputs = []
          for output in rule['outputs']:
            # Make sure all concrete rule outputs are added to the source
            # group of the project file.
            output_path = output.replace('*', basename[:dot])
            pbxp.SourceGroup().AddOrGetFileByPath(output_path, True)
        if extension in rules_by_ext or extension in source_extensions:
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

    # So can "inputs" and "outputs" sections of "actions" groups.
    if 'actions' in spec:
      for action in spec['actions']:
        groups = ['inputs', 'inputs_excluded', 'outputs', 'outputs_excluded']
        for group in groups:
          if not group in action:
            continue
          for item in action[group]:
            if item.startswith('$(BUILT_PRODUCTS_DIR)/'):
              # Exclude anything in BUILT_PRODUCTS_DIR.  They're products, not
              # sources.
              continue
            pbxp.SourceGroup().AddOrGetFileByPath(item, True)

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

    for configuration_name in configuration_names:
      configuration = spec['configurations'][configuration_name]
      xcbc = xct.ConfigurationNamed(configuration_name)
      if 'xcode_framework_dirs' in configuration:
        for include_dir in configuration['xcode_framework_dirs']:
          xcbc.AppendBuildSetting('FRAMEWORK_SEARCH_PATHS', include_dir)
      if 'include_dirs' in configuration:
        for include_dir in configuration['include_dirs']:
          xcbc.AppendBuildSetting('HEADER_SEARCH_PATHS', include_dir)
      if 'defines' in configuration:
        for define in configuration['defines']:
          if isinstance(define, str):
            xcbc.AppendBuildSetting('GCC_PREPROCESSOR_DEFINITIONS', define)
          elif isinstance(define, list):
            xcbc.AppendBuildSetting('GCC_PREPROCESSOR_DEFINITIONS',
                                    define[0] + '=' + str(define[1]))
      if 'xcode_settings' in configuration:
        for xck, xcv in configuration['xcode_settings'].iteritems():
          xcbc.SetBuildSetting(xck, xcv)

  build_files = []
  for build_file, build_file_dict in data.iteritems():
    if build_file.endswith('.gyp'):
      build_files.append(build_file)

  for build_file in build_files:
    xcode_projects[build_file].Finalize1(xcode_targets, data[build_file])

  for build_file in build_files:
    xcode_projects[build_file].Finalize2()

  for build_file in build_files:
    xcode_projects[build_file].Write()
