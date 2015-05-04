goog.provide('image.collections.extension.AdElementFilter');

goog.require('goog.array');
goog.require('goog.dom');
goog.require('goog.dom.classlist');
goog.require('image.collections.extension.ElementFilter');

goog.scope(function() {
var ElementFilter = image.collections.extension.ElementFilter;



/**
 * Filters out potential ad elements.
 * @constructor
 * @implements {ElementFilter}
 */
image.collections.extension.AdElementFilter = function() {
  /** @private {!Array.<string>} */
  this.adWords_ = ['ad', 'advertisement', 'ads', 'sponsor', 'sponsored'];
};
var AdElementFilter = image.collections.extension.AdElementFilter;


/** @override */
AdElementFilter.prototype.filter = function(element) {
  var ancestorElement = element;
  while (ancestorElement) {
    var classNames = goog.dom.classlist.get(ancestorElement);
    for (var i = 0; i < classNames.length; ++i) {
      var tokens = classNames[i].split(/\W+/);
      for (var j = 0; j < tokens.length; ++j) {
        if (goog.array.contains(this.adWords_, tokens[j].toLowerCase())) {
          return false;
        }
      }
    }
    ancestorElement = goog.dom.getParentElement(ancestorElement);
  }
  return true;
};
});  // goog.scope
