# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This module contains classes that help to emulate xcodebuild behavior on top of
other build systems, such as make and ninja.
"""

import os.path

class XcodeSettings(object):
  """A class that understands the gyp 'xcode_settings' object."""

  def __init__(self, spec):
    self.spec = spec

    # Per-target 'xcode_settings' are pushed down into configs earlier by gyp.
    # This means self.xcode_settings[config] always contains all settings
    # for that config -- the per-target settings as well. Settings that are
    # the same for all configs are implicitly per-target settings.
    self.xcode_settings = {}
    configs = spec['configurations']
    for configname, config in configs.iteritems():
      self.xcode_settings[configname] = config.get('xcode_settings', {})

    # This is only non-None temporarily during the execution of some methods.
    self.configname = None

  def _Settings(self):
    assert self.configname
    return self.xcode_settings[self.configname]

  def _Test(self, test_key, cond_key, default):
    return self._Settings().get(test_key, default) == cond_key

  def _Appendf(self, lst, test_key, format_str, default=None):
    if test_key in self._Settings():
      lst.append(format_str % str(self._Settings()[test_key]))
    elif default:
      lst.append(format_str % str(default))

  def _WarnUnimplemented(self, test_key):
    if test_key in self._Settings():
      print 'Warning: Ignoring not yet implemented key "%s".' % test_key

  def _IsBundle(self):
    return int(self.spec.get('mac_bundle', 0)) != 0

  def GetFrameworkVersion(self):
    """Returns the framework version of the current target. Only valid for
    bundles."""
    assert self._IsBundle()
    return self.GetPerTargetSetting('FRAMEWORK_VERSION', default='A')

  def GetWrapperExtension(self):
    """Returns the bundle extension (.app, .framework, .plugin, etc).  Only
    valid for bundles."""
    assert self._IsBundle()
    if self.spec['type'] in ('loadable_module', 'shared_library'):
      default_wrapper_extension = {
        'loadable_module': 'bundle',
        'shared_library': 'framework',
      }[self.spec['type']]
      wrapper_extension = self.GetPerTargetSetting(
          'WRAPPER_EXTENSION', default=default_wrapper_extension)
      return '.' + self.spec.get('product_extension', wrapper_extension)
    elif self.spec['type'] == 'executable':
      return '.app'
    else:
      assert False, "Don't know extension for '%s', target '%s'" % (
          self.spec['type'], self.spec['target_name'])

  def GetProductName(self):
    """Returns PRODUCT_NAME."""
    return self.spec.get('product_name', self.spec['target_name'])

  def GetWrapperName(self):
    """Returns the directory name of the bundle represented by this target.
    Only valid for bundles."""
    assert self._IsBundle()
    return self.GetProductName() + self.GetWrapperExtension()

  def GetBundleContentsFolderPath(self):
    """Returns the qualified path to the bundle's contents folder. E.g.
    Chromium.app/Contents or Foo.bundle/Versions/A. Only valid for bundles."""
    assert self._IsBundle()
    if self.spec['type'] == 'shared_library':
      return os.path.join(
          self.GetWrapperName(), 'Versions', self.GetFrameworkVersion())
    else:
      # loadable_modules have a 'Contents' folder like executables.
      return os.path.join(self.GetWrapperName(), 'Contents')

  def GetBundleResourceFolder(self):
    """Returns the qualified path to the bundle's resource folder. E.g.
    Chromium.app/Contents/Resources. Only valid for bundles."""
    assert self._IsBundle()
    return os.path.join(self.GetBundleContentsFolderPath(), 'Resources')

  def GetBundlePlistPath(self):
    """Returns the qualified path to the bundle's plist file. E.g.
    Chromium.app/Contents/Info.plist. Only valid for bundles."""
    assert self._IsBundle()
    if self.spec['type'] in ('executable', 'loadable_module'):
      return os.path.join(self.GetBundleContentsFolderPath(), 'Info.plist')
    else:
      return os.path.join(self.GetBundleContentsFolderPath(),
                          'Resources', 'Info.plist')

  def GetProductType(self):
    """Returns the PRODUCT_TYPE of this target."""
    if self._IsBundle():
      return {
        'executable': 'com.apple.product-type.application',
        'loadable_module': 'com.apple.product-type.bundle',
        'shared_library': 'com.apple.product-type.framework',
      }[self.spec['type']]
    else:
      return {
        'executable': 'com.apple.product-type.tool',
        'loadable_module': 'com.apple.product-type.library.dynamic',
        'shared_library': 'com.apple.product-type.library.dynamic',
        'static_library': 'com.apple.product-type.library.static',
      }[self.spec['type']]

  def GetMachOType(self):
    """Returns the MACH_O_TYPE of this target."""
    # Weird, but matches Xcode.
    if not self._IsBundle() and self.spec['type'] == 'executable':
      return ''
    return {
      'executable': 'mh_execute',
      'static_library': 'staticlib',
      'shared_library': 'mh_dylib',
      'loadable_module': 'mh_bundle',
    }[self.spec['type']]

  def _GetBundleBinaryPath(self):
    """Returns the name of the bundle binary of by this target.
    E.g. Chromium.app/Contents/MacOS/Chromium. Only valid for bundles."""
    assert self._IsBundle()
    if self.spec['type'] in ('shared_library'):
      path = self.GetBundleContentsFolderPath()
    elif self.spec['type'] in ('executable', 'loadable_module'):
      path = os.path.join(self.GetBundleContentsFolderPath(), 'MacOS')
    return os.path.join(path, self.spec.get('product_name',
                                            self.spec['target_name']))

  def _GetStandaloneExecutableSuffix(self):
    if 'product_extension' in self.spec:
      return '.' + self.spec['product_extension']
    return {
      'executable': '',
      'static_library': '.a',
      'shared_library': '.dylib',
      'loadable_module': '.so',
    }[self.spec['type']]

  def _GetStandaloneExecutablePrefix(self):
    return self.spec.get('product_prefix', {
      'executable': '',
      'static_library': 'lib',
      'shared_library': 'lib',
      # Non-bundled loadable_modules are called foo.so for some reason
      # (that is, .so and no prefix) with the xcode build -- match that.
      'loadable_module': '',
    }[self.spec['type']])

  def _GetStandaloneBinaryPath(self):
    """Returns the name of the non-bundle binary represented by this target.
    E.g. hello_world. Only valid for non-bundles."""
    assert not self._IsBundle()
    assert self.spec['type'] in (
        'executable', 'shared_library', 'static_library', 'loadable_module')
    target = self.spec['target_name']
    if self.spec['type'] == 'static_library':
      if target[:3] == 'lib':
        target = target[3:]
    elif self.spec['type'] in ('loadable_module', 'shared_library'):
      if target[:3] == 'lib':
        target = target[3:]

    target_prefix = self._GetStandaloneExecutablePrefix()
    target = self.spec.get('product_name', target)
    target_ext = self._GetStandaloneExecutableSuffix()
    return target_prefix + target + target_ext

  def GetExecutablePath(self):
    """Returns the directory name of the bundle represented by this target. E.g.
    Chromium.app/Contents/MacOS/Chromium."""
    if self._IsBundle():
      return self._GetBundleBinaryPath()
    else:
      return self._GetStandaloneBinaryPath()

  def _SdkPath(self):
    sdk_root = self.GetPerTargetSetting('SDKROOT', default='macosx10.5')
    if sdk_root.startswith('macosx'):
      sdk_root = 'MacOSX' + sdk_root[len('macosx'):]
    return '/Developer/SDKs/%s.sdk' % sdk_root

  def GetCflags(self, configname):
    """Returns flags that need to be added to .c, .cc, .m, and .mm
    compilations."""
    # This functions (and the similar ones below) do not offer complete
    # emulation of all xcode_settings keys. They're implemented on demand.

    self.configname = configname
    cflags = []

    sdk_root = self._SdkPath()
    if 'SDKROOT' in self._Settings():
      cflags.append('-isysroot %s' % sdk_root)

    if self._Test('GCC_CW_ASM_SYNTAX', 'YES', default='YES'):
      cflags.append('-fasm-blocks')

    if 'GCC_DYNAMIC_NO_PIC' in self._Settings():
      if self._Settings()['GCC_DYNAMIC_NO_PIC'] == 'YES':
        cflags.append('-mdynamic-no-pic')
    else:
      pass
      # TODO: In this case, it depends on the target. xcode passes
      # mdynamic-no-pic by default for executable and possibly static lib
      # according to mento

    if self._Test('GCC_ENABLE_PASCAL_STRINGS', 'YES', default='YES'):
      cflags.append('-mpascal-strings')

    self._Appendf(cflags, 'GCC_OPTIMIZATION_LEVEL', '-O%s', default='s')

    if self._Test('GCC_GENERATE_DEBUGGING_SYMBOLS', 'YES', default='YES'):
      dbg_format = self._Settings().get('DEBUG_INFORMATION_FORMAT', 'dwarf')
      if dbg_format == 'dwarf':
        cflags.append('-gdwarf-2')
      elif dbg_format == 'stabs':
        raise NotImplementedError('stabs debug format is not supported yet.')
      elif dbg_format == 'dwarf-with-dsym':
        cflags.append('-gdwarf-2')
      else:
        raise NotImplementedError('Unknown debug format %s' % dbg_format)

    if self._Test('GCC_SYMBOLS_PRIVATE_EXTERN', 'YES', default='NO'):
      cflags.append('-fvisibility=hidden')

    if self._Test('GCC_TREAT_WARNINGS_AS_ERRORS', 'YES', default='NO'):
      cflags.append('-Werror')

    if self._Test('GCC_WARN_ABOUT_MISSING_NEWLINE', 'YES', default='NO'):
      cflags.append('-Wnewline-eof')

    self._Appendf(cflags, 'MACOSX_DEPLOYMENT_TARGET', '-mmacosx-version-min=%s')

    # TODO:
    self._WarnUnimplemented('ARCHS')
    if self._Test('COPY_PHASE_STRIP', 'YES', default='NO'):
      self._WarnUnimplemented('COPY_PHASE_STRIP')
    self._WarnUnimplemented('GCC_DEBUGGING_SYMBOLS')
    self._WarnUnimplemented('GCC_ENABLE_OBJC_EXCEPTIONS')
    self._WarnUnimplemented('GCC_ENABLE_OBJC_GC')

    # TODO: This is exported correctly, but assigning to it is not supported.
    self._WarnUnimplemented('MACH_O_TYPE')
    self._WarnUnimplemented('PRODUCT_TYPE')

    # TODO: Do not hardcode arch. Supporting fat binaries will be annoying.
    cflags.append('-arch i386')

    cflags += self._Settings().get('OTHER_CFLAGS', [])
    cflags += self._Settings().get('WARNING_CFLAGS', [])

    config = self.spec['configurations'][self.configname]
    framework_dirs = config.get('mac_framework_dirs', [])
    for directory in framework_dirs:
      cflags.append('-F ' + directory.replace('$(SDKROOT)', sdk_root))

    self.configname = None
    return cflags

  def GetCflagsC(self, configname):
    """Returns flags that need to be added to .c, and .m compilations."""
    self.configname = configname
    cflags_c = []
    self._Appendf(cflags_c, 'GCC_C_LANGUAGE_STANDARD', '-std=%s')
    self.configname = None
    return cflags_c

  def GetCflagsCC(self, configname):
    """Returns flags that need to be added to .cc, and .mm compilations."""
    self.configname = configname
    cflags_cc = []
    if self._Test('GCC_ENABLE_CPP_RTTI', 'NO', default='YES'):
      cflags_cc.append('-fno-rtti')
    if self._Test('GCC_ENABLE_CPP_EXCEPTIONS', 'NO', default='YES'):
      cflags_cc.append('-fno-exceptions')
    if self._Test('GCC_INLINES_ARE_PRIVATE_EXTERN', 'YES', default='NO'):
      cflags_cc.append('-fvisibility-inlines-hidden')
    if self._Test('GCC_THREADSAFE_STATICS', 'NO', default='YES'):
      cflags_cc.append('-fno-threadsafe-statics')
    self.configname = None
    return cflags_cc

  def GetCflagsObjC(self, configname):
    """Returns flags that need to be added to .m compilations."""
    self.configname = configname
    self.configname = None
    return []

  def GetCflagsObjCC(self, configname):
    """Returns flags that need to be added to .mm compilations."""
    self.configname = configname
    cflags_objcc = []
    if self._Test('GCC_OBJC_CALL_CXX_CDTORS', 'YES', default='NO'):
      cflags_objcc.append('-fobjc-call-cxx-cdtors')
    self.configname = None
    return cflags_objcc

  def GetLdflags(self, configname, product_dir, map_gyp_to_build_path):
    """Returns flags that need to be passed to the linker.

    Args:
        configname: The name of the configuration to get ld flags for.
        product_dir: The directory where products such static and dynamic
            libraries are placed. This is added to the library search path.
        map_gyp_to_build_path: A function that converts paths relative to the
            current gyp file to paths relative to the build direcotry.
    """
    self.configname = configname
    ldflags = []

    # The xcode build is relative to a gyp file's directory, and OTHER_LDFLAGS
    # contains two entries that depend on this. Explicitly absolutify for these
    # two cases.
    def MapGypPathWithPrefix(flag, prefix):
      if flag.startswith(prefix):
        flag = prefix + map_gyp_to_build_path(flag[len(prefix):])
      return flag
    for ldflag in self._Settings().get('OTHER_LDFLAGS', []):
      # Required for ffmpeg (no idea why they don't use LIBRARY_SEARCH_PATHS,
      # TODO(thakis): Update ffmpeg.gyp):
      ldflag = MapGypPathWithPrefix(ldflag, '-L')
      # Required for the nacl plugin:
      ldflag = MapGypPathWithPrefix(ldflag, '-Wl,-exported_symbols_list ')
      ldflags.append(ldflag)

    if self._Test('DEAD_CODE_STRIPPING', 'YES', default='NO'):
      ldflags.append('-Wl,-dead_strip')

    if self._Test('PREBINDING', 'YES', default='NO'):
      ldflags.append('-Wl,-prebind')

    self._Appendf(
        ldflags, 'DYLIB_COMPATIBILITY_VERSION', '-compatibility_version %s')
    self._Appendf(
        ldflags, 'DYLIB_CURRENT_VERSION', '-current_version %s')
    self._Appendf(
        ldflags, 'MACOSX_DEPLOYMENT_TARGET', '-mmacosx-version-min=%s')
    if 'SDKROOT' in self._Settings():
      ldflags.append('-isysroot ' + self._SdkPath())

    for library_path in self._Settings().get('LIBRARY_SEARCH_PATHS', []):
      ldflags.append('-L' + library_path)

    if 'ORDER_FILE' in self._Settings():
      ldflags.append('-Wl,-order_file ' +
                     '-Wl,' + map_gyp_to_build_path(
                                  self._Settings()['ORDER_FILE']))

    # TODO: Do not hardcode arch. Supporting fat binaries will be annoying.
    ldflags.append('-arch i386')

    # Xcode adds the product directory by default.
    ldflags.append('-L' + product_dir)

    install_name = self.GetPerTargetSetting('LD_DYLIB_INSTALL_NAME')
    install_base = self.GetPerTargetSetting('DYLIB_INSTALL_NAME_BASE')
    default_install_name = \
          '$(DYLIB_INSTALL_NAME_BASE:standardizepath)/$(EXECUTABLE_PATH)'
    if not install_name and install_base:
      install_name = default_install_name

    if install_name:
      # Hardcode support for the variables used in chromium for now, to unblock
      # people using the make build.
      if '$' in install_name:
        assert install_name in ('$(DYLIB_INSTALL_NAME_BASE:standardizepath)/'
            '$(WRAPPER_NAME)/$(PRODUCT_NAME)', default_install_name), (
            'Variables in LD_DYLIB_INSTALL_NAME are not generally supported yet'
            ' in target \'%s\' (got \'%s\')' %
                (self.spec['target_name'], install_name))
        # I'm not quite sure what :standardizepath does. Just call normpath(),
        # but don't let @executable_path/../foo collapse to foo.
        if '/' in install_base:
          prefix, rest = '', install_base
          if install_base.startswith('@'):
            prefix, rest = install_base.split('/', 1)
          rest = os.path.normpath(rest)  # :standardizepath
          install_base = os.path.join(prefix, rest)

        install_name = install_name.replace(
            '$(DYLIB_INSTALL_NAME_BASE:standardizepath)', install_base)
        if self._IsBundle():
          # These are only valid for bundles, hence the |if|.
          install_name = install_name.replace(
              '$(WRAPPER_NAME)', self.GetWrapperName())
          install_name = install_name.replace(
              '$(PRODUCT_NAME)', self.GetProductName())
        else:
          assert '$(WRAPPER_NAME)' not in install_name
          assert '$(PRODUCT_NAME)' not in install_name

        install_name = install_name.replace(
            '$(EXECUTABLE_PATH)', self.GetExecutablePath())

      install_name = install_name.replace(' ', r'\ ')
      ldflags.append('-install_name ' + install_name)

    self.configname = None
    return ldflags

  def GetPerTargetSettings(self):
    """Gets a list of all the per-target settings. This will only fetch keys
    whose values are the same across all configurations."""
    first_pass = True
    result = {}
    for configname in sorted(self.xcode_settings.keys()):
      if first_pass:
        result = dict(self.xcode_settings[configname])
        first_pass = False
      else:
        for key, value in self.xcode_settings[configname].iteritems():
          if key not in result:
            continue
          elif result[key] != value:
            del result[key]
    return result

  def GetPerTargetSetting(self, setting, default=None):
    """Tries to get xcode_settings.setting from spec. Assumes that the setting
       has the same value in all configurations and throws otherwise."""
    first_pass = True
    result = None
    for configname in sorted(self.xcode_settings.keys()):
      if first_pass:
        result = self.xcode_settings[configname].get(setting, None)
        first_pass = False
      else:
        assert result == self.xcode_settings[configname].get(setting, None), (
            "Expected per-target setting for '%s', got per-config setting "
            "(target %s)" % (setting, spec['target_name']))
    if result is None:
      return default
    return result

  def _GetStripPostbuilds(self, configname, output_binary):
    """Returns a list of shell commands that contain the shell commands
    neccessary to strip this target's binary. These should be run as postbuilds
    before the actual postbuilds run."""
    self.configname = configname

    result = []
    if (self._Test('DEPLOYMENT_POSTPROCESSING', 'YES', default='NO') and
        self._Test('STRIP_INSTALLED_PRODUCT', 'YES', default='NO')):

      default_strip_style = 'debugging'
      if self._IsBundle():
        default_strip_style = 'non-global'
      elif self.spec['type'] == 'executable':
        default_strip_style = 'all'

      strip_style = self._Settings().get('STRIP_STYLE', default_strip_style)
      strip_flags = {
        'all': '',
        'non-global': '-x',
        'debugging': '-S',
      }[strip_style]

      explicit_strip_flags = self._Settings().get('STRIPFLAGS', '')
      if explicit_strip_flags:
        strip_flags += ' ' + explicit_strip_flags

      result.append('echo STRIP\\(%s\\)' % self.spec['target_name'])
      result.append('strip %s %s' % (strip_flags, output_binary))

    self.configname = None
    return result

  def _GetDebugPostbuilds(self, configname, output, output_binary):
    """Returns a list of shell commands that contain the shell commands
    neccessary to massage this target's debug information. These should be run
    as postbuilds before the actual postbuilds run."""
    self.configname = configname

    # For static libraries, no dSYMs are created.
    result = []
    if (self._Test('GCC_GENERATE_DEBUGGING_SYMBOLS', 'YES', default='YES') and
        self._Test(
            'DEBUG_INFORMATION_FORMAT', 'dwarf-with-dsym', default='dwarf') and
        self.spec['type'] != 'static_library'):
      result.append('echo DSYMUTIL\\(%s\\)' % self.spec['target_name'])
      result.append('dsymutil %s -o %s' % (output_binary, output + '.dSYM'))

    self.configname = None
    return result

  def GetTargetPostbuilds(self, configname, output, output_binary):
    """Returns a list of shell commands that contain the shell commands
    to run as postbuilds for this target, before the actual postbuilds."""
    # dSYMs need to build before stripping happens.
    return (self._GetDebugPostbuilds(configname, output, output_binary) +
            self._GetStripPostbuilds(configname, output_binary))

  def AdjustFrameworkLibraries(self, libraries):
    """Transforms entries like 'Cocoa.framework' in libraries into entries like
    '-framework Cocoa'.
    """
    libraries = [
        '-framework ' + os.path.splitext(os.path.basename(library))[0]
        if library.endswith('.framework') else library
        for library in libraries]
    libraries = [library.replace('$(SDKROOT)', self._SdkPath())
        for library in libraries]
    return libraries


def MergeGlobalXcodeSettingsToSpec(global_dict, spec):
  """Merges the global xcode_settings dictionary into each configuration of the
  target represented by spec. For keys that are both in the global and the local
  xcode_settings dict, the local key gets precendence.
  """
  # The xcode generator special-cases global xcode_settings and does something
  # that amounts to merging in the global xcode_settings into each local
  # xcode_settings dict.
  global_xcode_settings = global_dict.get('xcode_settings', {})
  for config in spec['configurations'].values():
    if 'xcode_settings' in config:
      new_settings = global_xcode_settings.copy()
      new_settings.update(config['xcode_settings'])
      config['xcode_settings'] = new_settings
