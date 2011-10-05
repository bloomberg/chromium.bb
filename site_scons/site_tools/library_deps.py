# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simple harness for defining library dependencies for scons files."""

# The following is a map from a library, to the corresponding
# list of dependent libraries that must be included after that library, in
# the list of libraries.
# Note: These are default rules that are used if platform specific rules are
# not specified below in PLATFORM_LIBRARY_DEPENDENCIES.
LIBRARY_DEPENDENCIES_DEFAULT = {
    'platform': [
        'gio',
        ],
    }

def _AddDefaultLibraryDependencies(dependencies):
  """ Adds default library dependencies to library dependencies.

  Takes the contents of the platform-specific library dependencies, and
  adds default dependencies if needed.
  """
  for key in LIBRARY_DEPENDENCIES_DEFAULT:
    if key not in dependencies:
      dependencies[key] = LIBRARY_DEPENDENCIES_DEFAULT[key]
  return dependencies

# Platform specific library dependencies. Mapping from a platform,
# to a map from a library, to the corresponding list of dependendent
# libraries that must be included after that library, in the list
# of libraries.
PLATFORM_LIBRARY_DEPENDENCIES = {
    'x86-32': _AddDefaultLibraryDependencies({
        'nc_decoder_x86_32': [
            'ncval_base_x86_32',
            'nc_opcode_modeling_x86_32',
            ],
        'ncdis_util_x86_32': [
            'ncval_reg_sfi_verbose_x86_32',
            'ncdis_seg_sfi_verbose_x86_32',
            ],
        'ncdis_seg_sfi_verbose_x86_32': [
            'ncdis_seg_sfi_x86_32',
            'ncval_base_verbose_x86_32',
            ],
        'ncvalidate_verbose_x86_32': [
            'ncvalidate_x86_32',
            'ncdis_seg_sfi_verbose_x86_32',
            ],
        'ncvalidate_x86_32': [
            'ncval_seg_sfi_x86_32',
            ],
        'ncval_base_verbose_x86_32': [
            'ncval_base_x86_32',
            ],
        'ncval_base_x86_32': [
            'platform',
            ],
        'nc_opcode_modeling_verbose_x86_32': [
            'nc_opcode_modeling_x86_32',
            'ncval_base_verbose_x86_32',
            ],
        'nc_opcode_modeling_x86_32': [
            'ncval_base_x86_32',
            ],
        'ncval_reg_sfi_verbose_x86_32': [
            'ncval_reg_sfi_x86_32',
            'nc_opcode_modeling_verbose_x86_32',
            ],
        'ncval_reg_sfi_x86_32': [
            'nccopy_x86_32',
            'ncval_base_x86_32',
            'nc_decoder_x86_32',
            ],
        'ncval_seg_sfi_x86_32': [
            'nccopy_x86_32',
            'ncdis_seg_sfi_x86_32',
            'ncval_base_x86_32',
            # When turning on the DEBUGGING flag in the x86-32 validator
            # or decoder, add the following:
            #'nc_opcode_modeling_verbose_x86_32',
            ],
        }),
    'x86-64': _AddDefaultLibraryDependencies({
        'nc_decoder_x86_64': [
            'ncval_base_x86_64',
            'nc_opcode_modeling_x86_64',
            # When turning on the DEBUGGING flag in the x86-64 validator
            # or decoder, add the following:
            #'nc_opcode_modeling_verbose_x86_64',
            ],
        'ncdis_util_x86_64': [
            'ncval_reg_sfi_verbose_x86_64',
            'ncdis_seg_sfi_verbose_x86_64',
            ],
        'ncdis_seg_sfi_verbose_x86_64': [
            'ncdis_seg_sfi_x86_64',
            'ncval_base_verbose_x86_64',
            ],
        'ncvalidate_verbose_x86_64': [
            'ncvalidate_x86_64',
            'ncval_reg_sfi_verbose_x86_64',
            ],
        'ncvalidate_x86_64': [
            'ncval_reg_sfi_x86_64',
            ],
        'ncval_base_verbose_x86_64': [
            'ncval_base_x86_64',
            ],
        'ncval_base_x86_64': [
            'platform',
            ],
        'nc_opcode_modeling_verbose_x86_64': [
            'nc_opcode_modeling_x86_64',
            'ncval_base_verbose_x86_64',
            ],
        'nc_opcode_modeling_x86_64': [
            'ncval_base_x86_64',
            ],
        'ncval_reg_sfi_verbose_x86_64': [
            'ncval_reg_sfi_x86_64',
            'nc_opcode_modeling_verbose_x86_64',
            ],
        'ncval_reg_sfi_x86_64': [
            'nccopy_x86_64',
            'ncval_base_x86_64',
            'nc_decoder_x86_64',
            ],
        'ncval_seg_sfi_x86_64': [
            'nccopy_x86_64',
            'ncdis_seg_sfi_x86_64',
            'ncval_base_x86_64',
            ],
        }),
    'arm': _AddDefaultLibraryDependencies({
        'ncvalidate_arm_v2': [
            'arm_validator_core',
            ],
        }),
    'arm-thumb2': _AddDefaultLibraryDependencies({
        'ncvalidate_arm_v2': [
            'arm_validator_core',
            ],
        }),
    }

def AddLibDeps(platform, libraries):
  """ Adds dependent libraries to list of libraries.

  Computes the transitive closure of library dependencies for each library
  in the given list. Dependent libraries are added after libraries
  as defined in LIBRARY_DEPENDENCIES, unless there is a cycle. If
  a cycle occurs, it is broken and the remaining (acyclic) graph
  is used. Also removes duplicate library entries.

  Note: Keeps libraries (in same order) as given
  in the argument list. This includes duplicates if specified.
  """
  if not libraries: return []

  visited = set()                    # Nodes already visited
  closure = []                       # Collected closure
  if platform in PLATFORM_LIBRARY_DEPENDENCIES:
    # Use platform specific library dependencies.
    dependencies = PLATFORM_LIBRARY_DEPENDENCIES[platform]
  else:
    # No specific library dependencies defined, use default.
    dependencies = LIBRARY_DEPENDENCIES_DEFAULT

  # Add dependencies of each library to the closure, then add the library to
  # the closure.
  for lib in reversed(libraries):
    # Be sure to remove the library if it is already there, so that it will be
    # added again.
    if lib in visited: visited.remove(lib)
    to_visit = [lib]                 # Nodes needing dependencies added.
    expanding = []                   # libraries with expanded dependencies,
                                     # but not yet added to the closure.
    while to_visit:
      lib = to_visit.pop()
      if lib in visited:
        if expanding and lib is expanding[-1]:
          # Second visit, so we know that dependencies have been added.
          # It is now safe to add lib.
          closure.append(lib)
          expanding.pop()
      else:
        visited.add(lib)
        if lib in dependencies:
          # Must process library dependencies first.
          # Be sure to add library to list of nodes to visit,
          # so that we can add it to the closure once all
          # dependencies have been added.
          to_visit.append(lib)
          for dep in dependencies[lib]:
            to_visit.append(dep)
          expanding.append(lib)
        else:
          # No dependent library, just add.
          closure.append(lib)
  closure.reverse()
  return closure
