#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Manages configuring Microsoft Internet Explorer."""

import logging
import os
import re
import subprocess
import urllib
import urlparse
import util


def _ConfigureSecurityKeyForZone(zone_number, key, value):
  """Set key under the Zone settings in Security.

  Args:
    zone_number: Integer identifier of zone to edit.
    key: key under "HKEY_CURRENT_USER\Software\Microsoft\Windows\
         CurrentVersion\Internet Settings\Zones\<Zone>"
         to edit.
    value: new value for key.

  Returns:
    True on success.
  """
  if not util.RegAdd(('HKEY_CURRENT_USER\\Software\\Microsoft\\'
                      'Windows\\CurrentVersion\\Internet Settings\\'
                      'Zones\\%s' % zone_number), key, util.REG_DWORD, value):
    return False
  return True


def GetInstalledVersion():
  """Reads the version of IE installed by checking the registry.

  The complete version is returned.  For example: 6.0.2900.55.5512.

  Returns:
    String for version or None on failure.
  """
  return util.RegQuery('HKLM\\Software\\Microsoft\\Internet Explorer','Version')


def ConfigurePopupBlocker(active):
  """Configure if popup blocker is active for IE 8.

  Args:
    active: new active state for popup blocker.

  Returns:
    True on success.
  """
  if active:
    value = '1'
  else:
    value = '0'

  # IE 8
  if not util.RegAdd(('HKEY_LOCAL_MACHINE\\Software\\Microsoft'
                      '\\Internet Explorer\\Main\\FeatureControl\\'
                      'FEATURE_WEBOC_POPUPMANAGEMENT'), 'iexplore.exe', 
                      util.REG_DWORD, value):
    return False

  # IE 6
  if active:
    value = 'yes'
  else:
    value = 'no'
  
  if not util.RegAdd(('HKEY_CURRENT_USER\\Software\\Microsoft\\'
                      'Internet Explorer\\New Windows'),
                      'PopupMgr', util.REG_SZ, value):
    return False
  
  return True


def DisableDefaultBrowserCheck(active):
  """Turn off the check to be set as default browser on start up.

  Args:
    active: if True then disables browser check, otherwise enables.

  Returns:
    True on success.
  """
  if active:
    value = 'no'
  else:
    value = 'yes'

  path = 'HKEY_CURRENT_USER\\Software\\Microsoft\\Internet Explorer\\Main'

  if not util.RegAdd(path, 'Check_Associations', util.REG_SZ, value):
    return False

  return True


def DisableWelcomeWindowForInternetExplorer8():
  """Turn off the window which asks user to configure IE8 on start up.

  Returns:
    True on success.
  """
  path = 'HKEY_CURRENT_USER\\Software\\Microsoft\\Internet Explorer\\Main'

  if not util.RegAdd(path, 'IE8RunOnceLastShown', util.REG_DWORD, '1'):
    return False

  if not util.RegAdd(path, 'IE8RunOnceLastShown_TIMESTAMP', util.REG_BINARY,
                     '79a29ba0bbb7c901'):
    return False

  if not util.RegAdd(path, 'IE8RunOnceCompletionTime', util.REG_BINARY,
                     'f98da5b2bbb7c901'):
    return False

  if not util.RegAdd(path, 'IE8TourShown', util.REG_DWORD, '1'):
    return False

  if not util.RegAdd(path, 'IE8TourShownTime', util.REG_BINARY,
                     '1ada825779b6c901'):
    return False

  if not util.RegAdd(path, 'DisableFirstRunCustomize', util.REG_DWORD, '1'):
    return False

  return True


def DisableInformationBarIntro():
  """Turn off the dialog which tells you about information bar.

  Returns:
    True on success.
  """
  path = (r'HKEY_CURRENT_USER\Software\Microsoft\Internet Explorer'
          r'\InformationBar')

  if not util.RegAdd(path, 'FirstTime', util.REG_DWORD, '0'):
    return False

  return True


