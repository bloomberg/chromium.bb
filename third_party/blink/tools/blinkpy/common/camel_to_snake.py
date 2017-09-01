# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re


def convert(name):
    special_cases = [
        ('CSS', 'Css'),
        ('Context2D', 'Context_2d'),
        ('DList', 'Dlist'),
        ('ETC1', 'Etc1'),
        ('IFrame', 'Iframe'),
        ('OList', 'Olist'),
        ('OnLine', 'Online'),
        ('Path2D', 'Path_2d'),
        ('Point2D', 'Point_2d'),
        ('RTCDTMF', 'Rtc_dtmf'),
        ('S3TC', 'S3tc'),
        ('UList', 'Ulist'),
        ('XPath', 'Xpath'),
        ('sRGB', 'Srgb'),

        ('SVGFE', 'Svg_fe'),
        ('SVGMPath', 'Svg_mpath'),
        ('SVGTSpan', 'Svg_tspan'),
        ('SVG', 'Svg'),

        ('XHTML', 'Xhtml'),
        ('HTML', 'Html'),

        ('WebGL2', 'Webgl2'),
        ('WebGL', 'Webgl'),
    ]
    for old, new in special_cases:
        name = re.sub(old, new, name)

    name = re.sub(r'([A-Z][A-Z0-9]*)([A-Z][a-z0-9])', r'\1_\2', name)
    name = re.sub(r'([a-z0-9])([A-Z])', r'\1_\2', name)
    return name.lower()
