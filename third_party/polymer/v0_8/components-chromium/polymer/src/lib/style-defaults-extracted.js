

  (function() {
    
    var defaultSheet = document.createElement('style'); 

    function applyCss(cssText) {
      defaultSheet.textContent += cssText;
      defaultSheet.__cssRules =
        Polymer.StyleUtil.parser.parse(defaultSheet.textContent);
    }

    applyCss('');

    // exports
    Polymer.StyleDefaults = {
      applyCss: applyCss,
      defaultSheet: defaultSheet
    };

  })();
