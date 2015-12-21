// This file is included by tests using Ahem font to ensure that the font is
// loaded before window.onload fires and the test finishes.
// Note: this doesn't work if the test doesn't contain any visible element with
// 'font-family: Ahem' style.
(function() {
  var getPathToAhem = function() {
    var scripts = document.getElementsByTagName('script');
    for (var scriptIndex = 0; scriptIndex < scripts.length; scriptIndex++) {
      var src = scripts[scriptIndex].src || scripts[scriptIndex].getAttribute('xlink:href');
      if (src && src.indexOf('ahem.js') !== -1)
        return src.substr(0, src.lastIndexOf('/')) + '/Ahem.ttf';
    }
    console.log("Error: <script> referencing ahem.js could not be found.");
  }

  window.addEventListener('DOMContentLoaded', function() {
    var style = document.createElement('style');
    style.appendChild(document.createTextNode(
      '@font-face { font-family: Ahem; src: url(' + getPathToAhem() + '); }'));
    document.documentElement.appendChild(style);

    // Force a layout to start loading the font.
    if (document.documentElement.getBBox)
      document.documentElement.getBBox();
    else
      document.documentElement.offsetTop;
  }, true);
}());
