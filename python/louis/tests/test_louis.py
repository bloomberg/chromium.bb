from unittest import TestCase

import louis

class TestLouis(TestCase):
    def test_translate(self):
        translated = louis.translate([b'../tables/en-us-g2.ctb'], 'Hello world!', cursorPos=5)
        self.assertEqual(type(translated[0]), unicode)
        self.assertEqual(translated[0].encode('utf-8'), ",hello _w6", msg="error probably caused by UCS2/4 mismatch. Try ./configure --enable-ucs4")
