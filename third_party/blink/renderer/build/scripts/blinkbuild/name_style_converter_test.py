# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=import-error,print-statement,relative-import,protected-access

"""Unit tests for name_style_converter.py."""

import unittest

from name_style_converter import NameStyleConverter
from name_style_converter import SmartTokenizer


class SmartTokenizerTest(unittest.TestCase):
    def test_simple_cases(self):
        tokenizer = SmartTokenizer('foo')
        self.assertEqual(tokenizer.tokenize(), ['foo'])

        tokenizer = SmartTokenizer('fooBar')
        self.assertEqual(tokenizer.tokenize(), ['foo', 'Bar'])

        tokenizer = SmartTokenizer('fooBarBaz')
        self.assertEqual(tokenizer.tokenize(), ['foo', 'Bar', 'Baz'])

        tokenizer = SmartTokenizer('Baz')
        self.assertEqual(tokenizer.tokenize(), ['Baz'])

        tokenizer = SmartTokenizer('')
        self.assertEqual(tokenizer.tokenize(), [])

        tokenizer = SmartTokenizer('FOO')
        self.assertEqual(tokenizer.tokenize(), ['FOO'])

        tokenizer = SmartTokenizer('foo2')
        self.assertEqual(tokenizer.tokenize(), ['foo', '2'])

    def test_tricky_cases(self):
        tokenizer = SmartTokenizer('XMLHttpRequest')
        self.assertEqual(tokenizer.tokenize(), ['XML', 'Http', 'Request'])

        tokenizer = SmartTokenizer('HTMLElement')
        self.assertEqual(tokenizer.tokenize(), ['HTML', 'Element'])

        tokenizer = SmartTokenizer('WebGLRenderingContext')
        self.assertEqual(tokenizer.tokenize(),
                         ['WebGL', 'Rendering', 'Context'])

        tokenizer = SmartTokenizer('CanvasRenderingContext2D')
        self.assertEqual(tokenizer.tokenize(),
                         ['Canvas', 'Rendering', 'Context', '2D'])
        tokenizer = SmartTokenizer('CanvasRenderingContext2DAPITest')
        self.assertEqual(tokenizer.tokenize(),
                         ['Canvas', 'Rendering', 'Context', '2D', 'API', 'Test'])

        tokenizer = SmartTokenizer('SVGSVGElement')
        self.assertEqual(tokenizer.tokenize(), ['SVG', 'SVG', 'Element'])

        tokenizer = SmartTokenizer('CanvasRenderingContext2D')
        self.assertEqual(tokenizer.tokenize(), ['Canvas', 'Rendering', 'Context', '2D'])

        tokenizer = SmartTokenizer('CSSURLImageValue')
        self.assertEqual(tokenizer.tokenize(), ['CSS', 'URL', 'Image', 'Value'])
        tokenizer = SmartTokenizer('CSSPropertyAPID')
        self.assertEqual(tokenizer.tokenize(), ['CSS', 'Property', 'API', 'D'])
        tokenizer = SmartTokenizer('AXARIAGridCell')
        self.assertEqual(tokenizer.tokenize(), ['AX', 'ARIA', 'Grid', 'Cell'])

        tokenizer = SmartTokenizer('CDATASection')
        self.assertEqual(tokenizer.tokenize(), ['CDATA', 'Section'])

        tokenizer = SmartTokenizer('ASCIICType')
        self.assertEqual(tokenizer.tokenize(), ['ASCII', 'CType'])
        tokenizer = SmartTokenizer('CString')
        self.assertEqual(tokenizer.tokenize(), ['CString'])

        tokenizer = SmartTokenizer('HTMLDListElement')
        self.assertEqual(tokenizer.tokenize(), ['HTML', 'DList', 'Element'])
        tokenizer = SmartTokenizer('HTMLOListElement')
        self.assertEqual(tokenizer.tokenize(), ['HTML', 'OList', 'Element'])
        tokenizer = SmartTokenizer('HTMLIFrameElement')
        self.assertEqual(tokenizer.tokenize(), ['HTML', 'IFrame', 'Element'])
        tokenizer = SmartTokenizer('HTMLPlugInElement')
        self.assertEqual(tokenizer.tokenize(), ['HTML', 'PlugIn', 'Element'])

        # No special handling for OptGroup, FieldSet, and TextArea.
        tokenizer = SmartTokenizer('HTMLOptGroupElement')
        self.assertEqual(tokenizer.tokenize(), ['HTML', 'Opt', 'Group', 'Element'])
        tokenizer = SmartTokenizer('HTMLFieldSetElement')
        self.assertEqual(tokenizer.tokenize(), ['HTML', 'Field', 'Set', 'Element'])
        tokenizer = SmartTokenizer('HTMLTextAreaElement')
        self.assertEqual(tokenizer.tokenize(), ['HTML', 'Text', 'Area', 'Element'])

        tokenizer = SmartTokenizer('Path2D')
        self.assertEqual(tokenizer.tokenize(), ['Path', '2D'])
        tokenizer = SmartTokenizer('Point2D')
        self.assertEqual(tokenizer.tokenize(), ['Point', '2D'])
        tokenizer = SmartTokenizer('CanvasRenderingContext2DState')
        self.assertEqual(tokenizer.tokenize(), ['Canvas', 'Rendering', 'Context', '2D', 'State'])

        tokenizer = SmartTokenizer('RTCDTMFSender')
        self.assertEqual(tokenizer.tokenize(), ['RTC', 'DTMF', 'Sender'])

        tokenizer = SmartTokenizer('WebGLCompressedTextureS3TCsRGB')
        self.assertEqual(tokenizer.tokenize(), ['WebGL', 'Compressed', 'Texture', 'S3TC', 'sRGB'])
        tokenizer = SmartTokenizer('WebGL2CompressedTextureETC1')
        self.assertEqual(tokenizer.tokenize(), ['WebGL2', 'Compressed', 'Texture', 'ETC1'])
        tokenizer = SmartTokenizer('EXTsRGB')
        self.assertEqual(tokenizer.tokenize(), ['EXT', 'sRGB'])

        tokenizer = SmartTokenizer('SVGFEBlendElement')
        self.assertEqual(tokenizer.tokenize(), ['SVG', 'FE', 'Blend', 'Element'])
        tokenizer = SmartTokenizer('SVGMPathElement')
        self.assertEqual(tokenizer.tokenize(), ['SVG', 'MPath', 'Element'])
        tokenizer = SmartTokenizer('SVGTSpanElement')
        self.assertEqual(tokenizer.tokenize(), ['SVG', 'TSpan', 'Element'])
        tokenizer = SmartTokenizer('SVGURIReference')
        self.assertEqual(tokenizer.tokenize(), ['SVG', 'URI', 'Reference'])

        tokenizer = SmartTokenizer('UTF16TextIterator')
        self.assertEqual(tokenizer.tokenize(), ['UTF16', 'Text', 'Iterator'])
        tokenizer = SmartTokenizer('UTF8Decoder')
        self.assertEqual(tokenizer.tokenize(), ['UTF8', 'Decoder'])
        tokenizer = SmartTokenizer('Uint8Array')
        self.assertEqual(tokenizer.tokenize(), ['Uint8', 'Array'])
        tokenizer = SmartTokenizer('DOMWindowBase64')
        self.assertEqual(tokenizer.tokenize(), ['DOM', 'Window', 'Base64'])
        tokenizer = SmartTokenizer('TextCodecLatin1')
        self.assertEqual(tokenizer.tokenize(), ['Text', 'Codec', 'Latin1'])
        tokenizer = SmartTokenizer('V8BindingForCore')
        self.assertEqual(tokenizer.tokenize(), ['V8', 'Binding', 'For', 'Core'])
        tokenizer = SmartTokenizer('V8DOMRect')
        self.assertEqual(tokenizer.tokenize(), ['V8', 'DOM', 'Rect'])
        tokenizer = SmartTokenizer('String16MojomTraits')
        self.assertEqual(tokenizer.tokenize(), ['String16', 'Mojom', 'Traits'])

        tokenizer = SmartTokenizer('V0InsertionPoint')
        self.assertEqual(tokenizer.tokenize(), ['V0', 'Insertion', 'Point'])
        tokenizer = SmartTokenizer('ShadowDOMV0Test')
        self.assertEqual(tokenizer.tokenize(), ['Shadow', 'DOM', 'V0', 'Test'])
        tokenizer = SmartTokenizer('ElementShadowV0')
        self.assertEqual(tokenizer.tokenize(), ['Element', 'Shadow', 'V0'])
        tokenizer = SmartTokenizer('StubChromeClientForSPv2')
        self.assertEqual(tokenizer.tokenize(), ['Stub', 'Chrome', 'Client', 'For', 'SPv2'])

        tokenizer = SmartTokenizer('SQLiteAuthorizer')
        self.assertEqual(tokenizer.tokenize(), ['SQLite', 'Authorizer'])
        tokenizer = SmartTokenizer('XPathEvaluator')
        self.assertEqual(tokenizer.tokenize(), ['XPath', 'Evaluator'])

        tokenizer = SmartTokenizer('IsXHTMLDocument')
        self.assertEqual(tokenizer.tokenize(), ['Is', 'XHTML', 'Document'])

        tokenizer = SmartTokenizer('Animation.idl')
        self.assertEqual(tokenizer.tokenize(), ['Animation', '.idl'])


class NameStyleConverterTest(unittest.TestCase):
    def test_snake_case(self):
        converter = NameStyleConverter('HTMLElement')
        self.assertEqual(converter.to_snake_case(), 'html_element')

    def test_upper_camel_case(self):
        converter = NameStyleConverter('someSuperThing')
        self.assertEqual(converter.to_upper_camel_case(), 'SomeSuperThing')

        converter = NameStyleConverter('SVGElement')
        self.assertEqual(converter.to_upper_camel_case(), 'SVGElement')

    def test_macro_case(self):
        converter = NameStyleConverter('WebGLBaz2D')
        self.assertEqual(converter.to_macro_case(), 'WEBGL_BAZ_2D')

    def test_all_cases(self):
        converter = NameStyleConverter('SVGScriptElement')
        self.assertEqual(converter.to_all_cases(), {
            'snake_case': 'svg_script_element',
            'upper_camel_case': 'SVGScriptElement',
            'macro_case': 'SVG_SCRIPT_ELEMENT',
        })
