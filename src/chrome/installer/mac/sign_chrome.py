#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os.path
import sys

sys.path.append(os.path.dirname(__file__))

from signing import config, model, pipeline


def create_config(identity, keychain, development):
    """Creates the |model.CodeSignConfig| for the signing operations.

    If |development| is True, the config will be modified to not require
    restricted internal assets, nor will the products be required to match
    specific certificate hashes.

    Args:
        identity: The code signing identity to use.
        keychain: Optional path to the keychain file, in which |identity|
            will be searched for.
        development: Boolean indicating whether or not to modify the chosen
            config for development testing.

    Returns:
        An instance of |model.CodeSignConfig|.
    """
    config_class = config.CodeSignConfig
    try:
        import signing.internal_config
        config_class = signing.internal_config.InternalCodeSignConfig
    except ImportError as e:
        # If the build specified Google Chrome as the product, then the
        # internal config has to be available.
        if config_class(identity, keychain).product == 'Google Chrome':
            raise e

    if development:

        class DevelopmentCodeSignConfig(config_class):

            @property
            def codesign_requirements_basic(self):
                return ''

            @property
            def provisioning_profile_basename(self):
                return None

            @property
            def run_spctl_assess(self):
                return False

        config_class = DevelopmentCodeSignConfig

    return config_class(identity, keychain)


def main():
    parser = argparse.ArgumentParser(
        description='Code sign and package Chrome for channel distribution.')
    parser.add_argument(
        '--keychain', help='The keychain to load the identity from.')
    parser.add_argument(
        '--identity', required=True, help='The identity to sign with.')
    parser.add_argument('--development', action='store_true',
            help='The specified identity is for development. ' \
                 'Certain codesign requirements will be omitted.')
    parser.add_argument('--input', required=True,
            help='Path to the input directory. The input directory should ' \
                    'contain the products to sign, as well as the Packaging ' \
                    'directory.')
    parser.add_argument('--output', required=True,
            help='Path to the output directory. The signed DMG products and ' \
                    'installer tools will be placed here.')
    parser.add_argument(
        '--no-dmg',
        action='store_true',
        help='Only sign Chrome and do not package the bundle into a DMG.')
    args = parser.parse_args()

    config = create_config(args.identity, args.keychain, args.development)
    paths = model.Paths(args.input, args.output, None)

    if not os.path.exists(paths.output):
        os.mkdir(paths.output)

    pipeline.sign_all(paths, config, package_dmg=not args.no_dmg)


if __name__ == '__main__':
    main()
