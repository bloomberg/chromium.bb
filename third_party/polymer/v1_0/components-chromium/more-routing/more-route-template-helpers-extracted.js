(function(scope) {
var MoreRouting = scope.MoreRouting = scope.MoreRouting || {};

/**
 * Helper functions to spruce up your templates.
 *
 * @behavior
 */
MoreRouting.TemplateHelpers = {

  urlFor: function(pathOrName, params) {
    return MoreRouting.urlFor(pathOrName, params);
  },

  isCurrentUrl: function(pathOrName, params) {
    return MoreRouting.isCurrentUrl(pathOrName, params);
  },

};

})(window);