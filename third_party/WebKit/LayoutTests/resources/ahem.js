// A test using Ahem font includes this file to ensure that the
// font is loaded before window.onload fires and test finishes.
// Note: this doesn't work if the test doesn't contain any visible
// element with 'font-family: Ahem' style.
(function() {
  var scripts = document.getElementsByTagName('script');
  var src = scripts[scripts.length - 1].src;
  var lastSlash = src.lastIndexOf('/');
  var relativePath = src.substr(0, lastSlash);

  window.addEventListener('DOMContentLoad', function() {
    var style = document.createElement('style');
    style.appendChild(document.createTextNode(
      '@font-face { font-family: Ahem; src: url(' + relativePath + '/Ahem.ttf'));
    document.head.appendChild(style);
    // Force a layout to start loading the font.
    document.documentElement.offsetTop;
  }, true);
}());
