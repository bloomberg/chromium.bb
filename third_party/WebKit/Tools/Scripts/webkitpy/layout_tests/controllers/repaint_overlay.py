# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re


def result_contains_repaint_rects(text):
    return isinstance(text, str) and re.search('^\s*\(repaint rects$', text, re.MULTILINE) != None


def generate_repaint_overlay_html(test_name, actual_text, expected_text):
    if not result_contains_repaint_rects(expected_text):
        return ''

    def make_js_rect(input_str):
        rect_pattern = '\(rect\s+(\d+\.\d+)\s+(\d+\.\d+)\s+(\d+\.\d+)\s+(\d+\.\d+)\)'
        rects = []
        for m in re.finditer(rect_pattern, input_str):
            rects.append('[' + ','.join(m.groups()) + ']')
        return '[' + ','.join(rects) + ']'

    # FIXME: Need to consider layer offset and transforms.
    expected_rects = make_js_rect(expected_text)
    actual_rects = make_js_rect(actual_text)

    return """<!DOCTYPE HTML>
<html>
<head>
<title>%(title)s</title>
<style>
    body {
        margin: 0;
        padding: 0;
    }
    iframe {
      position: absolute;
      top: 60px;
      left: 0;
      border: 0;
      z-index: -1;
    }
    canvas {
      position: absolute;
      top: 60px;
      left: 0;
      z-index: 1;
    }
    #actual { display: none; }
    #dump {
      position: absolute;
      top: 60px;
      left: 0;
      z-index: 2;
      display: none;
    }
</style>
</head>
<body>
Note: This version doesn't support transformation and layer offset yet.<br>
<label><input type="checkbox" checked onchange="toggle_test(this.checked)">Show test</label>
<label><input type="checkbox" checked onchange="toggle_diff_only(this.checked)">Show differences only</label>
<label><input type="checkbox" checked onchange="toggle_hide_duplicate_rects(this.checked)">Hide duplicate rects</label>
<label><input type="checkbox" onchange="toggle_print_rects(this.checked)">Dump rects</label>
<br>
<span id='type'>Expected Invalidations</span>
<div id=overlay>
    <canvas id='expected' width='2000' height='2000'></canvas>
    <canvas id='actual' width='2000' height='2000'></canvas>
    <pre id='dump'></pre>
</div>
<script>
var show_diff_only = true;
var hide_duplicate_rects = true;

function toggle_test(show_test) {
    iframe.style.display = show_test ? 'block' : 'none';
}

function toggle_diff_only(new_show_diff_only) {
    show_diff_only = new_show_diff_only;
    draw_repaint_rects();
}

function toggle_hide_duplicate_rects(new_hide_duplicate_rects) {
    hide_duplicate_rects = new_hide_duplicate_rects;
    draw_repaint_rects();
}

function toggle_print_rects(dump_rects) {
    document.getElementById('dump').style.display = dump_rects ? 'block' : 'none';
}

var original_expected_rects = %(expected_rects)s;
var original_actual_rects = %(actual_rects)s;

function rectsEqual(rect1, rect2) {
    return rect1[0] == rect2[0] && rect1[1] == rect2[1] && rect1[2] == rect2[2] && rect1[3] == rect2[3];
}

function findDifference(rects1, rects2) {
    for (var i = rects1.length - 1; i >= 0; i--) {
        for (var k = rects2.length - 1; k >= 0; k--) {
            if (rectsEqual(rects1[i], rects2[k])) {
                rects1.splice(i, 1);
                rects2.splice(k, 1);
                break;
            }
        }
    }
}

function removeDuplicateRects(rects) {
    for (var i = rects.length - 1; i > 0; i--) {
        for (var k = i - 1; k >= 0; k--) {
            if (rectsEqual(rects[i], rects[k])) {
                rects.splice(i, 1);
                break;
            }
        }
    }
}

function draw_rects(context, rects) {
    context.clearRect(0, 0, 2000, 2000);
    for (var i = 0; i < rects.length; i++) {
        var rect = rects[i];
        context.fillRect(rect[0], rect[1], rect[2], rect[3]);
        context.strokeRect(rect[0], rect[1], rect[2], rect[3]);
    }
}

var expected_canvas = document.getElementById('expected');
var actual_canvas = document.getElementById('actual');

function dump_rects(rects) {
    var result = '';
    for (var i = 0; i < rects.length; i++)
        result += '(' + rects[i].toString() + ')\\n';
    return result;
}

function draw_repaint_rects() {
    var expected_rects = original_expected_rects.slice(0);
    var actual_rects = original_actual_rects.slice(0);
    if (hide_duplicate_rects) {
        removeDuplicateRects(expected_rects);
        removeDuplicateRects(actual_rects);
    }

    if (show_diff_only)
        findDifference(expected_rects, actual_rects);

    document.getElementById('dump').textContent =
        'Expected:\\n' + dump_rects(expected_rects)
        + '\\nActual:\\n' + dump_rects(actual_rects);

    var expected_ctx = expected_canvas.getContext("2d");
    expected_ctx.lineWidth = 0.5;
    expected_ctx.strokeStyle = 'rgba(255, 0, 0, 1)';
    expected_ctx.fillStyle = 'rgba(255, 0, 0, 0.25)';
    draw_rects(expected_ctx, expected_rects);

    var actual_ctx = actual_canvas.getContext("2d");
    actual_ctx.lineWidth = 1;
    actual_ctx.strokeStyle = 'rgba(0, 255, 0, 1)';
    actual_ctx.fillStyle = 'rgba(0, 255, 0, 0.25)';
    draw_rects(actual_ctx, actual_rects);
}
draw_repaint_rects();

var path = decodeURIComponent(location.search).substr(1);
var iframe = document.createElement('iframe');
iframe.id = 'test-frame';
iframe.width = 800;
iframe.height = 600;
iframe.src = path;

var overlay = document.getElementById('overlay');
overlay.appendChild(iframe);

var type = document.getElementById('type');
var expected_showing = true;
function flip() {
    if (expected_showing) {
        type.textContent = 'Actual Invalidations';
        expected_canvas.style.display = 'none';
        actual_canvas.style.display = 'block';
    } else {
        type.textContent = 'Expected Invalidations';
        actual_canvas.style.display = 'none';
        expected_canvas.style.display = 'block';
    }
    expected_showing = !expected_showing
}
setInterval(flip, 3000);
</script>
</body>
</html>
""" % {
        'title': test_name,
        'expected_rects': expected_rects,
        'actual_rects': actual_rects,
    }
