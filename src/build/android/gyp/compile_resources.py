#!/usr/bin/env python
# encoding: utf-8
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Compile Android resources into an intermediate APK.

This can also generate an R.txt, and an .srcjar file containing the proper
final R.java class for all resource packages the APK depends on.

This will crunch images with aapt2.
"""

import argparse
import collections
import multiprocessing.pool
import os
import re
import shutil
import subprocess
import sys
import zipfile
from xml.etree import ElementTree


from util import build_utils
from util import resource_utils

# Name of environment variable that can be used to force this script to
# put temporary resource files into specific sub-directories, instead of
# temporary ones.
_ENV_DEBUG_VARIABLE = 'ANDROID_DEBUG_TEMP_RESOURCES_DIR'

# Import jinja2 from third_party/jinja2
sys.path.insert(1, os.path.join(build_utils.DIR_SOURCE_ROOT, 'third_party'))
from jinja2 import Template # pylint: disable=F0401

# Pngs that we shouldn't convert to webp. Please add rationale when updating.
_PNG_WEBP_BLACKLIST_PATTERN = re.compile('|'.join([
    # Crashes on Galaxy S5 running L (https://crbug.com/807059).
    r'.*star_gray\.png',
    # Android requires pngs for 9-patch images.
    r'.*\.9\.png',
    # Daydream requires pngs for icon files.
    r'.*daydream_icon_.*\.png']))

# Regular expression for package declaration in 'aapt dump resources' output.
_RE_PACKAGE_DECLARATION = re.compile(
    r'^Package Group ([0-9]+) id=0x([0-9a-fA-F]+)')


def _PackageIdArgument(x):
  """Convert a string into a package ID while checking its range.

  Args:
    x: argument string.
  Returns:
    the package ID as an int, or -1 in case of error.
  """
  try:
    x = int(x, 0)
    if x < 0 or x > 127:
      x = -1
  except ValueError:
    x = -1
  return x


def _ListToDictionary(lst, separator):
  """Splits each element of the passed-in |lst| using |separator| and creates
  dictionary treating first element of the split as the key and second as the
  value."""
  return dict(item.split(separator, 1) for item in lst)


def _ParseArgs(args):
  """Parses command line options.

  Returns:
    An options object as from argparse.ArgumentParser.parse_args()
  """
  parser, input_opts, output_opts = resource_utils.ResourceArgsParser()

  input_opts.add_argument('--android-manifest', required=True,
                          help='AndroidManifest.xml path')

  input_opts.add_argument(
      '--shared-resources',
      action='store_true',
      help='Make all resources in R.java non-final and allow the resource IDs '
           'to be reset to a different package index when the apk is loaded by '
           'another application at runtime.')

  input_opts.add_argument(
      '--app-as-shared-lib',
      action='store_true',
      help='Same as --shared-resources, but also ensures all resource IDs are '
           'directly usable from the APK loaded as an application.')

  input_opts.add_argument(
      '--shared-resources-whitelist',
      help='An R.txt file acting as a whitelist for resources that should be '
           'non-final and have their package ID changed at runtime in R.java. '
           'Implies and overrides --shared-resources.')

  input_opts.add_argument(
      '--shared-resources-whitelist-locales',
      default='[]',
      help='Optional GN-list of locales. If provided, all strings corresponding'
      ' to this locale list will be kept in the final output for the '
      'resources identified through --shared-resources-whitelist, even '
      'if --locale-whitelist is being used.')

  input_opts.add_argument('--proto-format', action='store_true',
                          help='Compile resources to protocol buffer format.')

  input_opts.add_argument('--support-zh-hk', action='store_true',
                          help='Use zh-rTW resources for zh-rHK.')

  input_opts.add_argument('--debuggable',
                          action='store_true',
                          help='Whether to add android:debuggable="true"')

  input_opts.add_argument('--version-code', help='Version code for apk.')
  input_opts.add_argument('--version-name', help='Version name for apk.')

  input_opts.add_argument(
      '--no-compress',
      help='disables compression for the given comma-separated list of '
           'extensions')

  input_opts.add_argument(
      '--locale-whitelist',
      default='[]',
      help='GN list of languages to include. All other language configs will '
          'be stripped out. List may include a combination of Android locales '
          'or Chrome locales.')

  input_opts.add_argument('--resource-blacklist-regex', default='',
                          help='Do not include matching drawables.')

  input_opts.add_argument(
      '--resource-blacklist-exceptions',
      default='[]',
      help='GN list of globs that say which blacklisted images to include even '
           'when --resource-blacklist-regex is set.')

  input_opts.add_argument('--png-to-webp', action='store_true',
                          help='Convert png files to webp format.')

  input_opts.add_argument('--webp-binary', default='',
                          help='Path to the cwebp binary.')

  input_opts.add_argument('--no-xml-namespaces',
                          action='store_true',
                          help='Whether to strip xml namespaces from processed '
                               'xml resources')
  input_opts.add_argument(
      '--resources-config-path', help='Path to aapt2 resources config file.')
  input_opts.add_argument(
      '--optimize-resources',
      default=False,
      action='store_true',
      help='Whether to run the `aapt2 optimize` step on the resources.')
  input_opts.add_argument(
      '--unoptimized-resources-path',
      help='Path to output the intermediate apk before running '
      '`aapt2 optimize`.')

  input_opts.add_argument(
      '--check-resources-pkg-id', type=_PackageIdArgument,
      help='Check the package ID of the generated resources table. '
           'Value must be integer in [0..127] range.')

  output_opts.add_argument('--apk-path', required=True,
                           help='Path to output (partial) apk.')

  output_opts.add_argument('--apk-info-path', required=True,
                           help='Path to output info file for the partial apk.')

  output_opts.add_argument('--srcjar-out',
                           help='Path to srcjar to contain generated R.java.')

  output_opts.add_argument('--r-text-out',
                           help='Path to store the generated R.txt file.')

  output_opts.add_argument('--proguard-file',
                           help='Path to proguard.txt generated file')

  output_opts.add_argument(
      '--proguard-file-main-dex',
      help='Path to proguard.txt generated file for main dex')

  options = parser.parse_args(args)

  resource_utils.HandleCommonOptions(options)

  options.locale_whitelist = build_utils.ParseGnList(options.locale_whitelist)
  options.shared_resources_whitelist_locales = build_utils.ParseGnList(
      options.shared_resources_whitelist_locales)
  options.resource_blacklist_exceptions = build_utils.ParseGnList(
      options.resource_blacklist_exceptions)

  if options.check_resources_pkg_id is not None:
    if options.check_resources_pkg_id < 0:
      raise Exception(
          'Package resource id should be integer in [0..127] range.')

  if options.shared_resources and options.app_as_shared_lib:
    raise Exception('Only one of --app-as-shared-lib or --shared-resources '
                    'can be used.')

  if options.package_name_to_id_mapping:
    package_names_list = build_utils.ParseGnList(
        options.package_name_to_id_mapping)
    options.package_name_to_id_mapping = _ListToDictionary(
        package_names_list, '=')

  return options


def _ExtractPackageIdFromApk(apk_path, aapt_path):
  """Extract the package ID of a given APK (even intermediate ones).

  Args:
    apk_path: Input apk path.
    aapt_path: Path to aapt tool.
  Returns:
    An integer corresponding to the APK's package id.
  Raises:
    Exception if there is no resources table in the input file.
  """
  cmd_args = [ aapt_path, 'dump', 'resources', apk_path ]
  output = build_utils.CheckOutput(cmd_args)

  for line in output.splitlines():
    m = _RE_PACKAGE_DECLARATION.match(line)
    if m:
      return int(m.group(2), 16)

  raise Exception("No resources in this APK!")


def _SortZip(original_path, sorted_path):
  """Generate new zip archive by sorting all files in the original by name."""
  with zipfile.ZipFile(sorted_path, 'w') as sorted_zip, \
      zipfile.ZipFile(original_path, 'r') as original_zip:
    for info in sorted(original_zip.infolist(), key=lambda i: i.filename):
      sorted_zip.writestr(info, original_zip.read(info))


def _IterFiles(root_dir):
  for root, _, files in os.walk(root_dir):
    for f in files:
      yield os.path.join(root, f)


def _DuplicateZhResources(resource_dirs):
  """Duplicate Taiwanese resources into Hong-Kong specific directory."""
  renamed_paths = dict()
  for resource_dir in resource_dirs:
    # We use zh-TW resources for zh-HK (if we have zh-TW resources).
    for path in _IterFiles(resource_dir):
      if 'zh-rTW' in path:
        hk_path = path.replace('zh-rTW', 'zh-rHK')
        build_utils.MakeDirectory(os.path.dirname(hk_path))
        shutil.copyfile(path, hk_path)
        renamed_paths[os.path.relpath(hk_path, resource_dir)] = os.path.relpath(
            path, resource_dir)
  return renamed_paths


def _RenameLocaleResourceDirs(resource_dirs):
  """Rename locale resource directories into standard names when necessary.

  This is necessary to deal with the fact that older Android releases only
  support ISO 639-1 two-letter codes, and sometimes even obsolete versions
  of them.

  In practice it means:
    * 3-letter ISO 639-2 qualifiers are renamed under a corresponding
      2-letter one. E.g. for Filipino, strings under values-fil/ will be moved
      to a new corresponding values-tl/ sub-directory.

    * Modern ISO 639-1 codes will be renamed to their obsolete variant
      for Indonesian, Hebrew and Yiddish (e.g. 'values-in/ -> values-id/).

    * Norwegian macrolanguage strings will be renamed to Bokmål (main
      Norway language). See http://crbug.com/920960. In practice this
      means that 'values-no/ -> values-nb/' unless 'values-nb/' already
      exists.

    * BCP 47 langauge tags will be renamed to an equivalent ISO 639-1
      locale qualifier if possible (e.g. 'values-b+en+US/ -> values-en-rUS').
      Though this is not necessary at the moment, because no third-party
      package that Chromium links against uses these for the current list of
      supported locales, this may change when the list is extended in the
      future).

  Args:
    resource_dirs: list of top-level resource directories.
  Returns:
    A dictionary mapping renamed paths to their original location
    (e.g. '.../values-tl/strings.xml' -> ' .../values-fil/strings.xml').
  """
  renamed_paths = dict()
  for resource_dir in resource_dirs:
    for path in _IterFiles(resource_dir):
      locale = resource_utils.FindLocaleInStringResourceFilePath(path)
      if not locale:
        continue
      cr_locale = resource_utils.ToChromiumLocaleName(locale)
      if not cr_locale:
        continue  # Unsupported Android locale qualifier!?
      locale2 = resource_utils.ToAndroidLocaleName(cr_locale)
      if locale != locale2:
        path2 = path.replace('/values-%s/' % locale, '/values-%s/' % locale2)
        if path == path2:
          raise Exception('Could not substitute locale %s for %s in %s' %
                          (locale, locale2, path))
        if os.path.exists(path2):
          # This happens sometimes, e.g. some libraries provide both
          # values-nb/ and values-no/ with the same content.
          continue
        build_utils.MakeDirectory(os.path.dirname(path2))
        shutil.move(path, path2)
        renamed_paths[os.path.relpath(path2, resource_dir)] = os.path.relpath(
            path, resource_dir)
  return renamed_paths


def _ToAndroidLocales(locale_whitelist, support_zh_hk):
  """Converts the list of Chrome locales to Android config locale qualifiers.

  Args:
    locale_whitelist: A list of Chromium locale names.
    support_zh_hk: True if we need to support zh-HK by duplicating
      the zh-TW strings.
  Returns:
    A set of matching Android config locale qualifier names.
  """
  ret = set()
  for locale in locale_whitelist:
    locale = resource_utils.ToAndroidLocaleName(locale)
    if locale is None or ('-' in locale and '-r' not in locale):
      raise Exception('Unsupported Chromium locale name: %s' % locale)
    ret.add(locale)
    # Always keep non-regional fall-backs.
    language = locale.split('-')[0]
    ret.add(language)

  # We don't actually support zh-HK in Chrome on Android, but we mimic the
  # native side behavior where we use zh-TW resources when the locale is set to
  # zh-HK. See https://crbug.com/780847.
  if support_zh_hk:
    assert not any('HK' in l for l in locale_whitelist), (
        'Remove special logic if zh-HK is now supported (crbug.com/780847).')
    ret.add('zh-rHK')
  return set(ret)


def _MoveImagesToNonMdpiFolders(res_root):
  """Move images from drawable-*-mdpi-* folders to drawable-* folders.

  Why? http://crbug.com/289843
  """
  renamed_paths = dict()
  for src_dir_name in os.listdir(res_root):
    src_components = src_dir_name.split('-')
    if src_components[0] != 'drawable' or 'mdpi' not in src_components:
      continue
    src_dir = os.path.join(res_root, src_dir_name)
    if not os.path.isdir(src_dir):
      continue
    dst_components = [c for c in src_components if c != 'mdpi']
    assert dst_components != src_components
    dst_dir_name = '-'.join(dst_components)
    dst_dir = os.path.join(res_root, dst_dir_name)
    build_utils.MakeDirectory(dst_dir)
    for src_file_name in os.listdir(src_dir):
      if not os.path.splitext(src_file_name)[1] in ('.png', '.webp'):
        continue
      src_file = os.path.join(src_dir, src_file_name)
      dst_file = os.path.join(dst_dir, src_file_name)
      assert not os.path.lexists(dst_file)
      shutil.move(src_file, dst_file)
      renamed_paths[os.path.relpath(dst_file, res_root)] = os.path.relpath(
          src_file, res_root)
  return renamed_paths


def _GetLinkOptionsForPackage(options):
  """Returns package-specific link options for aapt2 based on package_name and
  package_name_to_id_mapping passed through command-line options."""
  link_options = []
  if not options.package_name:
    return link_options

  if options.package_name in options.package_name_to_id_mapping:
    package_id = options.package_name_to_id_mapping[options.package_name]
    link_options += ['--package-id', package_id]
    if int(package_id, 16) < 0x7f:
      link_options.append('--allow-reserved-package-id')
  else:
    raise Exception(
        'Package name %s is not present in package_name_to_id_mapping.' %
        options.package_name)
  return link_options


def _CreateLinkApkArgs(options):
  """Create command-line arguments list to invoke 'aapt2 link'.

  Args:
    options: The command-line options tuple.
  Returns:
    A list of strings corresponding to the command-line invokation for
    the command, matching the arguments from |options|.
  """
  link_command = [
    options.aapt2_path,
    'link',
    '--version-code', options.version_code,
    '--version-name', options.version_name,
    '--auto-add-overlay',
    '--no-version-vectors',
  ]

  for j in options.include_resources:
    link_command += ['-I', j]
  if options.proguard_file:
    link_command += ['--proguard', options.proguard_file]
  if options.proguard_file_main_dex:
    link_command += ['--proguard-main-dex', options.proguard_file_main_dex]

  if options.no_compress:
    for ext in options.no_compress.split(','):
      link_command += ['-0', ext]

  # Note: only one of --proto-format, --shared-lib or --app-as-shared-lib
  #       can be used with recent versions of aapt2.
  if options.proto_format:
    link_command.append('--proto-format')
  elif options.shared_resources:
    link_command.append('--shared-lib')

  if options.no_xml_namespaces:
    link_command.append('--no-xml-namespaces')

  link_command += _GetLinkOptionsForPackage(options)

  return link_command


def _ExtractVersionFromSdk(aapt_path, sdk_path):
  """Extract version code and name from Android SDK .jar file.

  Args:
    aapt_path: Path to 'aapt' build tool.
    sdk_path: Path to SDK-specific android.jar file.
  Returns:
    A (version_code, version_name) pair of strings.
  """
  output = build_utils.CheckOutput(
      [aapt_path, 'dump', 'badging', sdk_path],
      print_stdout=False, print_stderr=False)
  version_code = re.search(r"versionCode='(.*?)'", output).group(1)
  version_name = re.search(r"versionName='(.*?)'", output).group(1)
  return version_code, version_name,


def _FixManifest(options, temp_dir):
  """Fix the APK's AndroidManifest.xml.

  This adds any missing namespaces for 'android' and 'tools', and
  sets certains elements like 'platformBuildVersionCode' or
  'android:debuggable' depending on the content of |options|.

  Args:
    options: The command-line arguments tuple.
    temp_dir: A temporary directory where the fixed manifest will be written to.
  Returns:
    Path to the fixed manifest within |temp_dir|.
  """
  debug_manifest_path = os.path.join(temp_dir, 'AndroidManifest.xml')
  _ANDROID_NAMESPACE = 'http://schemas.android.com/apk/res/android'
  _TOOLS_NAMESPACE = 'http://schemas.android.com/tools'
  ElementTree.register_namespace('android', _ANDROID_NAMESPACE)
  ElementTree.register_namespace('tools', _TOOLS_NAMESPACE)
  original_manifest = ElementTree.parse(options.android_manifest)

  def maybe_extract_version(j):
    try:
      return _ExtractVersionFromSdk(options.aapt_path, j)
    except build_utils.CalledProcessError:
      return None

  android_sdk_jars = [j for j in options.include_resources
                      if os.path.basename(j) in ('android.jar',
                                                 'android_system.jar')]
  extract_all = [maybe_extract_version(j) for j in android_sdk_jars]
  successful_extractions = [x for x in extract_all if x]
  if len(successful_extractions) == 0:
    raise Exception(
        'Unable to find android SDK jar among candidates: %s'
            % ', '.join(android_sdk_jars))
  elif len(successful_extractions) > 1:
    raise Exception(
        'Found multiple android SDK jars among candidates: %s'
            % ', '.join(android_sdk_jars))
  version_code, version_name = successful_extractions.pop()

  # ElementTree.find does not work if the required tag is the root.
  if original_manifest.getroot().tag == 'manifest':
    manifest_node = original_manifest.getroot()
  else:
    manifest_node = original_manifest.find('manifest')

  manifest_node.set('platformBuildVersionCode', version_code)
  manifest_node.set('platformBuildVersionName', version_name)

  if options.debuggable:
    app_node = original_manifest.find('application')
    app_node.set('{%s}%s' % (_ANDROID_NAMESPACE, 'debuggable'), 'true')

  with open(debug_manifest_path, 'w') as debug_manifest:
    debug_manifest.write(ElementTree.tostring(
        original_manifest.getroot(), encoding='UTF-8'))

  return debug_manifest_path


def _ResourceNameFromPath(path):
  return os.path.splitext(os.path.basename(path))[0]


def _CreateKeepPredicate(resource_dirs, resource_blacklist_regex,
                         resource_blacklist_exceptions):
  """Return a predicate lambda to determine which resource files to keep.

  Args:
    resource_dirs: list of top-level resource directories.
    resource_blacklist_regex: A regular expression describing all resources
      to exclude, except if they are mip-maps, or if they are listed
      in |resource_blacklist_exceptions|.
    resource_blacklist_exceptions: A list of glob patterns corresponding
      to exceptions to the |resource_blacklist_regex|.
  Returns:
    A lambda that takes a path, and returns true if the corresponding file
    must be kept.
  """
  naive_predicate = lambda path: os.path.basename(path)[0] != '.'
  if resource_blacklist_regex == '':
    # Do not extract dotfiles (e.g. ".gitkeep"). aapt ignores them anyways.
    return naive_predicate

  if resource_blacklist_regex != '':
    # A simple predicate that only removes (returns False for) paths covered by
    # the blacklist regex, except if they are mipmaps, or listed as exceptions.
    naive_predicate = lambda path: (
        not re.search(resource_blacklist_regex, path) or
        re.search(r'[/-]mipmap[/-]', path) or
        build_utils.MatchesGlob(path, resource_blacklist_exceptions))

  # Build a set of all names from drawables kept by naive_predicate().
  # Used later to ensure that we never exclude drawables from densities
  # that are filtered-out by naive_predicate().
  non_filtered_drawables = set()
  for resource_dir in resource_dirs:
    for path in _IterFiles(resource_dir):
      if re.search(r'[/-]drawable[/-]', path) and naive_predicate(path):
        non_filtered_drawables.add(_ResourceNameFromPath(path))

  # NOTE: Defined as a function, instead of a lambda to avoid the
  # auto-formatter to put this on a very long line that overflows.
  def drawable_predicate(path):
    return (naive_predicate(path)
            or _ResourceNameFromPath(path) not in non_filtered_drawables)

  return drawable_predicate


def _ConvertToWebP(webp_binary, png_files):
  renamed_paths = dict()
  pool = multiprocessing.pool.ThreadPool(10)
  def convert_image(png_path_tuple):
    png_path, original_dir = png_path_tuple
    root = os.path.splitext(png_path)[0]
    webp_path = root + '.webp'
    args = [webp_binary, png_path, '-mt', '-quiet', '-m', '6', '-q', '100',
        '-lossless', '-o', webp_path]
    subprocess.check_call(args)
    os.remove(png_path)
    renamed_paths[os.path.relpath(webp_path, original_dir)] = os.path.relpath(
        png_path, original_dir)

  pool.map(convert_image, [f for f in png_files
                           if not _PNG_WEBP_BLACKLIST_PATTERN.match(f[0])])
  pool.close()
  pool.join()
  return renamed_paths


def _CompileDeps(aapt2_path, dep_subdirs, temp_dir):
  partials_dir = os.path.join(temp_dir, 'partials')
  build_utils.MakeDirectory(partials_dir)
  partial_compile_command = [
      aapt2_path,
      'compile',
      # TODO(wnwen): Turn this on once aapt2 forces 9-patch to be crunched.
      # '--no-crunch',
  ]
  pool = multiprocessing.pool.ThreadPool(10)
  def compile_partial(directory):
    dirname = os.path.basename(directory)
    partial_path = os.path.join(partials_dir, dirname + '.zip')
    compile_command = (partial_compile_command +
                       ['--dir', directory, '-o', partial_path])
    build_utils.CheckOutput(
        compile_command,
        stderr_filter=lambda output:
            build_utils.FilterLines(
                output, r'ignoring configuration .* for styleable'))

    # Sorting the files in the partial ensures deterministic output from the
    # aapt2 link step which uses order of files in the partial.
    sorted_partial_path = os.path.join(partials_dir, dirname + '.sorted.zip')
    _SortZip(partial_path, sorted_partial_path)

    return sorted_partial_path

  partials = pool.map(compile_partial, dep_subdirs)
  pool.close()
  pool.join()
  return partials


def _CreateResourceInfoFile(
    renamed_paths, apk_info_path, dependencies_res_zips):
  lines = set()
  for zip_file in dependencies_res_zips:
    zip_info_file_path = zip_file + '.info'
    if os.path.exists(zip_info_file_path):
      with open(zip_info_file_path, 'r') as zip_info_file:
        lines.update(zip_info_file.readlines())
  for dest, source in renamed_paths.iteritems():
    lines.add('Rename:{},{}\n'.format(dest, source))
  with build_utils.AtomicOutput(apk_info_path) as info_file:
    info_file.writelines(sorted(lines))


def _RemoveUnwantedLocalizedStrings(dep_subdirs, options):
  """Remove localized strings that should not go into the final output.

  Args:
    dep_subdirs: List of resource dependency directories.
    options: Command-line options namespace.
  """
  if (not options.locale_whitelist
      and not options.shared_resources_whitelist_locales):
    # Keep everything, there is nothing to do.
    return

  # Collect locale and file paths from the existing subdirs.
  # The following variable maps Android locale names to
  # sets of corresponding xml file paths.
  locale_to_files_map = collections.defaultdict(set)
  for directory in dep_subdirs:
    for f in _IterFiles(directory):
      locale = resource_utils.FindLocaleInStringResourceFilePath(f)
      if locale:
        locale_to_files_map[locale].add(f)

  all_locales = set(locale_to_files_map)

  # Set A: wanted locales, either all of them or the
  # list provided by --locale-whitelist.
  wanted_locales = all_locales
  if options.locale_whitelist:
    wanted_locales = _ToAndroidLocales(options.locale_whitelist,
                                       options.support_zh_hk)

  # Set B: shared resources locales, which is either set A
  # or the list provided by --shared-resources-whitelist-locales
  shared_resources_locales = wanted_locales
  shared_names_whitelist = set()
  if options.shared_resources_whitelist_locales:
    shared_names_whitelist = set(
        resource_utils.GetRTxtStringResourceNames(
            options.shared_resources_whitelist))

    shared_resources_locales = _ToAndroidLocales(
        options.shared_resources_whitelist_locales, options.support_zh_hk)

  # Remove any file that belongs to a locale not covered by
  # either A or B.
  removable_locales = (all_locales - wanted_locales - shared_resources_locales)
  for locale in removable_locales:
    for path in locale_to_files_map[locale]:
      os.remove(path)

  # For any locale in B but not in A, only keep the shared
  # resource strings in each file.
  for locale in shared_resources_locales - wanted_locales:
    for path in locale_to_files_map[locale]:
      resource_utils.FilterAndroidResourceStringsXml(
          path, lambda x: x in shared_names_whitelist)

  # For any locale in A but not in B, only keep the strings
  # that are _not_ from shared resources in the file.
  for locale in wanted_locales - shared_resources_locales:
    for path in locale_to_files_map[locale]:
      resource_utils.FilterAndroidResourceStringsXml(
          path, lambda x: x not in shared_names_whitelist)


def _PackageApk(options, dep_subdirs, temp_dir, gen_dir, r_txt_path):
  """Compile resources with aapt2 and generate intermediate .ap_ file.

  Args:
    options: The command-line options tuple. E.g. the generated apk
      will be written to |options.apk_path|.
    dep_subdirs: The list of directories where dependency resource zips
      were extracted (its content will be altered by this function).
    temp_dir: A temporary directory.
    gen_dir: Another temp directory where some intermediate files are
      generated.
    r_txt_path: The path where the R.txt file will written to.
  """
  renamed_paths = dict()
  renamed_paths.update(_DuplicateZhResources(dep_subdirs))
  renamed_paths.update(_RenameLocaleResourceDirs(dep_subdirs))

  _RemoveUnwantedLocalizedStrings(dep_subdirs, options)

  # Create a function that selects which resource files should be packaged
  # into the final output. Any file that does not pass the predicate will
  # be removed below.
  keep_predicate = _CreateKeepPredicate(dep_subdirs,
                                        options.resource_blacklist_regex,
                                        options.resource_blacklist_exceptions)
  png_paths = []
  for directory in dep_subdirs:
    for f in _IterFiles(directory):
      if not keep_predicate(f):
        os.remove(f)
      elif f.endswith('.png'):
        png_paths.append((f, directory))
  if png_paths and options.png_to_webp:
    renamed_paths.update(_ConvertToWebP(options.webp_binary, png_paths))
  for directory in dep_subdirs:
    renamed_paths.update(_MoveImagesToNonMdpiFolders(directory))

  if options.optimize_resources:
    if options.unoptimized_resources_path:
      unoptimized_apk_path = options.unoptimized_resources_path
    else:
      unoptimized_apk_path = os.path.join(gen_dir, 'intermediate.ap_')
  else:
    unoptimized_apk_path = options.apk_path
  link_command = _CreateLinkApkArgs(options)
  # TODO(digit): Is this below actually required for R.txt generation?
  link_command += ['--java', gen_dir]

  fixed_manifest = _FixManifest(options, temp_dir)
  link_command += ['--manifest', fixed_manifest]

  partials = _CompileDeps(options.aapt2_path, dep_subdirs, temp_dir)
  for partial in partials:
    link_command += ['-R', partial]

  # Creates a .zip with AndroidManifest.xml, resources.arsc, res/*
  # Also creates R.txt
  with build_utils.AtomicOutput(unoptimized_apk_path) as unoptimized, \
      build_utils.AtomicOutput(r_txt_path) as r_txt:
    link_command += ['-o', unoptimized.name]
    link_command += ['--output-text-symbols', r_txt.name]
    build_utils.CheckOutput(
        link_command, print_stdout=False, print_stderr=False)

    if options.optimize_resources:
      with build_utils.AtomicOutput(options.apk_path) as optimized:
        _OptimizeApk(optimized.name, options, temp_dir, unoptimized.name,
                     r_txt.name)

  _CreateResourceInfoFile(
      renamed_paths, options.apk_info_path, options.dependencies_res_zips)


def _OptimizeApk(output, options, temp_dir, unoptimized_apk_path, r_txt_path):
  """Optimize intermediate .ap_ file with aapt2.

  Args:
    output: Path to write to.
    options: The command-line options.
    temp_dir: A temporary directory.
    unoptimized_apk_path: path of the apk to optimize.
    r_txt_path: path to the R.txt file of the unoptimized apk.
  """
  # Resources of type ID are references to UI elements/views. They are used by
  # UI automation testing frameworks. They are kept in so that they dont break
  # tests, even though they may not actually be used during runtime. See
  # https://crbug.com/900993
  id_resources = _ExtractIdResources(r_txt_path)
  gen_config_path = os.path.join(temp_dir, 'aapt2.config')
  if options.resources_config_path:
    shutil.copyfile(options.resources_config_path, gen_config_path)
  with open(gen_config_path, 'a+') as config:
    for resource in id_resources:
      config.write('{}#no_obfuscate\n'.format(resource))

  # Optimize the resources.arsc file by obfuscating resource names and only
  # allow usage via R.java constant.
  optimize_command = [
      options.aapt2_path,
      'optimize',
      '--enable-resource-obfuscation',
      '-o',
      output,
      '--resources-config-path',
      gen_config_path,
      unoptimized_apk_path,
  ]
  build_utils.CheckOutput(
      optimize_command, print_stdout=False, print_stderr=False)


def _ExtractIdResources(rtxt_path):
  """Extract resources of type ID from the R.txt file

  Args:
    rtxt_path: Path to R.txt file with all the resources
  Returns:
    List of id resources in the form of id/<resource_name>
  """
  id_resources = []
  with open(rtxt_path) as rtxt:
    for line in rtxt:
      if ' id ' in line:
        resource_name = line.split()[2]
        id_resources.append('id/{}'.format(resource_name))
  return id_resources


def _WriteFinalRTxtFile(options, aapt_r_txt_path):
  """Determine final R.txt and return its location.

  This handles --r-text-in and --r-text-out options at the same time.

  Args:
    options: The command-line options tuple.
    aapt_r_txt_path: The path to the R.txt generated by aapt.
  Returns:
    Path to the final R.txt file.
  """
  if options.r_text_in:
    r_txt_file = options.r_text_in
  else:
    # When an empty res/ directory is passed, aapt does not write an R.txt.
    r_txt_file = aapt_r_txt_path
    if not os.path.exists(r_txt_file):
      build_utils.Touch(r_txt_file)

  if options.r_text_out:
    shutil.copyfile(r_txt_file, options.r_text_out)

  return r_txt_file


def main(args):
  args = build_utils.ExpandFileArgs(args)
  options = _ParseArgs(args)

  debug_temp_resources_dir = os.environ.get(_ENV_DEBUG_VARIABLE)
  if debug_temp_resources_dir:
    debug_temp_resources_dir = os.path.join(debug_temp_resources_dir,
                                            os.path.basename(options.apk_path))
    build_utils.DeleteDirectory(debug_temp_resources_dir)
    build_utils.MakeDirectory(debug_temp_resources_dir)

  with resource_utils.BuildContext(debug_temp_resources_dir) as build:
    dep_subdirs = resource_utils.ExtractDeps(options.dependencies_res_zips,
                                             build.deps_dir)

    _PackageApk(options, dep_subdirs, build.temp_dir, build.gen_dir,
                build.r_txt_path)

    r_txt_path = _WriteFinalRTxtFile(options, build.r_txt_path)

    # If --shared-resources-whitelist is used, the all resources listed in
    # the corresponding R.txt file will be non-final, and an onResourcesLoaded()
    # will be generated to adjust them at runtime.
    #
    # Otherwise, if --shared-resources is used, the all resources will be
    # non-final, and an onResourcesLoaded() method will be generated too.
    #
    # Otherwise, all resources will be final, and no method will be generated.
    #
    rjava_build_options = resource_utils.RJavaBuildOptions()
    if options.shared_resources_whitelist:
      rjava_build_options.ExportSomeResources(
          options.shared_resources_whitelist)
      rjava_build_options.GenerateOnResourcesLoaded()
    elif options.shared_resources or options.app_as_shared_lib:
      rjava_build_options.ExportAllResources()
      rjava_build_options.GenerateOnResourcesLoaded()

    resource_utils.CreateRJavaFiles(
        build.srcjar_dir, None, r_txt_path, options.extra_res_packages,
        options.extra_r_text_files, rjava_build_options)

    if options.srcjar_out:
      build_utils.ZipDir(options.srcjar_out, build.srcjar_dir)

    if options.check_resources_pkg_id is not None:
      expected_id = options.check_resources_pkg_id
      package_id = _ExtractPackageIdFromApk(options.apk_path,
                                            options.aapt_path)
      if package_id != expected_id:
        raise Exception('Invalid package ID 0x%x (expected 0x%x)' %
                        (package_id, expected_id))

  if options.depfile:
    build_utils.WriteDepfile(
        options.depfile,
        options.apk_path,
        inputs=options.dependencies_res_zips + options.extra_r_text_files,
        add_pydeps=False)


if __name__ == '__main__':
  main(sys.argv[1:])
