onmessage = function(event) {
  var str = event.data;
  var args = str.split(':');
  var canvas_width = parseInt(args[0]);
  var tile_width   = parseInt(args[1]);
  var x            = parseInt(args[2]);
  var y            = parseInt(args[3]);

  // Compute the color for one pixel.
  var mandelJavaScript = function(i, j, canvas_width) {
    var cx = (4.0 / canvas_width) * i - 3.0;
    var cy = 1.5 - (3.0 / canvas_width) * j;
    var re = cx;
    var im = cy;
    var count = 0;
    var threshold = 1.0e8;
    while (count < 256 && re * re + im * im < threshold) {
      var new_re = re * re - im * im + cx;
      var new_im = 2 * re * im + cy;
      re = new_re;
      im = new_im;
      ++count;
    }
    var r;
    var g;
    var b;
    if (count < 8) {
      r = 128;
      g = 0;
      b = 0;
    } else if (count < 16) {
      r = 255;
      g = 0;
      b = 0;
    } else if (count < 32) {
      r = 255;
      g = 255;
      b = 0;
    } else if (count < 64) {
      r = 0;
      g = 255;
      b = 0;
    } else if (count < 128) {
      r = 0;
      g = 255;
      b = 255;
    } else if (count < 256) {
      r = 0;
      g = 0;
      b = 255;
    } else {
      r = 0;
      g = 0;
      b = 0;
    }
    return [r, g, b];
  }

  // Make an exactly three character string from an in-range number.
  var formatNum = function(number) {
    str = '' + parseInt(number / 100);
    number = number % 100;
    str = str + parseInt(number / 10) + parseInt(number % 10);
    return str;
  }

  // Produce a string containing "x:y:rgbarray".
  str = '' + x + ':' + y + ':';
  for (var i = x; i < x + tile_width; ++i) {
    for (var j = y; j < y + tile_width; ++j) {
      var arr = mandelJavaScript(i, j, canvas_width);
      str = str + 'rgb(' + formatNum(arr[0]) + ',' +
                           formatNum(arr[1]) + ',' +
                           formatNum(arr[2]) + ')';
    }
  }
  postMessage(str);
}
