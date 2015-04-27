
(function() {

  Polymer({

    is: 'x-style',
    extends: 'style',

    created: function() {
      var rules = Polymer.StyleUtil.parser.parse(this.textContent);
      this.applyProperties(rules);
      // TODO(sorvell): since custom rules must match directly, they tend to be
      // made with selectors like `*`.
      // We *remove them here* so they don't apply too widely and nerf recalc.
      // This means that normal properties mixe in rules with custom 
      // properties will *not* apply.
      var cssText = Polymer.StyleUtil.parser.stringify(rules);
      this.textContent = this.scopeCssText(cssText);
    },

    scopeCssText: function(cssText) {
      return Polymer.Settings.useNativeShadow ?
        cssText :
        Polymer.StyleUtil.toCssText(cssText, function(rule) {
          Polymer.StyleTransformer.rootRule(rule);
      });
    },

    applyProperties: function(rules) {
      var cssText = '';
      Polymer.StyleUtil.forEachStyleRule(rules, function(rule) {
        if (rule.cssText.match(CUSTOM_RULE)) {
          // TODO(sorvell): use parser.stringify, it needs an option not to
          // strip custom properties.
          cssText += rule.selector + ' {\n' + rule.cssText + '\n}\n';
        }
      });
      if (cssText) {
        Polymer.StyleDefaults.applyCss(cssText);
      }
    }

  });

  var CUSTOM_RULE = /--[^;{'"]*\:/;

})();
