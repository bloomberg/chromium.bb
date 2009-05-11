#!/usr/bin/python


import os
import re
import subprocess
import sys
import gyp.common
import gyp.MSVSNew as MSVSNew
import gyp.MSVSToolFile as MSVSToolFile
import gyp.MSVSProject as MSVSProject
import gyp.MSVSVersion as MSVSVersion


generator_default_variables = {
    'EXECUTABLE_PREFIX': '',
    'EXECUTABLE_SUFFIX': '.exe',
    'INTERMEDIATE_DIR': '$(IntDir)',
    'SHARED_INTERMEDIATE_DIR': '$(OutDir)/obj/global_intermediate',
    'OS': 'win',
    'PRODUCT_DIR': '$(OutDir)',
    'RULE_INPUT_ROOT': '$(InputName)',
    'RULE_INPUT_EXT': '$(InputExt)',
    'RULE_INPUT_NAME': '$(InputFileName)',
    'RULE_INPUT_PATH': '$(InputPath)',
}


def _FixPath(path):
  """Convert paths to a form that will make sense in a vcproj file.

  Arguments:
    path: The path to convert, may contain / etc.
  Returns:
    The path with all slashes made into backslashes.
  """
  return path.replace('/', '\\')


def _SourceInFolders(sources, prefix=None, excluded=None):
  """Converts a list split source file paths into a vcproj folder hierarchy.

  Arguments:
    sources: A list of source file paths split.
    prefix: A list of source file path layers meant to apply to each of sources.
  Returns:
    A hierarchy of filenames and MSVSProject.Filter objects that matches the
    layout of the source tree.
    For example:
    _SourceInFolders([['a', 'bob1.c'], ['b', 'bob2.c']], prefix=['joe'])
    -->
    [MSVSProject.Filter('a', contents=['joe\\a\\bob1.c']),
     MSVSProject.Filter('b', contents=['joe\\b\\bob2.c'])]
  """
  if not prefix: prefix = []
  result = []
  excluded_result = []
  folders = dict()
  # Gather files into the final result, excluded, or folders.
  for s in sources:
    if len(s) == 1:
      filename = '\\'.join(prefix + s)
      if filename in excluded:
        excluded_result.append(filename)
      else:
        result.append(filename)
    else:
      if not folders.get(s[0]):
        folders[s[0]] = []
      folders[s[0]].append(s[1:])
  # Add a folder for excluded files.
  if excluded_result:
    excluded_folder = MSVSProject.Filter('_excluded_files',
                                         contents=excluded_result)
    result.append(excluded_folder)
  # Populate all the folders.
  for f in folders:
    contents = _SourceInFolders(folders[f], prefix=prefix + [f],
                                excluded=excluded)
    contents = MSVSProject.Filter(f, contents=contents)
    result.append(contents)

  return result


def _ToolAppend(tools, tool_name, setting, value):
  if not value: return
  if not tools.get(tool_name):
    tools[tool_name] = dict()
  tool = tools[tool_name]
  if tool.get(setting):
    if type(tool[setting]) == list:
      tool[setting] += value
    else:
      raise TypeError(
          'Appending "%s" to a non-list setting "%s" for tool "%s" is '
          'not allowed, previous value: %s' % (
              value, setting, tool_name, str(tool[setting])))
  else:
    tool[setting] = value


def _ConfigFullName(config_name, config_data):
  return '|'.join([config_name,
                   config_data.get('configuration_platform', 'Win32')])


