

  Polymer = {
    Settings: (function() {
      // NOTE: Users must currently opt into using ShadowDOM. They do so by doing:
      // Polymer = {dom: 'shadow'};
      // TODO(sorvell): Decide if this should be auto-use when available.
      // TODO(sorvell): if SD is auto-use, then the flag above should be something
      // like: Polymer = {dom: 'shady'}
      
      // via Polymer object
      var user = window.Polymer || {};

      // via url
      location.search.slice(1).split('&').forEach(function(o) {
        o = o.split('=');
        o[0] && (user[o[0]] = o[1] || true);
      });

      var wantShadow = (user.dom === 'shadow');
      var hasShadow = Boolean(Element.prototype.createShadowRoot);
      var nativeShadow = hasShadow && !window.ShadowDOMPolyfill;
      var useShadow = wantShadow && hasShadow;

      var hasNativeImports = Boolean('import' in document.createElement('link'));
      var useNativeImports = hasNativeImports;

      var useNativeCustomElements = (!window.CustomElements ||
        window.CustomElements.useNative);

      return {
        wantShadow: wantShadow,
        hasShadow: hasShadow,
        nativeShadow: nativeShadow,
        useShadow: useShadow,
        useNativeShadow: useShadow && nativeShadow,
        useNativeImports: useNativeImports,
        useNativeCustomElements: useNativeCustomElements
      };
    })()
  };

