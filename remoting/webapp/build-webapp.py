#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates a directory with with the unpacked contents of the remoting webapp.

The directory will contain a copy-of or a link-to to all remoting webapp
resources.  This includes HTML/JS and any plugin binaries. The script also
massages resulting files appropriately with host plugin data. Finally,
a zip archive for all of the above is produced.
"""

# Python 2.5 compatibility
from __future__ import with_statement

import io
import os
import platform
import re
import shutil
import subprocess
import sys
import time
import zipfile

# Update the module path, assuming that this script is in src/remoting/webapp,
# and that the google_api_keys module is in src/google_apis. Note that
# sys.path[0] refers to the directory containing this script.
if __name__ == '__main__':
  sys.path.append(
      os.path.abspath(os.path.join(sys.path[0], '../../google_apis')))
import google_api_keys

def findAndReplace(filepath, findString, replaceString):
  """Does a search and replace on the contents of a file."""
  oldFilename = os.path.basename(filepath) + '.old'
  oldFilepath = os.path.join(os.path.dirname(filepath), oldFilename)
  os.rename(filepath, oldFilepath)
  with open(oldFilepath) as input:
    with open(filepath, 'w') as output:
      for s in input:
        output.write(s.replace(findString, replaceString))
  os.remove(oldFilepath)


def createZip(zip_path, directory):
  """Creates a zipfile at zip_path for the given directory."""
  zipfile_base = os.path.splitext(os.path.basename(zip_path))[0]
  zip = zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED)
  for (root, dirs, files) in os.walk(directory):
    for f in files:
      full_path = os.path.join(root, f)
      rel_path = os.path.relpath(full_path, directory)
      zip.write(full_path, os.path.join(zipfile_base, rel_path))
  zip.close()


def replaceString(destination, placeholder, value):
  findAndReplace(os.path.join(destination, 'plugin_settings.js'),
                 "'" + placeholder + "'", "'" + value + "'")


def processJinjaTemplate(input_file, output_file, context):
  jinja2_path = os.path.normpath(
      os.path.join(os.path.abspath(__file__),
                   '../../../third_party/jinja2'))
  sys.path.append(os.path.split(jinja2_path)[0])
  import jinja2
  (template_path, template_name) = os.path.split(input_file)
  env = jinja2.Environment(loader=jinja2.FileSystemLoader(template_path))
  template = env.get_template(template_name)
  rendered = template.render(context)
  io.open(output_file, 'w', encoding='utf-8').write(rendered)



def buildWebApp(buildtype, version, destination, zip_path,
                manifest_template, webapp_type, files, locales):
  """Does the main work of building the webapp directory and zipfile.

  Args:
    buildtype: the type of build ("Official" or "Dev").
    destination: A string with path to directory where the webapp will be
                 written.
    zipfile: A string with path to the zipfile to create containing the
             contents of |destination|.
    manifest_template: jinja2 template file for manifest.
    webapp_type: webapp type ("v1", "v2" or "v2_pnacl").
    files: An array of strings listing the paths for resources to include
           in this webapp.
    locales: An array of strings listing locales, which are copied, along
             with their directory structure from the _locales directory down.
  """
  # Ensure a fresh directory.
  try:
    shutil.rmtree(destination)
  except OSError:
    if os.path.exists(destination):
      raise
    else:
      pass
  os.mkdir(destination, 0775)

  # Use symlinks on linux and mac for faster compile/edit cycle.
  #
  # On Windows Vista platform.system() can return 'Microsoft' with some
  # versions of Python, see http://bugs.python.org/issue1082
  # should_symlink = platform.system() not in ['Windows', 'Microsoft']
  #
  # TODO(ajwong): Pending decision on http://crbug.com/27185 we may not be
  # able to load symlinked resources.
  should_symlink = False

  # Copy all the files.
  for current_file in files:
    destination_file = os.path.join(destination, os.path.basename(current_file))
    destination_dir = os.path.dirname(destination_file)
    if not os.path.exists(destination_dir):
      os.makedirs(destination_dir, 0775)

    if should_symlink:
      # TODO(ajwong): Detect if we're vista or higher.  Then use win32file
      # to create a symlink in that case.
      targetname = os.path.relpath(os.path.realpath(current_file),
                                   os.path.realpath(destination_file))
      os.symlink(targetname, destination_file)
    else:
      shutil.copy2(current_file, destination_file)

  # Copy all the locales, preserving directory structure
  destination_locales = os.path.join(destination, "_locales")
  os.mkdir(destination_locales , 0775)
  remoting_locales = os.path.join(destination, "remoting_locales")
  os.mkdir(remoting_locales , 0775)
  for current_locale in locales:
    extension = os.path.splitext(current_locale)[1]
    if extension == '.json':
      locale_id = os.path.split(os.path.split(current_locale)[0])[1]
      destination_dir = os.path.join(destination_locales, locale_id)
      destination_file = os.path.join(destination_dir,
                                      os.path.split(current_locale)[1])
      os.mkdir(destination_dir, 0775)
      shutil.copy2(current_locale, destination_file)
    elif extension == '.pak':
      destination_file = os.path.join(remoting_locales,
                                      os.path.split(current_locale)[1])
      shutil.copy2(current_locale, destination_file)
    else:
      raise Exception("Unknown extension: " + current_locale);

  # Set client plugin type.
  client_plugin = 'pnacl' if webapp_type == 'v2_pnacl' else 'native'
  findAndReplace(os.path.join(destination, 'plugin_settings.js'),
                 "'CLIENT_PLUGIN_TYPE'", "'" + client_plugin + "'")

  # Allow host names for google services/apis to be overriden via env vars.
  oauth2AccountsHost = os.environ.get(
      'OAUTH2_ACCOUNTS_HOST', 'https://accounts.google.com')
  oauth2ApiHost = os.environ.get(
      'OAUTH2_API_HOST', 'https://www.googleapis.com')
  directoryApiHost = os.environ.get(
      'DIRECTORY_API_HOST', 'https://www.googleapis.com')
  oauth2BaseUrl = oauth2AccountsHost + '/o/oauth2'
  oauth2ApiBaseUrl = oauth2ApiHost + '/oauth2'
  directoryApiBaseUrl = directoryApiHost + '/chromoting/v1'
  replaceString(destination, 'OAUTH2_BASE_URL', oauth2BaseUrl)
  replaceString(destination, 'OAUTH2_API_BASE_URL', oauth2ApiBaseUrl)
  replaceString(destination, 'DIRECTORY_API_BASE_URL', directoryApiBaseUrl)
  # Substitute hosts in the manifest's CSP list.
  # Ensure we list the API host only once if it's the same for multiple APIs.
  googleApiHosts = ' '.join(set([oauth2ApiHost, directoryApiHost]))

  # WCS and the OAuth trampoline are both hosted on talkgadget. Split them into
  # separate suffix/prefix variables to allow for wildcards in manifest.json.
  talkGadgetHostSuffix = os.environ.get(
      'TALK_GADGET_HOST_SUFFIX', 'talkgadget.google.com')
  talkGadgetHostPrefix = os.environ.get(
      'TALK_GADGET_HOST_PREFIX', 'https://chromoting-client.')
  oauth2RedirectHostPrefix = os.environ.get(
      'OAUTH2_REDIRECT_HOST_PREFIX', 'https://chromoting-oauth.')

  # Use a wildcard in the manifest.json host specs if the prefixes differ.
  talkGadgetHostJs = talkGadgetHostPrefix + talkGadgetHostSuffix
  talkGadgetBaseUrl = talkGadgetHostJs + '/talkgadget/'
  if talkGadgetHostPrefix == oauth2RedirectHostPrefix:
    talkGadgetHostJson = talkGadgetHostJs
  else:
    talkGadgetHostJson = 'https://*.' + talkGadgetHostSuffix

  # Set the correct OAuth2 redirect URL.
  oauth2RedirectHostJs = oauth2RedirectHostPrefix + talkGadgetHostSuffix
  oauth2RedirectHostJson = talkGadgetHostJson
  oauth2RedirectPath = '/talkgadget/oauth/chrome-remote-desktop'
  oauth2RedirectBaseUrlJs = oauth2RedirectHostJs + oauth2RedirectPath
  oauth2RedirectBaseUrlJson = oauth2RedirectHostJson + oauth2RedirectPath
  if buildtype == 'Official':
    oauth2RedirectUrlJs = ("'" + oauth2RedirectBaseUrlJs +
                           "/rel/' + chrome.i18n.getMessage('@@extension_id')")
    oauth2RedirectUrlJson = oauth2RedirectBaseUrlJson + '/rel/*'
  else:
    oauth2RedirectUrlJs = "'" + oauth2RedirectBaseUrlJs + "/dev'"
    oauth2RedirectUrlJson = oauth2RedirectBaseUrlJson + '/dev*'
  thirdPartyAuthUrlJs = oauth2RedirectBaseUrlJs + "/thirdpartyauth"
  thirdPartyAuthUrlJson = oauth2RedirectBaseUrlJson + '/thirdpartyauth*'
  replaceString(destination, "TALK_GADGET_URL", talkGadgetBaseUrl)
  findAndReplace(os.path.join(destination, 'plugin_settings.js'),
                 "'OAUTH2_REDIRECT_URL'", oauth2RedirectUrlJs)

  # Configure xmpp server and directory bot settings in the plugin.
  xmppServerAddress = os.environ.get(
      'XMPP_SERVER_ADDRESS', 'talk.google.com:5222')
  xmppServerUseTls = os.environ.get('XMPP_SERVER_USE_TLS', 'true')
  directoryBotJid = os.environ.get(
      'DIRECTORY_BOT_JID', 'remoting@bot.talk.google.com')

  findAndReplace(os.path.join(destination, 'plugin_settings.js'),
                 "Boolean('XMPP_SERVER_USE_TLS')", xmppServerUseTls)
  replaceString(destination, "XMPP_SERVER_ADDRESS", xmppServerAddress)
  replaceString(destination, "DIRECTORY_BOT_JID", directoryBotJid)
  replaceString(destination, "THIRD_PARTY_AUTH_REDIRECT_URL",
                thirdPartyAuthUrlJs)

  # Set the correct API keys.
  # For overriding the client ID/secret via env vars, see google_api_keys.py.
  apiClientId = google_api_keys.GetClientID('REMOTING')
  apiClientSecret = google_api_keys.GetClientSecret('REMOTING')
  apiClientIdV2 = google_api_keys.GetClientID('REMOTING_IDENTITY_API')

  replaceString(destination, "API_CLIENT_ID", apiClientId)
  replaceString(destination, "API_CLIENT_SECRET", apiClientSecret)

  # Use a consistent extension id for unofficial builds.
  if buildtype != 'Official':
    manifestKey = '"key": "remotingdevbuild",'
  else:
    manifestKey = ''

  # Generate manifest.
  context = {
    'webapp_type': webapp_type,
    'FULL_APP_VERSION': version,
    'MANIFEST_KEY_FOR_UNOFFICIAL_BUILD': manifestKey,
    'OAUTH2_REDIRECT_URL': oauth2RedirectUrlJson,
    'TALK_GADGET_HOST': talkGadgetHostJson,
    'THIRD_PARTY_AUTH_REDIRECT_URL': thirdPartyAuthUrlJson,
    'REMOTING_IDENTITY_API_CLIENT_ID': apiClientIdV2,
    'OAUTH2_BASE_URL': oauth2BaseUrl,
    'OAUTH2_API_BASE_URL': oauth2ApiBaseUrl,
    'DIRECTORY_API_BASE_URL': directoryApiBaseUrl,
    'OAUTH2_ACCOUNTS_HOST': oauth2AccountsHost,
    'GOOGLE_API_HOSTS': googleApiHosts,
  }
  processJinjaTemplate(manifest_template,
                       os.path.join(destination, 'manifest.json'),
                       context)

  # Make the zipfile.
  createZip(zip_path, destination)

  return 0


def main():
  if len(sys.argv) < 6:
    print ('Usage: build-webapp.py '
           '<build-type> <version> <dst> <zip-path> <manifest_template> '
           '<webapp_type> <other files...> '
           '[--locales <locales...>]')
    return 1

  arg_type = ''
  files = []
  locales = []
  for arg in sys.argv[7:]:
    if arg in ['--locales']:
      arg_type = arg
    elif arg_type == '--locales':
      locales.append(arg)
    else:
      files.append(arg)

  return buildWebApp(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4],
                     sys.argv[5], sys.argv[6], files, locales)


if __name__ == '__main__':
  sys.exit(main())
