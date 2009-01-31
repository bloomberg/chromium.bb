#!/usr/bin/python


import gyp
import gyp.MSVSNew as MSVSNew
import gyp.MSVSProject as MSVSProject
import os.path
import subprocess
import re
import sys


# TODO(mark): Obvious hack, see __init__.py.
variables_hack = [
  'OS==win',
  'OS!=mac',
  'OS!=linux',
]


def FixPath(path):
  if sys.platform == 'cygwin':
    path = subprocess.Popen(['cygpath', '-w', path],
                            stdout=subprocess.PIPE).communicate()[0]
    path = path.replace('\n', '')
    return path
  else:
    return path.replace('/', '\\')


def SourceInFolders(sources, prefix=[]):
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
    contents = SourceInFolders(folders[f], prefix + [f])
    contents = MSVSProject.Filter(f, contents=contents)
    result.append(contents)

  return result


def GenerateProject(filename, spec):
  print 'Generating %s' % filename

  p = MSVSProject.Writer(filename)
  p.Create(spec['name'])

  # Get directory gyp file is in.
  gyp_dir = os.path.split(filename)[0]
  # Figure out which vsprops to use.
  vsprops_dirs = spec.get('vs_props', [])
  # Make them relative to the gyp file.
  vsprops_dirs = [os.path.join(gyp_dir, i) for i in vsprops_dirs]
  vsprops_dirs = [FixPath(i) for i in vsprops_dirs]

  # Prepare compiler tool.
  include_dirs = spec.get('include_dirs',[])
  include_dirs = [os.path.join(gyp_dir, i) for i in include_dirs]
  include_dirs = [FixPath(i) for i in include_dirs]
  defines = ['OS_WIN'] + spec.get('defines',[])
  compiler_tool = MSVSProject.Tool(
      'VCCLCompilerTool', {
        'AdditionalIncludeDirectories': ';'.join(include_dirs),
        'PreprocessorDefinitions': ';'.join(defines),
        })

  # Pre-build and Post-build.
  prebuild = MSVSProject.Tool(
      'VCPreBuildEventTool', {
        'CommandLine': spec.get('vs_prebuild', ''),
        })
  postbuild = MSVSProject.Tool(
      'VCPostBuildEventTool', {
        'CommandLine': spec.get('vs_postbuild', ''),
        })

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
      'none': '10',
      'executable': '1',
      'static_library': '4',
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
  # Add in file list.
  sources = [i.replace('/', '\\') for i in spec['sources']]
  sources = [i.split('\\') for i in sources]
  sources = SourceInFolders(sources)
  p.AddFiles(sources)
  # Write it out.
  p.Write()

  # Return the guid so we can refer to it elsewhere.
  return p.guid


def PopulateProjectObjects(projects, target_list, target_dicts,
                           qualified_target):
  # Done if this project has an object.
  if projects[qualified_target].get('obj'): return
  # Get dependencies for this project.
  spec = target_dicts[qualified_target]
  deps = spec.get('dependencies', [])
  # Populate all dependencies first.
  for d in deps:
    PopulateProjectObjects(projects, target_list, target_dicts, d)
  # Get objects for each dependency.
  deps = [projects[d]['obj'] for d in deps]
  # Create object for this project.
  projects[qualified_target]['obj'] = MSVSNew.MSVSProject(
      FixPath(projects[qualified_target]['vcproj_path']),
      name=spec['name'],
      guid=projects[qualified_target]['guid'],
      dependencies=deps)


def DependentProjects(target_dicts, roots):
  dependents = set()
  for r in roots:
    spec = target_dicts[r]
    r_deps = spec.get('dependencies', [])
    for d in r_deps:
      if d not in roots:
        dependents.add(d)
    for d in DependentProjects(target_dicts, r_deps):
      if d not in roots:
        dependents.add(d)
  return list(dependents)


def GenerateOutput(target_list, target_dicts, data):
  solutions = {}
  for build_file, build_file_dict in data.iteritems():
    if build_file[-4:] != '.gyp':
      # TODO(mark): Pick an exception class
      raise 'Build file name must end in .gyp'
    build_file_stem = build_file[:-4]
    solutions[build_file] = {
        'sln_path': os.path.abspath(build_file_stem + '_gyp.sln'),
    }

  projects = {}
  for qualified_target in target_list:
    [build_file, target] = gyp.BuildFileAndTarget('', qualified_target)[0:2]
    spec = target_dicts[qualified_target]
    vcproj_path = os.path.abspath(os.path.join(os.path.split(build_file)[0],
                                               spec['name'] + '_gyp.vcproj'))
    projects[qualified_target] = {
        'vcproj_path': vcproj_path,
        'guid': GenerateProject(vcproj_path, spec),
    }

  for qualified_target in target_list:
    PopulateProjectObjects(projects, target_list, target_dicts,
                           qualified_target)

  for s, s_dict in solutions.iteritems():
    sln_path = s_dict['sln_path']
    print 'Generating %s' % sln_path
    # Get projects in the solution, and their dependents in a separate bucket.
    sln_projects = [p for p in target_list if
                    gyp.BuildFileAndTarget('', p)[0] == s]
    dep_projects = DependentProjects(target_dicts, sln_projects)
    # Convert to entries.
    entries = [projects[e]['obj'] for e in sln_projects]
    dep_entries = [projects[e]['obj'] for e in dep_projects]
    entries.append(MSVSNew.MSVSFolder('dependencies', entries=dep_entries))
    # Create solution.
    sln = MSVSNew.MSVSSolution(FixPath(sln_path),
                               entries=entries,
                               variants=[
                                 'Debug|Win32',
                                 'Release|Win32',
                               ])
    sln.Write()
