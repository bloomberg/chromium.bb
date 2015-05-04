goog.provide('image.collections.extension.ElementFilter');

goog.scope(function() {



/**
 * An element filter interface.
 * @interface
 */
image.collections.extension.ElementFilter = function() {};
var ElementFilter = image.collections.extension.ElementFilter;


/**
 * Returns true iff the element passes the filter.
 * @param {!Element} element
 * @return {boolean}
 */
ElementFilter.prototype.filter = function(element) {};
});  // goog.scope