def _PrepareAction(c, r, has_input_path):
  # Find path to cygwin.
  cygwin_dir = _FixPath(c.get('msvs_cygwin_dirs', ['.'])[0])

  # Currently this weird argument munging is used to duplicate the way a
  # python script would need to be run as part of the chrome tree.
  # Eventually we should add some sort of rule_default option to set this
  # per project. For now the behavior chrome needs is the default.
  if int(r.get('msvs_cygwin_shell', 1)):
    # Prepare command.
    direct_cmd = r['action']
    direct_cmd = [i.replace('$(IntDir)',
                            '`cygpath -m "${INTDIR}"`') for i in direct_cmd]
    direct_cmd = [i.replace('$(OutDir)',
                            '`cygpath -m "${OUTDIR}"`') for i in direct_cmd]
    if has_input_path:
      direct_cmd = [i.replace('$(InputPath)',
                              '`cygpath -m "${INPUTPATH}"`')
                    for i in direct_cmd]
    direct_cmd = ['"%s"' % i for i in direct_cmd]
    direct_cmd = [i.replace('"', '\\"') for i in direct_cmd]
    #direct_cmd = gyp.common.EncodePOSIXShellList(direct_cmd)
    direct_cmd = ' '.join(direct_cmd)
    cmd = (
      '$(ProjectDir)%(cygwin_dir)s\\setup_env.bat && '
      'set CYGWIN=nontsec && '
      'set INTDIR=$(IntDir)&& '
      'set OUTDIR=$(OutDir)&& ')
    if has_input_path:
      cmd += 'set INPUTPATH=$(InputPath) && '
    cmd += (
      'bash -c "%(cmd)s"')
    cmd = cmd % {'cygwin_dir': cygwin_dir,
                 'cmd': direct_cmd}
    return cmd
  else:
    # Support a mode for using cmd directly.
    direct_cmd = r['action']
    return ' '.join(direct_cmd)


