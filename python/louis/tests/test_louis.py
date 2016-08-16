from unittest import TestCase

import louis
import sys

class TestLouis(TestCase):
    def test_charSizeCompatibility(self):
        compiledCharSize = louis.charSize()
        pythonMaxUnicode = sys.maxunicode

        if compiledCharSize == 2:
            self.assertEqual(pythonMaxUnicode, 65535, msg="UCS2/4 mismatch. Try ./configure --enable-ucs4")
        elif compiledCharSize == 4:
            self.assertEqual(pythonMaxUnicode, 1114111, msg="UCS2/4 mismatch. If you compiled with --enable-ucs4, try without")

    def test_unicode(self):
        translated = louis.translate([b'../tables/en-us-g2.ctb'], 'Hello world!', cursorPos=5)
        self.assertEqual(type(translated[0]), unicode)

    def test_translate(self):
        translated = louis.translate([b'../tables/en-us-g2.ctb'], 'Hello world!', cursorPos=5)
        self.assertEqual(translated[0].encode('utf-8'), ",hello _w6")
