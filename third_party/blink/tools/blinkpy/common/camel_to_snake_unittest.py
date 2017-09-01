# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.common.camel_to_snake import convert


class TestCamelToSnake(unittest.TestCase):

    def test_convert(self):
        self.assertEqual(convert('Animation.idl'), 'animation.idl')
        self.assertEqual(convert('CSS.idl'), 'css.idl')
        self.assertEqual(convert('CSSURLImageValue'), 'css_url_image_value')
        self.assertEqual(convert('CDATASection'), 'cdata_section')
        self.assertEqual(convert('NavigatorOnLine'), 'navigator_online')
        self.assertEqual(convert('HTMLDListElement'), 'html_dlist_element')
        self.assertEqual(convert('HTMLIFrameElement'), 'html_iframe_element')
        self.assertEqual(convert('HTMLOptGroupElement'), 'html_opt_group_element')
        self.assertEqual(convert('HTMLFieldSetElement'), 'html_field_set_element')
        self.assertEqual(convert('HTMLTextAreaElement'), 'html_text_area_element')
        self.assertEqual(convert('SVGFEBlendElement'), 'svg_fe_blend_element')
        self.assertEqual(convert('SVGURIReference'), 'svg_uri_reference')
        self.assertEqual(convert('XPathEvaluator'), 'xpath_evaluator')
        self.assertEqual(convert('UTF16TextIterator'), 'utf16_text_iterator')
        self.assertEqual(convert('WebGLCompressedTextureETC1.idl'), 'webgl_compressed_texture_etc1.idl')
