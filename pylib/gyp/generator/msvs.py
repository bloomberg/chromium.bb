#!/usr/bin/python


import os.path
import subprocess
import sys
import gyp.common
import gyp.MSVSNew as MSVSNew
import gyp.MSVSToolFile as MSVSToolFile
import gyp.MSVSProject as MSVSProject


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


def _ToolAppend(tools, tool_name, option, value):
  if not value: return
  if not tools.get(tool_name):
    tools[tool_name] = dict()
  tool = tools[tool_name]
  if tool.get(option):
    if type(tool[option]) == list:
      tool[option] += value
    else:
      # TODO(bradnelson): Pick an exception class.
      print '@@@', option, tool[option], value
      raise 'Append to non-list option is invalid'
  else:
    tool[option] = value


def _ConfigFullName(config_name, config_data):
  return '|'.join([config_name,
                   config_data.get('configuration_platform', 'Win32')])


def _PrepareAction(c, r, has_input_path):
  # Find path to cygwin.
  cygwin_dir = _FixPath(c.get('msvs_cygwin_dirs', ['.'])[0])

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
#  '$(ProjectDir)%(cygwin_dir)s\\setup_mount.bat && '
    '$(ProjectDir)%(cygwin_dir)s\\setup_env.bat && '
    'set INTDIR=$(IntDir) && '
    'set OUTDIR=$(OutDir) && ')
  if has_input_path:
    cmd += 'set INPUTPATH=$(InputPath) && '
  cmd += (
    'bash -c "%(cmd)s"')
  cmd = cmd % {'cygwin_dir': cygwin_dir, 'cmd': direct_cmd}
  return cmd


def _GenerateProject(vcproj_filename, build_file, spec):
  """Generates a vcproj file.

  Arguments:
    vcproj_filename: Filename of the vcproj file to generate.
    build_file: Filename of the .gyp file that the vcproj file comes from.
    spec: The target dictionary containing the properties of the target.
  """
  print 'Generating %s' % vcproj_filename

  p = MSVSProject.Writer(vcproj_filename)
  p.Create(spec['target_name'])

  # Get directory project file is in.
  gyp_dir = os.path.split(vcproj_filename)[0]

  # Pick target configuration type.
  config_type = {
      'executable': '1',
      'application': '1',
      'shared_library': '2',
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
      options = c['msvs_settings'][tool]
      for option in options:
        _ToolAppend(tools, tool, option, options[option])

    # Add in includes.
    include_dirs = c.get('include_dirs', [])
    include_dirs = [_FixPath(i) for i in include_dirs]
    _ToolAppend(tools, 'VCCLCompilerTool',
                'AdditionalIncludeDirectories', include_dirs)

    # Add in libraries.
    libraries = spec.get('libraries', []) + c.get('msvs_system_libraries', [])
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

    # Convert tools to expected form.
    tool_list = []
    for tool, options in tools.iteritems():
      # Collapse options with lists.
      options_fixed = {}
      for option, value in options.iteritems():
        if type(value) == list:
          if tool == 'VCLinkerTool' and option == 'AdditionalDependencies':
            options_fixed[option] = ' '.join(value)
          else:
            options_fixed[option] = ';'.join(value)
        else:
          options_fixed[option] = value
      # Add in this tool.
      tool_list.append(MSVSProject.Tool(tool, options_fixed))

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
  actions = spec.get('actions', [])
  for a in actions:
    for i in a.get('inputs', []):
      if i not in sources:
        sources.append(i)
    if a.get('process_outputs_as_sources', False):
      for i in a.get('outputs', []):
        if i not in sources:
            sources.append(i)

  # Add rules.
  rules = spec.get('rules', [])
  for config_name, c in spec['configurations'].iteritems():
    # Don't generate rules file if not needed.
    if not rules: continue
    # Create rules file.
    rule_filename = '%s_%s_gyp.rules' % (spec['target_name'], config_name)
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


def GenerateOutput(target_list, target_dicts, data):
  """Generate .sln and .vcproj files.

  This is the entry point for this generator.
  Arguments:
    target_list: List of target pairs: 'base/base.gyp:base'.
    target_dicts: Dict of target properties keyed on target pair.
    data: Dictionary containing per .gyp data.
  """
  configs = set()
  for qualified_target in target_list:
    build_file = gyp.common.BuildFileAndTarget('', qualified_target)[0]
    spec = target_dicts[qualified_target]
    for config_name, c in spec['configurations'].iteritems():
      configs.add('|'.join([config_name,
                            c.get('configuration_platform', 'Win32')]))
  configs = list(configs)


  projects = {}
  for qualified_target in target_list:
    build_file = gyp.common.BuildFileAndTarget('', qualified_target)[0]
    spec = target_dicts[qualified_target]
    vcproj_path = os.path.join(os.path.split(build_file)[0],
                               spec['target_name'] + '_gyp.vcproj')
    projects[qualified_target] = {
        'vcproj_path': vcproj_path,
        'guid': _GenerateProject(vcproj_path, build_file, spec),
        'spec': spec,
    }

  for build_file in data.keys():
    # Validate build_file extension
    if build_file[-4:] != '.gyp':
      continue
    sln_path = build_file[:-4] + '_gyp.sln'
    print 'Generating %s' % sln_path
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
                               websiteProperties=False)
    sln.Write()
