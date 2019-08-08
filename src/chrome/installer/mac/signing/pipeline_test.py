# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import unittest

from . import model, pipeline, test_common, test_config

mock = test_common.import_mock()


def _get_work_dir(*args, **kwargs):
    _get_work_dir.count += 1
    return '$W_{}'.format(_get_work_dir.count)


_get_work_dir.count = 0


def _get_adjacent_item(l, o):
    """Finds object |o| in collection |l| and returns the item at its index
    plus 1.
    """
    index = l.index(o)
    return l[index + 1]


@mock.patch.multiple(
    'signing.commands', **{
        m: mock.DEFAULT for m in ('move_file', 'copy_files', 'run_command',
                                  'make_dir', 'shutil')
    })
@mock.patch.multiple(
    'signing.signing',
    **{m: mock.DEFAULT for m in ('sign_part', 'sign_chrome', 'verify_part')})
@mock.patch('signing.commands.tempfile.mkdtemp', _get_work_dir)
class TestPipeline(unittest.TestCase):

    def setUp(self):
        _get_work_dir.count = 0
        self.paths = model.Paths('$I', '$O', None)

    def test_package_dmg_no_customize(self, **kwargs):
        dist = model.Distribution()
        config = test_config.TestConfig()
        paths = self.paths.replace_work('$W')

        dmg_path = pipeline._package_dmg(paths, dist, config)
        self.assertEqual('$O/AppProduct-99.0.9999.99.dmg', dmg_path)

        pkg_dmg_args = kwargs['run_command'].mock_calls[0][1][0]

        self.assertEqual(dmg_path, _get_adjacent_item(pkg_dmg_args, '--target'))
        self.assertEqual('$I/Product Packaging/chrome_dmg_icon.icns',
                         _get_adjacent_item(pkg_dmg_args, '--icon'))
        self.assertEqual('App Product',
                         _get_adjacent_item(pkg_dmg_args, '--volname'))
        self.assertEqual('$W/empty', _get_adjacent_item(pkg_dmg_args,
                                                        '--source'))

        copy_specs = [
            pkg_dmg_args[i + 1]
            for i, arg in enumerate(pkg_dmg_args)
            if arg == '--copy'
        ]
        self.assertEqual(
            set(copy_specs),
            set([
                '$W/App Product.app:/',
                '$I/Product Packaging/keystone_install.sh:/.keystone_install',
                '$I/Product Packaging/chrome_dmg_background.png:/.background/background.png',
                '$I/Product Packaging/chrome_dmg_dsstore:/.DS_Store'
            ]))

    def test_package_dmg_customize(self, **kwargs):
        dist = model.Distribution(
            channel_customize=True,
            channel='canary',
            app_name_fragment='Canary')
        config = dist.to_config(test_config.TestConfig())
        paths = self.paths.replace_work('$W')

        dmg_path = pipeline._package_dmg(paths, dist, config)
        self.assertEqual('$O/AppProductCanary-99.0.9999.99.dmg', dmg_path)

        pkg_dmg_args = kwargs['run_command'].mock_calls[0][1][0]

        self.assertEqual(dmg_path, _get_adjacent_item(pkg_dmg_args, '--target'))
        self.assertEqual('$I/Product Packaging/chrome_canary_dmg_icon.icns',
                         _get_adjacent_item(pkg_dmg_args, '--icon'))
        self.assertEqual('App Product Canary',
                         _get_adjacent_item(pkg_dmg_args, '--volname'))
        self.assertEqual('$W/empty', _get_adjacent_item(pkg_dmg_args,
                                                        '--source'))

        copy_specs = [
            pkg_dmg_args[i + 1]
            for i, arg in enumerate(pkg_dmg_args)
            if arg == '--copy'
        ]
        self.assertEqual(
            set(copy_specs),
            set([
                '$W/App Product Canary.app:/',
                '$I/Product Packaging/keystone_install.sh:/.keystone_install',
                '$I/Product Packaging/chrome_dmg_background.png:/.background/background.png',
                '$I/Product Packaging/chrome_canary_dmg_dsstore:/.DS_Store'
            ]))

    def test_package_installer_tools(self, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)

        config = test_config.TestConfig()
        pipeline._package_installer_tools(self.paths, config)

        # Start and end with the work dir.
        self.assertEqual(
            mock.call.make_dir('$W_1/diff_tools'), manager.mock_calls[0])
        self.assertEqual(
            mock.call.shutil.rmtree('$W_1'), manager.mock_calls[-1])

        self.assertEqual(
            mock.call.run_command(
                ['zip', '-9ry', '$O/diff_tools.zip', 'diff_tools'], cwd='$W_1'),
            manager.mock_calls[-2])

        files_to_copy = set([
            'goobspatch',
            'liblzma_decompress.dylib',
            'goobsdiff',
            'xz',
            'xzdec',
            'dirdiffer.sh',
            'dirpatcher.sh',
            'dmgdiffer.sh',
            'keystone_install.sh',
            'pkg-dmg',
        ])
        copied_files = []
        for call in manager.mock_calls:
            if call[0] == 'copy_files':
                args = call[1]
                self.assertTrue(args[0].startswith('$I/Product Packaging/'))
                self.assertEqual('$W_1/diff_tools', args[1])
                copied_files.append(os.path.basename(args[0]))

        self.assertEqual(len(copied_files), len(files_to_copy))
        self.assertEqual(set(copied_files), files_to_copy)

        files_to_sign = set([
            'goobspatch',
            'liblzma_decompress.dylib',
            'goobsdiff',
            'xz',
            'xzdec',
        ])
        signed_files = []
        verified_files = []

        for call in manager.mock_calls:
            args = call[1]
            if call[0] == 'sign_part':
                signed_files.append(os.path.basename(args[2].path))
            elif call[0] == 'verify_part':
                path = os.path.basename(args[1].path)
                self.assertTrue(path in signed_files)
                verified_files.append(path)

        self.assertEqual(len(signed_files), len(files_to_sign))
        self.assertEqual(len(verified_files), len(files_to_sign))
        self.assertEqual(set(signed_files), files_to_sign)
        self.assertEqual(set(verified_files), files_to_sign)

    @mock.patch('signing.pipeline._package_installer_tools')
    @mock.patch('signing.modification.customize_distribution')
    def test_sign_basic_distribution(self, customize, package_installer,
                                     **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)
        manager.attach_mock(customize, 'customize_distribution')
        manager.attach_mock(package_installer, 'package_installer_tools')

        config = test_config.TestConfig()
        pipeline.sign_all(self.paths, config)

        package_installer.assert_called_once()

        manager.assert_has_calls([
            # Then the customization and distribution.
            mock.call.copy_files('$I/App Product.app', '$W_1'),
            mock.call.customize_distribution(mock.ANY, mock.ANY, mock.ANY),
            mock.call.sign_chrome(mock.ANY, mock.ANY),
            mock.call.make_dir('$W_1/empty'),
            mock.call.run_command(mock.ANY),
            mock.call.sign_part(mock.ANY, mock.ANY, mock.ANY),
            mock.call.verify_part(mock.ANY, mock.ANY),
            mock.call.shutil.rmtree('$W_1'),

            # Finally the installer tools.
            mock.call.package_installer_tools(mock.ANY, mock.ANY),
        ])

        run_command = [
            call for call in manager.mock_calls if call[0] == 'run_command'
        ][0]
        pkg_dmg_args = run_command[1][0]

        self.assertEqual('$O/AppProduct-99.0.9999.99.dmg',
                         _get_adjacent_item(pkg_dmg_args, '--target'))
        self.assertEqual(config.app_product,
                         _get_adjacent_item(pkg_dmg_args, '--volname'))
        self.assertEqual('AppProduct-99.0.9999.99',
                         kwargs['sign_part'].mock_calls[0][1][2].identifier)

    @mock.patch('signing.pipeline._package_installer_tools')
    @mock.patch('signing.modification.customize_distribution')
    def test_sign_no_dmg(self, customize, package_installer, **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)
        manager.attach_mock(customize, 'customize_distribution')
        manager.attach_mock(package_installer, 'package_installer_tools')

        config = test_config.TestConfig()
        pipeline.sign_all(self.paths, config, package_dmg=False)

        package_installer.assert_called_once()

        manager.assert_has_calls([
            # Then the customization and distribution.
            mock.call.copy_files('$I/App Product.app', '$W_1'),
            mock.call.customize_distribution(mock.ANY, mock.ANY, mock.ANY),
            mock.call.sign_chrome(mock.ANY, mock.ANY),
            mock.call.make_dir('$O/AppProduct-99.0.9999.99'),
            mock.call.copy_files('$W_1/App Product.app',
                                 '$O/AppProduct-99.0.9999.99'),
            mock.call.shutil.rmtree('$W_1'),

            # Finally the installer tools.
            mock.call.package_installer_tools(mock.ANY, mock.ANY),
        ])

        self.assertEqual(0, kwargs['run_command'].call_count)

    @mock.patch('signing.pipeline._package_installer_tools')
    @mock.patch('signing.modification.customize_distribution')
    def test_sign_branded_distribution(self, customize, package_installer,
                                       **kwargs):
        manager = mock.Mock()
        for attr in kwargs:
            manager.attach_mock(kwargs[attr], attr)
        manager.attach_mock(customize, 'customize_distribution')
        manager.attach_mock(package_installer, 'package_installer_tools')

        class Config(test_config.TestConfig):

            @property
            def distributions(self):
                return [
                    model.Distribution(),
                    model.Distribution(
                        branding_code='MOO', dmg_name_fragment='ForCows'),
                ]

        config = Config()
        pipeline.sign_all(self.paths, config)

        package_installer.assert_called_once()
        self.assertEqual(2, customize.call_count)

        manager.assert_has_calls([
            # First distribution.
            mock.call.copy_files('$I/App Product.app', '$W_1'),
            mock.call.customize_distribution(mock.ANY, mock.ANY, mock.ANY),
            mock.call.sign_chrome(mock.ANY, mock.ANY),
            mock.call.make_dir('$W_1/empty'),
            mock.call.run_command(mock.ANY),
            mock.call.sign_part(mock.ANY, mock.ANY, mock.ANY),
            mock.call.verify_part(mock.ANY, mock.ANY),
            mock.call.shutil.rmtree('$W_1'),

            # Customized distribution.
            mock.call.copy_files('$I/App Product.app', '$W_2'),
            mock.call.customize_distribution(mock.ANY, mock.ANY, mock.ANY),
            mock.call.sign_chrome(mock.ANY, mock.ANY),
            mock.call.make_dir('$W_2/empty'),
            mock.call.run_command(mock.ANY),
            mock.call.sign_part(mock.ANY, mock.ANY, mock.ANY),
            mock.call.verify_part(mock.ANY, mock.ANY),
            mock.call.shutil.rmtree('$W_2'),

            # Finally the installer tools.
            mock.call.package_installer_tools(mock.ANY, mock.ANY),
        ])

        run_commands = [
            call for call in manager.mock_calls if call[0] == 'run_command'
        ]

        target = _get_adjacent_item(run_commands[0][1][0], '--target')
        self.assertEqual('$O/AppProduct-99.0.9999.99.dmg', target)
        self.assertEqual('AppProduct-99.0.9999.99',
                         kwargs['sign_part'].mock_calls[0][1][2].identifier)

        target = _get_adjacent_item(run_commands[1][1][0], '--target')
        self.assertEqual('$O/AppProduct-99.0.9999.99-ForCows.dmg', target)
        self.assertEqual('AppProduct-99.0.9999.99-MOO',
                         kwargs['sign_part'].mock_calls[1][1][2].identifier)