def DisableInternetExplorerPhishingFilter():
  """Turn off the dialog for Phishing filter which blocks tests on IE 7.

  Returns:
    True on success.
  """
  path = ('HKEY_CURRENT_USER\\Software\\Microsoft\\'
          'Internet Explorer\\PhishingFilter')

  if not util.RegAdd(path, 'Enabled', util.REG_DWORD, '1'):
    return False

  if not util.RegAdd(path, 'ShownVerifyBalloon', util.REG_DWORD, '1'):
    return False

  if not util.RegAdd(path, 'EnabledV8', util.REG_DWORD, '1'):
    return False

  return True


def ConfigureAllowActiveX(activate):
  """Configure internet zone to use Active X.

  Args:
    activate: Turn on if True, otherwise turn off.

  Returns:
    True on success.
  """
  internet_zone = 3

  if activate:
    value = '00000000'
  else:
    value = '00000003'

  return _ConfigureSecurityKeyForZone(internet_zone, '1200', value)


def ConfigureAllowPreviouslyUnusedActiveXControlsToRun(activate):
  """Configure internet zone to allow new Active X controls to run.

  Args:
    activate: Turn on if True, otherwise turn off.

  Returns:
    True on success.
  """
  internet_zone = 3

  if activate:
    value = '00000000'
  else:
    value = '00000003'

  return _ConfigureSecurityKeyForZone(internet_zone, '1208', value)


def ConfigureAllowAllDomainsToUseActiveX(activate):
  """Configure internet zone to allow all domains to use Active X.

  Args:
    activate: Turn on if True, otherwise turn off.

  Returns:
    True on success.
  """
  internet_zone = 3

  if activate:
    value = '00000000'
  else:
    value = '00000003'

  return _ConfigureSecurityKeyForZone(internet_zone, '120B', value)


def ConfigureAllowActiveContentFromMyComputer(activate):
  """Allow Active X to run from local files loaded from My Computer.

  This option is shown under Security of the Advanced tab of Internet Options.

  Args:
    activate: Turn on if True, otherwise turn off.

  Returns:
    True on success.
  """
  if activate:
    value = '0'
  else:
    value = '1'

  if not util.RegAdd(('HKEY_CURRENT_USER\\Software\\Microsoft\\'
                      'Internet Explorer\\Main\\FeatureControl\\'
                      'FEATURE_LOCALMACHINE_LOCKDOWN'),
                     'iexplore.exe', util.REG_DWORD, value):
    return False
  if not util.RegAdd(('HKLM\\SOFTWARE\\Microsoft\\Internet Explorer\\Main\\'
                      'FeatureControl\\FEATURE_LOCALMACHINE_LOCKDOWN'),
                      'iexplore.exe', util.REG_DWORD, value):
    return False
  
  return True
  
  
def ConfigureIE():
  """Calls the other methods to allow testing with Internet Explorer.

  Returns:
    True on success.
  """
  on = True
  logging.info('Configuring IE...')
  logging.info('Allowing all active domains to use ActiveX...')
  if not ConfigureAllowAllDomainsToUseActiveX(on):
    return False

  logging.info('Allowing ActiveX...')
  if not ConfigureAllowActiveX(on):
    return False

  logging.info('Allowing previously unused ActiveX to run...')
  if not ConfigureAllowPreviouslyUnusedActiveXControlsToRun(on):
    return False

  logging.info('Disabling default browser check...')
  if not DisableDefaultBrowserCheck(on):
    return False

  logging.info('Allowing active content from my computer...')
  if not ConfigureAllowActiveContentFromMyComputer(on):
    return False

  logging.info('Disabling popup blocker...')
  if not ConfigurePopupBlocker(False):
    return False

  logging.info('Disabling Phishing Filter...')
  if not DisableInternetExplorerPhishingFilter():
    return False

  logging.info('Disabling welcome window for IE 8...')
  if not DisableWelcomeWindowForInternetExplorer8():
    return False

  logging.info('Disabling info bar intro...')
  if not DisableInformationBarIntro():
    return False

  return True
