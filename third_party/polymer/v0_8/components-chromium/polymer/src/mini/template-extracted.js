

  /**
   * Automatic template management.
   * 
   * The `template` feature locates and instances a `<template>` element
   * corresponding to the current Polymer prototype.
   * 
   * The `<template>` element may be immediately preceeding the script that 
   * invokes `Polymer()`.
   *  
   * @class standard feature: template
   */
  
  Polymer.Base._addFeature({

    _prepTemplate: function() {
      // locate template using dom-module
      this._template = 
        this._template || Polymer.DomModule.import(this.is, 'template');
      // fallback to look at the node previous to the currentScript.
      if (!this._template) {
        var script = document._currentScript || document.currentScript;
        var prev = script && script.previousElementSibling;
        if (prev && prev.localName === 'template') {
          this._template = prev;
        }
      }
    },

    _stampTemplate: function() {
      if (this._template) {
        // note: root is now a fragment which can be manipulated
        // while not attached to the element.
        this.root = this.instanceTemplate(this._template);
      }
    },

    instanceTemplate: function(template) {
      var dom = 
        document.importNode(template._content || template.content, true);
      return dom;
    }

  });

