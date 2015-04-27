

  Polymer.Base._addFeature({

    resolveUrl: function(url) {
      // TODO(sorvell): do we want to put the module reference on the prototype?
      var module = Polymer.DomModule.import(this.is);
      var root = '';
      if (module) {
        var assetPath = module.getAttribute('assetpath') || '';
        root = Polymer.ResolveUrl.resolveUrl(assetPath, module.ownerDocument.baseURI);
      }
      return Polymer.ResolveUrl.resolveUrl(url, root);
    }

  });