def _GenerateProject(vcproj_filename, build_file, spec, options, version):
  """Generates a vcproj file.

  Arguments:
    vcproj_filename: Filename of the vcproj file to generate.
    build_file: Filename of the .gyp file that the vcproj file comes from.
    spec: The target dictionary containing the properties of the target.
  """
  #print 'Generating %s' % vcproj_filename

  p = MSVSProject.Writer(vcproj_filename, version=version)
  default_config = spec['configurations'][spec['default_configuration']]
  guid = default_config.get('msvs_guid')
  if guid: guid = '{%s}' % guid
  p.Create(spec['target_name'], guid=guid)

  # Get directory project file is in.
  gyp_dir = os.path.split(vcproj_filename)[0]

  # Pick target configuration type.
  config_type = {
      'executable': '1',
      'shared_library': '2',
      'loadable_module': '2',
      'static_library': '4',
      'none': '10',
      }[spec['type']]

  for config_name, c in spec['configurations'].iteritems():
    # Process each configuration.
    vsprops_dirs = c.get('msvs_props', [])
    vsprops_dirs = [_FixPath(i) for i in vsprops_dirs]

    # Prepare the list of tools as a dictionary.
    tools = dict()

    # Add in msvs_settings.
    for tool in c.get('msvs_settings', {}):
      settings = c['msvs_settings'][tool]
      for setting in settings:
        _ToolAppend(tools, tool, setting, settings[setting])

    # Add in includes.
    include_dirs = c.get('include_dirs', [])
    include_dirs = [_FixPath(i) for i in include_dirs]
    _ToolAppend(tools, 'VCCLCompilerTool',
                'AdditionalIncludeDirectories', include_dirs)
    _ToolAppend(tools, 'VCResourceCompilerTool',
                'AdditionalIncludeDirectories', include_dirs)

    # Add in libraries.
    libraries = spec.get('libraries', [])
    # Strip out -l, as it is not used on windows (but is needed so we can pass
    # in libraries that are assumed to be in the default library path).
    libraries = [re.sub('^(\-l)', '', lib) for lib in libraries]
    # Add them.
    _ToolAppend(tools, 'VCLinkerTool',
                'AdditionalDependencies', libraries)

    # Add defines.
    defines = []
    for d in c.get('defines', []):
      if type(d) == list:
        fd = '='.join([str(dpart).replace('"', '\\"') for dpart in d])
      else:
        fd = str(d).replace('"', '\\"')
      defines.append(fd)

    _ToolAppend(tools, 'VCCLCompilerTool',
                'PreprocessorDefinitions', defines)
    _ToolAppend(tools, 'VCResourceCompilerTool',
                'PreprocessorDefinitions', defines)

    # Change program database directory to prevent collisions.
    _ToolAppend(tools, 'VCCLCompilerTool', 'ProgramDataBaseFileName',
                '$(IntDir)\\$(ProjectName)\\vc80.pdb')

    # Add disabled warnings.
    disabled_warnings = [str(i) for i in c.get('msvs_disabled_warnings', [])]
    _ToolAppend(tools, 'VCCLCompilerTool',
                'DisableSpecificWarnings', disabled_warnings)

    # Add Pre-build.
    prebuild = c.get('msvs_prebuild')
    _ToolAppend(tools, 'VCPreBuildEventTool', 'CommandLine', prebuild)

    # Add Post-build.
    postbuild = c.get('msvs_postbuild')
    _ToolAppend(tools, 'VCPostBuildEventTool', 'CommandLine', postbuild)

    # Turn on precompiled headers if appropriate.
    header = c.get('msvs_precompiled_header')
    if header:
      header = os.path.split(header)[1]
      _ToolAppend(tools, 'VCCLCompilerTool', 'UsePrecompiledHeader', '2')
      _ToolAppend(tools, 'VCCLCompilerTool',
                  'PrecompiledHeaderThrough', header)
      _ToolAppend(tools, 'VCCLCompilerTool',
                  'ForcedIncludeFiles', header)

    # Set the module definition file if any.
    if spec['type'] in ['shared_library', 'loadable_module']:
      def_files = [s for s in spec.get('sources', []) if s.endswith('.def')]
      if len(def_files) == 1:
        _ToolAppend(tools, 'VCLinkerTool', 'ModuleDefinitionFile',
                    _FixPath(def_files[0]))
      elif def_files:
        raise ValueError('Multiple module definition files in one target, '
                         'target %s lists multiple .def files: %s' % (
            spec['target_name'], ' '.join(def_files)))

    # Convert tools to expected form.
    tool_list = []
    for tool, settings in tools.iteritems():
      # Collapse settings with lists.
      settings_fixed = {}
      for setting, value in settings.iteritems():
        if type(value) == list:
          if tool == 'VCLinkerTool' and setting == 'AdditionalDependencies':
            settings_fixed[setting] = ' '.join(value)
          else:
            settings_fixed[setting] = ';'.join(value)
        else:
          settings_fixed[setting] = value
      # Add in this tool.
      tool_list.append(MSVSProject.Tool(tool, settings_fixed))

    # Prepare configuration attributes.
    prepared_attrs = {}
    source_attrs = c.get('msvs_configuration_attributes', {})
    for a in source_attrs:
      prepared_attrs[a] = source_attrs[a]
    # Add props files.
    prepared_attrs['InheritedPropertySheets'] = ';'.join(vsprops_dirs)
    # Set configuration type.
    prepared_attrs['ConfigurationType'] = config_type

    # Add in this configuration.
    p.AddConfig('|'.join([config_name,
                          c.get('configuration_platform', 'Win32')]),
                attrs=prepared_attrs, tools=tool_list)

  # Prepare list of sources.
  sources = spec.get('sources', [])
  # Add in the gyp file.
  sources.append(os.path.split(build_file)[1])
  # Add in 'action' inputs and outputs.
  for a in spec.get('actions', []):
    for i in a.get('inputs', []):
      if i not in sources:
        sources.append(i)
    if a.get('process_outputs_as_sources', False):
      for i in a.get('outputs', []):
        if i not in sources:
            sources.append(i)
  # Add in 'copies' inputs and outputs.
  for cpy in spec.get('copies', []):
    for f in cpy.get('files', []):
      if f not in sources:
        sources.append(f)

  # Add rules.
  rules = spec.get('rules', [])
  for config_name, c in spec['configurations'].iteritems():
    # Don't generate rules file if not needed.
    if not rules: continue
    # Create rules file.
    rule_filename = '%s_%s%s.rules' % (spec['target_name'],
                                       config_name,
                                       options.suffix)
    rules_file = MSVSToolFile.Writer(os.path.join(gyp_dir, rule_filename))
    rules_file.Create(spec['target_name'])
    # Add each rule.
    for r in rules:
      rule_name = r['rule_name']
      rule_ext = r['extension']
      outputs = [_FixPath(i) for i in r.get('outputs', [])]
      cmd = _PrepareAction(c, r, has_input_path=True)
      rules_file.AddCustomBuildRule(name=rule_name, extensions=[rule_ext],
                                    outputs=outputs, cmd=cmd)
    # Write out rules file.
    rules_file.Write()

    # Add rule file into project.
    p.AddToolFile(rule_filename)

    # Add sources for each applicable rule.
    for r in rules:
      # Done if not processing outputs as sources.
      if not r.get('process_outputs_as_sources', False): continue
      # Get some properties for this rule.
      rule_ext = r['extension']
      outputs = r.get('outputs', [])
      # Find sources to which this applies.
      rule_sources = [s for s in sources if s.endswith('.' + rule_ext)]
      for s in rule_sources:
        for o in outputs:
          nout = o
          nout = nout.replace('$(InputName)',
                              os.path.splitext(os.path.split(s)[1])[0])
          nout = nout.replace('$(InputExt)',
                              os.path.splitext(os.path.split(s)[1])[1])
          nout = nout.replace('$(InputFileName)', os.path.split(s)[1])
          nout = nout.replace('$(InputPath)', os.path.split(s)[0])
          if nout not in sources:
            sources.append(nout)

  # Convert to proper windows form.
  sources = [_FixPath(i) for i in sources]
  # Exclude excluded ones.
  excluded_sources = spec.get('sources_excluded', [])
  # Convert to proper windows form.
  excluded_sources = [_FixPath(i) for i in excluded_sources]
  # Add excluded into sources
  sources += excluded_sources

  # List of precompiled header related keys.
  precomp_keys = [
      'msvs_precompiled_header',
      'msvs_precompiled_source',
  ]

  # Gather a list of precompiled header related sources.
  precompiled_related = []
  for config_name, c in spec['configurations'].iteritems():
    for k in precomp_keys:
      f = c.get(k)
      if f:
        precompiled_related.append(_FixPath(f))

  # Find the excluded ones, minus the precompiled header related ones.
  fully_excluded = [i for i in excluded_sources if i not in precompiled_related]

  # Convert to folders and the right slashes.
  sources = [i.split('\\') for i in sources]
  sources = _SourceInFolders(sources, excluded=fully_excluded)
  # Add in files.
  p.AddFiles(sources)

  # Exclude excluded sources from being built.
  for f in excluded_sources:
    for config_name, c in spec['configurations'].iteritems():
      precomped = [_FixPath(c.get(i, '')) for i in precomp_keys]
      # Don't do this for ones that are precompiled header related.
      if f not in precomped:
        p.AddFileConfig(f, _ConfigFullName(config_name, c),
                        {'ExcludedFromBuild': 'true'})

  # Add in tool files (rules).
  tool_files = set()
  for config_name, c in spec['configurations'].iteritems():
    for f in c.get('msvs_tool_files', []):
      tool_files.add(f)
  for f in tool_files:
    p.AddToolFile(f)

  # Handle pre-compiled headers source stubs specially.
  for config_name, c in spec['configurations'].iteritems():
    source = c.get('msvs_precompiled_source')
    if source:
      source = _FixPath(source)
      # UsePrecompiledHeader=1 for if using precompiled headers.
      tool = MSVSProject.Tool('VCCLCompilerTool',
                              {'UsePrecompiledHeader': '1'})
      p.AddFileConfig(source, _ConfigFullName(config_name, c),
                      {}, tools=[tool])

  # Add actions.
  actions = spec.get('actions', [])
  for a in actions:
    for config_name, c in spec['configurations'].iteritems():
      inputs = [_FixPath(i) for i in a.get('inputs', [])]
      outputs = [_FixPath(i) for i in a.get('outputs', [])]
      cmd = _PrepareAction(c, a, has_input_path=False)
      tool = MSVSProject.Tool(
          'VCCustomBuildTool', {
            'Description': a['action_name'],
            'AdditionalDependencies': ';'.join(inputs),
            'Outputs': ';'.join(outputs),
            'CommandLine': cmd,
            })
      # Pick second input as the primary one, unless there's only one.
      # TODO(bradnelson): this is a bit of a hack, find something more general.
      if len(inputs) > 1:
        primary_input = inputs[1]
      else:
        primary_input = inputs[0]
      # Add to the properties of primary input.
      p.AddFileConfig(primary_input,
                      _ConfigFullName(config_name, c), tools=[tool])

  # Add copies.
  for cpy in spec.get('copies', []):
    for config_name, c in spec['configurations'].iteritems():
      for f in cpy.get('files', []):
        if os.path.isabs(f) or f[:1] == '$':
          src = _FixPath(f)
        else:
          src = _FixPath(os.path.join('$(ProjectDir)', f))
        dst = _FixPath(os.path.join(cpy['destination'],
                                    os.path.basename(f)))
        cmd = 'copy /Y "%s" "%s"' % (src, dst)
        tool = MSVSProject.Tool(
            'VCCustomBuildTool', {
              'Description': 'Copying %s to %s' % (src, dst),
              'AdditionalDependencies': src,
              'Outputs': dst,
              'CommandLine': cmd,
              })
        # Add to the properties of the file.
        p.AddFileConfig(_FixPath(f),
                        _ConfigFullName(config_name, c), tools=[tool])

  # Write it out.
  p.Write()

  # Return the guid so we can refer to it elsewhere.
  return p.guid


