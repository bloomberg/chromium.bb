# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Harness for defining library dependencies for scons files."""


# The following is a map from a library, to the corresponding
# list of dependent libraries that must be included after that library, in
# the list of libraries.
LIBRARY_DEPENDENCIES_DEFAULT = {
    'arm_validator_core': [
        'cpu_features',
        ],
    'debug_stub': [
        'sel',
        ],
    'nonnacl_util': [
        'serialization',
        ],
    'platform': [
        'gio',
        ],
    'sel': [
        'nacl_error_code',
        'env_cleanser',
        'manifest_proxy',
        'simple_service',
        'thread_interface',
        'gio_wrapped_desc',
        'nonnacl_srpc',
        'nrd_xfer',
        'nacl_perf_counter',
        'nacl_base',
        'imc',
        'nacl_fault_inject',
        'nacl_interval',
        'platform',
        'platform_qual_lib',
        'gio',
        'validators',
        ],
    'sel_main_chrome': [
        'sel',
        'debug_stub',
        ],
    'sel_main': [
        'sel',
        'debug_stub',
        ],
    'serialization': [
        'platform',
        ],
    'testrunner_browser': [
        'ppapi',
        ],
    'validation_cache': [
        # For CHECK(...)
        'platform',
        ],
    }

# Untrusted only library dependencies.
# Include names here that otherwise clash with trusted names.
UNTRUSTED_LIBRARY_DEPENDENCIES = {
    'ppapi_cpp': [
        'ppapi',
        ],
    }

# Platform specific library dependencies. Mapping from a platform,
# to a map from a library, to the corresponding list of dependendent
# libraries that must be included after that library, in the list
# of libraries.
PLATFORM_LIBRARY_DEPENDENCIES = {
    'x86-32': {
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
            'cpu_features',
            ],
        'ncval_base_verbose_x86_32': [
            'ncval_base_x86_32',
            ],
        'ncval_base_x86_32': [
            'platform',
            'cpu_features',
            'validation_cache',
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
        'dfa_validate_caller_x86_32': [
            'ncval_base_x86_32',
            'nccopy_x86_32',
            ],
        },
    'x86-64': {
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
            'cpu_features',
            ],
        'ncval_base_verbose_x86_64': [
            'ncval_base_x86_64',
            ],
        'ncval_base_x86_64': [
            'platform',
            'cpu_features',
            'validation_cache',
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
            'cpu_features',
            ],
        'ncval_seg_sfi_x86_64': [
            'nccopy_x86_64',
            'ncdis_seg_sfi_x86_64',
            'ncval_base_x86_64',
            ],
        'dfa_validate_caller_x86_64': [
            'ncval_base_x86_64',
            'nccopy_x86_64',
            ],
        },
    'arm': {
        'ncvalidate_arm_v2': [
            'arm_validator_core',
            ],
        'validators': [
            'ncvalidate_arm_v2',
            ],
        },
    'mips32': {
        'ncvalidate_mips': [
            'mips_validator_core',
            'cpu_features',
            ],
        'validators': [
            'ncvalidate_mips',
            ],
        },
    }


def AddLibDeps(env, platform, libraries):
  """ Adds dependent libraries to list of libraries.

  Computes the transitive closure of library dependencies for each library
  in the given list. Dependent libraries are added after libraries
  as defined in LIBRARY_DEPENDENCIES, unless there is a cycle. If
  a cycle occurs, it is broken and the remaining (acyclic) graph
  is used. Also removes duplicate library entries.

  Note: Keeps libraries (in same order) as given
  in the argument list. This includes duplicates if specified.
  """
  visited = set()                    # Nodes already visited
  closure = []                       # Collected closure

  # If library A depends on library B, B must appear in the link line
  # after A.  This is why we reverse the list and reverse it back
  # again later.
  def VisitList(libraries):
    for library in reversed(libraries):
      if library not in visited:
        VisitLibrary(library)

  def GetLibraryDeps(library):
    ret = (LIBRARY_DEPENDENCIES_DEFAULT.get(library, []) +
        PLATFORM_LIBRARY_DEPENDENCIES.get(platform, {}).get(library, []))
    if env['NACL_BUILD_FAMILY'] != 'TRUSTED':
      ret.extend(UNTRUSTED_LIBRARY_DEPENDENCIES.get(library, []))
    if library == 'validators' and env.Bit('target_x86'):
      if env.Bit('validator_ragel'):
        ret.append(env.NaClTargetArchSuffix('dfa_validate_caller'))
      else:
        ret.append(env.NaClTargetArchSuffix('ncvalidate'))
    return ret

  def VisitLibrary(library):
    visited.add(library)
    VisitList(GetLibraryDeps(library))
    closure.append(library)

  # Ideally we would just do "VisitList(libraries)" here, but some
  # PPAPI tests (specifically, tests/ppapi_gles_book) list "ppapi_cpp"
  # twice in the link line, and we need to maintain these duplicates.
  for library in reversed(libraries):
    VisitLibrary(library)

  closure.reverse()
  return closure
