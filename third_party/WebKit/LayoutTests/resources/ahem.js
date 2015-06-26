// This file is included by tests using Ahem font to ensure that the font is
// loaded before window.onload fires and the test finishes.
// Note: this doesn't work if the test doesn't contain any visible element with
// 'font-family: Ahem' style.
(function() {
  var scripts = document.getElementsByTagName('script');
  var scriptSrc = scripts[scripts.length - 1].src;
  var relativePath = scriptSrc.substr(0, scriptSrc.lastIndexOf('/'));

  window.addEventListener('DOMContentLoaded', function() {
    var style = document.createElement('style');
    style.appendChild(document.createTextNode(
      '@font-face { font-family: Ahem; src: url(' + relativePath + '/Ahem.ttf); }'));
    document.head.appendChild(style);
    // Force a layout to start loading the font.
    document.documentElement.offsetTop;
  }, true);
}());
