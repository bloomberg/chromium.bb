#!/usr/bin/python


import os.path
import subprocess
import sys
import gyp.common
import gyp.MSVSNew as MSVSNew
import gyp.MSVSProject as MSVSProject


generator_default_variables = {
    'OS': 'win',
    }


def _FixPath(path):
  """Convert paths to a form that will make sense in a vcproj file.

  Arguments:
    path: The path to convert, may contain / etc.
  Returns:
    The path with all slashes made into backslashes.
  """
  return path.replace('/', '\\')


def _SourceInFolders(sources, prefix=None):
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
  folders = dict()
  for s in sources:
    if len(s) == 1:
      filename = '\\'.join(prefix + s)
      result.append(filename)
    else:
      if not folders.get(s[0]):
        folders[s[0]] = []
      folders[s[0]].append(s[1:])
  for f in folders:
    contents = _SourceInFolders(folders[f], prefix + [f])
    contents = MSVSProject.Filter(f, contents=contents)
    result.append(contents)

  return result


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
  # Figure out which vsprops to use.
  vsprops_dirs = spec.get('msvs_props', [])
  vsprops_dirs = [_FixPath(i) for i in vsprops_dirs]

  # Prepare compiler tool.
  include_dirs = spec.get('include_dirs', [])
  include_dirs = [_FixPath(i) for i in include_dirs]
  defines = ['OS_WIN'] + spec.get('defines', [])
  disabled_warnings = [str(i) for i in spec.get('msvs_disabled_warnings', [])]
  compiler_tool = MSVSProject.Tool(
      'VCCLCompilerTool', {
          'AdditionalIncludeDirectories': ';'.join(include_dirs),
          'PreprocessorDefinitions': ';'.join(defines),
          'DisableSpecificWarnings': ';'.join(disabled_warnings),
      })

  # Pre-build.
  if spec.get('msvs_prebuild'):
    prebuild_dict = { 'CommandLine': spec.get('msvs_prebuild', '') }
  else:
    prebuild_dict = {}
  prebuild = MSVSProject.Tool('VCPreBuildEventTool', prebuild_dict)
  # Post-build.
  if spec.get('msvs_postbuild'):
    postbuild_dict = { 'CommandLine': spec.get('msvs_postbuild', '') }
  else:
    postbuild_dict = {}
  postbuild = MSVSProject.Tool('VCPostBuildEventTool', postbuild_dict)

  # Prepare tools.
  tools = [
      prebuild,
      'VCCustomBuildTool',
      'VCXMLDataGeneratorTool',
      'VCWebServiceProxyGeneratorTool',
      'VCMIDLTool',
      compiler_tool,
      'VCManagedResourceCompilerTool',
      'VCResourceCompilerTool',
      'VCPreLinkEventTool',
      'VCLibrarianTool',
      'VCALinkTool',
      'VCXDCMakeTool',
      'VCBscMakeTool',
      'VCFxCopTool',
      postbuild,
      ]

  # Pick configuration type.
  config_type = {
      'executable': '1',
      'shared_library': '2',
      'static_library': '4',
      'none': '10',
      }[spec['type']]

  # Add Debug/Release.
  p.AddConfig(
      'Debug|Win32',
      attrs={'InheritedPropertySheets':
             ';'.join(['$(SolutionDir)\\..\\build\\debug.vsprops'] +
                      vsprops_dirs),
             'ConfigurationType': config_type},
      tools=tools)
  p.AddConfig(
      'Release|Win32',
      attrs={'InheritedPropertySheets':
             ';'.join(['$(SolutionDir)\\..\\build\\release.vsprops'] +
                      vsprops_dirs),
             'ConfigurationType': config_type},
      tools=tools)
  # Prepare list of sources.
  sources = spec['sources']
  # Add in the gyp file.
  sources.append(os.path.split(build_file)[1])
  # Convert to folders and the right slashes.
  sources = [_FixPath(i) for i in sources]
  sources = [i.split('\\') for i in sources]
  sources = _SourceInFolders(sources)
  # Add in files.
  p.AddFiles(sources)
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
      # TODO(mark): Pick an exception class.
      raise 'Build file name must end in .gyp'
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
                               variants=['Debug|Win32', 'Release|Win32'],
                               websiteProperties=False)
    sln.Write()
