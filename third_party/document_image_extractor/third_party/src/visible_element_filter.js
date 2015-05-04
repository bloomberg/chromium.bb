goog.provide('image.collections.extension.VisibleElementFilter');

goog.require('goog.dom');
goog.require('goog.style');
goog.require('image.collections.extension.ElementFilter');

goog.scope(function() {
var ElementFilter = image.collections.extension.ElementFilter;



/**
 * Filters elements by visibility.
 * @implements {ElementFilter}
 * @constructor
 */
image.collections.extension.VisibleElementFilter = function() {};
var VisibleElementFilter = image.collections.extension.VisibleElementFilter;


/** @override */
VisibleElementFilter.prototype.filter = function(element) {
  // TODO(busaryev): handle the overflow: hidden case.
  var ancestorElement = element;
  while (ancestorElement) {
    if (goog.style.getComputedStyle(ancestorElement, 'display') == 'none') {
      return false;
    }
    var size = goog.style.getSize(ancestorElement);
    var overflow = goog.style.getComputedStyle(ancestorElement, 'overflow');
    if ((overflow == 'hidden') && size.isEmpty()) {
      return false;
    }
    if (goog.style.getComputedStyle(ancestorElement, 'visibility') ==
        'hidden') {
      return false;
    }

    ancestorElement = goog.dom.getParentElement(ancestorElement);
  }
  return true;
};
});  // goog.scope
