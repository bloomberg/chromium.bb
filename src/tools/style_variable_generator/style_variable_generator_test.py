#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import os
sys.path += [os.path.dirname(os.path.dirname(__file__))]

from style_variable_generator.base_generator import Modes
from style_variable_generator.css_generator import CSSStyleGenerator
from style_variable_generator.proto_generator import ProtoStyleGenerator, ProtoJSONStyleGenerator
from style_variable_generator.views_generator import ViewsStyleGenerator
from style_variable_generator.ts_generator import TSStyleGenerator
import unittest


class BaseStyleGeneratorTest:
    def assertEqualToFile(self, value, filename):
        with open(filename, 'r') as f:
            self.maxDiff = None
            self.assertEqual(value, f.read())

    def testColorTestJSON(self):
        self.generator.out_file_path = (
            'tools/style_variable_generator/colors_test_expected.h')
        self.assertEqualToFile(self.generator.Render(),
                               self.expected_output_file)


class ViewsStyleGeneratorTest(unittest.TestCase, BaseStyleGeneratorTest):
    def setUp(self):
        self.generator = ViewsStyleGenerator()
        self.generator.AddJSONFilesToModel(
            ['colors_test_palette.json5', 'colors_test.json5'])
        self.expected_output_file = 'colors_test_expected.h.generated'


class CSSStyleGeneratorTest(unittest.TestCase, BaseStyleGeneratorTest):
    def setUp(self):
        self.generator = CSSStyleGenerator()
        self.generator.AddJSONFilesToModel(
            ['colors_test_palette.json5', 'colors_test.json5'])
        self.expected_output_file = 'colors_test_expected.css'

    def testCustomDarkModeSelector(self):
        expected_file_name = 'colors_test_custom_dark_toggle_expected.css'
        self.generator.generator_options = {
            'dark_mode_selector': 'html[dark]:not(body)'
        }
        self.assertEqualToFile(self.generator.Render(), expected_file_name)

    def testCustomDarkModeOnly(self):
        expected_file_name = 'colors_test_dark_only_expected.css'
        self.generator.generate_single_mode = Modes.DARK
        self.assertEqualToFile(self.generator.Render(), expected_file_name)

    def testDebugPlaceholder(self):
        expected_file_name = 'colors_test_debug_placeholder_expected.css'
        self.generator.generator_options = {
            'debug_placeholder': 'DEBUG_CSS_GOES_HERE\n'
        }
        self.assertEqualToFile(self.generator.Render(), expected_file_name)

    def testUntypedCSS(self):
        self.generator = CSSStyleGenerator()
        self.generator.AddJSONFilesToModel(['untyped_css_test.json5'])
        expected_file_name = 'untyped_css_test_expected.css'
        self.assertEqualToFile(self.generator.Render(), expected_file_name)

    def testTypography(self):
        self.generator = CSSStyleGenerator()
        self.generator.AddJSONFilesToModel(['typography_test.json5'])
        expected_file_name = 'typography_test_expected.css'
        self.assertEqualToFile(self.generator.Render(), expected_file_name)

    def testSuppressSourcesComment(self):
        self.generator.generator_options = {'suppress_sources_comment': 'true'}
        expected_file_name = 'suppress_sources_comment_test_expected.css'
        self.assertEqualToFile(self.generator.Render(), expected_file_name)


class TSStyleGeneratorTest(unittest.TestCase, BaseStyleGeneratorTest):
    def setUp(self):
        self.generator = TSStyleGenerator()
        self.generator.AddJSONFilesToModel(
            ['colors_test_palette.json5', 'colors_test.json5'])
        self.expected_output_file = 'colors_test_expected.ts'

    def testIncludeStyleSheet(self):
        expected_file_name = 'colors_test_include_style_sheet_expected.ts'
        self.generator.generator_options = {'include_style_sheet': 'true'}
        self.assertEqualToFile(self.generator.Render(), expected_file_name)

    def testTypography(self):
        expected_file_name = 'colors_test_typography_expected.ts'
        self.generator.AddJSONFilesToModel(['typography_test.json5'])
        self.generator.generator_options = {'include_style_sheet': 'true'}
        self.assertEqualToFile(self.generator.Render(), expected_file_name)

    def testUntypedCSS(self):
        expected_file_name = 'colors_test_untyped_css_expected.ts'
        self.generator.AddJSONFilesToModel(['untyped_css_test.json5'])
        self.generator.generator_options = {'include_style_sheet': 'true'}
        self.assertEqualToFile(self.generator.Render(), expected_file_name)

    def testTypographyAndUntypedCSS(self):
        expected_file_name = 'colors_test_typography_and_untyped_css_expected.ts'
        self.generator.AddJSONFilesToModel(
            ['typography_test.json5', 'untyped_css_test.json5'])
        self.generator.generator_options = {'include_style_sheet': 'true'}
        self.assertEqualToFile(self.generator.Render(), expected_file_name)

    def testSuppressSourcesComment(self):
        self.generator.generator_options = {'suppress_sources_comment': 'true'}
        expected_file_name = 'suppress_sources_comment_test_expected.ts'
        self.assertEqualToFile(self.generator.Render(), expected_file_name)


class ProtoStyleGeneratorTest(unittest.TestCase, BaseStyleGeneratorTest):
    def setUp(self):
        self.generator = ProtoStyleGenerator()
        self.generator.AddJSONFilesToModel(
            ['colors_test_palette.json5', 'colors_test.json5'])
        self.expected_output_file = 'colors_test_expected.proto'


class ProtoJSONStyleGeneratorTest(unittest.TestCase, BaseStyleGeneratorTest):
    def setUp(self):
        self.generator = ProtoJSONStyleGenerator()
        paths = [
            'colors_test_palette.json5',
            'colors_test.json5',
            # Add in a separate file which adds more colors to test_colors so we
            # can confirm we do not generate duplicate fields.
            'additional_colors_test.json5',
        ]
        self.generator.AddJSONFilesToModel(paths)
        self.expected_output_file = 'colors_test_expected.protojson'


class BlendStyleGeneratorTest(unittest.TestCase, BaseStyleGeneratorTest):
    def setUp(self):
        self.generator = CSSStyleGenerator()
        self.generator.AddJSONFilesToModel(
            ['colors_test_palette.json5', 'blend_colors_test.json5'])
        self.expected_output_file = 'blend_colors_test_expected.css'


class InvertedStyleGeneratorTest(unittest.TestCase, BaseStyleGeneratorTest):
    def setUp(self):
        self.generator = CSSStyleGenerator()
        self.generator.AddJSONFilesToModel(
            ['colors_test_palette.json5', 'inverted_colors_test.json5'])
        self.expected_output_file = 'inverted_colors_test_expected.css'


if __name__ == '__main__':
    unittest.main()
