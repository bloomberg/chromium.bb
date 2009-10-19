#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# WINDOWS ONLY

"""Installs a specified browser."""

import ConfigParser
import copy
import os
import logging
import time

import util


BROWSERS = [{'name': 'ie6',
             'version': '6',
             'family': 'ie',
             'selenium_name': '*iexplore',
            },
            {'name': 'ie7',
             'version': '7',
             'family': 'ie',
             'selenium_name': '*iexplore',
            },
            {'name': 'ie8',
             'version': '8',
             'family': 'ie',
             'selenium_name': '*iexplore',
            },
            {'name': 'ff3',
             'version': '3',
             'family': 'firefox',
             'selenium_name': '*firefox'
            },
            {'name': 'ff3.5',
             'version': '3.5',
             'family': 'firefox',
             'selenium_name': '*firefox'
            },
            {'name': 'ff2',
             'version': '2',
             'family': 'firefox',
             'selenium_name': '*firefox'
            },
            {'name': 'chrome-dev',
             'version': '4',
             'family': 'chrome',
             'selenium_name': '*googlechrome'
            },
            {'name': 'chrome-beta',
             'version': '3',
             'family': 'chrome',
             'selenium_name': '*googlechrome'
            },
            {'name': 'chrome-stable',
             'version': '3',
             'family': 'chrome',
             'selenium_name': '*googlechrome'
            },
            {'name': 'saf3',
             'family': 'safari',
             'selenium_name': '*safari'
            },
            {'name': 'saf4',
             'family': 'safari',
             'selenium_name': '*safari'
            }]

if util.IsXP():
  HOME_PATH = 'C:\\Documents and Settings\\testing'
elif util.IsVista() or util.IsWin7():
  HOME_PATH = 'C:\\Users\\testing'
   
SERVER = 'http://tsqaserver/mac/o3d/imaging/software/browsers'

SOFTWARE_PATH = 'C:\\imaging\\software'

_FIREFOX_VERSIONS = {'ff3': SERVER + '/ff3.0.13.exe',
                     'ff2': SERVER + '/ff2.0.11.exe',
                     'ff3.5': SERVER + '/ff3.5.2.exe'}

_CHROME_VERSIONS = {'chrome-stable': SERVER + '/chrome_stable.exe',
                    'chrome-dev': SERVER + '/chrome_dev.exe',
                    'chrome-beta': SERVER + '/chrome_beta.exe'}

_IE_7_URLS = {'xp': SERVER + '/IE7-WindowsXP-x86-enu.exe',
              'xp_64': SERVER + '/IE7-WindowsServer2003-x64-enu.exe'}

_IE_8_URLS = {'xp': SERVER + '/IE8-WindowsXP-x86-ENU.exe',
              'xp_64': SERVER + '/IE8-WindowsServer2003-x64-ENU.exe',
              'vista': SERVER + '/IE8-WindowsVista-x86-ENU.exe',
              'vista_64': SERVER + '/IE8-WindowsVista-x64-ENU.exe'}


def GetVersionNumber(family):
  """Get the version number of the installed browser in |family|. Returns
  None if browser is not installed."""
  if family == 'ie':
    path = r'HKLM\SOFTWARE\Microsoft\Internet Explorer'
    version = util.RegQuery(path, 'Version')
    if version is not None:
      return version[0:1]
  
  elif family == 'firefox':
    path = r'HKLM\SOFTWARE\Mozilla\Mozilla Firefox'
    version = util.RegQuery(path, 'CurrentVersion')
    if version is not None:
      if version.startswith('3.0'):
        return '3'
      elif version.startswith('3.5'):
        return '3.5'
      elif version.startswith('2'):
        return '2'
      
  elif family == 'chrome':
    chrome_dir = (HOME_PATH + r'\Local Settings\Application Data\Google' +
                  r'\Chrome\Application')
    files = os.listdir(chrome_dir)
    
    for file in files:
      # The Chrome application directory has a folder named with the version #.
      if file.startswith('2.'):
        return '2'
      elif file.startswith('3.'):
        return '3'
      elif file.startswith('4.'):
        return '4'
      
  return None
    

def IsBrowserInstalled(browser):
  """Checks if |browser| is installed."""
  
  return GetVersionNumber(browser['family']).startswith(browser['version'])

def Install(browser):
  """Installs |browser|, if necessary. It is not possible to install
  an older version of the already installed browser currently.
  
  Args:
    browser: specific browst to install.
  Returns:
    whether browser is installed.
  """
  
  # Only dynamic installation of browsers for Windows now.
  if not util.IsWindows():
    return True
  
  logging.info('Wants to install ' + browser['name'])
  version = GetVersionNumber(browser['family'])
  if version is None:
    logging.info('No version of %s is installed' % browser['family'])
  else:
    logging.info('Version %s of %s is installed already' 
                 % (version, browser['family']))
   
  if not IsBrowserInstalled(browser):
    
    install_cmd = None
    
    # Download browser.
    logging.info('Downloading ' + browser['name'])
    if browser['family'] == 'ie':
      
      if browser['name'] == 'ie7':
        install_cmd = util.Download(_IE_7_URLS[util.GetOSPrefix()],
                                    SOFTWARE_PATH)
      elif browser['name'] == 'ie8':
        install_cmd = util.Download(_IE_8_URLS[util.GetOSPrefix()],
                                    SOFTWARE_PATH)
        
      install_cmd += ' /passive /no-default'  
      
    elif browser['family'] == 'firefox':
      if util.IsWindows():
        install = util.Download(_FIREFOX_VERSIONS[browser['name']], 
                                SOFTWARE_PATH)
        install_cmd = install + ' -ms'
    elif browser['family'] == 'chrome':
      if util.IsWindows():
        install_cmd = util.Download(_CHROME_VERSIONS[browser['name']],
                                    SOFTWARE_PATH)
    else:
      logging.error('Browser %s is not currently supported' % browser['name'])
    
    
    # Run installation.
    if install_cmd is not None:
      logging.info('Installing browser: ' + install_cmd)
    if install_cmd is None or util.RunStr(install_cmd) != 0:
      logging.error('Could not install %s' % browser['name'])
      return False
    
    # Do post installation things.
    if browser['family'] == 'chrome':
      first_run = file(HOME_PATH + '\\Local Settings\\'
                       'Application Data\\Google\\Chrome\\Application\\'
                       'First Run', 'w')
      first_run.close()
      
      # Wait for Chrome to install. Reboot to get rid of that first run UI.
      time.sleep(90)
      util.Reboot()
      logging.error('Could not reboot. Needed for Chrome installation.')
      return False
    
  else:
    logging.info(browser['name'] + ' already installed')
    
  return True

