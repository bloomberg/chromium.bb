# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provision a recovery image for OOBE autoconfiguration.

This script populates the OOBE autoconfiguration data
(/stateful/unencrypted/oobe_auto_config/config.json) with the given parameters.

Additionally, it marks the image as being "hands-free", i.e. requiring no
physical user interaction to remove the recovery media before rebooting after
the recovery procedure has completed.

Any parameters prefixed with --x (e.g. --x-demo-mode) correspond directly to
generated elements in the configuration expected by OOBE.
"""

from __future__ import print_function

import json
import os
import uuid

from chromite.lib import commandline
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import image_lib
from chromite.lib import osutils


# OOBE auto-config parameters as they appear in
#   chrome/browser/chromeos/login/configuration_keys.h
# Please keep the keys grouped in the same order as the source file.
_CONFIG_PARAMETERS = (
    ('demo-mode', bool, 'Whether the device should be placed into demo mode.'),
    ('network-onc', str, 'ONC blob for network configuration.'),
    ('network-auto-connect', bool,
     'Whether the network screen should automatically proceed with '
     'connected network.'),
    ('eula-send-statistics', bool,
     'Whether the device should send usage statistics.'),
    ('eula-auto-accept', bool,
     'Whether the EULA should be automatically accepted.'),
    ('update-skip', bool,
     'Whether the udpate check should be skipped entirely (it may be '
     'required for future version pinning).'),
    ('wizard-auto-enroll', bool,
     'Whether the wizard should automatically start enrollment at the '
     'appropriate moment.'),
)

# Set of flags to specify when building with --generic.
_GENERIC_FLAGS = {
    'network-auto-connect': True,
    'eula-send-statistics': True,
    'eula-auto-accept': True,
    'update-skip': True,
}

# Mapping of flag type to argparse kwargs.
_ARG_TYPES = {
    str: {},
    bool: {'action': 'store_true'},
}

# Name of the OOBE directory in unencrypted/.
_OOBE_DIRECTORY = 'oobe_auto_config'

# Name of the configuration file in the recovery image.
_CONFIG_PATH = 'config.json'

# Name of the file containing the enrollment domain.
_DOMAIN_PATH = 'enrollment_domain'


def SanitizeDomain(domain):
  """Returns a sanitized |domain| for use in recovery."""
  # Domain is a byte string when passed by command-line flag.
  domain = domain.decode('utf-8')
  # Lowercase, so we don't need to ship uppercase glyphs in initramfs.
  domain = domain.lower()
  # Encode to IDNA to prevent homograph attacks.
  return domain.encode('idna')


def GetConfigContent(opts):
  """Formats OOBE autoconfiguration from commandline namespace.

  Args:
    opts: A commandline namespace containing OOBE autoconfig opts.

  Returns:
    A JSON string representation of the requested configuration.
  """
  conf = {}

  for flag, _, _ in _CONFIG_PARAMETERS:
    conf[flag] = getattr(opts, 'x_' + flag.replace('-', '_'))

  if opts.wifi_ssid:
    conf['network-onc'] = {
        'GUID': str(uuid.uuid4()),
        'Name': opts.wifi_ssid,
        'Type': 'WiFi',
        'WiFi': {
            'AutoConnect': True,
            'HiddenSSID': False,
            'SSID': opts.wifi_ssid,
            'Security': 'None',
        },
    }

  if opts.use_ethernet:
    conf['network-onc'] = {
        'GUID': str(uuid.uuid4()),
        'Name': 'Ethernet',
        'Type': 'Ethernet',
        'Ethernet': {
            'Authentication': 'None',
        },
    }

  return json.dumps(conf)


def PrepareImage(image, content, domain=None):
  """Prepares a recovery image for OOBE autoconfiguration.

  Args:
    image: Path to the recovery image.
    content: The content of the OOBE autoconfiguration.
    domain: Which domain to enroll to.
  """
  with osutils.TempDir() as tmp, \
    image_lib.LoopbackPartitions(image, tmp) as image:
    stateful_mnt = image.Mount((constants.CROS_PART_STATEFUL,),
                               mount_opts=('rw',))[0]

    # /stateful/unencrypted may not exist at this point in time on the
    # recovery image, so create it root-owned here.
    unencrypted = os.path.join(stateful_mnt, 'unencrypted')
    osutils.SafeMakedirs(unencrypted, mode=0o755, sudo=True)

    # The OOBE autoconfig directory must be owned by the chronos user so
    # that we can delete the config file from it from Chrome.
    oobe_autoconf = os.path.join(unencrypted, _OOBE_DIRECTORY)
    osutils.SafeMakedirsNonRoot(oobe_autoconf, user='chronos')

    # Create the config file to be owned by the chronos user, and write the
    # given data into it.
    config = os.path.join(oobe_autoconf, _CONFIG_PATH)
    osutils.WriteFile(config, content, sudo=True)
    cros_build_lib.SudoRunCommand(['chown', 'chronos:chronos', config])

    # If we have a plaintext domain name, write it.
    if domain:
      domain_path = os.path.join(oobe_autoconf, _DOMAIN_PATH)
      osutils.WriteFile(domain_path, SanitizeDomain(domain), sudo=True)
      cros_build_lib.SudoRunCommand(['chown', 'chronos:chronos', domain_path])


def ParseArguments(argv):
  """Returns a namespace for the CLI arguments."""
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('image', help='Path of recovery image to populate.')

  # Prefix raw config elements with --x.
  for flag, flag_type, help_text in _CONFIG_PARAMETERS:
    parser.add_argument('--x-%s' % flag, help=help_text,
                        **_ARG_TYPES[flag_type])

  parser.add_argument('--generic', action='store_true',
                      help='Set defaults for common configuration options.')
  parser.add_argument('--dump-config', action='store_true',
                      help='Dump generated configuration file to stdout.')
  parser.add_argument('--config', type='path', required=False,
                      help='Path to pre-generated configuration file to use, '
                           'overriding other flags set.')
  parser.add_argument('--wifi-ssid', type=str, required=False,
                      help='If specified, generates an ONC for auto-connecting '
                           'to the given SSID. The network must not use any '
                           'security (i.e. be an open network), or the device '
                           'will fail to connect.')
  parser.add_argument('--use-ethernet', action='store_true',
                      help='If specified, generates an ONC for auto-connecting '
                           'via ethernet.')
  parser.add_argument('--enrollment-domain', type=str, required=False,
                      help='Text to visually identify the enrollment token in '
                           'recovery.')

  opts = parser.parse_args(argv)

  if opts.use_ethernet and opts.wifi_ssid:
    parser.error('cannot specify --wifi-ssid and --use-ethernet together')

  if opts.generic:
    for opt, val in _GENERIC_FLAGS.items():
      setattr(opts, 'x_' + opt.replace('-', '_'), val)

  opts.Freeze()
  return opts


def main(argv):
  cros_build_lib.AssertInsideChroot()

  opts = ParseArguments(argv)

  if opts.config:
    config_content = osutils.ReadFile(opts.config)
  else:
    config_content = GetConfigContent(opts)

  logging.info('Using config: %s', config_content)

  if opts.dump_config:
    print(config_content)

  PrepareImage(opts.image, config_content, opts.enrollment_domain)