def _ProjectObject(sln, qualified_target, project_objs, projects):
  # Done if this project has an object.
  if project_objs.get(qualified_target):
    return project_objs[qualified_target]
  # Get dependencies for this project.
  spec = projects[qualified_target]['spec']
  deps = spec.get('dependencies', [])
  # Get objects for each dependency.
  deps = [_ProjectObject(sln, d, project_objs, projects) for d in deps]
  # Find relative path to vcproj from sln.
  vcproj_rel_path = gyp.common.RelativePath(
      projects[qualified_target]['vcproj_path'], os.path.split(sln)[0])
  vcproj_rel_path = _FixPath(vcproj_rel_path)
  # Create object for this project.
  obj = MSVSNew.MSVSProject(
      vcproj_rel_path,
      name=spec['target_name'],
      guid=projects[qualified_target]['guid'],
      dependencies=deps)
  # Store it to the list of objects.
  project_objs[qualified_target] = obj
  return obj


def GenerateOutput(target_list, target_dicts, data, params):
  """Generate .sln and .vcproj files.

  This is the entry point for this generator.
  Arguments:
    target_list: List of target pairs: 'base/base.gyp:base'.
    target_dicts: Dict of target properties keyed on target pair.
    data: Dictionary containing per .gyp data.
  """

  options = params['options']

  # Select project file format version.
  msvs_version = MSVSVersion.SelectVisualStudioVersion(options.msvs_version)

  # Prepare the set of configurations.
  configs = set()
  for qualified_target in target_list:
    build_file = gyp.common.BuildFileAndTarget('', qualified_target)[0]
    spec = target_dicts[qualified_target]
    for config_name, c in spec['configurations'].iteritems():
      configs.add('|'.join([config_name,
                            c.get('configuration_platform', 'Win32')]))
  configs = list(configs)

  # Generate each project.
  projects = {}
  for qualified_target in target_list:
    build_file = gyp.common.BuildFileAndTarget('', qualified_target)[0]
    spec = target_dicts[qualified_target]
    vcproj_path = os.path.join(os.path.split(build_file)[0],
                               spec['target_name'] + options.suffix +
                               '.vcproj')
    projects[qualified_target] = {
        'vcproj_path': vcproj_path,
        'guid': _GenerateProject(vcproj_path, build_file,
                                 spec, options, version=msvs_version),
        'spec': spec,
    }

  for build_file in data.keys():
    # Validate build_file extension
    if build_file[-4:] != '.gyp':
      continue
    sln_path = build_file[:-4] + options.suffix + '.sln'
    #print 'Generating %s' % sln_path
    # Get projects in the solution, and their dependents in a separate bucket.
    sln_projects = gyp.common.BuildFileTargets(target_list, build_file)
    dep_projects = gyp.common.DeepDependencyTargets(target_dicts, sln_projects)
    # Convert to entries.
    project_objs = {}
    entries = [_ProjectObject(sln_path, p, project_objs, projects)
               for p in sln_projects]
    dep_entries = [_ProjectObject(sln_path, p, project_objs, projects)
                   for p in dep_projects]
    entries.append(MSVSNew.MSVSFolder('dependencies', entries=dep_entries))
    # Create solution.
    sln = MSVSNew.MSVSSolution(sln_path,
                               entries=entries,
                               variants=configs,
                               websiteProperties=False,
                               version=msvs_version)
    sln.Write()
