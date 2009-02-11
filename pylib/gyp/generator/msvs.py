#!/usr/bin/python


import os.path
import subprocess
import sys
import gyp.common
import gyp.MSVSNew as MSVSNew
import gyp.MSVSProject as MSVSProject


generator_default_variables = {
    'OS': 'win',
    'INTERMEDIATE_DIR': '$(IntDir)',
    'PRODUCT_DIR': '$(OutDir)',
    'EXECUTABLE_PREFIX': '',
    'EXECUTABLE_SUFFIX': '.exe',
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
      raise 'Append to non-list option is invalid'
  else:
    tool[option] = value


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

    # Add in libraries (really system libraries at this point).
    libraries = spec.get('libraries', [])
    _ToolAppend(tools, 'VCLibrarianTool',
                'AdditionalDependencies', libraries)

    # Add defines.
    defines = []
    for d in c.get('defines', []):
      if type(d)==list:
        fd = '='.join([str(dpart) for dpart in d])
      else:
        fd = str(d)
      defines.append(fd)

    _ToolAppend(tools, 'VCCLCompilerTool',
                'PreprocessorDefinitions', defines)
    _ToolAppend(tools, 'VCResourceCompilerTool',
                'PreprocessorDefinitions', defines)

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
    precompiled_headers_enabled = c.get('msvs_precompiled_headers_enabled', 0)
    if precompiled_headers_enabled:
      header = os.path.split(c['msvs_precompiled_header'])[1]
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
  sources = spec['sources']
  # Add in the gyp file.
  sources.append(os.path.split(build_file)[1])
  # Add in 'action' inputs.
  actions = spec.get('actions', [])
  for a in actions:
    for i in a.get('inputs', []):
      if i not in sources:
        sources.append(i)
  # Convert to proper windows form.
  sources = [_FixPath(i) for i in sources]

  # Exclude excluded ones.
  excluded_sources = spec.get('sources_excluded', [])
  # Convert to proper windows form.
  excluded_sources = [_FixPath(i) for i in excluded_sources]
  # Add excluded into sources
  sources += excluded_sources

  # Convert to folders and the right slashes.
  sources = [i.split('\\') for i in sources]
  sources = _SourceInFolders(sources, excluded=excluded_sources)
  # Add in files.
  p.AddFiles(sources)

  # Exclude excluded sources from being built.
  for f in excluded_sources:
    for config_name in spec['configurations']:
      p.AddFileConfig(f, config_name, {'ExcludedFromBuild': 'true'})

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
      precompiled_headers_enabled = c.get('msvs_precompiled_headers_enabled', 0)
      if precompiled_headers_enabled:
        # UsePrecompiledHeader=1 for if using precompiled headers.
        tool = MSVSProject.Tool('VCCLCompilerTool',
                                {'UsePrecompiledHeader': '1'})
        p.AddFileConfig(source, config_name, {}, tools=[tool])
      else:
        # Exclude from build if not.
        p.AddFileConfig(source, config_name, {'ExcludedFromBuild': 'true'})

  # Add actions.
  actions = spec.get('actions', [])
  for a in actions:
    inputs = [_FixPath(i) for i in a.get('inputs', [])]
    outputs = [_FixPath(i) for i in a.get('outputs', [])]
    tool = MSVSProject.Tool(
        'VCCustomBuildTool', {
          'Description': a['action_name'],
          'AdditionalDependencies': ';'.join(inputs),
          'Outputs': ';'.join(outputs),
          'CommandLine': a['action'].replace('/', '\\'),
          })
    for config_name in spec['configurations']:
      p.AddFileConfig(inputs[0], config_name, tools=[tool])

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


def _DependentProjects(target_dicts, roots):
  dependents = set()
  for r in roots:
    spec = target_dicts[r]
    r_deps = spec.get('dependencies', [])
    for d in r_deps:
      if d not in roots:
        dependents.add(d)
    for d in _DependentProjects(target_dicts, r_deps):
      if d not in roots:
        dependents.add(d)
  return list(dependents)


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
    sln_projects = [p for p in target_list if
                    gyp.common.BuildFileAndTarget('', p)[0] == build_file]
    dep_projects = _DependentProjects(target_dicts, sln_projects)
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
