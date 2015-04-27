

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


;

  // until ES6 modules become standard, we follow Occam and simply stake out 
  // a global namespace

  // Polymer is a Function, but of course this is also an Object, so we 
  // hang various other objects off of Polymer.*
  (function() {
    var userPolymer = window.Polymer;
    
    window.Polymer = function(prototype) {
      var ctor = desugar(prototype);
      // native Custom Elements treats 'undefined' extends property
      // as valued, the property must not exist to be ignored
      var options = {
        prototype: ctor.prototype
      };
      if (prototype.extends) {
        options.extends = prototype.extends;
      }
      Polymer.telemetry._registrate(prototype);
      document.registerElement(prototype.is, options);
      return ctor;
    };

    var desugar = function(prototype) {
      prototype = Polymer.Base.chainObject(prototype, Polymer.Base);
      prototype.registerCallback();
      return prototype.constructor;
    };

    window.Polymer = Polymer;

    if (userPolymer) {
      for (var i in userPolymer) {
        Polymer[i] = userPolymer[i];
      }
    }

    Polymer.Class = desugar;

  })();
  /*
  // Raw usage
  [ctor =] Polymer.Class(prototype);
  document.registerElement(name, ctor);
  
  // Simplified usage
  [ctor = ] Polymer(prototype);
  */

  // telemetry: statistics, logging, and debug

  Polymer.telemetry = {
    registrations: [],
    _regLog: function(prototype) {
      console.log('[' + prototype.is + ']: registered')
    },
    _registrate: function(prototype) {
      this.registrations.push(prototype);
      Polymer.log && this._regLog(prototype);
    },
    dumpRegistrations: function() {
      this.registrations.forEach(this._regLog);
    }
  };


;

  // a tiny bit of sugar for `document.currentScript.ownerDocument`
  Object.defineProperty(window, 'currentImport', {
    enumerable: true,
    configurable: true,
    get: function() {
      return (document._currentScript || document.currentScript).ownerDocument;
    }
  });


;

  Polymer.Base = {

    // pluggable features
    // `this` context is a prototype, not an instance
    _addFeature: function(feature) {
      this.extend(this, feature);
    },

    // `this` context is a prototype, not an instance
    registerCallback: function() {
      this._registerFeatures();  // abstract
      this._doBehavior('registered'); // abstract
    },

    createdCallback: function() {
      Polymer.telemetry.instanceCount++;
      this.root = this;
      this._doBehavior('created'); // abstract
      this._initFeatures(); // abstract
    },

    // reserved for canonical behavior
    attachedCallback: function() {
      this.isAttached = true;
      this._doBehavior('attached'); // abstract
    },

    // reserved for canonical behavior
    detachedCallback: function() {
      this.isAttached = false;
      this._doBehavior('detached'); // abstract
    },

    // reserved for canonical behavior
    attributeChangedCallback: function(name) {
      this.setAttributeToProperty(this, name);
      this._doBehavior('attributeChanged', arguments); // abstract
    },

    // copy own properties from `api` to `prototype`
    extend: function(prototype, api) {
      if (prototype && api) {
        Object.getOwnPropertyNames(api).forEach(function(n) {
          this.copyOwnProperty(n, api, prototype);
        }, this);
      }
      return prototype || api;
    },

    copyOwnProperty: function(name, source, target) {
      var pd = Object.getOwnPropertyDescriptor(source, name);
      if (pd) {
        Object.defineProperty(target, name, pd);
      }
    }

  };

  if (Object.__proto__) {
    Polymer.Base.chainObject = function(object, inherited) {
      if (object && inherited && object !== inherited) {
        object.__proto__ = inherited;
      }
      return object;
    };
  } else {
    Polymer.Base.chainObject = function(object, inherited) {
      if (object && inherited && object !== inherited) {
        var chained = Object.create(inherited);
        object = Polymer.Base.extend(chained, object);
      }
      return object;
    };
  }

  Polymer.Base = Polymer.Base.chainObject(Polymer.Base, HTMLElement.prototype);

  // TODO(sjmiles): ad hoc telemetry
  Polymer.telemetry.instanceCount = 0;


;

(function() {

  var modules = {};

  var DomModule = function() {
    return document.createElement('dom-module');
  };

  DomModule.prototype = Object.create(HTMLElement.prototype);

  DomModule.prototype.constructor = DomModule;

  DomModule.prototype.createdCallback = function() {
    var id = this.id || this.getAttribute('name') || this.getAttribute('is');
    if (id) {
      this.id = id;
      modules[id] = this;
    }
  };

  DomModule.prototype.import = function(id, slctr) {
    var m = modules[id];
    if (!m) {
      // If polyfilling, a script can run before a dom-module element
      // is upgraded. We force the containing document to upgrade
      // and try again to workaround this polyfill limitation.
      forceDocumentUpgrade();
      m = modules[id];
    }
    if (m && slctr) {
      m = m.querySelector(slctr);
    }
    return m;
  };

  // NOTE: HTMLImports polyfill does not
  // block scripts on upgrading elements. However, we want to ensure that
  // any dom-module in the tree is available prior to a subsequent script
  // processing.
  // Therefore, we force any dom-modules in the tree to upgrade when dom-module
  // is registered by temporarily setting CE polyfill to crawl the entire
  // imports tree. (Note: this should only upgrade any imports that have been
  // loaded by this point. In addition the HTMLImports polyfill should be
  // changed to upgrade elements prior to running any scripts.)
  var cePolyfill = window.CustomElements && !CustomElements.useNative;
  if (cePolyfill) {
    var ready = CustomElements.ready;
    CustomElements.ready = true;
  }
  document.registerElement('dom-module', DomModule);
  if (cePolyfill) {
    CustomElements.ready = ready;
  }

  function forceDocumentUpgrade() {
    if (cePolyfill) {
      var script = document._currentScript || document.currentScript;
      if (script) {
        CustomElements.upgradeAll(script.ownerDocument);
      }
    }
  }

})();


;

  Polymer.Base._addFeature({

    _prepIs: function() {
      if (!this.is) {
        var module =
          (document._currentScript || document.currentScript).parentNode;
        if (module.localName === 'dom-module') {
          var id = module.id || module.getAttribute('name')
            || module.getAttribute('is')
          this.is = id;
        }
      }
    }

  });


;

  /**
   * Automatically extend using objects referenced in `behaviors` array.
   *
   *     someBehaviorObject = {
   *       accessors: {
   *        value: {type: Number, observer: '_numberChanged'}
   *       },
   *       observers: [
   *         // ...
   *       ],
   *       ready: function() {
   *         // called before prototoype's ready
   *       },
   *       _numberChanged: function() {}
   *     };
   *
   *     Polymer({
   *
   *       behaviors: [
   *         someBehaviorObject
   *       ]
   *
   *       ...
   *
   *     });
   *
   * @class base feature: behaviors
   */

  Polymer.Base._addFeature({

    behaviors: [],

    _prepBehaviors: function() {
      this._flattenBehaviors();
      this._prepBehavior(this);
      this.behaviors.forEach(function(b) {
        this._mixinBehavior(b);
        this._prepBehavior(b);
      }, this);
    },

    _flattenBehaviors: function() {
      var flat = [];
      this.behaviors.forEach(function(b) {
        if (!b) {
          console.warn('Polymer: undefined behavior in [' + this.is + ']');
        } else if (b instanceof Array) {
          flat = flat.concat(b);
        } else {
          flat.push(b);
        }
      }, this);
      this.behaviors = flat;
    },

    _mixinBehavior: function(b) {
      Object.getOwnPropertyNames(b).forEach(function(n) {
        switch (n) {
          case 'registered':
          case 'properties':
          case 'observers':
          case 'listeners':
          case 'keyPresses':
          case 'hostAttributes':
          case 'created':
          case 'attached':
          case 'detached':
          case 'attributeChanged':
          case 'configure':
          case 'ready':
            break;
          default:
            this.copyOwnProperty(n, b, this);
            break;
        }
      }, this);
    },

    _doBehavior: function(name, args) {
      this.behaviors.forEach(function(b) {
        this._invokeBehavior(b, name, args);
      }, this);
      this._invokeBehavior(this, name, args);
    },

    _invokeBehavior: function(b, name, args) {
      var fn = b[name];
      if (fn) {
        fn.apply(this, args || Polymer.nar);
      }
    },

    _marshalBehaviors: function() {
      this.behaviors.forEach(function(b) {
        this._marshalBehavior(b);
      }, this);
      this._marshalBehavior(this);
    }

  });


;

  /**
   * Support `extends` property (for type-extension only).
   *
   * If the mixin is String-valued, the corresponding Polymer module
   * is mixed in.
   *
   *     Polymer({
   *       is: 'pro-input',
   *       extends: 'input',
   *       ...
   *     });
   * 
   * Type-extension objects are created using `is` notation in HTML, or via
   * the secondary argument to `document.createElement` (the type-extension
   * rules are part of the Custom Elements specification, not something 
   * created by Polymer). 
   * 
   * Example:
   * 
   *     <!-- right: creates a pro-input element -->
   *     <input is="pro-input">
   *   
   *     <!-- wrong: creates an unknown element -->
   *     <pro-input>  
   * 
   *     <script>
   *        // right: creates a pro-input element
   *        var elt = document.createElement('input', 'pro-input');
   * 
   *        // wrong: creates an unknown element
   *        var elt = document.createElement('pro-input');
   *     <\script>
   *
   *   @class base feature: extends
   */

  Polymer.Base._addFeature({

    _prepExtends: function() {
      if (this.extends) {
        this.__proto__ = this.getExtendedPrototype(this.extends);
      }
    },

    getExtendedPrototype: function(tag) {
      return this.getExtendedNativePrototype(tag);
    },

    nativePrototypes: {}, // static

    getExtendedNativePrototype: function(tag) {
      var p = this.nativePrototypes[tag];
      if (!p) {
        var np = this.getNativePrototype(tag);
        p = this.extend(Object.create(np), Polymer.Base);
        this.nativePrototypes[tag] = p;
      }
      return p;
    },

    getNativePrototype: function(tag) {
      // TODO(sjmiles): sad necessity
      return Object.getPrototypeOf(document.createElement(tag));
    }

  });


;

  /**
   * Generates a boilerplate constructor.
   * 
   *     XFoo = Polymer({
   *       is: 'x-foo'
   *     });
   *     ASSERT(new XFoo() instanceof XFoo);
   *  
   * You can supply a custom constructor on the prototype. But remember that 
   * this constructor will only run if invoked **manually**. Elements created
   * via `document.createElement` or from HTML _will not invoke this method_.
   * 
   * Instead, we reuse the concept of `constructor` for a factory method which 
   * can take arguments. 
   * 
   *     MyFoo = Polymer({
   *       is: 'my-foo',
   *       constructor: function(foo) {
   *         this.foo = foo;
   *       }
   *       ...
   *     });
   * 
   * @class base feature: constructor
   */

  Polymer.Base._addFeature({

    // registration-time

    _prepConstructor: function() {
      // support both possible `createElement` signatures
      this._factoryArgs = this.extends ? [this.extends, this.is] : [this.is];
      // thunk the constructor to delegate allocation to `createElement`
      var ctor = function() { 
        return this._factory(arguments); 
      };
      if (this.hasOwnProperty('extends')) {
        ctor.extends = this.extends; 
      }
      // ensure constructor is set. The `constructor` property is
      // not writable on Safari; note: Chrome requires the property
      // to be configurable.
      Object.defineProperty(this, 'constructor', {value: ctor, 
        writable: true, configurable: true});
      ctor.prototype = this;
    },

    _factory: function(args) {
      var elt = document.createElement.apply(document, this._factoryArgs);
      if (this.factoryImpl) {
        this.factoryImpl.apply(elt, args);
      }
      return elt;
    }

  });


;

  /**
   * Define property metadata.
   *
   *     properties: {
   *       <property>: <Type || Object>,
   *       ...
   *     }
   *
   * Example:
   *
   *     properties: {
   *       // `foo` property can be assigned via attribute, will be deserialized to
   *       // the specified data-type. All `properties` properties have this behavior.
   *       foo: String,
   *
   *       // `bar` property has additional behavior specifiers.
   *       //   type: as above, type for (de-)serialization
   *       //   notify: true to send a signal when a value is set to this property
   *       //   reflectToAttribute: true to serialize the property to an attribute
   *       //   readOnly: if true, the property has no setter
   *       bar: {
   *         type: Boolean,
   *         notify: true
   *       }
   *     }
   *
   * By itself the properties feature doesn't do anything but provide property
   * information. Other features use this information to control behavior.
   *
   * The `type` information is used by the `attributes` feature to convert
   * String values in attributes to typed properties. The `bind` feature uses
   * property information to control property access.
   *
   * Marking a property as `notify` causes a change in the property to
   * fire a non-bubbling event called `<property>-changed`. Elements that
   * have enabled two-way binding to the property use this event to
   * observe changes.
   *
   * `readOnly` properties have a getter, but no setter. To set a read-only
   * property, use the private setter method `_set_<property>(value)`.
   *
   * @class base feature: properties
   */

  // null object
  Polymer.nob = Object.create(null);

  Polymer.Base._addFeature({

    properties: {
    },

    getPropertyInfo: function(property) {
      var info = this._getPropertyInfo(property, this.properties);
      if (!info) {
        this.behaviors.some(function(b) {
          return info = this._getPropertyInfo(property, b.properties);
        }, this);
      }
      return info || Polymer.nob;
    },

    _getPropertyInfo: function(property, properties) {
      var p = properties && properties[property];
      if (typeof(p) === 'function') {
        p = properties[property] = {
          type: p
        };
      }
      return p;
    },

    getPropertyType: function(property) {
      return this.getPropertyInfo(property).type;
    },

    isReadOnlyProperty: function(property) {
      return this.getPropertyInfo(property).readOnly;
    },

    isNotifyProperty: function(property) {
      return this.getPropertyInfo(property).notify;
    },

    isReflectedProperty: function(property) {
      return this.getPropertyInfo(property).reflectToAttribute;
    }

  });


;

  Polymer.CaseMap = {

    _caseMap: {},

    dashToCamelCase: function(dash) {
      var mapped = Polymer.CaseMap._caseMap[dash];
      if (mapped) {
        return mapped;
      }
      // TODO(sjmiles): is rejection test actually helping perf?
      if (dash.indexOf('-') < 0) {
        return Polymer.CaseMap._caseMap[dash] = dash;
      }
      return Polymer.CaseMap._caseMap[dash] = dash.replace(/-([a-z])/g, 
        function(m) {
          return m[1].toUpperCase(); 
        }
      );
    },

    camelToDashCase: function(camel) {
      var mapped = Polymer.CaseMap._caseMap[camel];
      if (mapped) {
        return mapped;
      }
      return Polymer.CaseMap._caseMap[camel] = camel.replace(/([a-z][A-Z])/g, 
        function (g) { 
          return g[0] + '-' + g[1].toLowerCase() 
        }
      );
    }

  };


;

  /**
   * Support for `hostAttributes` property.
   *
   *     hostAttributes: 'block vertical layout'
   *
   * `hostAttributes` is a space-delimited string of boolean attribute names to
   * set true on each instance.
   *
   * Support for mapping attributes to properties.
   *
   * Properties that are configured in `properties` with a type are mapped
   * to attributes.
   *
   * A value set in an attribute is deserialized into the specified
   * data-type and stored into the matching property.
   *
   * Example:
   *
   *     properties: {
   *       // values set to index attribute are converted to Number and propagated
   *       // to index property
   *       index: Number,
   *       // values set to label attribute are propagated to index property
   *       label: String
   *     }
   *
   * Types supported for deserialization:
   *
   * - Number
   * - Boolean
   * - String
   * - Object (JSON)
   * - Array (JSON)
   * - Date
   *
   * This feature implements `attributeChanged` to support automatic
   * propagation of attribute values at run-time. If you override
   * `attributeChanged` be sure to call this base class method
   * if you also want the standard behavior.
   *
   * @class base feature: attributes
   */

  Polymer.Base._addFeature({

    _marshalAttributes: function() {
      this._takeAttributes();
    },

    _installHostAttributes: function(attributes) {
      if (attributes) {
        this.applyAttributes(this, attributes);
      }
    },

    applyAttributes: function(node, attr$) {
      for (var n in attr$) {
        this.serializeValueToAttribute(attr$[n], n, this);
      }
    },

    _takeAttributes: function() {
      this._takeAttributesToModel(this);
    },

    _takeAttributesToModel: function(model) {
      for (var i=0, l=this.attributes.length; i<l; i++) {
        var a = this.attributes[i];
        var property = Polymer.CaseMap.dashToCamelCase(a.name);
        var info = this.getPropertyInfo(property);
        if (info || this._propertyEffects[property]) {
          model[property] =
            this.deserialize(a.value, info.type);
        }
      }
    },

    setAttributeToProperty: function(model, attrName) {
      // Don't deserialize back to property if currently reflecting
      if (!this._serializing) {
        var propName = Polymer.CaseMap.dashToCamelCase(attrName);
        if (propName in this.properties) {
          var type = this.getPropertyType(propName);
          var val = this.getAttribute(attrName);
          model[propName] = this.deserialize(val, type);
        }
      }
    },

    _serializing: false,
    reflectPropertyToAttribute: function(name) {
      this._serializing = true;
      this.serializeValueToAttribute(this[name],
        Polymer.CaseMap.camelToDashCase(name));
      this._serializing = false;
    },

    serializeValueToAttribute: function(value, attribute, node) {
      var str = this.serialize(value);
      (node || this)
        [str === undefined ? 'removeAttribute' : 'setAttribute']
          (attribute, str);
    },

    deserialize: function(value, type) {
      switch (type) {
        case Number:
          value = Number(value);
          break;

        case Boolean:
          value = (value !== null);
          break;

        case Object:
          try {
            value = JSON.parse(value);
          } catch(x) {
            // allow non-JSON literals like Strings and Numbers
          }
          break;

        case Array:
          try {
            value = JSON.parse(value);
          } catch(x) {
            value = null;
            console.warn('Polymer::Attributes: couldn`t decode Array as JSON');
          }
          break;

        case Date:
          value = new Date(value);
          break;

        case String:
        default:
          break;
      }
      return value;
    },

    serialize: function(value) {
      switch (typeof value) {
        case 'boolean':
          return value ? '' : undefined;

        case 'object':
          if (value instanceof Date) {
            return value;
          } else if (value) {
            try {
              return JSON.stringify(value);
            } catch(x) {
              return '';
            }
          }

        default:
          return value != null ? value : undefined;
      }
    }

  });


;

  Polymer.Base._addFeature({

    _setupDebouncers: function() {
      this._debouncers = {};
    },

    /**
     * Debounce signals.
     *
     * Call `debounce` to collapse multiple requests for a named task into
     * one invocation which is made after the wait time has elapsed with
     * no new request.
     *
     *     debouncedClickAction: function(e) {
     *       // will not call `processClick` more than once per 100ms
     *       this.debounce('click', function() {
     *        this.processClick;
     *       }, 100);
     *     }
     *
     * @method debounce
     * @param String {String} jobName A string to indentify the debounce job.
     * @param Function {Function} callback A function that is called (with `this` context) when the wait time elapses.
     * @param Number {Number} wait Time in milliseconds (ms) after the last signal that must elapse before invoking `callback`
     * @type Handle
     */
    debounce: function(jobName, callback, wait) {
      this._debouncers[jobName] = Polymer.Debounce.call(this,
        this._debouncers[jobName], callback, wait);
    },

    isDebouncerActive: function(jobName) {
      var debouncer = this._debouncers[jobName];
      return debouncer && debouncer.finish;
    },

    flushDebouncer: function(jobName) {
      var debouncer = this._debouncers[jobName];
      if (debouncer) {
        debouncer.complete();
      }
    }

  });


;

  Polymer.Base._addFeature({

    _registerFeatures: function() {
      // identity
      this._prepIs();
      // shared behaviors
      this._prepBehaviors();
      // inheritance
      this._prepExtends();
      // factory
      this._prepConstructor();
    },

    _prepBehavior: function() {},

    _initFeatures: function() {
      // setup debouncers
      this._setupDebouncers();
      // acquire behaviors
      this._marshalBehaviors();
    },

    _marshalBehavior: function(b) {
      // publish attributes to instance
      this._installHostAttributes(b.hostAttributes);
    }

  });


;

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


;

  /**
   * Provides `ready` lifecycle callback which is called parent to child.
   *
   * This can be useful in a number of cases. Here are some examples:
   *
   * Setting a default property value that should have a side effect: To ensure
   * the side effect, an element must set a default value no sooner than
   * `created`; however, since `created` flows child to host, this is before the
   * host has had a chance to set a property value on the child. The `ready`
   * method solves this problem since it's called host to child.
   *
   * Dom distribution: To support reprojection efficiently, it's important to
   * distribute from host to child in one shot. The `attachedCallback` mostly
   * goes in the desired order except for elements that are in dom to start; in
   * this case, all children are attached before the host element. Ready also
   * addresses this case since it's guaranteed to be called host to child.
   *
   * @class standard feature: ready
   */

(function() {

  var baseAttachedCallback = Polymer.Base.attachedCallback;

  Polymer.Base._addFeature({

    hostStack: [],

    // for overriding
    ready: function() {
    },

    // NOTE: The concept of 'host' is overloaded. There are two different
    // notions:
    // 1. an element hosts the elements in its local dom root.
    // 2. an element hosts the elements on which it configures data.
    // Practially, these notions are almost always coincident.
    // Some special elements like templates may separate them.
    // In order not to over-emphaisize this technical difference, we expose
    // one concept to the user and it maps to the dom-related meaning of host.
    //
    // 1. set this element's `host` and push this element onto the `host`'s
    // list of `client` elements
    // 2. establish this element as the current hosting element (allows
    // any elements we stamp to easily set host to us).
    _pushHost: function(host) {
      // NOTE: The `dataHost` of an element never changes.
      this.dataHost = host = host || 
        Polymer.Base.hostStack[Polymer.Base.hostStack.length-1];
      // this.dataHost reflects the parent element who manages
      // any bindings for the element.  Only elements originally
      // stamped from Polymer templates have a dataHost, and this
      // never changes
      if (host && host._clients) {
        host._clients.push(this);
      }
      this._beginHost();
    },

    _beginHost: function() {
      Polymer.Base.hostStack.push(this);
      if (!this._clients) {
        this._clients = [];
      }
    },

    _popHost: function() {
      // this element is no longer the current hosting element
      Polymer.Base.hostStack.pop();
    },

    _tryReady: function() {
      if (this._canReady()) {
        this._ready();
      }
    },

    _canReady: function() {
      return !this.dataHost || this.dataHost._clientsReadied;
    },

    _ready: function() {
      // extension point
      this._beforeClientsReady();
      this._readyClients();
      // extension point
      this._afterClientsReady();
      this._readySelf();
    },

    _readyClients: function() {
      // prepare root
      this._setupRoot();
      // logically distribute self
      this._beginDistribute();
      // now fully prepare localChildren
      var c$ = this._clients;
      for (var i=0, l= c$.length, c; (i<l) && (c=c$[i]); i++) {
        c._ready();
      }
      // perform actual dom composition
      this._finishDistribute();
      // ensure elements are attached if they are in the dom at ready time
      // helps normalize attached ordering between native and polyfill ce.
      // TODO(sorvell): worth perf cost? ~6%
      // if (!Polymer.Settings.useNativeCustomElements) {
      //   CustomElements.takeRecords();
      // }
      this._clientsReadied = true;
      this._clients = null;
    },

    // mark readied and call `ready`
    // note: called localChildren -> host
    _readySelf: function() {
      this._doBehavior('ready');
      this._readied = true;
      if (this._attachedPending) {
        this._attachedPending = false;
        this.attachedCallback();
      }
    },

    // for system overriding
    _beforeClientsReady: function() {},
    _afterClientsReady: function() {},

    // normalize lifecycle: ensure attached occurs only after ready.
    attachedCallback: function() {
      if (this._readied) {
        baseAttachedCallback.call(this);
      } else {
        this._attachedPending = true;
      }
    }

  });

})();


;

Polymer.ArraySplice = (function() {
  
  function newSplice(index, removed, addedCount) {
    return {
      index: index,
      removed: removed,
      addedCount: addedCount
    };
  }

  var EDIT_LEAVE = 0;
  var EDIT_UPDATE = 1;
  var EDIT_ADD = 2;
  var EDIT_DELETE = 3;

  function ArraySplice() {}

  ArraySplice.prototype = {

    // Note: This function is *based* on the computation of the Levenshtein
    // "edit" distance. The one change is that "updates" are treated as two
    // edits - not one. With Array splices, an update is really a delete
    // followed by an add. By retaining this, we optimize for "keeping" the
    // maximum array items in the original array. For example:
    //
    //   'xxxx123' -> '123yyyy'
    //
    // With 1-edit updates, the shortest path would be just to update all seven
    // characters. With 2-edit updates, we delete 4, leave 3, and add 4. This
    // leaves the substring '123' intact.
    calcEditDistances: function(current, currentStart, currentEnd,
                                old, oldStart, oldEnd) {
      // "Deletion" columns
      var rowCount = oldEnd - oldStart + 1;
      var columnCount = currentEnd - currentStart + 1;
      var distances = new Array(rowCount);

      // "Addition" rows. Initialize null column.
      for (var i = 0; i < rowCount; i++) {
        distances[i] = new Array(columnCount);
        distances[i][0] = i;
      }

      // Initialize null row
      for (var j = 0; j < columnCount; j++)
        distances[0][j] = j;

      for (var i = 1; i < rowCount; i++) {
        for (var j = 1; j < columnCount; j++) {
          if (this.equals(current[currentStart + j - 1], old[oldStart + i - 1]))
            distances[i][j] = distances[i - 1][j - 1];
          else {
            var north = distances[i - 1][j] + 1;
            var west = distances[i][j - 1] + 1;
            distances[i][j] = north < west ? north : west;
          }
        }
      }

      return distances;
    },

    // This starts at the final weight, and walks "backward" by finding
    // the minimum previous weight recursively until the origin of the weight
    // matrix.
    spliceOperationsFromEditDistances: function(distances) {
      var i = distances.length - 1;
      var j = distances[0].length - 1;
      var current = distances[i][j];
      var edits = [];
      while (i > 0 || j > 0) {
        if (i == 0) {
          edits.push(EDIT_ADD);
          j--;
          continue;
        }
        if (j == 0) {
          edits.push(EDIT_DELETE);
          i--;
          continue;
        }
        var northWest = distances[i - 1][j - 1];
        var west = distances[i - 1][j];
        var north = distances[i][j - 1];

        var min;
        if (west < north)
          min = west < northWest ? west : northWest;
        else
          min = north < northWest ? north : northWest;

        if (min == northWest) {
          if (northWest == current) {
            edits.push(EDIT_LEAVE);
          } else {
            edits.push(EDIT_UPDATE);
            current = northWest;
          }
          i--;
          j--;
        } else if (min == west) {
          edits.push(EDIT_DELETE);
          i--;
          current = west;
        } else {
          edits.push(EDIT_ADD);
          j--;
          current = north;
        }
      }

      edits.reverse();
      return edits;
    },

    /**
     * Splice Projection functions:
     *
     * A splice map is a representation of how a previous array of items
     * was transformed into a new array of items. Conceptually it is a list of
     * tuples of
     *
     *   <index, removed, addedCount>
     *
     * which are kept in ascending index order of. The tuple represents that at
     * the |index|, |removed| sequence of items were removed, and counting forward
     * from |index|, |addedCount| items were added.
     */

    /**
     * Lacking individual splice mutation information, the minimal set of
     * splices can be synthesized given the previous state and final state of an
     * array. The basic approach is to calculate the edit distance matrix and
     * choose the shortest path through it.
     *
     * Complexity: O(l * p)
     *   l: The length of the current array
     *   p: The length of the old array
     */
    calcSplices: function(current, currentStart, currentEnd,
                          old, oldStart, oldEnd) {
      var prefixCount = 0;
      var suffixCount = 0;

      var minLength = Math.min(currentEnd - currentStart, oldEnd - oldStart);
      if (currentStart == 0 && oldStart == 0)
        prefixCount = this.sharedPrefix(current, old, minLength);

      if (currentEnd == current.length && oldEnd == old.length)
        suffixCount = this.sharedSuffix(current, old, minLength - prefixCount);

      currentStart += prefixCount;
      oldStart += prefixCount;
      currentEnd -= suffixCount;
      oldEnd -= suffixCount;

      if (currentEnd - currentStart == 0 && oldEnd - oldStart == 0)
        return [];

      if (currentStart == currentEnd) {
        var splice = newSplice(currentStart, [], 0);
        while (oldStart < oldEnd)
          splice.removed.push(old[oldStart++]);

        return [ splice ];
      } else if (oldStart == oldEnd)
        return [ newSplice(currentStart, [], currentEnd - currentStart) ];

      var ops = this.spliceOperationsFromEditDistances(
          this.calcEditDistances(current, currentStart, currentEnd,
                                 old, oldStart, oldEnd));

      var splice = undefined;
      var splices = [];
      var index = currentStart;
      var oldIndex = oldStart;
      for (var i = 0; i < ops.length; i++) {
        switch(ops[i]) {
          case EDIT_LEAVE:
            if (splice) {
              splices.push(splice);
              splice = undefined;
            }

            index++;
            oldIndex++;
            break;
          case EDIT_UPDATE:
            if (!splice)
              splice = newSplice(index, [], 0);

            splice.addedCount++;
            index++;

            splice.removed.push(old[oldIndex]);
            oldIndex++;
            break;
          case EDIT_ADD:
            if (!splice)
              splice = newSplice(index, [], 0);

            splice.addedCount++;
            index++;
            break;
          case EDIT_DELETE:
            if (!splice)
              splice = newSplice(index, [], 0);

            splice.removed.push(old[oldIndex]);
            oldIndex++;
            break;
        }
      }

      if (splice) {
        splices.push(splice);
      }
      return splices;
    },

    sharedPrefix: function(current, old, searchLength) {
      for (var i = 0; i < searchLength; i++)
        if (!this.equals(current[i], old[i]))
          return i;
      return searchLength;
    },

    sharedSuffix: function(current, old, searchLength) {
      var index1 = current.length;
      var index2 = old.length;
      var count = 0;
      while (count < searchLength && this.equals(current[--index1], old[--index2]))
        count++;

      return count;
    },

    calculateSplices: function(current, previous) {
      return this.calcSplices(current, 0, current.length, previous, 0,
                              previous.length);
    },

    equals: function(currentValue, previousValue) {
      return currentValue === previousValue;
    }
  };

  return new ArraySplice();

})();

;

  Polymer.EventApi = (function() {

    var Settings = Polymer.Settings;

    var EventApi = function(event) {
      this.event = event;
    };

    if (Settings.useShadow) {

      EventApi.prototype = {
        
        get rootTarget() {
          return this.event.path[0];
        },

        get localTarget() {
          return this.event.target;
        },

        get path() {
          return this.event.path;
        }

      };

    } else {

      EventApi.prototype = {
      
        get rootTarget() {
          return this.event.target;
        },

        get localTarget() {
          var current = this.event.currentTarget;
          var currentRoot = current && Polymer.dom(current).getOwnerRoot();
          var p$ = this.path;
          for (var i=0; i < p$.length; i++) {
            if (Polymer.dom(p$[i]).getOwnerRoot() === currentRoot) {
              return p$[i];
            }
          }
        },

        // TODO(sorvell): simulate event.path. This probably incorrect for
        // non-bubbling events.
        get path() {
          if (!this.event._path) {
            var path = [];
            var o = this.rootTarget;
            while (o) {
              path.push(o);
              o = Polymer.dom(o).parentNode || o.host;
            }
            // event path includes window in most recent native implementations
            path.push(window);
            this.event._path = path;
          }
          return this.event._path;
        }

      };

    }

    var factory = function(event) {
      if (!event.__eventApi) {
        event.__eventApi = new EventApi(event);
      }
      return event.__eventApi;
    };

    return {
      factory: factory
    };

  })();


;

  Polymer.DomApi = (function() {

    var Debounce = Polymer.Debounce;
    var Settings = Polymer.Settings;

    var nativeInsertBefore = Element.prototype.insertBefore;
    var nativeRemoveChild = Element.prototype.removeChild;
    var nativeAppendChild = Element.prototype.appendChild;

    var dirtyRoots = [];

    var DomApi = function(node, patch) {
      this.node = node;
      if (patch) {
        this.patch();
      }
    };

    DomApi.prototype = {

      // experimental: support patching selected native api.
      patch: function() {
        var self = this;
        this.node.appendChild = function(node) {
          return self.appendChild(node);
        };
        this.node.insertBefore = function(node, ref_node) {
          return self.insertBefore(node, ref_node);
        };
        this.node.removeChild = function(node) {
          return self.removeChild(node);
        };
      },

      get childNodes() {
        var c$ = getLightChildren(this.node);
        return Array.isArray(c$) ? c$ : Array.prototype.slice.call(c$);
      },

      get children() {
        return Array.prototype.filter.call(this.childNodes, function(n) {
          return (n.nodeType === Node.ELEMENT_NODE);
        });
      },

      get parentNode() {
        return this.node.lightParent || this.node.parentNode;
      },

      flush: function() {
        for (var i=0, host; i<dirtyRoots.length; i++) {
          host = dirtyRoots[i];
          host.flushDebouncer('_distribute');
        }
        dirtyRoots = [];
      },

      _lazyDistribute: function(host) {
        if (host.shadyRoot) {
          host.shadyRoot._distributionClean = false;
        }
        // TODO(sorvell): optimize debounce so it does less work by default
        // and then remove these checks...
        // need to dirty distribution once.
        if (!host.isDebouncerActive('_distribute')) {
          host.debounce('_distribute', host._distributeContent);
          dirtyRoots.push(host);
        }
      },

      // cases in which we may not be able to just do standard appendChild
      // 1. container has a shadyRoot (needsDistribution IFF the shadyRoot
      // has an insertion point)
      // 2. container is a shadyRoot (don't distribute, instead set
      // container to container.host.
      // 3. node is <content> (host of container needs distribution)
      appendChild: function(node) {
        var distributed;
        this._removeNodeFromHost(node);
        if (this._nodeIsInLogicalTree(this.node)) {
          var host = this._hostForNode(this.node);
          this._addLogicalInfo(node, this.node, host && host.shadyRoot);
          this._addNodeToHost(node);
          if (host) {
            distributed = this._maybeDistribute(node, this.node, host);
          }
        }
        if (!distributed) {
          // if adding to a shadyRoot, add to host instead
          var container = this.node._isShadyRoot ? this.node.host : this.node;
          nativeAppendChild.call(container, node);
        }
        return node;
      },

      insertBefore: function(node, ref_node) {
        if (!ref_node) {
          return this.appendChild(node);
        }
        var distributed;
        this._removeNodeFromHost(node);
        if (this._nodeIsInLogicalTree(this.node)) {
          saveLightChildrenIfNeeded(this.node);
          var children = this.childNodes;
          var index = children.indexOf(ref_node);
          if (index < 0) {
            throw Error('The ref_node to be inserted before is not a child ' +
              'of this node');
          }
          var host = this._hostForNode(this.node);
          this._addLogicalInfo(node, this.node, host && host.shadyRoot, index);
          this._addNodeToHost(node);
          if (host) {
            distributed = this._maybeDistribute(node, this.node, host);
          }
        }
        if (!distributed) {
          // if ref_node is <content> replace with first distributed node
          ref_node = ref_node.localName === CONTENT ?
            this._firstComposedNode(ref_node) : ref_node;
          // if adding to a shadyRoot, add to host instead
          var container = this.node._isShadyRoot ? this.node.host : this.node;
          nativeInsertBefore.call(container, node, ref_node);
        }
        return node;
      },

      /**
        Removes the given `node` from the element's `lightChildren`.
        This method also performs dom composition.
      */
      removeChild: function(node) {
        var distributed;
        if (this._nodeIsInLogicalTree(this.node)) {
          var host = this._hostForNode(this.node);
          distributed = this._maybeDistribute(node, this.node, host);
          this._removeNodeFromHost(node);
        }
        if (!distributed) {
          // if removing from a shadyRoot, remove form host instead
          var container = this.node._isShadyRoot ? this.node.host : this.node;
          nativeRemoveChild.call(container, node);
        }
        return node;
      },

      replaceChild: function(node, ref_node) {
        this.insertBefore(node, ref_node);
        this.removeChild(ref_node);
        return node;
      },

      getOwnerRoot: function() {
        return this._ownerShadyRootForNode(this.node);
      },

      _ownerShadyRootForNode: function(node) {
        if (node._ownerShadyRoot === undefined) {
          var root;
          if (node._isShadyRoot) {
            root = node;
          } else {
            var parent = Polymer.dom(node).parentNode;
            if (parent) {
              root = parent._isShadyRoot ? parent :
                this._ownerShadyRootForNode(parent);
            } else {
             root = null;
            }
          }
          node._ownerShadyRoot = root;
        }
        return node._ownerShadyRoot;

      },

      _maybeDistribute: function(node, parent, host) {
        var nodeNeedsDistribute = this._nodeNeedsDistribution(node);
        var distribute = this._parentNeedsDistribution(parent) ||
          nodeNeedsDistribute;
        if (nodeNeedsDistribute) {
          this._updateInsertionPoints(host);
        }
        if (distribute) {
          this._lazyDistribute(host);
        }
        return distribute;
      },

      _updateInsertionPoints: function(host) {
        host.shadyRoot._insertionPoints =
          factory(host.shadyRoot).querySelectorAll(CONTENT);
      },

      _nodeIsInLogicalTree: function(node) {
        return Boolean(node._isShadyRoot ||
          this._ownerShadyRootForNode(node) ||
          node.shadyRoot);
      },

      // note: a node is its own host
      _hostForNode: function(node) {
        var root = node.shadyRoot || (node._isShadyRoot ?
          node : this._ownerShadyRootForNode(node));
        return root && root.host;
      },

      _parentNeedsDistribution: function(parent) {
        return parent.shadyRoot && hasInsertionPoint(parent.shadyRoot);
      },

      // TODO(sorvell): technically we should check non-fragment nodes for
      // <content> children but since this case is assumed to be exceedingly
      // rare, we avoid the cost and will address with some specific api
      // when the need arises.
      _nodeNeedsDistribution: function(node) {
        return (node.localName === CONTENT) ||
          ((node.nodeType === Node.DOCUMENT_FRAGMENT_NODE) &&
            node.querySelector(CONTENT));
      },

      _removeNodeFromHost: function(node) {
        if (node.lightParent) {
          var root = this._ownerShadyRootForNode(node);
          if (root) {
            root.host._elementRemove(node);
          }
          this._removeLogicalInfo(node, node.lightParent);
        }
        this._removeOwnerShadyRoot(node);
      },

      _addNodeToHost: function(node) {
        var checkNode = node.nodeType === Node.DOCUMENT_FRAGMENT_NODE ?
          node.firstChild : node;
        var root = this._ownerShadyRootForNode(checkNode);
        if (root) {
          root.host._elementAdd(node);
        }
      },

      _addLogicalInfo: function(node, container, root, index) {
        saveLightChildrenIfNeeded(container);
        var children = factory(container).childNodes;
        index = index === undefined ? children.length : index;
        // handle document fragments
        if (node.nodeType === Node.DOCUMENT_FRAGMENT_NODE) {
          var n = node.firstChild;
          while (n) {
            children.splice(index++, 0, n);
            n.lightParent = container;
            n = n.nextSibling;
          }
        } else {
          children.splice(index, 0, node);
          node.lightParent = container;
        }
      },

      // NOTE: in general, we expect contents of the lists here to be small-ish
      // and therefore indexOf to be nbd. Other optimizations can be made
      // for larger lists (linked list)
      _removeLogicalInfo: function(node, container) {
        var children = factory(container).childNodes;
        var index = children.indexOf(node);
        if ((index < 0) || (container !== node.lightParent)) {
          throw Error('The node to be removed is not a child of this node');
        }
        children.splice(index, 1);
        node.lightParent = null;
      },

      _removeOwnerShadyRoot: function(node) {
        // TODO(sorvell): need to clear any children of element?
        node._ownerShadyRoot = undefined;
      },

      // TODO(sorvell): This will fail if distribution that affects this
      // question is pending; this is expected to be exceedingly rare, but if
      // the issue comes up, we can force a flush in this case.
      _firstComposedNode: function(content) {
        var n$ = factory(content).getDistributedNodes();
        for (var i=0, l=n$.length, n, p$; (i<l) && (n=n$[i]); i++) {
          p$ = factory(n).getDestinationInsertionPoints();
          // means that we're composed to this spot.
          if (p$[p$.length-1] === content) {
            return n;
          }
        }
      },

      // TODO(sorvell): consider doing native QSA and filtering results.
      querySelector: function(selector) {
        return this.querySelectorAll(selector)[0];
      },

      querySelectorAll: function(selector) {
        return this._query(function(n) {
          return matchesSelector.call(n, selector);
        }, this.node);
      },

      _query: function(matcher, node) {
        var list = [];
        this._queryElements(factory(node).childNodes, matcher, list);
        return list;
      },

      _queryElements: function(elements, matcher, list) {
        for (var i=0, l=elements.length, c; (i<l) && (c=elements[i]); i++) {
          if (c.nodeType === Node.ELEMENT_NODE) {
            this._queryElement(c, matcher, list);
          }
        }
      },

      _queryElement: function(node, matcher, list) {
        if (matcher(node)) {
          list.push(node);
        }
        this._queryElements(factory(node).childNodes, matcher, list);
      },

      getDestinationInsertionPoints: function() {
        return this.node._destinationInsertionPoints || [];
      },

      getDistributedNodes: function() {
        return this.node._distributedNodes || [];
      },

      /*
        Returns a list of nodes distributed within this element. These can be
        dom children or elements distributed to children that are insertion
        points.
      */
      queryDistributedElements: function(selector) {
        var c$ = this.childNodes;
        var list = [];
        this._distributedFilter(selector, c$, list);
        for (var i=0, l=c$.length, c; (i<l) && (c=c$[i]); i++) {
          if (c.localName === CONTENT) {
            this._distributedFilter(selector, factory(c).getDistributedNodes(),
              list);
          }
        }
        return list;
      },

      _distributedFilter: function(selector, list, results) {
        results = results || [];
        for (var i=0, l=list.length, d; (i<l) && (d=list[i]); i++) {
          if ((d.nodeType === Node.ELEMENT_NODE) &&
            (d.localName !== CONTENT) &&
            matchesSelector.call(d, selector)) {
            results.push(d);
          }
        }
        return results;
      }

    };

    if (Settings.useShadow) {

      DomApi.prototype.querySelectorAll = function(selector) {
        return Array.prototype.slice.call(this.node.querySelectorAll(selector));
      };

      DomApi.prototype.patch = function() {};

      DomApi.prototype.getOwnerRoot = function() {
        var n = this.node;
        while (n) {
          if (n.nodeType === Node.DOCUMENT_FRAGMENT_NODE && n.host) {
            return n;
          }
          n = n.parentNode;
        }
      };

      DomApi.prototype.getDestinationInsertionPoints = function() {
        var n$ = this.node.getDestinationInsertionPoints();
        return n$ ? Array.prototype.slice.call(n$) : [];
      };

      DomApi.prototype.getDistributedNodes = function() {
        var n$ = this.node.getDistributedNodes();
        return n$ ? Array.prototype.slice.call(n$) : [];
      };


    }

    var CONTENT = 'content';

    var factory = function(node, patch) {
      node = node || document;
      if (!node.__domApi) {
        node.__domApi = new DomApi(node, patch);
      }
      return node.__domApi;
    };

    Polymer.dom = function(obj, patch) {
      if (obj instanceof Event) {
        return Polymer.EventApi.factory(obj);
      } else {
        return factory(obj, patch);
      }
    };

    // make flush available directly.
    Polymer.dom.flush = DomApi.prototype.flush;

    function getLightChildren(node) {
      var children = node.lightChildren;
      return children ? children : node.childNodes;
    }

    function saveLightChildrenIfNeeded(node) {
      // Capture the list of light children. It's important to do this before we
      // start transforming the DOM into "rendered" state.
      //
      // Children may be added to this list dynamically. It will be treated as the
      // source of truth for the light children of the element. This element's
      // actual children will be treated as the rendered state once lightChildren
      // is populated.
      if (!node.lightChildren) {
        var children = [];
        for (var child = node.firstChild; child; child = child.nextSibling) {
          children.push(child);
          child.lightParent = child.lightParent || node;
        }
        node.lightChildren = children;
      }
    }

    function hasInsertionPoint(root) {
      return Boolean(root._insertionPoints.length);
    }

    var p = Element.prototype;
    var matchesSelector = p.matches || p.matchesSelector ||
        p.mozMatchesSelector || p.msMatchesSelector ||
        p.oMatchesSelector || p.webkitMatchesSelector;

    return {
      getLightChildren: getLightChildren,
      saveLightChildrenIfNeeded: saveLightChildrenIfNeeded,
      matchesSelector: matchesSelector,
      hasInsertionPoint: hasInsertionPoint,
      factory: factory
    };

  })();


;

  (function() {
    /**

      Implements a pared down version of ShadowDOM's scoping, which is easy to
      polyfill across browsers.

    */
    Polymer.Base._addFeature({

      _prepShady: function() {
        // Use this system iff localDom is needed.
        this._useContent = this._useContent || Boolean(this._template);
        if (this._useContent) {
          this._template._hasInsertionPoint =
            this._template.content.querySelector('content');
        }
      },

      // called as part of content initialization, prior to template stamping
      _poolContent: function() {
        if (this._useContent) {
          // capture lightChildren to help reify dom scoping
          saveLightChildrenIfNeeded(this);
        }
      },

      // called as part of content initialization, after template stamping
      _setupRoot: function() {
        if (this._useContent) {
          this._createLocalRoot();
        }
      },

      _createLocalRoot: function() {
        this.shadyRoot = this.root;
        this.shadyRoot._distributionClean = false;
        this.shadyRoot._isShadyRoot = true;
        this.shadyRoot._dirtyRoots = [];
        // capture insertion point list
        // TODO(sorvell): it's faster to do this via native qSA than annotator.
        this.shadyRoot._insertionPoints = this._template._hasInsertionPoint ?
          this.shadyRoot.querySelectorAll('content') : [];
        // save logical tree info for shadyRoot.
        saveLightChildrenIfNeeded(this.shadyRoot);
        this.shadyRoot.host = this;
      },

      /**
       * Return the element whose local dom within which this element 
       * is contained. This is a shorthand for 
       * `Polymer.dom(this).getOwnerRoot().host`.
       */
      get domHost() {
        var root = Polymer.dom(this).getOwnerRoot();
        return root && root.host;
      },

      /**
       * Force this element to distribute its children to its local dom.
       * A user should call `distributeContent` if distribution has been
       * invalidated due to changes to selectors on child elements that
       * effect distribution. For example, if an element contains an
       * insertion point with <content select=".foo"> and a `foo` class is
       * added to a child, then `distributeContent` must be called to update
       * local dom distribution.
       */
      distributeContent: function() {
        if (this._useContent) {
          this.shadyRoot._distributionClean = false;
          this._distributeContent();
        }
      },

      _distributeContent: function() {
        if (this._useContent && !this.shadyRoot._distributionClean) {
          // logically distribute self
          this._beginDistribute();
          this._distributeDirtyRoots();
          this._finishDistribute();
        }
      },

      _beginDistribute: function() {
        if (this._useContent && hasInsertionPoint(this.shadyRoot)) {
          // reset distributions
          this._resetDistribution();
          // compute which nodes should be distributed where
          // TODO(jmesserly): this is simplified because we assume a single
          // ShadowRoot per host and no `<shadow>`.
          this._distributePool(this.shadyRoot, this._collectPool());
        }
      },

      _distributeDirtyRoots: function() {
        var c$ = this.shadyRoot._dirtyRoots;
        for (var i=0, l= c$.length, c; (i<l) && (c=c$[i]); i++) {
          c._distributeContent();
        }
        this.shadyRoot._dirtyRoots = [];
      },

      _finishDistribute: function() {
        // compose self
        if (this._useContent) {
          if (hasInsertionPoint(this.shadyRoot)) {
            this._composeTree();
          } else {
            if (!this.shadyRoot._hasDistributed) {
              this.textContent = '';
              this.appendChild(this.shadyRoot);
            } else {
              // simplified non-tree walk composition
              var children = this._composeNode(this);
              this._updateChildNodes(this, children);
            }
          }
          this.shadyRoot._hasDistributed = true;
          this.shadyRoot._distributionClean = true;
        }
      },

      // This is a polyfill for Element.prototype.matches, which is sometimes
      // still prefixed. Alternatively we could just polyfill it somewhere.
      // Note that the arguments are reversed from what you might expect.
      elementMatches: function(selector, node) {
        if (node === undefined) {
          node = this;
        }
        return matchesSelector.call(node, selector);
      },

      // Many of the following methods are all conceptually static, but they are
      // included here as "protected" methods to allow overriding.

      _resetDistribution: function() {
        // light children
        var children = getLightChildren(this);
        for (var i = 0; i < children.length; i++) {
          var child = children[i];
          if (child._destinationInsertionPoints) {
            child._destinationInsertionPoints = undefined;
          }
        }
        // insertion points
        var root = this.shadyRoot;
        var p$ = root._insertionPoints;
        for (var j = 0; j < p$.length; j++) {
          p$[j]._distributedNodes = [];
        }
      },

      // Gather the pool of nodes that should be distributed. We will combine
      // these with the "content root" to arrive at the composed tree.
      _collectPool: function() {
        var pool = [];
        var children = getLightChildren(this);
        for (var i = 0; i < children.length; i++) {
          var child = children[i];
          if (isInsertionPoint(child)) {
            pool.push.apply(pool, child._distributedNodes);
          } else {
            pool.push(child);
          }
        }
        return pool;
      },

      // perform "logical" distribution; note, no actual dom is moved here,
      // instead elements are distributed into a `content._distributedNodes`
      // array where applicable.
      _distributePool: function(node, pool) {
        var p$ = node._insertionPoints;
        for (var i=0, l=p$.length, p; (i<l) && (p=p$[i]); i++) {
          this._distributeInsertionPoint(p, pool);
        }
      },

      _distributeInsertionPoint: function(content, pool) {
        // distribute nodes from the pool that this selector matches
        var anyDistributed = false;
        for (var i=0, l=pool.length, node; i < l; i++) {
          node=pool[i];
          // skip nodes that were already used
          if (!node) {
            continue;
          }
          // distribute this node if it matches
          if (this._matchesContentSelect(node, content)) {
            distributeNodeInto(node, content);
            // remove this node from the pool
            pool[i] = undefined;
            // since at least one node matched, we won't need fallback content
            anyDistributed = true;
            var parent = content.lightParent;
            // dirty a shadyRoot if a change may trigger reprojection!
            if (parent && parent.shadyRoot &&
              hasInsertionPoint(parent.shadyRoot)) {
              parent.shadyRoot._distributionClean = false;
              this.shadyRoot._dirtyRoots.push(parent);
            }
          }
        }
        // Fallback content if nothing was distributed here
        if (!anyDistributed) {
          var children = getLightChildren(content);
          for (var j = 0; j < children.length; j++) {
            distributeNodeInto(children[j], content);
          }
        }
      },

      // Reify dom such that it is at its correct rendering position
      // based on logical distribution.
      _composeTree: function() {
        this._updateChildNodes(this, this._composeNode(this));
        var p$ = this.shadyRoot._insertionPoints;
        for (var i=0, l=p$.length, p, parent; (i<l) && (p=p$[i]); i++) {
          parent = p.lightParent || p.parentNode;
          if (!parent._useContent && (parent !== this) &&
            (parent !== this.shadyRoot)) {
            this._updateChildNodes(parent, this._composeNode(parent));
          }
        }
      },

      // Returns the list of nodes which should be rendered inside `node`.
      _composeNode: function(node) {
        var children = [];
        var c$ = getLightChildren(node.shadyRoot || node);
        for (var i = 0; i < c$.length; i++) {
          var child = c$[i];
          if (isInsertionPoint(child)) {
            var distributedNodes = child._distributedNodes;
            for (var j = 0; j < distributedNodes.length; j++) {
              var distributedNode = distributedNodes[j];
              if (isFinalDestination(child, distributedNode)) {
                children.push(distributedNode);
              }
            }
          } else {
            children.push(child);
          }
        }
        return children;
      },

      // Ensures that the rendered node list inside `node` is `children`.
      _updateChildNodes: function(node, children) {
        var splices =
          Polymer.ArraySplice.calculateSplices(children, node.childNodes);
        for (var i=0; i<splices.length; i++) {
          var s = splices[i];
          // remove
          for (var j=0, c; j < s.removed.length; j++) {
            c = s.removed[j];
            if (c.previousSibling == children[s.index-1]) {
              remove(c);
            }
          }
          // insert
          for (var idx=s.index, ch, o; idx < s.index + s.addedCount; idx++) {
            ch = children[idx];
            o = node.childNodes[idx];
            while (o && o === ch) {
              o = o.nextSibling;
            }
            insertBefore(node, ch, o);
          }
        }
      },

      _matchesContentSelect: function(node, contentElement) {
        var select = contentElement.getAttribute('select');
        // no selector matches all nodes (including text)
        if (!select) {
          return true;
        }
        select = select.trim();
        // same thing if it had only whitespace
        if (!select) {
          return true;
        }
        // selectors can only match Elements
        if (!(node instanceof Element)) {
          return false;
        }
        // only valid selectors can match:
        //   TypeSelector
        //   *
        //   ClassSelector
        //   IDSelector
        //   AttributeSelector
        //   negation
        var validSelectors = /^(:not\()?[*.#[a-zA-Z_|]/;
        if (!validSelectors.test(select)) {
          return false;
        }
        return this.elementMatches(select, node);
      },

      // system override point
      _elementAdd: function() {},

      // system override point
      _elementRemove: function() {}

    });

    var saveLightChildrenIfNeeded = Polymer.DomApi.saveLightChildrenIfNeeded;
    var getLightChildren = Polymer.DomApi.getLightChildren;
    var matchesSelector = Polymer.DomApi.matchesSelector;
    var hasInsertionPoint = Polymer.DomApi.hasInsertionPoint;

    function distributeNodeInto(child, insertionPoint) {
      insertionPoint._distributedNodes.push(child);
      var points = child._destinationInsertionPoints;
      if (!points) {
        child._destinationInsertionPoints = [insertionPoint];
      // TODO(sorvell): _destinationInsertionPoints may not be cleared when
      // nodes are dynamically added/removed, therefore test before adding
      // insertion points.
      } else if (points.indexOf(insertionPoint) < 0) {
        points.push(insertionPoint);
      }
    }

    function isFinalDestination(insertionPoint, node) {
      var points = node._destinationInsertionPoints;
      return points && points[points.length - 1] === insertionPoint;
    }

    function isInsertionPoint(node) {
      // TODO(jmesserly): we could add back 'shadow' support here.
      return node.localName == 'content';
    }

    var nativeInsertBefore = Element.prototype.insertBefore;
    var nativeRemoveChild = Element.prototype.removeChild;

    function insertBefore(parentNode, newChild, refChild) {
      // remove child from its old parent first
      remove(newChild);
      // make sure we never lose logical DOM information:
      // if the parentNode doesn't have lightChildren, save that information now.
      saveLightChildrenIfNeeded(parentNode);
      // insert it into the real DOM
      nativeInsertBefore.call(parentNode, newChild, refChild || null);
    }

    function remove(node) {
      var parentNode = node.parentNode;
      if (parentNode) {
        // make sure we never lose logical DOM information:
        // if the parentNode doesn't have lightChildren, save that information now.
        saveLightChildrenIfNeeded(parentNode);
        // remove it from the real DOM
        nativeRemoveChild.call(parentNode, node);
      }
    }

  })();


;
  
  /**
    Implements `shadyRoot` compatible dom scoping using native ShadowDOM.
  */

  // Transform styles if not using ShadowDOM or if flag is set.

  if (Polymer.Settings.useShadow) {

    Polymer.Base._addFeature({

      // no-op's when ShadowDOM is in use
      _poolContent: function() {},
      _beginDistribute: function() {},
      distributeContent: function() {},
      _distributeContent: function() {},
      _finishDistribute: function() {},
      
      // create a shadowRoot
      _createLocalRoot: function() {
        this.createShadowRoot();
        this.shadowRoot.appendChild(this.root);
        this.root = this.shadowRoot;
      }

    });

  }


;

  Polymer.DomModule = document.createElement('dom-module');

  Polymer.Base._addFeature({

    _registerFeatures: function() {
      // identity
      this._prepIs();
      // shared behaviors
      this._prepBehaviors();
      // inheritance
      this._prepExtends();
     // factory
      this._prepConstructor();
      // template
      this._prepTemplate();
      // dom encapsulation
      this._prepShady();
    },

    _prepBehavior: function() {},

    _initFeatures: function() {
      // manage local dom
      this._poolContent();
      // host stack
      this._pushHost();
      // instantiate template
      this._stampTemplate();
      // host stack
      this._popHost();
      // setup debouncers
      this._setupDebouncers();
      // instance shared behaviors
      this._marshalBehaviors();
      // top-down initial distribution, configuration, & ready callback
      this._tryReady();
    },

    _marshalBehavior: function(b) {
      // publish attributes to instance
      this._installHostAttributes(b.hostAttributes);
    }

  });


;
(function(scope) {

  function withDependencies(task, depends) {
    depends = depends || [];
    if (!depends.map) {
      depends = [depends];
    }
    return task.apply(this, depends.map(marshal));
  }

  function module(name, dependsOrFactory, moduleFactory) {
    var module = null;
    switch (arguments.length) {
      case 0:
        return;
      case 2:
        // dependsOrFactory is `factory` in this case
        module = dependsOrFactory.apply(this);
        break;
      default:
        // dependsOrFactory is `depends` in this case
        module = withDependencies(moduleFactory, dependsOrFactory);
        break;
    }
    modules[name] = module;
  };

  function marshal(name) {
    return modules[name];
  }

  var modules = {};

  var using = function(depends, task) {
    withDependencies(task, depends);
  };

  // exports

  scope.marshal = marshal;
  // `module` confuses commonjs detectors
  scope.modulate = module;
  scope.using = using;

})(this);

;
/**
 * Scans a template to produce an annotation list that that associates
 * metadata culled from markup with tree locations 
 * metadata and information to associate the metadata with nodes in an instance.
 *
 * Supported expressions include:
 *
 * Double-mustache annotations in text content. The annotation must be the only
 * content in the tag, compound expressions are not supported.
 *
 *     <[tag]>{{annotation}}<[tag]>
 *
 * Double-escaped annotations in an attribute, either {{}} or [[]].
 *
 *     <[tag] someAttribute="{{annotation}}" another="[[annotation]]"><[tag]>
 *
 * `on-` style event declarations.
 *
 *     <[tag] on-<event-name>="annotation"><[tag]>
 *
 * Note that the `annotations` feature does not implement any behaviors
 * associated with these expressions, it only captures the data.
 *
 * Generated data-structure:
 * 
 *     [
 *       {
 *         id: '<id>',
 *         events: [
 *           {
 *             name: '<name>'
 *             value: '<annotation>'
 *           }, ...
 *         ],
 *         bindings: [
 *           {
 *             kind: ['text'|'attribute'],
 *             mode: ['{'|'['],
 *             name: '<name>'
 *             value: '<annotation>'
 *           }, ...
 *         ],
 *         // TODO(sjmiles): this is annotation-parent, not node-parent
 *         parent: <reference to parent annotation object>,
 *         index: <integer index in parent's childNodes collection>
 *       },
 *       ...
 *     ]
 * 
 * @class Template feature
 */

  // null-array (shared empty array to avoid null-checks)
  Polymer.nar = [];

  Polymer.Annotations = {

    // preprocess-time

    // construct and return a list of annotation records
    // by scanning `template`'s content
    //
    parseAnnotations: function(template) {
      var list = [];
      var content = template._content || template.content; 
      this._parseNodeAnnotations(content, list);
      return list;
    },

    // add annotations gleaned from subtree at `node` to `list`
    _parseNodeAnnotations: function(node, list) {
      return node.nodeType === Node.TEXT_NODE ?
        this._parseTextNodeAnnotation(node, list) :
          // TODO(sjmiles): are there other nodes we may encounter
          // that are not TEXT_NODE but also not ELEMENT?
          this._parseElementAnnotations(node, list);
    },

    // add annotations gleaned from TextNode `node` to `list`
    _parseTextNodeAnnotation: function(node, list) {
      var v = node.textContent, escape = v.slice(0, 2);
      if (escape === '{{' || escape === '[[') {
        // NOTE: use a space here so the textNode remains; some browsers
        // (IE) evacipate an empty textNode.
        node.textContent = ' ';
        var annote = {
          bindings: [{
            kind: 'text',
            mode: escape[0],
            value: v.slice(2, -2)
          }]
        };
        list.push(annote);
        return annote;
      }
    },

    // add annotations gleaned from Element `node` to `list`
    _parseElementAnnotations: function(element, list) {
      var annote = {
        bindings: [],
        events: []
      };
      this._parseChildNodesAnnotations(element, annote, list);
      // TODO(sjmiles): is this for non-ELEMENT nodes? If so, we should
      // change the contract of this method, or filter these out above.
      if (element.attributes) {
        this._parseNodeAttributeAnnotations(element, annote, list);
        // TODO(sorvell): ad hoc callback for doing work on elements while
        // leveraging annotator's tree walk.
        // Consider adding an node callback registry and moving specific 
        // processing out of this module.
        if (this.prepElement) {
          this.prepElement(element);
        }
      }
      if (annote.bindings.length || annote.events.length || annote.id) {
        list.push(annote);
      }
      return annote;
    },

    // add annotations gleaned from children of `root` to `list`, `root`'s
    // `annote` is supplied as it is the annote.parent of added annotations 
    _parseChildNodesAnnotations: function(root, annote, list, callback) {
      if (root.firstChild) {
        for (var i=0, node=root.firstChild; node; node=node.nextSibling, i++){
          if (node.localName === 'template' &&
            !node.hasAttribute('preserve-content')) {
            this._parseTemplate(node, i, list, annote);
          }
          //
          var childAnnotation = this._parseNodeAnnotations(node, list, callback);
          if (childAnnotation) {
            childAnnotation.parent = annote;
            childAnnotation.index = i;
          }
        }
      }
    },

    // 1. Parse annotations from the template and memoize them on
    //    content._notes (recurses into nested templates)
    // 2. Parse template bindings for parent.* properties and memoize them on
    //    content._parentProps
    // 3. Create bindings in current scope's annotation list to template for
    //    parent props found in template
    // 4. Remove template.content and store it in annotation list, where it
    //    will be the responsibility of the host to set it back to the template
    //    (this is both an optimization to avoid re-stamping nested template
    //    children and avoids a bug in Chrome where nested template children
    //    upgrade)
    _parseTemplate: function(node, index, list, parent) {
      // TODO(sjmiles): simply altering the .content reference didn't
      // work (there was some confusion, might need verification)
      var content = document.createDocumentFragment();
      content._notes = this.parseAnnotations(node);
      content.appendChild(node.content);
      // Special-case treatment of 'parent.*' props for nested templates
      // Automatically bind `prop` on host to `_parent_prop` on template
      // for any `parent.prop`'s encountered in template binding; it is
      // responsibility of the template implementation to forward
      // these properties as appropriate
      var bindings = [];
      this._discoverTemplateParentProps(content);
      for (var prop in content._parentProps) {
        bindings.push({
          index: index,
          kind: 'property',
          mode: '{',
          name: '_parent_' + prop,
          value: prop
        });
      }
      // TODO(sjmiles): using `nar` to avoid unnecessary allocation;
      // in general the handling of these arrays needs some cleanup 
      // in this module
      list.push({
        bindings: bindings,
        events: Polymer.nar,
        templateContent: content,
        parent: parent,
        index: index
      });
    },

    // Finds all parent.* properties in template content and stores
    // the path members in content._parentPropChain, which is an array
    // of maps listing the properties of parent templates required at
    // each level. Each outer template merges inner _parentPropChains to
    // propagate inner parent property needs to outer templates.
    // The top-level parent props from the chain (corresponding to this 
    // template) are stored in content._parentProps.
    _discoverTemplateParentProps: function(content) {
      var chain = content._parentPropChain = [];
      content._notes.forEach(function(n) {
        // Find all bindings to parent.* and spread them into _parentPropChain
        n.bindings.forEach(function(b) {
          var m;
          if (m = b.value.match(/parent\.((parent\.)*[^.]*)/)) {
            var parts = m[1].split('.');
            for (var i=0; i<parts.length; i++) {
              var pp = chain[i] || (chain[i] = {});
              pp[parts[i]] = true;
            }
          }
        });
        // Merge child _parentPropChain[n+1] into this _parentPropChain[n]
        if (n.templateContent) {
          var tpp = n.templateContent._parentPropChain;
          for (var i=1; i<tpp.length; i++) {
            if (tpp[i]) {
              var pp = chain[i-1] || (chain[i-1] = {});
              Polymer.Base.simpleMixin(pp, tpp[i]);              
            }
          }
        }
      });
      // Store this template's parentProps map
      content._parentProps = chain[0];
    },

    // add annotation data from attributes to the `annotation` for node `node`
    // TODO(sjmiles): the distinction between an `annotation` and 
    // `annotation data` is not as clear as it could be
    // Walk attributes backwards, since removeAttribute can be vetoed by
    // IE in certain cases (e.g. <input value="foo">), resulting in the
    // attribute staying in the attributes list
    _parseNodeAttributeAnnotations: function(node, annotation) {
      for (var i=node.attributes.length-1, a; (a=node.attributes[i]); i--) {
        var n = a.name, v = a.value;
        // id
        if (n === 'id') {
          annotation.id = v;
        }
        // events (on-*)
        else if (n.slice(0, 3) === 'on-') {
          node.removeAttribute(n);
          annotation.events.push({
            name: n.slice(3),
            value: v
          });
        }
        // bindings (other attributes)
        else {
          var b = this._parseNodeAttributeAnnotation(node, n, v);
          if (b) {
            annotation.bindings.push(b);
          }
        }
      }
    },

    // construct annotation data from a generic attribute, or undefined
    _parseNodeAttributeAnnotation: function(node, n, v) {
      var mode = '', escape = v.slice(0, 2), name = n;
      if (escape === '{{' || escape === '[[') {
        // Mode (one-way or two)
        mode = escape[0];
        v = v.slice(2, -2);
        // Negate
        var not = false;
        if (v[0] == '!') {
          v = v.substring(1);
          not = true;
        }
        // Attribute or property
        var kind = 'property';
        if (n[n.length-1] == '$') {
          name = n.slice(0, -1);
          kind = 'attribute';
        }
        // Custom notification event
        var notifyEvent, colon;
        if (mode == '{' && (colon = v.indexOf('::')) > 0) {
          notifyEvent = v.substring(colon + 2);
          v = v.substring(0, colon);
        }
        // Remove annotation
        node.removeAttribute(n);
        // Case hackery: attributes are lower-case, but bind targets 
        // (properties) are case sensitive. Gambit is to map dash-case to 
        // camel-case: `foo-bar` becomes `fooBar`.
        // Attribute bindings are excepted.
        if (kind === 'property') {
          name = Polymer.CaseMap.dashToCamelCase(name);
        }
        return {
          kind: kind,
          mode: mode,
          name: name,
          value: v,
          negate: not,
          event: notifyEvent
        };
      }
    },

    // instance-time

    _localSubTree: function(node, host) {
      return (node === host) ? node.childNodes :
         (node.lightChildren || node.childNodes);
    },

    findAnnotatedNode: function(root, annote) {
      // recursively ascend tree until we hit root
      var parent = annote.parent && 
        Polymer.Annotations.findAnnotatedNode(root, annote.parent);
      // unwind the stack, returning the indexed node at each level
      return !parent ? root : 
        Polymer.Annotations._localSubTree(parent, root)[annote.index];
    }

  };



;

  (function() {

    // path fixup for urls in cssText that's expected to 
    // come from a given ownerDocument
    function resolveCss(cssText, ownerDocument) {
      return cssText.replace(CSS_URL_RX, function(m, pre, url, post) {
        return pre + '\'' + 
          resolve(url.replace(/["']/g, ''), ownerDocument) + 
          '\'' + post;
      });
    }

    // url fixup for urls in an element's attributes made relative to 
    // ownerDoc's base url
    function resolveAttrs(element, ownerDocument) {
      for (var name in URL_ATTRS) {
        var a$ = URL_ATTRS[name];
        for (var i=0, l=a$.length, a, at, v; (i<l) && (a=a$[i]); i++) {
          if (name === '*' || element.localName === name) {
            at = element.attributes[a];
            v = at && at.value;
            if (v && (v.search(BINDING_RX) < 0)) {
              at.value = (a === 'style') ?
                resolveCss(v, ownerDocument) :
                resolve(v, ownerDocument);
            }
          }
        }
      }
    }

    function resolve(url, ownerDocument) {
      var resolver = getUrlResolver(ownerDocument);
      resolver.href = url;
      return resolver.href || url;
    }

    var tempDoc;
    var tempDocBase;
    function resolveUrl(url, baseUri) {
      if (!tempDoc) {
        tempDoc = document.implementation.createHTMLDocument('temp');
        tempDocBase = tempDoc.createElement('base');
        tempDoc.head.appendChild(tempDocBase);
      }
      tempDocBase.href = baseUri;
      return resolve(url, tempDoc);
    }

    function getUrlResolver(ownerDocument) {
      return ownerDocument.__urlResolver || 
        (ownerDocument.__urlResolver = ownerDocument.createElement('a'));
    }

    var CSS_URL_RX = /(url\()([^)]*)(\))/g;
    var URL_ATTRS = {
      '*': ['href', 'src', 'style', 'url'],
      form: ['action']
    };
    var BINDING_RX = /\{\{|\[\[/;

    // exports
    Polymer.ResolveUrl = {
      resolveCss: resolveCss,
      resolveAttrs: resolveAttrs,
      resolveUrl: resolveUrl
    };

  })();


;

/**
 * Scans a template to produce an annotation object that stores expression 
 * metadata along with information to associate the metadata with nodes in an 
 * instance.
 *
 * Elements with `id` in the template are noted and marshaled into an 
 * the `$` hash in an instance. 
 * 
 * Example
 * 
 *     &lt;template>
 *       &lt;div id="foo">&lt;/div>
 *     &lt;/template>
 *     &lt;script>
 *      Polymer({
 *        task: function() {
 *          this.$.foo.style.color = 'red';
 *        }
 *      });
 *     &lt;/script>
 * 
 * Other expressions that are noted include:
 *
 * Double-mustache annotations in text content. The annotation must be the only
 * content in the tag, compound expressions are not (currently) supported.
 *
 *     <[tag]>{{path.to.host.property}}<[tag]>
 *
 * Double-mustache annotations in an attribute.
 *
 *     <[tag] someAttribute="{{path.to.host.property}}"><[tag]>
 *
 * Only immediate host properties can automatically trigger side-effects.
 * Setting `host.path` in the example above triggers the binding, setting
 * `host.path.to.host.property` does not.
 *
 * `on-` style event declarations.
 *
 *     <[tag] on-<event-name>="{{hostMethodName}}"><[tag]>
 *
 * Note: **the `annotations` feature does not actually implement the behaviors
 * associated with these expressions, it only captures the data**. 
 * 
 * Other optional features contain actual data implementations.
 *
 * @class standard feature: annotations
 */

/*

Scans a template to produce an annotation map that stores expression metadata
and information that associates the metadata to nodes in a template instance.

Supported annotations are:

  * id attributes
  * binding annotations in text nodes
    * double-mustache expressions: {{expression}}
    * double-bracket expressions: [[expression]]
  * binding annotations in attributes
    * attribute-bind expressions: name="{{expression}} || [[expression]]"
    * property-bind expressions: name*="{{expression}} || [[expression]]"
    * property-bind expressions: name:="expression"
  * event annotations
    * event delegation directives: on-<eventName>="expression"

Generated data-structure:

  [
    {
      id: '<id>',
      events: [
        {
          mode: ['auto'|''],
          name: '<name>'
          value: '<expression>'
        }, ...
      ],
      bindings: [
        {
          kind: ['text'|'attribute'|'property'],
          mode: ['auto'|''],
          name: '<name>'
          value: '<expression>'
        }, ...
      ],
      // TODO(sjmiles): confusingly, this is annotation-parent, not node-parent
      parent: <reference to parent annotation>,
      index: <integer index in parent's childNodes collection>
    },
    ...
  ]

TODO(sjmiles): this module should produce either syntactic metadata
(e.g. double-mustache, double-bracket, star-attr), or semantic metadata
(e.g. manual-bind, auto-bind, property-bind). Right now it's half and half.

*/

  Polymer.Base._addFeature({

    // registration-time

    _prepAnnotations: function() {
      if (!this._template) {
        this._notes = [];
      } else {
        // TODO(sorvell): ad hoc method of plugging behavior into Annotations
        Polymer.Annotations.prepElement = this._prepElement.bind(this);
        this._notes = Polymer.Annotations.parseAnnotations(this._template);
        Polymer.Annotations.prepElement = null;
      }
    },

    _prepElement: function(element) {
      Polymer.ResolveUrl.resolveAttrs(element, this._template.ownerDocument);
    },

    // instance-time

    findAnnotatedNode: Polymer.Annotations.findAnnotatedNode,

    // marshal all teh things
    _marshalAnnotationReferences: function() {
      if (this._template) {
        this._marshalIdNodes();
        this._marshalAnnotatedNodes();
        this._marshalAnnotatedListeners();
      }
    },

    // push configuration references at configure time
    _configureAnnotationReferences: function() {
      this._configureTemplateContent();
    },

    // nested template contents have been stored prototypically to avoid 
    // unnecessary duplication, here we put references to the 
    // indirected contents onto the nested template instances
    _configureTemplateContent: function() {
      this._notes.forEach(function(note) {
        if (note.templateContent) {
          var template = this.findAnnotatedNode(this.root, note);
          template._content = note.templateContent;
        }
      }, this);
    },

    // construct `$` map (from id annotations)
    _marshalIdNodes: function() {
      this.$ = {};
      this._notes.forEach(function(a) {
        if (a.id) {
          this.$[a.id] = this.findAnnotatedNode(this.root, a);
        }
      }, this);
    },

    // concretize `_nodes` map (from anonymous annotations)
    _marshalAnnotatedNodes: function() {
      if (this._nodes) {
        this._nodes = this._nodes.map(function(a) {
          return this.findAnnotatedNode(this.root, a);
        }, this);
      }
    },

    // install event listeners (from event annotations)
    _marshalAnnotatedListeners: function() {
      this._notes.forEach(function(a) {
        if (a.events && a.events.length) {
          var node = this.findAnnotatedNode(this.root, a);
          a.events.forEach(function(e) {
            this.listen(node, e.name, e.value);
          }, this);
        }
      }, this);
    }

  });


;

(function(scope) {

 'use strict';

  var async = scope.Base.async;

  var Gestures = {
    gestures: {},

    // automate the event listeners for the native events
    // TODO(dfreedm): add a way to remove handlers.
    add: function(evType, node, handler) {
      // listen for events in order to "recognize" this event
      var g = this.gestures[evType];
      var gn = '_' + evType;
      var info = {started: false, abortTrack: false, oneshot: false};
      if (g && !node[gn]) {
        if (g.touchaction) {
          this._setupTouchAction(node, g.touchaction, info);
        }
        for (var i = 0, n, sn, fn; i < g.deps.length; i++) {
          n = g.deps[i];
          fn = g[n].bind(g, info);
          sn = '_' + evType + '-' + n;
          // store the handler on the node for future removal
          node[sn] = fn;
          node.addEventListener(n, fn);
        }
        node[gn] = 0;
      }
      // listen for the gesture event
      node[gn]++;
      node.addEventListener(evType, handler);
    },

    remove: function(evType, node, handler) {
      var g = this.gestures[evType];
      var gn = '_' + evType;
      if (g && node[gn]) {
        for (var i = 0, n, sn, fn; i < g.deps.length; i++) {
          n = g.deps[i];
          sn = '_' + evType + '-' + n;
          fn = node[sn];
          if (fn){
            node.removeEventListener(n, fn);
            // remove stored handler to allow GC
            node[sn] = undefined;
          }
        }
        node[gn] = node[gn] ? (node[gn] - 1) : 0;
        node.removeEventListener(evType, handler);
      }
    },

    register: function(recog) {
      this.gestures[recog.name] = recog;
    },

    // touch will make synthetic mouse events
    // preventDefault on touchend will cancel them,
    // but this breaks <input> focus and link clicks
    // Disabling "mouse" handlers for 500ms is enough

    _cancelFunction: null,

    cancelNextClick: function(timeout) {
      if (!this._cancelFunction) {
        timeout = timeout || 500;
        var self = this;
        var reset = function() {
          var cfn = self._cancelFunction;
          if (cfn) {
            clearTimeout(cfn.id);
            document.removeEventListener('click', cfn, true);
            self._cancelFunction = null;
          }
        };
        var canceller = function(e) {
          e.tapPrevented = true;
          reset();
        };
        canceller.id = setTimeout(reset, timeout);
        this._cancelFunction = canceller;
        document.addEventListener('click', canceller, true);
      }
    },

    // try to use the native touch-action, if it exists
    _hasNativeTA: typeof document.head.style.touchAction === 'string',

    // set scrolling direction on node to check later on first move
    // must call this before adding event listeners!
    setTouchAction: function(node, value) {
      if (this._hasNativeTA) {
        node.style.touchAction = value;
      }
      node.touchAction = value;
    },

    _setupTouchAction: function(node, value, info) {
      // reuse custom value on node if set
      var ta = node.touchAction;
      value = ta || value;
      // set an anchor point to see how far first move is
      node.addEventListener('touchstart', function(e) {
        var t = e.changedTouches[0];
        info.initialTouch = {x: t.clientX, y: t.clientY};
        info.abortTrack = false;
        info.oneshot = false;
      });
      node.addEventListener('touchmove', function(e) {
        // only run this once
        if (info.oneshot) {
          return;
        }
        info.oneshot = true;
        // "none" means always track
        if (value === 'none') {
          return;
        }
        // "auto" is default, always scroll
        // bail-out if touch-action did its job
        // the touchevent is non-cancelable if the page/area is scrolling
        if (value === 'auto' || !value || (ta && !e.cancelable)) {
          info.abortTrack = true;
          return;
        }
        // check first move direction
        // unfortunately, we can only make the decision in the first move,
        // so we have to use whatever values are available.
        // Typically, this can be a really small amount, :(
        var t = e.changedTouches[0];
        var x = t.clientX, y = t.clientY;
        var dx = Math.abs(info.initialTouch.x - x);
        var dy = Math.abs(info.initialTouch.y - y);
        // scroll in x axis, abort track if we move more in x direction
        if (value === 'pan-x') {
          info.abortTrack = dx >= dy;
          // scroll in y axis, abort track if we move more in y direction
        } else if (value === 'pan-y') {
          info.abortTrack = dy >= dx;
        }
      });
    },

    fire: function(target, type, detail, bubbles, cancelable) {
      return target.dispatchEvent(
        new CustomEvent(type, {
          detail: detail,
          bubbles: bubbles,
          cancelable: cancelable
        })
      );
    }

  };

  Gestures.register({
    name: 'track',
    touchaction: 'none',
    deps: ['mousedown', 'touchmove', 'touchend'],

    mousedown: function(info, e) {
      var t = e.currentTarget;
      var self = this;
      var movefn = function movefn(e, up) {
        if (!info.tracking && !up) {
          // set up tap prevention
          Gestures.cancelNextClick();
        }
        // first move is 'start', subsequent moves are 'move', mouseup is 'end'
        var state = up ? 'end' : (!info.started ? 'start' : 'move');
        info.started = true;
        self.fire(t, e, state);
        e.preventDefault();
      };
      var upfn = function upfn(e) {
        // call mousemove function with 'end' state
        movefn(e, true);
        info.started = false;
        // remove the temporary listeners
        document.removeEventListener('mousemove', movefn);
        document.removeEventListener('mouseup', upfn);
      };
      // add temporary document listeners as mouse retargets
      document.addEventListener('mousemove', movefn);
      document.addEventListener('mouseup', upfn);
    },

    touchmove: function(info, e) {
      var t = e.currentTarget;
      var ct = e.changedTouches[0];
      // if track was aborted, stop tracking
      if (info.abortTrack) {
        return;
      }
      e.preventDefault();
      // the first track event is sent after some hysteresis with touchmove.
      // Use `started` state variable to differentiate the "first" move from
      // the rest to make track.state == 'start'
      // first move is 'start', subsequent moves are 'move'
      var state = !info.started ? 'start' : 'move';
      info.started = true;
      this.fire(t, ct, state);
    },

    touchend: function(info, e) {
      var t = e.currentTarget;
      var ct = e.changedTouches[0];
      // only trackend if track was started and not aborted
      if (info.started && !info.abortTrack) {
        // reset started state on up
        info.started = false;
        var ne = this.fire(t, ct, 'end');
        // iff tracking, always prevent tap
        e.tapPrevented = true;
      }
    },

    fire: function(target, touch, state) {
      return Gestures.fire(target, 'track', {
        state: state,
        x: touch.clientX,
        y: touch.clientY
      });
    }

  });

  // dispatch a *bubbling* "tap" only at the node that is the target of the
  // generating event.
  // dispatch *synchronously* so that we can implement prevention of native
  // actions like links being followed.
  //
  // TODO(dfreedm): a tap should not occur when there's too much movement.
  // Right now, a tap can occur when a touchend happens very far from the
  // generating touch.
  // This *should* obviate the need for tapPrevented via track.
  Gestures.register({
    name: 'tap',
    deps: ['click', 'touchend'],

    click: function(info, e) {
      this.forward(e);
    },

    touchend: function(info, e) {
      Gestures.cancelNextClick();
      this.forward(e);
    },

    forward: function(e) {
      // prevent taps from being generated from events that have been
      // canceled (e.g. via cancelNextClick) or already handled via
      // a listener lower in the tree.
      if (!e.tapPrevented) {
        e.tapPrevented = true;
        this.fire(e.target);
      }
    },

    // fire a bubbling event from the generating target.
    fire: function(target) {
      Gestures.fire(target, 'tap', {}, true);
    }

  });

  scope.Gestures = Gestures;

})(Polymer);


;

  /**
   * Supports `listeners` and `keyPresses` objects.
   *
   * Example:
   *
   *     using('Base', function(Base) {
   *
   *       Polymer({
   *
   *         listeners: {
   *           // `click` events on the host are delegated to `clickHandler`
   *           'click': 'clickHandler'
   *         },
   *
   *         keyPresses: {
   *           // 'ESC' key presses are delegated to `escHandler`
   *           Base.ESC_KEY: 'escHandler'
   *         },
   *
   *         ...
   *
   *       });
   *
   *     });
   *
   * @class standard feature: events
   *
   */

  Polymer.Base._addFeature({

    listeners: {},

    _listenListeners: function(listeners) {
      var node, name, key;
      for (key in listeners) {
        if (key.indexOf('.') < 0) {
          node = this;
          name = key;
        } else {
          name = key.split('.');
          node = this.$[name[0]];
          name = name[1];
        }
        this.listen(node, name, listeners[key]);
      }
    },

    listen: function(node, eventName, methodName) {
      var host = this;
      var handler = function(e) {
        if (host[methodName]) {
          host[methodName](e, e.detail);
        } else {
          console.warn('[%s].[%s]: event handler [%s] is null in scope (%o)',
            node.localName, eventName, methodName, host);
        }
      };
      switch (eventName) {
        case 'tap':
        case 'track':
          Polymer.Gestures.add(eventName, node, handler);
          break;

        default:
          node.addEventListener(eventName, handler);
          break;
      }
    },

    keyCodes: {
      ESC_KEY: 27,
      ENTER_KEY: 13,
      LEFT: 37,
      UP: 38,
      RIGHT: 39,
      DOWN: 40,
      SPACE: 32
    }

  });


;

Polymer.Async = (function() {
  
  var currVal = 0;
  var lastVal = 0;
  var callbacks = [];
  var twiddle = document.createTextNode('');

  function runAsync(callback, waitTime) {
    if (waitTime > 0) {
      return ~setTimeout(callback, waitTime);
    } else {
      twiddle.textContent = currVal++;
      callbacks.push(callback);
      return currVal - 1;
    }
  }

  function cancelAsync(handle) {
    if (handle < 0) {
      clearTimeout(~handle);
    } else {
      var idx = handle - lastVal;
      if (idx >= 0) {
        if (!callbacks[idx]) {
          throw 'invalid async handle: ' + handle;
        }
        callbacks[idx] = null;
      }
    }
  }

  function atEndOfMicrotask() {
    var len = callbacks.length;
    for (var i=0; i<len; i++) {
      var cb = callbacks[i];
      if (cb) {
        cb();
      }
    }
    callbacks.splice(0, len);
    lastVal += len;
  }

  new (window.MutationObserver || JsMutationObserver)(atEndOfMicrotask)
    .observe(twiddle, {characterData: true})
    ;
  
  // exports 

  return {
    run: runAsync,
    cancel: cancelAsync
  };
  
})();


;

Polymer.Debounce = (function() {
  
  // usage
  
  // invoke cb.call(this) in 100ms, unless the job is re-registered,
  // which resets the timer
  // 
  // this.job = this.debounce(this.job, cb, 100)
  //
  // returns a handle which can be used to re-register a job

  var Async = Polymer.Async;
  
  var Debouncer = function(context) {
    this.context = context;
    this.boundComplete = this.complete.bind(this);
  };
  
  Debouncer.prototype = {
    go: function(callback, wait) {
      var h;
      this.finish = function() {
        Async.cancel(h);
      };
      h = Async.run(this.boundComplete, wait);
      this.callback = callback;
    },
    stop: function() {
      if (this.finish) {
        this.finish();
        this.finish = null;
      }
    },
    complete: function() {
      if (this.finish) {
        this.stop();
        this.callback.call(this.context);
      }
    }
  };

  function debounce(debouncer, callback, wait) {
    if (debouncer) {
      debouncer.stop();
    } else {
      debouncer = new Debouncer(this);
    }
    debouncer.go(callback, wait);
    return debouncer;
  }
  
  // exports 

  return debounce;
  
})();


;

  Polymer.Base._addFeature({

    $$: function(slctr) {
      return Polymer.dom(this.root).querySelector(slctr);
    },

    toggleClass: function(name, bool, node) {
      node = node || this;
      if (arguments.length == 1) {
        bool = !node.classList.contains(name);
      }
      if (bool) {
        node.classList.add(name);
      } else {
        node.classList.remove(name);
      }
    },

    toggleAttribute: function(name, bool, node) {
      (node || this)[bool ? 'setAttribute' : 'removeAttribute'](name, '');
    },

    classFollows: function(className, neo, old) {
      if (old) {
        old.classList.remove(className);
      }
      if (neo) {
        neo.classList.add(className);
      }
    },

    attributeFollows: function(name, neo, old) {
      if (old) {
        old.removeAttribute(name);
      }
      if (neo) {
        neo.setAttribute(name, '');
      }
    },

    getContentChildNodes: function(slctr) {
      return Polymer.dom(Polymer.dom(this.root).querySelector(
          slctr || 'content')).getDistributedNodes();
    },

    getContentChildren: function(slctr) {
      return this.getContentChildNodes(slctr).filter(function(n) {
        return (n.nodeType === Node.ELEMENT_NODE);
      });
    },

    fire: function(type, detail, options) {
      options = options || Polymer.nob;
      var node = options.node || this;
      var detail = (detail === null || detail === undefined) ? Polymer.nob : detail;
      var bubbles = options.bubbles === undefined ? true : options.bubbles;
      var event = new CustomEvent(type, {
        bubbles: Boolean(bubbles),
        cancelable: Boolean(options.cancelable),
        detail: detail
      });
      node.dispatchEvent(event);
      return event;
    },

    async: function(method, waitTime) {
      return Polymer.Async.run(method.bind(this), waitTime);
    },

    cancelAsync: function(handle) {
      Polymer.Async.cancel(handle);
    },

    arrayDelete: function(array, item) {
      var index = array.indexOf(item);
      if (index >= 0) {
        return array.splice(index, 1);
      }
    },

    transform: function(node, transform) {
      node.style.webkitTransform = transform;
      node.style.transform = transform;
    },

    translate3d: function(node, x, y, z) {
      this.transform(node, 'translate3d(' + x + ',' + y + ',' + z + ')');
    },

    importHref: function(href, onload, onerror) {
      var l = document.createElement('link');
      l.rel = 'import';
      l.href = href;
      if (onload) {
        l.onload = onload.bind(this);
      }
      if (onerror) {
        l.onerror = onerror.bind(this);
      }
      document.head.appendChild(l);
      return l;
    },

    create: function(tag, props) {
      var elt = document.createElement(tag);
      if (props) {
        for (var n in props) {
          elt[n] = props[n];
        }
      }
      return elt;
    },

    simpleMixin: function(a, b) {
      for (var i in b) {
        a[i] = b[i];
      }
    }

  });


;

  Polymer.Bind = {

    // for prototypes (usually)

    prepareModel: function(model) {
      model._propertyEffects = {};
      model._bindListeners = [];
      // TODO(sjmiles): no mixin function?
      var api = this._modelApi;
      for (var n in api) {
        model[n] = api[n];
      }
    },

    _modelApi: {

      _notifyChange: function(property) {
        var eventName = Polymer.CaseMap.camelToDashCase(property) + '-changed';
        // TODO(sjmiles): oops, `fire` doesn't exist at this layer
        this.fire(eventName, {
          value: this[property]
        }, {bubbles: false});
      },

      // TODO(sjmiles): removing _notifyListener from here breaks accessors.html
      // as a standalone lib. This is temporary, as standard/configure.html
      // installs it's own version on Polymer.Base, and we need that to work
      // right now.
      // NOTE: exists as a hook for processing listeners
      /*
      _notifyListener: function(fn, e) {
        // NOTE: pass e.target because e.target can get lost if this function
        // is queued asynchrously
        return fn.call(this, e, e.target);
      },
      */

      _propertySet: function(property, value, effects) {
        var old = this._data[property];
        if (old !== value) {
          this._data[property] = value;
          if (typeof value == 'object') {
            this._clearPath(property);
          }
          if (effects) {
            this._effectEffects(property, value, effects, old);
          }
        }
        return old;
      },

      _effectEffects: function(property, value, effects, old) {
        effects.forEach(function(fx) {
          //console.log(fx);
          var fn = Polymer.Bind[fx.kind + 'Effect'];
          if (fn) {
            fn.call(this, property, value, fx.effect, old);
          }
        }, this);
      },

      _clearPath: function(path) {
        for (var prop in this._data) {
          if (prop.indexOf(path + '.') === 0) {
            this._data[prop] = undefined;
          }
        }
      }

    },

    // a prepared model can acquire effects

    ensurePropertyEffects: function(model, property) {
      var fx = model._propertyEffects[property];
      if (!fx) {
        fx = model._propertyEffects[property] = [];
      }
      return fx;
    },

    addPropertyEffect: function(model, property, kind, effect) {
      var fx = this.ensurePropertyEffects(model, property);
      fx.push({
        kind: kind,
        effect: effect
      });
    },

    createBindings: function(model) {
      //console.group(model.is);
      // map of properties to effects
      var fx$ = model._propertyEffects;
      if (fx$) {
        // for each property with effects
        for (var n in fx$) {
          // array of effects
          var fx = fx$[n];
          // effects have priority
          fx.sort(this._sortPropertyEffects);
          // create accessors
          this._createAccessors(model, n, fx);
        }
      }
      //console.groupEnd();
    },

    _sortPropertyEffects: (function() {
      // TODO(sjmiles): EFFECT_ORDER buried this way is not ideal,
      // but presumably the sort method is going to be a hot path and not
      // have a `this`. There is also a problematic dependency on effect.kind
      // values here, which are otherwise pluggable.
      var EFFECT_ORDER = {
        'compute': 0,
        'annotation': 1,
        'computedAnnotation': 2,
        'reflect': 3,
        'notify': 4,
        'observer': 5,
        'complexObserver': 6,
        'function': 7
      };
      return function(a, b) {
        return EFFECT_ORDER[a.kind] - EFFECT_ORDER[b.kind];
      };
    })(),

    // create accessors that implement effects

    _createAccessors: function(model, property, effects) {
      var defun = {
        get: function() {
          // TODO(sjmiles): elide delegation for performance, good ROI?
          return this._data[property];
        }
      };
      var setter = function(value) {
        this._propertySet(property, value, effects);
      };
      // ReadOnly properties have a private setter only
      // TODO(kschaaf): Per current Bind factoring, we shouldn't
      // be interrogating the prototype here
      if (model.isReadOnlyProperty && model.isReadOnlyProperty(property)) {
        //model['_' + property + 'Setter'] = setter;
        //model['_set_' + property] = setter;
        model['_set' + this.upper(property)] = setter;
      } else {
        defun.set = setter;
      }
      Object.defineProperty(model, property, defun);
    },

    upper: function(name) {
      return name[0].toUpperCase() + name.substring(1);
    },

    _addAnnotatedListener: function(model, index, property, path, event) {
      var fn = this._notedListenerFactory(property, path,
        this._isStructured(path), this._isEventBogus);
      var eventName = event ||
        (Polymer.CaseMap.camelToDashCase(property) + '-changed');
      model._bindListeners.push({
        index: index,
        property: property,
        path: path,
        changedFn: fn,
        event: eventName
      });
    },

    _isStructured: function(path) {
      return path.indexOf('.') > 0;
    },

    _isEventBogus: function(e, target) {
      return e.path && e.path[0] !== target;
    },

    _notedListenerFactory: function(property, path, isStructured, bogusTest) {
      return function(e, target) {
        if (!bogusTest(e, target)) {
          if (e.detail && e.detail.path) {
            this.notifyPath(this._fixPath(path, property, e.detail.path),
              e.detail.value);
          } else {
            var value = target[property];
            if (!isStructured) {
              this[path] = target[property];
            } else {
              // TODO(kschaaf): dirty check avoids null references when the object has gone away
              if (this._data[path] != value) {
                this.setPathValue(path, value);
              }
            }
          }
        }
      };
    },

    // for instances

    prepareInstance: function(inst) {
      inst._data = Object.create(null);
    },

    setupBindListeners: function(inst) {
      inst._bindListeners.forEach(function(info) {
        // Property listeners:
        // <node>.on.<property>-changed: <path]> = e.detail.value
        //console.log('[_setupBindListener]: [%s][%s] listening for [%s][%s-changed]', this.localName, info.path, info.id || info.index, info.property);
        var node = inst._nodes[info.index];
        node.addEventListener(info.event, inst._notifyListener.bind(inst, info.changedFn));
      });
    }

  };


;

  Polymer.Base.extend(Polymer.Bind, {

    _shouldAddListener: function(effect) {
      return effect.name &&
             effect.mode === '{' &&
             !effect.negate &&
             effect.kind != 'attribute'
             ;
    },

    annotationEffect: function(source, value, effect) {
      if (source != effect.value) {
        value = this.getPathValue(effect.value);
        this._data[effect.value] = value;
      }
      var calc = effect.negate ? !value : value;
      return this._applyEffectValue(calc, effect);
    },

    reflectEffect: function(source) {
      this.reflectPropertyToAttribute(source);
    },

    notifyEffect: function(source) {
      this._notifyChange(source);
    },

    // Raw effect for extension; effect.function is an actual function
    functionEffect: function(source, value, effect, old) {
      effect.function.call(this, source, value, effect, old);
    },

    observerEffect: function(source, value, effect, old) {
      this[effect.method](value, old);
    },

    complexObserverEffect: function(source, value, effect) {
      var args = Polymer.Bind._marshalArgs(this._data, effect, source, value);
      if (args) {
        this[effect.method].apply(this, args);
      }
    },

    computeEffect: function(source, value, effect) {
      var args = Polymer.Bind._marshalArgs(this._data, effect, source, value);
      if (args) {
        this[effect.property] = this[effect.method].apply(this, args);
      }
    },

    annotatedComputationEffect: function(source, value, effect) {
      var args = Polymer.Bind._marshalArgs(this._data, effect, source, value);
      if (args) {
        var computedHost = this._rootDataHost || this;
        var computedvalue = 
          computedHost[effect.method].apply(computedHost, args);
        this._applyEffectValue(computedvalue, effect);
      }
    },

    // path & value are used to fill in wildcard descriptor when effect is
    // being called as a result of a path notification
    _marshalArgs: function(model, effect, path, value) {
      var values = [];
      var args = effect.args;
      for (var i=0, l=args.length; i<l; i++) {
        var arg = args[i];
        var name = arg.name;
        var v = arg.structured ?
          Polymer.Base.getPathValue(name, model) : model[name];
        if (v === undefined) {
          return;
        }
        if (arg.wildcard) {
          // Only send the actual path changed info if the change that
          // caused the observer to run matched the wildcard
          var baseChanged = (name.indexOf(path + '.') === 0);
          var matches = (effect.arg.name.indexOf(name) === 0 && !baseChanged);
          values[i] = {
            path: matches ? path : name,
            value: matches ? value : v,
            base: v
          };
        } else {
          values[i] = v;
        }
      }
      return values;
    }

  });


;

  /**
   * Support for property side effects.
   *
   * Key for effect objects:
   *
   * property | ann | anCmp | cmp | obs | cplxOb | description
   * ---------|-----|-------|-----|-----|--------|----------------------------------------
   * method   |     | X     | X   | X   | X      | function name to call on instance
   * args     |     | X     | X   |     | X      | list of all arg descriptors for fn
   * arg      |     | X     | X   |     | X      | arg descriptor for effect
   * property |     |       | X   | X   |        | property for effect to set or get
   * name     | X   |       |     |     |        | annotation value (text inside {{...}})
   * kind     | X   | X     |     |     |        | binding type (property or attribute)
   * index    | X   | X     |     |     |        | node index to set
   *
   */

  Polymer.Base._addFeature({

    _addPropertyEffect: function(property, kind, effect) {
     // TODO(sjmiles): everything to the right of the first '.' is lost, implies
     // there is some duplicate information flow (not the only sign)
     var model = property.split('.').shift();
     Polymer.Bind.addPropertyEffect(this, model, kind, effect);
    },

    // prototyping

    _prepEffects: function() {
      Polymer.Bind.prepareModel(this);
      this._addAnnotationEffects(this._notes);
    },

    _prepBindings: function() {
      Polymer.Bind.createBindings(this);
    },

    _addPropertyEffects: function(effects) {
      if (effects) {
        for (var n in effects) {
          var effect = effects[n];
          if (effect.observer) {
            this._addObserverEffect(n, effect.observer);
          }
          if (effect.computed) {
            this._addComputedEffect(n, effect.computed);
          }
          if (effect.notify) {
            this._addPropertyEffect(n, 'notify');
          }
          if (effect.reflectToAttribute) {
            this._addPropertyEffect(n, 'reflect');
          }
          if (this.isReadOnlyProperty(n)) {
            // Ensure accessor is created
            Polymer.Bind.ensurePropertyEffects(this, n);
          }
        }
      }
    },

    _parseMethod: function(expression) {
      var m = expression.match(/(\w*)\((.*)\)/);
      if (m) {
        return {
          method: m[1],
          args: m[2].split(/[^\w.*]+/).map(this._parseArg)
        };
      }
    },

    _parseArg: function(arg) {
      var a = { name: arg };
      a.structured = arg.indexOf('.') > 0;
      if (a.structured) {
        a.wildcard = (arg.slice(-2) == '.*');
        if (a.wildcard) {
          a.name = arg.slice(0, -2);
        }
      }
      return a;
    },

    _addComputedEffect: function(name, expression) {
      var sig = this._parseMethod(expression);
      sig.args.forEach(function(arg) {
        this._addPropertyEffect(arg.name, 'compute', {
          method: sig.method,
          args: sig.args,
          arg: arg,
          property: name
        });
      }, this);
    },

    _addObserverEffect: function(property, observer) {
      this._addPropertyEffect(property, 'observer', {
        method: observer,
        property: property
      });
    },

    _addComplexObserverEffects: function(observers) {
      if (observers) {
        observers.forEach(function(observer) {
          this._addComplexObserverEffect(observer);
        }, this);
      }
    },

    _addComplexObserverEffect: function(observer) {
      var sig = this._parseMethod(observer);
      sig.args.forEach(function(arg) {
        this._addPropertyEffect(arg.name, 'complexObserver', {
          method: sig.method,
          args: sig.args,
          arg: arg
        });
      }, this);
    },

    _addAnnotationEffects: function(notes) {
      // create a virtual annotation list, must be concretized at instance time
      this._nodes = [];
      // process annotations that have been parsed from template
      notes.forEach(function(note) {
        // where to find the node in the concretized list
        var index = this._nodes.push(note) - 1;
        note.bindings.forEach(function(binding) {
          this._addAnnotationEffect(binding, index);
        }, this);
      }, this);
    },

    _addAnnotationEffect: function(note, index) {
      // TODO(sjmiles): annotations have 'effects' proper and 'listener'
      if (Polymer.Bind._shouldAddListener(note)) {
        // <node>.on.<dash-case-property>-changed: <path> = e.detail.value
        Polymer.Bind._addAnnotatedListener(this, index,
          note.name, note.value, note.event);
      }
      var sig = this._parseMethod(note.value);
      if (sig) {
        this._addAnnotatedComputationEffect(sig, note, index);
      } else {
        // capture the node index
        note.index = index;
        // discover top-level property (model) from path
        var model = note.value.split('.').shift();
        // add 'annotation' binding effect for property 'model'
        this._addPropertyEffect(model, 'annotation', note);
      }
    },

    _addAnnotatedComputationEffect: function(sig, note, index) {
      sig.args.forEach(function(arg) {
        this._addPropertyEffect(arg.name, 'annotatedComputation', {
          kind: note.kind,
          method: sig.method,
          args: sig.args,
          arg: arg,
          property: note.name,
          index: index
        });
      }, this);
    },

    // instancing

    _marshalInstanceEffects: function() {
      Polymer.Bind.prepareInstance(this);
      Polymer.Bind.setupBindListeners(this);
    },

    _applyEffectValue: function(value, info) {
      var node = this._nodes[info.index];
      // TODO(sorvell): ideally, the info object is normalized for easy
      // lookup here.
      var property = info.property || info.name || 'textContent';
      // special processing for 'class' and 'className'; 'class' handled
      // when attr is serialized.
      if (info.kind == 'attribute') {
        this.serializeValueToAttribute(value, property, node);
      } else {
        // TODO(sorvell): consider pre-processing this step so we don't need
        // this lookup.
        if (property === 'className') {
          value = this._scopeElementClass(node, value);
        }
        return node[property] = value;
      }
    }

  });


;

  /*
    Process inputs efficiently via a configure lifecycle callback.
    Configure is called top-down, host before local dom. Users should
    implement configure to supply a set of default values for the element by
    returning an object containing the properties and values to set.

    Configured values are not immediately set, instead they are set when
    an element becomes ready, after its local dom is ready. This ensures
    that any user change handlers are not called before ready time.

  */

  /*
  Implementation notes:

  Configured values are collected into _config. At ready time, properties
  are set to the values in _config. This ensures properties are set child
  before host and change handlers are called only at ready time. The host
  will reset a value already propagated to a child, but this is not
  inefficient because of dirty checking at the set point.

  Bind notification events are sent when properties are set at ready time
  and thus received by the host before it is ready. Since notifications result
  in property updates and this triggers side effects, handling notifications
  is deferred until ready time.

  In general, events can be heard before an element is ready. This may occur
  when a user sends an event in a change handler or listens to a data event
  directly (on-foo-changed).
  */

  Polymer.Base._addFeature({

    // storage for configuration
    _setupConfigure: function(initialConfig) {
      this._config = initialConfig || {};
      this._handlers = [];
    },

    // static attributes are deserialized into _config
    _takeAttributes: function() {
      this._takeAttributesToModel(this._config);
    },

    // at configure time values are stored in _config
    _configValue: function(name, value) {
      this._config[name] = value;
    },

    // Override polymer-mini thunk
    _beforeClientsReady: function() {
      this._configure();
    },

    // configure: returns user supplied default property values
    // combines with _config to create final property values
    _configure: function() {
      this._configureAnnotationReferences();
      // get individual default values from property configs
      var config = {};
      this._configureProperties(this.properties, config);
      // behave!
      this.behaviors.forEach(function(b) {
        this._configureProperties(b.properties, config);
      }, this);
      // get add'l default values from central configure
      // combine defaults returned from configure with inputs in _config
      this._mixinConfigure(config, this._config);
      // this is the new _config, which are the final values to be applied
      this._config = config;
      // pass configuration data to bindings
      this._distributeConfig(this._config);
    },

    _configureProperties: function(properties, config) {
      for (i in properties) {
        var c = properties[i];
        if (c.value !== undefined) {
          var value = c.value;
          if (typeof value == 'function') {
            // pass existing config values (this._config) to value function
            value = value.call(this, this._config);
          }
          config[i] = value;
        }
      }
    },

    _mixinConfigure: function(a, b) {
      for (var prop in b) {
        if (!this.isReadOnlyProperty(prop)) {
          a[prop] = b[prop];
        }
      }
    },

    // distribute config values to bound nodes.
    _distributeConfig: function(config) {
      var fx$ = this._propertyEffects;
      if (fx$) {
        for (var p in config) {
          var fx = fx$[p];
          if (fx) {
            for (var i=0, l=fx.length, x; (i<l) && (x=fx[i]); i++) {
              if (x.kind === 'annotation') {
                var node = this._nodes[x.effect.index];
                // seeding configuration only
                if (node._configValue) {
                  var value = (p === x.effect.value) ? config[p] :
                    this.getPathValue(x.effect.value, config);
                  node._configValue(x.effect.name, value);
                }
              }
            }
          }
        }
      }
    },

    // Override polymer-mini thunk
    _afterClientsReady: function() {
      this._applyConfig(this._config);
      this._flushHandlers();
    },

    // NOTE: values are already propagated to children via
    // _distributeConfig so propagation triggered by effects here is
    // redundant, but safe due to dirty checking
    _applyConfig: function(config) {
      for (var n in config) {
        // Don't stomp on values that may have been set by other side effects
        if (this[n] === undefined) {
          // Call _propertySet for any properties with accessors, which will
          // initialize read-only properties also
          // TODO(kschaaf): consider passing fromAbove here to prevent
          // unnecessary notify for: 1) possible perf, 2) debuggability
          var effects = this._propertyEffects[n];
          if (effects) {
            this._propertySet(n, config[n], effects);
          } else {
            this[n] = config[n];
          }
        }
      }
    },

    // NOTE: Notifications can be processed before ready since
    // they are sent at *child* ready time. Since notifications cause side
    // effects and side effects must not be processed before ready time,
    // handling is queue/defered until then.
    _notifyListener: function(fn, e) {
      if (!this._clientsReadied) {
        this._queueHandler([fn, e, e.target]);
      } else {
        return fn.call(this, e, e.target);
      }
    },

    _queueHandler: function(args) {
      this._handlers.push(args);
    },

    _flushHandlers: function() {
      var h$ = this._handlers;
      for (var i=0, l=h$.length, h; (i<l) && (h=h$[i]); i++) {
        h[0].call(this, h[1], h[2]);
      }
    }

  });


;

  /**
   * Changes to an object sub-field (aka "path") via a binding
   * (e.g. `<x-foo value="{{item.subfield}}"`) will notify other elements bound to
   * the same object automatically.
   *
   * When modifying a sub-field of an object imperatively
   * (e.g. `this.item.subfield = 42`), in order to have the new value propagated
   * to other elements, a special `setPathValue(path, value)` API is provided.
   * `setPathValue` sets the object field at the path specified, and then notifies the
   * binding system so that other elements bound to the same path will update.
   *
   * Example:
   *
   *     Polymer({
   *
   *       is: 'x-date',
   *
   *       properties: {
   *         date: {
   *           type: Object,
   *           notify: true
   *          }
   *       },
   *
   *       attached: function() {
   *         this.date = {};
   *         setInterval(function() {
   *           var d = new Date();
   *           // Required to notify elements bound to date of changes to sub-fields
   *           // this.date.seconds = d.getSeconds(); <-- Will not notify
   *           this.setPathValue('date.seconds', d.getSeconds());
   *           this.setPathValue('date.minutes', d.getMinutes());
   *           this.setPathValue('date.hours', d.getHours() % 12);
   *         }.bind(this), 1000);
   *       }
   *
   *     });
   *
   *  Allows bindings to `date` sub-fields to update on changes:
   *
   *     <x-date date="{{date}}"></x-date>
   *
   *     Hour: <span>{{date.hours}}</span>
   *     Min:  <span>{{date.minutes}}</span>
   *     Sec:  <span>{{date.seconds}}</span>
   *
   * @class data feature: path notification
   */

  Polymer.Base._addFeature({
    /**
      Notify that a path has changed. For example:

          this.item.user.name = 'Bob';
          this.notifyPath('item.user.name', this.item.user.name);

      Returns true if notification actually took place, based on
      a dirty check of whether the new value was already known
    */
    notifyPath: function(path, value, fromAbove) {
      var old = this._propertySet(path, value);
      // manual dirty checking for now...
      if (old !== value) {
        // console.group((this.localName || this.dataHost.id + '-' + this.dataHost.dataHost.index) + '#' + (this.id || this.index) + ' ' + path, value);
        // Take path effects at this level for exact path matches,
        // and notify down for any bindings to a subset of this path
        this._pathEffector(path, value);
        // Send event to notify the path change upwards
        // Optimization: don't notify up if we know the notification
        // is coming from above already (avoid wasted event dispatch)
        if (!fromAbove) {
          // TODO(sorvell): should only notify if notify: true?
          this._notifyPath(path, value);
        }
        // console.groupEnd((this.localName || this.dataHost.id + '-' + this.dataHost.dataHost.index) + '#' + (this.id || this.index) + ' ' + path, value);
      }
    },

    /**
      Convienence method for setting a value to a path and calling
      notify path
    */
    setPathValue: function(path, value) {
      var parts = path.split('.');
      if (parts.length > 1) {
        var last = parts.pop();
        var prop = this;
        while (parts.length) {
          prop = prop[parts.shift()];
          if (!prop) {
            return;
          }
        }
        // TODO(kschaaf): want dirty-check here?
        // if (prop[last] !== value) {
          prop[last] = value;
          this.notifyPath(path, value);
        // }
      } else {
        this[path] = value;
      }
    },

    getPathValue: function(path, root) {
      var parts = path.split('.');
      var last = parts.pop();
      var prop = root || this;
      while (parts.length) {
        prop = prop[parts.shift()];
        if (!prop) {
          return;
        }
      }
      return prop[last];
    },

    // TODO(kschaaf): This machine can be optimized to memoize compiled path
    // effectors as new paths are notified for performance, since it involves
    // a fair amount of runtime lookup
    _pathEffector: function(path, value) {
      // get root property
      var model = this._modelForPath(path);
      // search property effects of the root property for 'annotation' effects
      var fx$ = this._propertyEffects[model];
      if (fx$) {
        fx$.forEach(function(fx) {
          var fxFn = this[fx.kind + 'PathEffect'];
          if (fxFn) {
            fxFn.call(this, path, value, fx.effect);
          }
        }, this);
      }
      // notify runtime-bound paths
      if (this._boundPaths) {
        this._notifyBoundPaths(path, value);
      }
    },

    annotationPathEffect: function(path, value, effect) {
      if (effect.value === path || effect.value.indexOf(path + '.') === 0) {
        // TODO(sorvell): ideally the effect function is on this prototype
        // so we don't have to call it like this.
        Polymer.Bind.annotationEffect.call(this, path, value, effect);
      } else if ((path.indexOf(effect.value + '.') === 0) && !effect.negate) {
        // locate the bound node
        var node = this._nodes[effect.index];
        if (node && node.notifyPath) {
          var p = this._fixPath(effect.name , effect.value, path);
          node.notifyPath(p, value, true);
        }
      }
    },

    complexObserverPathEffect: function(path, value, effect) {
      if (this._pathMatchesEffect(path, effect)) {
        Polymer.Bind.complexObserverEffect.call(this, path, value, effect);
      }
    },

    computePathEffect: function(path, value, effect) {
      if (this._pathMatchesEffect(path, effect)) {
        Polymer.Bind.computeEffect.call(this, path, value, effect);
      }
    },

    annotatedComputationPathEffect: function(path, value, effect) {
      if (this._pathMatchesEffect(path, effect)) {
        Polymer.Bind.annotatedComputationEffect.call(this, path, value, effect);
      }
    },

    _pathMatchesEffect: function(path, effect) {
      var effectArg = effect.arg.name;
      return (effectArg == path) ||
        (effectArg.indexOf(path + '.') === 0) ||
        (effect.arg.wildcard && path.indexOf(effectArg) === 0);
    },

    bindPaths: function(to, from) {
      this._boundPaths = this._boundPaths || {};
      if (from) {
        this._boundPaths[to] = from;
        // this.setPathValue(to, this.getPathValue(from));
      } else {
        this.unbindPath(to);
        // this.setPathValue(to, from);
      }
    },

    unbindPaths: function(path) {
      if (this._boundPaths) {
        delete this._boundPaths[path];
      }
    },

    _notifyBoundPaths: function(path, value) {
      var from, to;
      for (var a in this._boundPaths) {
        var b = this._boundPaths[a];
        if (path.indexOf(a + '.') == 0) {
          from = a;
          to = b;
          break;
        }
        if (path.indexOf(b + '.') == 0) {
          from = b;
          to = a;
          break;
        }
      }
      if (from && to) {
        var p = this._fixPath(to, from, path);
        this.notifyPath(p, value);
      }
    },

    _fixPath: function(property, root, path) {
      return property + path.slice(root.length);
    },

    _notifyPath: function(path, value) {
      var rootName = this._modelForPath(path);
      var dashCaseName = Polymer.CaseMap.camelToDashCase(rootName);
      var eventName = dashCaseName + this._EVENT_CHANGED;
      this.fire(eventName, {
        path: path,
        value: value
      }, {bubbles: false});
    },

    _modelForPath: function(path) {
      return path.split('.').shift();
    },

    _EVENT_CHANGED: '-changed',

  });


;

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


;

/*
  Extremely simple css parser. Intended to be not more than what we need
  and definitely not necessarly correct =).
*/
(function() {

  // given a string of css, return a simple rule tree
  function parse(text) {
    text = clean(text);
    return parseCss(lex(text), text);
  }

  // remove stuff we don't care about that may hinder parsing
  function clean(cssText) {
    return cssText.replace(rx.comments, '').replace(rx.port, '');
  }

  // super simple {...} lexer that returns a node tree
  function lex(text) {
    var root = {start: 0, end: text.length};
    var n = root;
    for (var i=0, s=0, l=text.length; i < l; i++) {
      switch (text[i]) {
        case OPEN_BRACE:
          //console.group(i);
          if (!n.rules) {
            n.rules = [];
          }
          var p = n;
          var previous = p.rules[p.rules.length-1];
          n = {start: i+1, parent: p, previous: previous};
          p.rules.push(n);
          break;
        case CLOSE_BRACE: 
          //console.groupEnd(n.start);
          n.end = i+1;
          n = n.parent || root;
          break;
      }
    }
    return root;
  }

  // add selectors/cssText to node tree
  function parseCss(node, text) {
    var t = text.substring(node.start, node.end-1);
    node.cssText = t.trim();
    if (node.parent) {
      var ss = node.previous ? node.previous.end : node.parent.start;
      t = text.substring(ss, node.start-1);
      // TODO(sorvell): ad hoc; make selector include only after last ;
      // helps with mixin syntax
      t = t.substring(t.lastIndexOf(';')+1);
      node.selector = t.trim();
    }
    var r$ = node.rules;
    if (r$) {
      for (var i=0, l=r$.length, r; (i<l) && (r=r$[i]); i++) {
        parseCss(r, text);
      }  
    }
    return node;  
  }

  // stringify parsed css.
  function stringify(node, text) {
    text = text || '';
    // calc rule cssText
    var cssText = '';
    if (node.cssText || node.rules) {
      var r$ = node.rules;
      if (r$ && !hasMixinRules(r$)) {
        for (var i=0, l=r$.length, r; (i<l) && (r=r$[i]); i++) {
          cssText = stringify(r, cssText);
        }  
      } else {
        cssText = removeCustomProps(node.cssText).trim();
        if (cssText) {
          cssText = '  ' + cssText + '\n';
        }
      }
    }
    // emit rule iff there is cssText
    if (cssText) {
      if (node.selector) {
        text += node.selector + ' ' + OPEN_BRACE + '\n';
      }
      text += cssText;
      if (node.selector) {
        text += CLOSE_BRACE + '\n\n';
      }
    }
    return text;
  }

  var OPEN_BRACE = '{';
  var CLOSE_BRACE = '}';

  function hasMixinRules(rules) {
    return (rules[0].selector.indexOf(VAR_START) >= 0);
  }

  function removeCustomProps(cssText) {
    return cssText
      .replace(rx.customProp, '')
      .replace(rx.mixinProp, '')
      .replace(rx.mixinApply, '');
  }

  var VAR_START = '--';

  // helper regexp's
  var rx = {
    comments: /\/\*[^*]*\*+([^/*][^*]*\*+)*\//gim,
    port: /@import[^;]*;/gim,
    customProp: /--[^;{]*?:[^{};]*?;/gim,
    mixinProp: /--[^;{]*?:[^{;]*?{[^}]*?}/gim,
    mixinApply: /@mixin[\s]*\([^)]*?\)[\s]*;/gim
  };

  // exports 
  Polymer.CssParse = {
    parse: parse,
    stringify: stringify
  };

})();


;

  (function() {

    function toCssText(rules, callback) {
      if (typeof rules === 'string') {
        rules = Polymer.CssParse.parse(rules);
      } 
      if (callback) {
        forEachStyleRule(rules, callback);
      }
      return Polymer.CssParse.stringify(rules);
    }

    function forEachStyleRule(node, cb) {
      var s = node.selector;
      var skipRules = false;
      if (s) {
        if ((s.indexOf(AT_RULE) !== 0) && (s.indexOf(MIXIN_SELECTOR) !== 0)) {
          cb(node);
        }
        skipRules = (s.indexOf(KEYFRAME_RULE) >= 0) || 
          (s.indexOf(MIXIN_SELECTOR) >= 0);
      }
      var r$ = node.rules;
      if (r$ && !skipRules) {
        for (var i=0, l=r$.length, r; (i<l) && (r=r$[i]); i++) {
          forEachStyleRule(r, cb);
        }
      }
    }

    // add a string of cssText to the document.
    function applyCss(cssText, moniker, target, lowPriority) {
      var style = document.createElement('style');
      if (moniker) {
        style.setAttribute('scope', moniker);
      }
      style.textContent = cssText;
      target = target || document.head;
      if (lowPriority) {
        var n$ = target.querySelectorAll('style[scope]');
        var ref = n$.length ? n$[n$.length-1].nextSibling : target.firstChild;
        target.insertBefore(style, ref);
     } else {
        target.appendChild(style);
      }
      return style;
    }

    var AT_RULE = '@';
    var KEYFRAME_RULE = 'keyframe';
    var MIXIN_SELECTOR = '--';

    // exports
    Polymer.StyleUtil = {
      parser: Polymer.CssParse,
      applyCss: applyCss,
      forEachStyleRule: forEachStyleRule,
      toCssText: toCssText
    };

  })();


;

  (function() {

    /* Transforms ShadowDOM styling into ShadyDOM styling

     * scoping: 

        * elements in scope get scoping selector class="x-foo-scope"
        * selectors re-written as follows:

          div button -> div.x-foo-scope button.x-foo-scope

     * :host -> scopeName

     * :host(...) -> scopeName...

     * ::content -> ' ' NOTE: requires use of scoping selector and selectors
       cannot otherwise be scoped:
       e.g. :host ::content > .bar -> x-foo > .bar

     * ::shadow, /deep/: processed simimlar to ::content

     * :host-context(...): NOT SUPPORTED

    */

    // Given a node and scope name, add a scoping class to each node 
    // in the tree. This facilitates transforming css into scoped rules. 
    function transformDom(node, scope, useAttr, shouldRemoveScope) {
      _transformDom(node, scope || '', useAttr, shouldRemoveScope);
    }

    function _transformDom(node, selector, useAttr, shouldRemoveScope) {
      if (node.setAttribute) {
        transformElement(node, selector, useAttr, shouldRemoveScope);
      }
      var c$ = Polymer.dom(node).childNodes;
      for (var i=0; i<c$.length; i++) {
        _transformDom(c$[i], selector, useAttr, shouldRemoveScope);
      }
    }

    function transformElement(element, scope, useAttr, shouldRemoveScope) {
      if (useAttr) {
        if (shouldRemoveScope) {
          element.removeAttribute(SCOPE_NAME);
        } else {
          element.setAttribute(SCOPE_NAME, scope);
        }
      } else {
        // note: if using classes, we add both the general 'style-scope' class
        // as well as the specific scope. This enables easy filtering of all
        // `style-scope` elements
        if (scope) {
          if (shouldRemoveScope) {
            element.classList.remove(SCOPE_NAME, scope);
          } else {
            element.classList.add(SCOPE_NAME, scope);
          }
        }
      }
    }

    function transformHost(host, scope) {
    }

    // Given a string of cssText and a scoping string (scope), returns
    // a string of scoped css where each selector is transformed to include
    // a class created from the scope. ShadowDOM selectors are also transformed
    // (e.g. :host) to use the scoping selector.
    function transformCss(rules, scope, ext, callback, useAttr) {
      var hostScope = calcHostScope(scope, ext);
      scope = calcElementScope(scope, useAttr);
      return Polymer.StyleUtil.toCssText(rules, function(rule) {
        transformRule(rule, scope, hostScope);
        if (callback) {
          callback(rule, scope, hostScope);
        }
      });
    }

    function calcElementScope(scope, useAttr) {
      if (scope) {
        return useAttr ?
          CSS_ATTR_PREFIX + scope + CSS_ATTR_SUFFIX :
          CSS_CLASS_PREFIX + scope;
      } else {
        return '';
      }
    }

    function calcHostScope(scope, ext) {
      return ext ? '[is=' +  scope + ']' : scope;
    }

    function transformRule(rule, scope, hostScope) {
      _transformRule(rule, transformComplexSelector,
        scope, hostScope);
    }

    // transforms a css rule to a scoped rule.
    function _transformRule(rule, transformer, scope, hostScope) {
      var p$ = rule.selector.split(COMPLEX_SELECTOR_SEP);
      for (var i=0, l=p$.length, p; (i<l) && (p=p$[i]); i++) {
        p$[i] = transformer(p, scope, hostScope);
      }
      rule.selector = p$.join(COMPLEX_SELECTOR_SEP);
    }

    function transformComplexSelector(selector, scope, hostScope) {
      var stop = false;
      selector = selector.replace(SIMPLE_SELECTOR_SEP, function(m, c, s) {
        if (!stop) {
          var o = transformCompoundSelector(s, c, scope, hostScope);
          if (o.stop) {
            stop = true;
          }
          c = o.combinator;
          s = o.value;  
        }
        return c + s;
      });
      return selector;
    }

    function transformCompoundSelector(selector, combinator, scope, hostScope) {
      // replace :host with host scoping class
      var jumpIndex = selector.search(SCOPE_JUMP);
      if (selector.indexOf(HOST) >=0) {
        // :host(...)
        selector = selector.replace(HOST_PAREN, function(m, host, paren) {
          return hostScope + paren;
        });
        // now normal :host
        selector = selector.replace(HOST, hostScope);
      // replace other selectors with scoping class
      } else if (jumpIndex !== 0) {
        selector = scope ? transformSimpleSelector(selector, scope) : selector;
      }
      // remove left-side combinator when dealing with ::content.
      if (selector.indexOf(CONTENT) >= 0) {
        combinator = '';
      }
      // process scope jumping selectors up to the scope jump and then stop
      // e.g. .zonk ::content > .foo ==> .zonk.scope > .foo
      var stop;
      if (jumpIndex >= 0) {
        selector = selector.replace(SCOPE_JUMP, ' ');
        stop = true;
      }
      return {value: selector, combinator: combinator, stop: stop};
    }

    function transformSimpleSelector(selector, scope) {
      var p$ = selector.split(PSEUDO_PREFIX);
      p$[0] += scope;
      return p$.join(PSEUDO_PREFIX);
    }

    function transformRootRule(rule) {
      _transformRule(rule, transformRootSelector);
    }

    function transformRootSelector(selector) {
      return selector.match(SCOPE_JUMP) ?
        transformComplexSelector(selector) :
        selector.trim() + SCOPE_ROOT_SELECTOR;
    }

    var SCOPE_NAME = 'style-scope';
    var SCOPE_ROOT_SELECTOR = ':not([' + SCOPE_NAME + '])' + 
      ':not(.' + SCOPE_NAME + ')';
    var COMPLEX_SELECTOR_SEP = ',';
    var SIMPLE_SELECTOR_SEP = /(^|[\s>+~]+)([^\s>+~]+)/g;
    var HOST = ':host';
    // NOTE: this supports 1 nested () pair for things like 
    // :host(:not([selected]), more general support requires
    // parsing which seems like overkill
    var HOST_PAREN = /(\:host)(?:\(((?:\([^)(]*\)|[^)(]*)+?)\))/g;
    var CONTENT = '::content';
    var SCOPE_JUMP = /\:\:content|\:\:shadow|\/deep\//;
    var CSS_CLASS_PREFIX = '.';
    var CSS_ATTR_PREFIX = '[' + SCOPE_NAME + '~=';
    var CSS_ATTR_SUFFIX = ']';
    var PSEUDO_PREFIX = ':';

    // exports
    Polymer.StyleTransformer = {
      element: transformElement,
      dom: transformDom,
      host: transformHost,
      css: transformCss,
      rule: transformRule,
      rootRule: transformRootRule,
      SCOPE_NAME: SCOPE_NAME
    };

  })();


;

  (function() {

    var prepTemplate = Polymer.Base._prepTemplate;
    var prepElement = Polymer.Base._prepElement;
    var baseStampTemplate = Polymer.Base._stampTemplate;
    var nativeShadow = Polymer.Settings.useNativeShadow;

    Polymer.Base._addFeature({

      // declaration-y
      _prepTemplate: function() {
        prepTemplate.call(this);
        var port = Polymer.DomModule.import(this.is);
        if (this._encapsulateStyle === undefined) {
          this._encapsulateStyle = 
            Boolean(port && !nativeShadow);
        }
        // scope css
        // NOTE: dom scoped via annotations
        if (nativeShadow || this._encapsulateStyle) {
          this._scopeCss();
        }
      },

      _prepElement: function(element) {
        if (this._encapsulateStyle) {
          Polymer.StyleTransformer.element(element, this.is,
            this._scopeCssViaAttr);
        }
        prepElement.call(this, element);
      },

      _scopeCss: function() {
        this._styles = this._prepareStyles();
        this._scopeStyles(this._styles);
      },

      // search for extra style modules via `styleModules`
      _prepareStyles: function() {
        var cssText = '', m$ = this.styleModules;
        if (m$) {
          for (var i=0, l=m$.length, m; (i<l) && (m=m$[i]); i++) {
            cssText += this._cssFromModule(m);
          }
        }
        cssText += this._cssFromModule(this.is);
        var styles = [];
        if (cssText) {
          var s = document.createElement('style');
          s.textContent = cssText;  
          styles.push(s);
        }
        return styles;
      },

      // returns cssText of styles in a given module; also un-applies any
      // styles that apply to the document.
      _cssFromModule: function(moduleId) {
        var m = Polymer.DomModule.import(moduleId);
        if (m && !m._cssText) {
          var cssText = '';
          var e$ = Array.prototype.slice.call(m.querySelectorAll('style'));
          this._unapplyStyles(e$);
          e$ = e$.concat(Array.prototype.map.call(
            m.querySelectorAll(REMOTE_SHEET_SELECTOR), function(l) {
              return l.import.body;
            }));
          m._cssText = this._cssFromStyles(e$);
        }
        return m && m._cssText || '';
      },

      _cssFromStyles: function(styles) {
        var cssText = '';
        for (var i=0, l=styles.length, s; (i<l) && (s = styles[i]); i++) {
          if (s && s.textContent) {
            cssText += 
              Polymer.ResolveUrl.resolveCss(s.textContent, s.ownerDocument);
          }
        }
        return cssText;
      },

      _unapplyStyles: function(styles) {
        for (var i=0, l=styles.length, s; (i<l) && (s = styles[i]); i++) {
          s = s.__appliedElement || s;
          s.parentNode.removeChild(s);
        }
      },

      _scopeStyles: function(styles) {
        for (var i=0, l=styles.length, s; (i<l) && (s=styles[i]); i++) {
          // transform style if necessary and place in correct place
          if (nativeShadow) {
            if (this._template) {
              this._template.content.appendChild(s);
            }
          } else {
            var rules = this._rulesForStyle(s);
            Polymer.StyleUtil.applyCss(
              Polymer.StyleTransformer.css(rules, this.is, this.extends, 
              null, this._scopeCssViaAttr), 
              this.is, null, true);
          }
        }
      },

      _rulesForStyle: function(style) {
        if (!style.__cssRules) {
          style.__cssRules = Polymer.StyleUtil.parser.parse(style.textContent);
        }
        return style.__cssRules;
      },

      // instance-y
      _stampTemplate: function() {
        if (this._encapsulateStyle) {
          Polymer.StyleTransformer.host(this, this.is);
        }
        baseStampTemplate.call(this);
      },

      // add scoping class whenever an element is added to localDOM
      _elementAdd: function(node) {
        if (this._encapsulateStyle && !node.__styleScoped) {
          Polymer.StyleTransformer.dom(node, this.is, this._scopeCssViaAttr);
        }
      },

      // remove scoping class whenever an element is removed from localDOM
      _elementRemove: function(node) {
        if (this._encapsulateStyle) {
          Polymer.StyleTransformer.dom(node, this.is, this._scopeCssViaAttr, true);
        }
      },

      /**
       * Apply style scoping to the specified `container` and all its 
       * descendants. If `shoudlObserve` is true, changes to the container are
       * monitored via mutation observer and scoping is applied.
       */
      scopeSubtree: function(container, shouldObserve) {
        if (nativeShadow) {
          return;
        }
        var self = this;
        var scopify = function(node) {
          if (node.nodeType === Node.ELEMENT_NODE) {
            node.className = self._scopeElementClass(node, node.className);
            var n$ = node.querySelectorAll('*');
            Array.prototype.forEach.call(n$, function(n) {
              n.className = self._scopeElementClass(n, n.className);
            });
          }
        };
        scopify(container);
        if (shouldObserve) {
          var mo = new MutationObserver(function(mxns) {
            mxns.forEach(function(m) {
              if (m.addedNodes) {
                for (var i=0; i < m.addedNodes.length; i++) {
                  scopify(m.addedNodes[i]);
                }
              }
            });
          });
          mo.observe(container, {childList: true, subtree: true});
          return mo;
        }
      }

    });

    var REMOTE_SHEET_SELECTOR = 'link[rel=import][type~=css]';

  })();


;

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

;
  (function() {

    var baseAttachedCallback = Polymer.Base.attachedCallback;
    var baseSerializeValueToAttribute = Polymer.Base.serializeValueToAttribute;

    var nativeShadow = Polymer.Settings.useNativeShadow;

    // TODO(sorvell): consider if calculating properties and applying
    // styles with properties should be separate modules.
    Polymer.Base._addFeature({

      attachedCallback: function() {
        baseAttachedCallback.call(this);
        if (!this._xScopeSelector) {
          this._updateOwnStyles();
        }
      },

      _updateOwnStyles: function() {
        if (this.enableCustomStyleProperties) {
          this._styleProperties = this._computeStyleProperties();
          this._applyStyleProperties(this._styleProperties);
        }
      },

      _computeStyleProperties: function() {
        var props = {};
        this.simpleMixin(props, this._computeStylePropertiesFromHost());
        this.simpleMixin(props, this._computeOwnStyleProperties());
        this._reifyCustomProperties(props);
        return props;
      },

      _computeStylePropertiesFromHost: function() {
        // TODO(sorvell): experimental feature, global defaults!
        var props = {}, styles = [Polymer.StyleDefaults.defaultSheet];
        var host = this.domHost;
        if (host) {
          // enable finding styles in hosts without `enableStyleCustomProperties`
          if (!host._styleProperties) {
            host._styleProperties = host._computeStyleProperties();
          }
          props = Object.create(host._styleProperties);
          styles = host._styles;
        }
        this.simpleMixin(props,
          this._customPropertiesFromStyles(styles, host));
        return props;

      },

      _computeOwnStyleProperties: function() {
        var props = {};
        this.simpleMixin(props, this._customPropertiesFromStyles(this._styles));
        if (this.styleProperties) {
          for (var i in this.styleProperties) {
            props[i] = this.styleProperties[i];
          }
        }
        return props;
      },

      _customPropertiesFromStyles: function(styles, hostNode) {
        var props = {};
        var p = this._customPropertiesFromRule.bind(this, props, hostNode);
        if (styles) {
          for (var i=0, l=styles.length, s; (i<l) && (s=styles[i]); i++) {
            Polymer.StyleUtil.forEachStyleRule(this._rulesForStyle(s), p);
          }
        }
        return props;
      },

      // test if a rule matches the given node and if so,
      // collect any custom properties
      // TODO(sorvell): support custom variable assignment within mixins
      _customPropertiesFromRule: function(props, hostNode, rule) {
        hostNode = hostNode || this;
        // TODO(sorvell): file crbug, ':host' does not match element.
        if (this.elementMatches(rule.selector) ||
          ((hostNode === this) && (rule.selector === ':host'))) {
          // --g: var(--b); or --g: 5;
          this._collectPropertiesFromRule(rule, CUSTOM_VAR_ASSIGN, props);
          // --g: { ... }
          this._collectPropertiesFromRule(rule, CUSTOM_MIXIN_ASSIGN, props);
        }
      },

      // given a rule and rx that matches key and value, set key in properties
      // to value
      _collectPropertiesFromRule: function(rule, rx, properties) {
        var m;
        while (m = rx.exec(rule.cssText)) {
          properties[m[1]] = m[2].trim();
        }
      },

      _reifyCustomProperties: function(props) {
        for (var i in props) {
          props[i] = this._valueForCustomProperty(props[i], props);
        }
      },

      _valueForCustomProperty: function(property, props) {
        var cv;
        while ((typeof property === 'string') &&
          (cv = property.match(CUSTOM_VAR_VALUE))) {
          property = props[cv[1]];
        }
        return property;
      },

      // apply styles
      _applyStyleProperties: function(bag) {
        var s$ = this._styles;
        if (s$) {
          var style = styleFromCache(this.is, bag, s$);
          var old = this._xScopeSelector;
          this._ensureScopeSelector(style ? style._scope : null);
          if (!style) {
            var cssText = this._generateCustomStyleCss(bag, s$);
            style = cssText ? this._applyCustomCss(cssText) : {};
            cacheStyle(this.is, style, this._xScopeSelector,
              this._styleProperties, s$);
          } else if (nativeShadow) {
            this._applyCustomCss(style.textContent);
          }
          if (style.textContent || old /*&& !nativeShadow*/) {
            this._applyXScopeSelector(this._xScopeSelector, old);
          }
        }
      },

      _applyXScopeSelector: function(selector, old) {
        var c = this._scopeCssViaAttr ? this.getAttribute(SCOPE_NAME) :
          this.className;
        v = old ? c.replace(old, selector) :
          (c ? c + ' ' : '') + XSCOPE_NAME + ' ' + selector;
        if (c !== v) {
          if (this._scopeCssViaAttr) {
            this.setAttribute(SCOPE_NAME, v);
          } else {
            this.className = v;
          }
        }
      },

      _generateCustomStyleCss: function(properties, styles) {
        var b = this._applyPropertiesToRule.bind(this, properties);
        var cssText = '';
        // TODO(sorvell): don't redo parsing work each time as below;
        // instead create a sheet with just custom properties
        for (var i=0, l=styles.length, s; (i<l) && (s=styles[i]); i++) {
          cssText += this._transformCss(s.textContent, b) + '\n\n';
        }
        return cssText.trim();
      },

      _transformCss: function(cssText, callback) {
        return nativeShadow ?
          Polymer.StyleUtil.toCssText(cssText, callback) :
          Polymer.StyleTransformer.css(cssText, this.is, this.extends, callback,
            this._scopeCssViaAttr);
      },

      _xScopeCount: 0,

      _ensureScopeSelector: function(selector) {
        selector = selector || (this.is + '-' +
          (Object.getPrototypeOf(this)._xScopeCount++));
        this._xScopeSelector = selector;
      },

      _applyCustomCss: function(cssText) {
        if (this._customStyle) {
          this._customStyle.textContent = cssText;
        } else if (cssText) {
          this._customStyle = Polymer.StyleUtil.applyCss(cssText,
            this._xScopeSelector,
            nativeShadow ? this.root : null);
        }
        return this._customStyle;
      },

      _applyPropertiesToRule: function(properties, rule) {
        if (!nativeShadow) {
          this._scopifyRule(rule);
        }
        if (rule.cssText.match(CUSTOM_RULE_RX)) {
          rule.cssText = this._applyPropertiesToText(rule.cssText, properties);
        } else {
          rule.cssText = '';
        }
        //console.log(rule.cssText);
      },

      _applyPropertiesToText: function(cssText, props) {
        var output = '';
        var m, v;
        // e.g. color: var(--color);
        while (m = CUSTOM_VAR_USE.exec(cssText)) {
          v = props[m[2]];
          if (v) {
            output += '\t' + m[1].trim() + ': ' + this._propertyToCss(v);
          }
        }
        // e.g. @mixin(--stuff);
        while (m = CUSTOM_MIXIN_USE.exec(cssText)) {
          v = m[1];
          if (v) {
            var parts = v.split(' ');
            for (var i=0, p; i < parts.length; i++) {
              p = props[parts[i].trim()];
              if (p) {
                output += '\t' + this._propertyToCss(p);
              }
            }
          }
        }
        return output;
      },

      _propertyToCss: function(property) {
        var p = property.trim();
        p = p[p.length-1] === ';' ? p : p + ';';
        return p + '\n';
      },

      // Strategy: x scope shim a selector e.g. to scope `.x-foo-42` (via classes):
      // non-host selector: .a.x-foo -> .x-foo-42 .a.x-foo
      // host selector: x-foo.wide -> x-foo.x-foo-42.wide
      _scopifyRule: function(rule) {
        var selector = rule.selector;
        var host = this.is;
        var rx = new RegExp(HOST_SELECTOR_PREFIX + host + HOST_SELECTOR_SUFFIX);
        var parts = selector.split(',');
        var scope = this._scopeCssViaAttr ?
          SCOPE_PREFIX + this._xScopeSelector + SCOPE_SUFFIX :
          '.' + this._xScopeSelector;
        for (var i=0, l=parts.length, p; (i<l) && (p=parts[i]); i++) {
          parts[i] = p.match(rx) ?
            p.replace(host, host + scope) :
            scope + ' ' + p;
        }
        rule.selector = parts.join(',');
      },

      _scopeElementClass: function(element, selector) {
        if (!nativeShadow && !this._scopeCssViaAttr) {
          selector += (selector ? ' ' : '') + SCOPE_NAME + ' ' + this.is +
            (element._xScopeSelector ? ' ' +  XSCOPE_NAME + ' ' +
            element._xScopeSelector : '');
        }
        return selector;
      },

      // override to ensure whenever classes are set, we need to shim them.
      serializeValueToAttribute: function(value, attribute, node) {
        if (attribute === 'class') {
          // host needed to scope styling.
          var host = node === this ?
            Polymer.dom(this).getOwnerRoot() || this.dataHost :
            this;
          if (host) {
            value = host._scopeElementClass(node, value);
          }
        }
        baseSerializeValueToAttribute.call(this, value, attribute, node);
      },

      updateStyles: function() {
        this._updateOwnStyles();
        this._updateRootStyles(this.root);
      },

      updateHostStyles: function() {
        var host = Polymer.dom(this).getOwnerRoot() || this.dataHost;
        if (host) {
          host.updateStyles();
        } else {
          this._updateRootStyles(document);
        }
      },

      _updateRootStyles: function(root) {
        // TODO(sorvell): temporary way to find local dom that needs
        // x-scope styling.
        var scopeSelector = this._scopeCssViaAttr ?
          '[' + SCOPE_NAME + '~=' + XSCOPE_NAME + ']' : '.' + XSCOPE_NAME;
        var c$ = Polymer.dom(root).querySelectorAll(scopeSelector);
        for (var i=0, l= c$.length, c; (i<l) && (c=c$[i]); i++) {
          if (c.updateStyles) {
            c.updateStyles();
          }
        }
      }

    });

    var styleCache = {};
    function cacheStyle(is, style, scope, bag, styles) {
      style._scope = scope;
      style._properties = bag;
      style._styles = styles;
      var s$ = styleCache[is] = styleCache[is] || [];
      s$.push(style);
    }

    function styleFromCache(is, bag, checkStyles) {
      var styles = styleCache[is];
      if (styles) {
        for (var i=0, s; i < styles.length; i++) {
          s = styles[i];
          if (objectsEqual(bag, s._properties) &&
            objectsEqual(checkStyles,  s._styles)) {
            return s;
          }
        }
      }
    }

    function objectsEqual(a, b) {
      for (var i in a) {
        if (a[i] !== b[i]) {
          return false;
        }
      }
      for (var i in b) {
        if (a[i] !== b[i]) {
          return false;
        }
      }
      return true;
    }

    var SCOPE_NAME= Polymer.StyleTransformer.SCOPE_NAME;
    var XSCOPE_NAME = 'x-scope';
    var SCOPE_PREFIX = '[' + SCOPE_NAME + '~=';
    var SCOPE_SUFFIX = ']';
    var HOST_SELECTOR_PREFIX = '(?:^|[^.])';
    var HOST_SELECTOR_SUFFIX = '($|[.:[\\s>+~])';
    var CUSTOM_RULE_RX = /mixin|var/;
    var CUSTOM_VAR_ASSIGN = /(--[^\:;]*?):\s*?([^;{]*?);/g;
    var CUSTOM_MIXIN_ASSIGN = /(--[^\:;]*?):[^{;]*?{([^}]*?)}/g;
    var CUSTOM_VAR_VALUE = /^var\(([^)]*?)\)/;
    var CUSTOM_VAR_USE = /(?:^|[;}\s])([^;{}]*?):[\s]*?var\(([^)]*)?\)/gim;
    var CUSTOM_MIXIN_USE = /mixin\(([^)]*)\)/gim;

  })();

;

  Polymer.Base._addFeature({

    _registerFeatures: function() {
      // identity
      this._prepIs();
      // inheritance
      this._prepExtends();
      // factory
      this._prepConstructor();
      // template
      this._prepTemplate();
      // template markup
      this._prepAnnotations();
      // accessors
      this._prepEffects();
      // shared behaviors
      this._prepBehaviors();
      // accessors part 2
      this._prepBindings();
      // dom encapsulation
      this._prepShady();
    },

    _prepBehavior: function(b) {
      this._addPropertyEffects(b.properties || b.accessors);
      this._addComplexObserverEffects(b.observers);
    },

    _initFeatures: function() {
      // manage local dom
      this._poolContent();
      // manage configuration
      this._setupConfigure();
      // host stack
      this._pushHost();
      // instantiate template
      this._stampTemplate();
      // host stack
      this._popHost();
      // concretize template references
      this._marshalAnnotationReferences();
      // setup debouncers
      this._setupDebouncers();
      // concretize effects on instance
      this._marshalInstanceEffects();
      // acquire instance behaviors
      this._marshalBehaviors();
      // acquire initial instance attribute values
      this._marshalAttributes();
      // top-down initial distribution, configuration, & ready callback
      this._tryReady();
    },

    _marshalBehavior: function(b) {
      // publish attributes to instance
      this._installHostAttributes(b.hostAttributes);
      // establish listeners on instance
      this._listenListeners(b.listeners);
    }

  });


;
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

;

  Polymer({

    is: 'x-autobind',

    extends: 'template',

    _registerFeatures: function() {
      this._prepExtends();
      this._prepConstructor();
    },

    _finishDistribute: function() {
      var parentDom = Polymer.dom(Polymer.dom(this).parentNode);
      parentDom.insertBefore(this.root, this);
    },

    _initFeatures: function() {
      this._template = this;
      this._prepAnnotations();
      this._prepEffects();
      this._prepBehaviors();
      this._prepBindings();
      Polymer.Base._initFeatures.call(this);
    }

  });


;

  Polymer.Templatizer = {

    templatize: function(template) {
      this._templatized = template;
      // TODO(sjmiles): supply _alternate_ content reference missing from root
      // templates (not nested). `_content` exists to provide content sharing
      // for nested templates.
      if (!template._content) {
        template._content = template.content;
      }
      // fast path if template's anonymous class has been memoized
      if (template._content._ctor) {
        this.ctor = template._content._ctor;
        //console.log('Templatizer.templatize: using memoized archetype');
        // forward parent properties to archetype
        this._prepParentProperties(this.ctor.prototype);
        return;
      }
      // `archetype` is the prototype of the anonymous
      // class created by the templatizer
      var archetype = Object.create(Polymer.Base);
      // normally Annotations.parseAnnotations(template) but
      // archetypes do special caching
      this.customPrepAnnotations(archetype, template);

      // setup accessors
      archetype._prepEffects();
      archetype._prepBehaviors();
      archetype._prepBindings();

      // forward parent properties to archetype
      this._prepParentProperties(archetype);

      // boilerplate code
      archetype._notifyPath = this._notifyPathImpl;
      archetype._scopeElementClass = this._scopeElementClassImpl;
      // boilerplate code
      var _constructor = this._constructorImpl;
      var ctor = function TemplateInstance(model, host) {
        _constructor.call(this, model, host);
      };
      // standard references
      ctor.prototype = archetype;
      archetype.constructor = ctor;
      // TODO(sjmiles): constructor cache?
      template._content._ctor = ctor;
      // TODO(sjmiles): choose less general name
      this.ctor = ctor;
    },

    _getRootDataHost: function() {
      return (this.dataHost && this.dataHost._rootDataHost) || this.dataHost;
    },

    _getAllStampedChildren: function(children) {
      children = children || [];
      if (this._getStampedChildren) {
        var c$ = this._getStampedChildren();
        for (var i=0, c; c = c$[i]; i++) {
          children.push(c);
          if (c._getAllStampedChildren) {
            c._getAllStampedChildren(children);
          }
        }
      }
      return children;
    },

    customPrepAnnotations: function(archetype, template) {
      if (template) {
        archetype._template = template;
        var c = template._content;
        if (c) {
          var rootDataHost = archetype._rootDataHost;
          if (rootDataHost) {
            Polymer.Annotations.prepElement =
              rootDataHost._prepElement.bind(rootDataHost);
          }
          archetype._notes = c._notes ||
            Polymer.Annotations.parseAnnotations(template);
          c._notes = archetype._notes;
          Polymer.Annotations.prepElement = null;
          archetype._parentProps = c._parentProps;
        }
        else {
          console.warn('no _content');
        }
      }
      else {
        console.warn('no _template');
      }
    },

    // Sets up accessors on the template to call abstract _forwardParentProp
    // API that should be implemented by Templatizer users to get parent
    // properties to their template instances.  These accessors are memoized
    // on the archetype and copied to instances.
    _prepParentProperties: function(archetype) {
      var parentProps = this._parentProps = archetype._parentProps;
      if (this._forwardParentProp && parentProps) {
        // Prototype setup (memoized on archetype)
        var proto = archetype._parentPropProto;
        if (!proto) {
          proto = archetype._parentPropProto = Object.create(null);
          if (this._templatized != this) {
            // Assumption: if `this` isn't the template being templatized,
            // assume that the template is not a Poylmer.Base, so prep it
            // for binding
            Polymer.Bind.prepareModel(proto);
          }
          // Create accessors for each parent prop that forward the property
          // to template instances through abstract _forwardParentProp API
          // that should be implemented by Templatizer users
          for (var prop in parentProps) {
            var parentProp = '_parent_' + prop;
            var effects = [{
              kind: 'function',
              effect: { function: this._createForwardPropEffector(prop) }
            }];
            Polymer.Bind._createAccessors(proto, parentProp, effects);
          }
        }
        // Instance setup
        if (this._templatized != this) {
          Polymer.Bind.prepareInstance(this._templatized);
          this._templatized._forwardParentProp =
            this._forwardParentProp.bind(this);
        }
        this._extendTemplate(this._templatized, proto);
      }
    },

    _createForwardPropEffector: function(prop) {
      return function(source, value) {
        this._forwardParentProp(prop, value);
      };
    },

    // Similar to Polymer.Base.extend, but retains any previously set instance
    // values (_propertySet back on instance once accessor is installed)
    _extendTemplate: function(template, proto) {
      Object.getOwnPropertyNames(proto).forEach(function(n) {
        var val = template[n];
        var pd = Object.getOwnPropertyDescriptor(proto, n);
        Object.defineProperty(template, n, pd);
        if (val !== undefined) {
          template._propertySet(n, val);
        }
      });
    },

    _notifyPathImpl: function(path, value) {
      var p = path.match(/([^.]*)\.(([^.]*).*)/);
                          // 'root.sub.path'
      var root = p[1];    // 'root'
      var sub = p[3];     // 'sub'
      var subPath = p[2]; // 'sub.path'
      // Notify host of parent.* path/property changes
      var dataHost = this.dataHost;
      if (root == 'parent') {
        if (sub == subPath) {
          dataHost.dataHost[sub] = value;
        } else {
          dataHost.notifyPath('_parent_' + subPath, value);
        }
      }
      // Extension point for Templatizer sub-classes
      if (dataHost._forwardInstancePath) {
        dataHost._forwardInstancePath.call(dataHost, this, root, subPath, value);
      }
    },

    // Overrides Base notify-path module
    _pathEffector: function(path, value, fromAbove) {
      if (this._forwardParentPath) {
        if (path.indexOf('_parent_') === 0) {
          this._forwardParentPath(path.substring(8), value);
        }
      }
      Polymer.Base._pathEffector.apply(this, arguments);
    },

    _constructorImpl: function(model, host) {
      var rootDataHost = host._getRootDataHost();
      if (rootDataHost) {
        this.listen = rootDataHost.listen.bind(rootDataHost);
        this._rootDataHost = rootDataHost;
      }
      this._setupConfigure(model);
      this._pushHost(host);
      this.root = this.instanceTemplate(this._template);
      this.root.__styleScoped = true;
      this._popHost();
      this._marshalAnnotatedNodes();
      this._marshalInstanceEffects();
      this._marshalAnnotatedListeners();
      this._tryReady();
    },

    _scopeElementClassImpl: function(node, value) {
      var host = this._rootDataHost;
      if (host) {
        return host._scopeElementClass(node, value);
      }
    },

    stamp: function(model) {
      model = model || {};
      if (this._parentProps) {
        // TODO(kschaaf): Maybe this is okay
        // model.parent = this.dataHost;
        model.parent = model.parent || {};
        for (var prop in this._parentProps) {
          model.parent[prop] = this['_parent_' + prop];
        }
      }
      return new this.ctor(model, this);
    }

    // TODO(sorvell): note, using the template as host is ~5-10% faster if
    // elements have no default values.
    // _constructorImpl: function(model, host) {
    //   this._setupConfigure(model);
    //   host._beginHost();
    //   this.root = this.instanceTemplate(this._template);
    //   host._popHost();
    //   this._marshalTemplateContent();
    //   this._marshalAnnotatedNodes();
    //   this._marshalInstanceEffects();
    //   this._marshalAnnotatedListeners();
    //   this._ready();
    // },

    // stamp: function(model) {
    //   return new this.ctor(model, this.dataHost);
    // }


  };


;

  /**
   * Creates a pseudo-custom-element that maps property values to bindings
   * in DOM.
   * 
   * `stamp` method creates an instance of the pseudo-element. The instance
   * references a document-fragment containing the stamped and bound dom
   * via it's `root` property. 
   *  
   */
  Polymer({

    is: 'x-template',
    extends: 'template',

    behaviors: [
      Polymer.Templatizer
    ],

    ready: function() {
      this.templatize(this);
    }

  });


;

(function() {

  var callbacks = new WeakMap();
  
  function observe(array, cb) {
    if (Array.observe) {
      var ncb = function(changes) {
        changes = changes.filter(function(o) { return o.type == 'splice'; });
        if (changes.length) {
          cb(changes);
        }
      };
      callbacks.set(cb, ncb);
      Array.observe(array, ncb);
    } else {
      if (!array.__polymerObservable) {
        makeObservable(array);
      }
      callbacks.get(array).push(cb);
    }
  }

  function unobserve(array, cb) {
    if (Array.observe) {
      var ncb = callbacks.get(cb);
      callbacks.delete(cb);
      Array.unobserve(array, ncb);
    } else {
      var cbs = callbacks.get(array);
      var idx = cbs.indexOf(cb);
      if (idx >= 0) {
        cbs.splice(idx, 1);
      }
    }
  }

  function makeObservable(array) {
    var splices = [];
    var debounce;
    var orig = {
      push: array.push,
      pop: array.pop,
      splice: array.splice,
      shift: array.shift,
      unshift: array.unshift,
      sort: array.sort
    };
    var addSplice = function(index, added, removed) {
      splices.push({
        index: index,
        addedCount: added,
        removed: removed,
        object: array,
        type: 'splice'
      });
    };
    callbacks.set(array, []);
    array.push = function() {
      debounce = Polymer.Debounce(debounce, fin);
      addSplice(array.length, 1, []);
      return orig.push.apply(this, arguments);
    };
    array.pop = function() {
      debounce = Polymer.Debounce(debounce, fin);
      addSplice(array.length - 1, 0, array.slice(-1));
      return orig.pop.apply(this, arguments);
    };
    array.splice = function(start, deleteCount) {
      debounce = Polymer.Debounce(debounce, fin);
      addSplice(start, arguments.length - 2, array.slice(start, start + deleteCount));
      return orig.splice.apply(this, arguments);
    };
    array.shift = function() {
      debounce = Polymer.Debounce(debounce, fin);
      addSplice(0, 0, [array[0]]);
      return orig.shift.apply(this, arguments);
    };
    array.unshift = function() {
      debounce = Polymer.Debounce(debounce, fin);
      addSplice(0, 1, []);
      return orig.unshift.apply(this, arguments);
    };
    array.sort = function() {
      debounce = Polymer.Debounce(debounce, fin);
      console.warn('[ArrayObserve]: sort not observable');
      return orig.sort.apply(this, arguments);
    };
    var fin = function() {
      var cbs = callbacks.get(array);
      for (var i=0; i<cbs.length; i++) {
        cbs[i](splices);
      }
      splices = [];
    };
    array.__polymerObservable = true;
  }

  Polymer.ArrayObserve = {
    observe: observe,
    unobserve: unobserve
  };
  
})();


;

  Polymer._collections = new WeakMap();

  Polymer.Collection = function(userArray, noObserve) {
    Polymer._collections.set(userArray, this);
    this.userArray = userArray;
    this.store = userArray.slice();
    this.callbacks = [];
    this.debounce = null;
    this.map = null;
    this.added = [];
    this.removed = [];
    if (!noObserve) {
      Polymer.ArrayObserve.observe(userArray, this.applySplices.bind(this));
      this.initMap();
    }
  };

  Polymer.Collection.prototype = {
    constructor: Polymer.Collection,

    initMap: function() {
      var map = this.map = new WeakMap();
      var s = this.store;
      var u = this.userArray;
      for (var i=0; i<s.length; i++) {
        var v = s[i];
        if (v) {
          switch (typeof v) {
            case 'string':
              v = s[i] = u[i]= new String(v);
              break;
            case 'number':
              v = s[i] = u[i]= new Number(v);
              break;          
            case 'boolean':
              v = s[i] = u[i]= new Boolean(v);
              break;          
          }
        map.set(v, i);
        }
      }
    },

    add: function(item, squelch) {
      var key = this.store.push(item) - 1;
      if (item != null && this.map) {
        this.map.set(item, key);
      }
      if (!squelch) {
        this.added.push(key);
        this.debounce = Polymer.Debounce(this.debounce, this.notify.bind(this));
      }
      return key;
    },

    removeKey: function(key) {
      if (this.map) {
        this.map.delete(this.store[key]);
      }
      delete this.store[key];
      this.removed.push(key);
      this.debounce = Polymer.Debounce(this.debounce, this.notify.bind(this));
    },

    remove: function(item, squelch) {
      var key = this.getKey(item);
      if (item != null && this.map) {
        this.map.delete(item);
      }
      delete this.store[key];
      if (!squelch) {
        this.removed.push(key);
        this.debounce = Polymer.Debounce(this.debounce, this.notify.bind(this));
      }
      return key;
    },

    notify: function(splices) {
      if (!splices) {
        splices = [{
          added: this.added,
          removed: this.removed
        }];
        this.added = [];
        this.removed = [];
      }
      this.callbacks.forEach(function(cb) {
        cb(splices);
      }, this);
    },

    observe: function(callback) {
      this.callbacks.push(callback);
    },

    unobserve: function(callback) {
      this.callbacks.splice(this.callbacks.indexOf(callback), 1);
    },

    getKey: function(item) {
      if (item != null && this.map) {
        return this.map.get(item);
      } else {
        return this.store.indexOf(item);      
      }
    },

    getKeys: function() {
      return Object.keys(this.store);
    },

    setItem: function(key, value) {
      this.store[key] = value;
    },

    getItem: function(key) {
      return this.store[key];
    },

    getItems: function() {
      var items = [], store = this.store;
      for (var key in store) {
        items.push(store[key]);
      }
      return items;
    },

    applySplices: function(splices) {
      var map = this.map;
      var keySplices = [];
      for (var i=0; i<splices.length; i++) {
        var j, o, key, s = splices[i];
        // Removed keys
        var removed = [];
        for (j=0; j<s.removed.length; j++) {
          o = s.removed[j];
          key = this.remove(o, true);
          removed.push(key);
        }
        // Added keys
        var added = [];
        for (j=0; j<s.addedCount; j++) {
          o = this.userArray[s.index + j];
          key = this.add(o, true);
          added.push(key);
        }
        // Record splice
        keySplices.push({
          index: s.index,
          removed: removed,
          added: added
        });
      }
      this.notify(keySplices);
    }

  };

  Polymer.Collection.get = function(userArray, noObserve) {
    return Polymer._collections.get(userArray) 
      || new Polymer.Collection(userArray, noObserve);
  };


;

  Polymer({

    is: 'x-repeat',
    extends: 'template',

    properties: {

      /**
       * An array containing items determining how many instances of the template
       * to stamp and that that each template instance should bind to.
       */
      items: {
        type: Array
      },

      /**
       * A function that should determine the sort order of the items.  This
       * property should either be provided as a string, indicating a method
       * name on the element's host, or else be an actual function.  The
       * function should match the sort function passed to `Array.sort`.
       * Using a sort function has no effect on the underlying `items` array.
       */
      sort: {
        type: Function,
        observer: '_sortChanged'
      },

      /**
       * A function that can be used to filter items out of the view.  This
       * property should either be provided as a string, indicating a method
       * name on the element's host, or else be an actual function.  The
       * function should match the sort function passed to `Array.filter`.
       * Using a filter function has no effect on the underlying `items` array.
       */
      filter: {
        type: Function,
        observer: '_filterChanged'
      },

      /**
       * When using a `filter` or `sort` function, the `observe` property
       * should be set to a space-separated list of the names of item
       * sub-fields that should trigger a re-sort or re-filter when changed.
       * These should generally be fields of `item` that the sort or filter
       * function depends on.
       */
      observe: {
        type: String,
        observer: '_observeChanged'
      },

      /**
       * When using a `filter` or `sort` function, the `delay` property
       * determines a debounce time after a change to observed item
       * properties that must pass before the filter or sort is re-run.
       * This is useful in rate-limiting shuffing of the view when
       * item changes may be frequent.
       */
      delay: Number
    },

    behaviors: [
      Polymer.Templatizer
    ],

    observers: [
      '_itemsChanged(items.*)'
    ],

    created: function() {
      this.boundCollectionObserver = this.render.bind(this);
    },

    ready: function() {
      // Templatizing (generating the instance constructor) needs to wait
      // until attached, since it may not have its template content handed
      // back to it until then, following its host template stamping
      if (!this.ctor) {
        this.templatize(this);
      }
    },

    _sortChanged: function() {
      var dataHost = this._getRootDataHost();
      this._sortFn = this.sort && (typeof this.sort == 'function' ?
        this.sort : dataHost[this.sort].bind(this.host));
      if (this.items) {
        this.debounce('render', this.render);
      }
    },

    _filterChanged: function() {
      var dataHost = this._getRootDataHost();
      this._filterFn = this.filter && (typeof this.filter == 'function' ?
        this.filter : dataHost[this.filter].bind(this.host));
      if (this.items) {
        this.debounce('render', this.render);
      }
    },

    _observeChanged: function() {
      this._observePaths = this.observe &&
        this.observe.replace('.*', '.').split(' ');
    },

    _itemsChanged: function(change) {
      if (change.path == 'items') {
        this._unobserveCollection();
        if (change.value) {
          this._observeCollection(change.value);
          this.debounce('render', this.render);
        }
      } else {
        this._forwardItemPath(change.path, change.value);
        this._checkObservedPaths(change.path);
      }
    },

    _checkObservedPaths: function(path) {
      if (this._observePaths && path.indexOf('items.') === 0) {
        path = path.substring(path.indexOf('.', 6) + 1);
        var paths = this._observePaths;
        for (var i=0; i<paths.length; i++) {
          if (path.indexOf(paths[i]) === 0) {
            this.debounce('render', this.render, this.delay);
            return;
          }
        }
      }
    },

    _observeCollection: function(items) {
      this.collection = Array.isArray(items) ? Polymer.Collection.get(items) : items;
      this.collection.observe(this.boundCollectionObserver);
    },

    _unobserveCollection: function() {
      if (this.collection) {
        this.collection.unobserve(this.boundCollectionObserver);
      }
    },

    render: function(splices) {
      this.flushDebouncer('render');
      var c = this.collection;
      if (splices) {
        if (this._sortFn || splices[0].index == null) {
          this._applySplicesViewSort(splices);
        } else {
          this._applySplicesArraySort(splices);
        }
      } else {
        this._sortAndFilter();
      }
      var rowForKey = this._rowForKey = {};
      var keys = this._orderedKeys;
      // Assign items and keys
      this.rows = this.rows || [];
      for (var i=0; i<keys.length; i++) {
        var key = keys[i];
        var item = c.getItem(key);
        var row = this.rows[i];
        rowForKey[key] = i;
        if (!row) {
          this.rows.push(row = this._insertRow(i, null, item));
        }
        row.item = item;
        row.key = key;
        row.index = i;
      }
      // Remove extra
      for (; i<this.rows.length; i++) {
        this._detachRow(i);
      }
      this.rows.splice(keys.length, this.rows.length-keys.length);
    },

    _sortAndFilter: function() {
      var c = this.collection;
      this._orderedKeys = c.getKeys();
      // Filter
      if (this._filterFn) {
        this._orderedKeys = this._orderedKeys.filter(function(a) {
          return this._filterFn(c.getItem(a));
        }, this);
      }
      // Sort
      if (this._sortFn) {
        this._orderedKeys.sort(function(a, b) {
          return this._sortFn(c.getItem(a), c.getItem(b));
        }.bind(this));
      }
    },

    _keySort: function(a, b) {
      return this.collection.getKey(a) - this.collection.getKey(b);
    },

    _applySplicesViewSort: function(splices) {
      var c = this.collection;
      var keys = this._orderedKeys;
      var rows = this.rows;
      var removedRows = [];
      var addedKeys = [];
      var pool = [];
      var sortFn = this._sortFn || this._keySort.bind(this);
      splices.forEach(function(s) {
        // Collect all removed row idx's
        for (var i=0; i<s.removed.length; i++) {
          var idx = this._rowForKey[s.removed[i]];
          if (idx != null) {
            removedRows.push(idx);
          }
        }
        // Collect all added keys
        for (i=0; i<s.added.length; i++) {
          addedKeys.push(s.added[i]);
        }
      }, this);
      if (removedRows.length) {
        // Sort removed rows idx's
        removedRows.sort();
        // Remove keys and pool rows (backwards, so we don't invalidate rowForKey)
        for (i=removedRows.length-1; i>=0 ; i--) {
          var idx = removedRows[i];
          pool.push(this._detachRow(idx));
          rows.splice(idx, 1);
          keys.splice(idx, 1);
        }
      }
      if (addedKeys.length) {
        // Filter added keys
        if (this._filterFn) {
          addedKeys = addedKeys.filter(function(a) {
            return this._filterFn(c.getItem(a));
          }, this);
        }
        // Sort added keys
        addedKeys.sort(function(a, b) {
          return this.sortFn(c.getItem(a), c.getItem(b));
        }, this);
        // Insert new rows using sort (from pool or newly created)
        var start = 0;
        for (i=0; i<addedKeys.length; i++) {
          start = this._insertRowIntoViewSort(start, addedKeys[i], pool);
        }
      }
    },

    _insertRowIntoViewSort: function(start, key, pool) {
      var c = this.collection;
      var item = c.getItem(key);
      var end = this.rows.length - 1;
      var idx = -1;
      var sortFn = this._sortFn || this._keySort.bind(this);
      // Binary search for insertion point
      while (start <= end) {
        var mid = (start + end) >> 1;
        var midKey = this._orderedKeys[mid];
        var cmp = sortFn(c.getItem(midKey), item);
        if (cmp < 0) {
          start = mid + 1;
        } else if (cmp > 0) {
          end = mid - 1;
        } else {
          idx = mid;
          break;
        }
      }
      if (idx < 0) {
        idx = end + 1;
      }
      // Insert key & row at insertion point
      this._orderedKeys.splice(idx, 0, key);
      this.rows.splice(idx, 0, this._insertRow(idx, pool));
      return idx;
    },

    _applySplicesArraySort: function(splices) {
      var keys = this._orderedKeys;
      var pool = [];
      splices.forEach(function(s) {
        // Remove & pool rows first, to ensure we can fully reuse removed rows
        for (var i=0; i<s.removed.length; i++) {
          pool.push(this._detachRow(s.index + i));
        }
        this.rows.splice(s.index, s.removed.length);
      }, this);
      var c = this.collection;
      var filterDelta = 0;
      splices.forEach(function(s) {
        // Filter added keys
        var addedKeys = s.added;
        if (this._filterFn) {
          addedKeys = addedKeys.filter(function(a) {
            return this._filterFn(c.getItem(a));
          }, this);
          filterDelta += (s.added.length - addedKeys.length);
        }
        var idx = s.index - filterDelta;
        // Apply splices to keys
        var args = [idx, s.removed.length].concat(addedKeys);
        keys.splice.apply(keys, args);
        // Insert new rows (from pool or newly created)
        var addedRows = [];
        for (i=0; i<s.added.length; i++) {
          addedRows.push(this._insertRow(idx + i, pool));
        }
        args = [s.index, 0].concat(addedRows);
        this.rows.splice.apply(this.rows, args);
      }, this);
    },

    _detachRow: function(idx) {
      var row = this.rows[idx];
      var parentNode = Polymer.dom(this).parentNode;
      for (var i=0; i<row._children.length; i++) {
        var el = row._children[i];
        Polymer.dom(row.root).appendChild(el);
      }
      return row;
    },

    _insertRow: function(idx, pool, item) {
      var row = (pool && pool.pop()) || this._generateRow(idx, item);
      var beforeRow = this.rows[idx];
      var beforeNode = beforeRow ? beforeRow._children[0] : this;
      var parentNode = Polymer.dom(this).parentNode;
      Polymer.dom(parentNode).insertBefore(row.root, beforeNode);
      return row;
    },

    _generateRow: function(idx, item) {
      var row = this.stamp({
        index: idx,
        key: this.collection.getKey(item),
        item: item
      });
      // each row is a document fragment which is lost when we appendChild,
      // so we have to track each child individually
      var children = [];
      for (var n = row.root.firstChild; n; n=n.nextSibling) {
        children.push(n);
        n._templateInstance = row;
      }
      // Since archetype overrides Base/HTMLElement, Safari complains
      // when accessing `children`
      row._children = children;
      return row;
    },

    // Implements extension point from Templatizer mixin
    _getStampedChildren: function() {
      var children = [];
      if (this.rows) {
        for (var i=0; i<this.rows.length; i++) {
          var c = this.rows[i]._children;
          for (var j=0; j<c.length; j++)
          children.push(c[j]);
        }
      }
      return children;
    },

    // Implements extension point from Templatizer
    // Called as a side effect of a template instance path change, responsible
    // for notifying items.<key-for-row>.<path> change up to host
    _forwardInstancePath: function(row, root, subPath, value) {
      if (root == 'item') {
        this.notifyPath('items.' + row.key + '.' + subPath, value);
      }
    },

    // Implements extension point from Templatizer mixin
    // Called as side-effect of a host property change, responsible for
    // notifying parent.<prop> path change on each row
    _forwardParentProp: function(prop, value) {
      if (this.rows) {
        this.rows.forEach(function(row) {
          row.parent[prop] = value;
          row.notifyPath('parent.' + prop, value, true);
        }, this);
      }
    },

    // Implements extension point from Templatizer
    // Called as side-effect of a host path change, responsible for
    // notifying parent.<path> path change on each row
    _forwardParentPath: function(path, value) {
      if (this.rows) {
        this.rows.forEach(function(row) {
          row.notifyPath('parent.' + path, value, true);
        }, this);
      }
    },

    // Called as a side effect of a host items.<key>.<path> path change,
    // responsible for notifying item.<path> changes to row for key
    _forwardItemPath: function(path, value) {
      if (this._rowForKey) {
        // 'items.'.length == 6
        var dot = path.indexOf('.', 6);
        var key = path.substring(6, dot < 0 ? path.length : dot);
        var idx = this._rowForKey[key];
        var row = this.rows[idx];
        if (row) {
          if (dot >= 0) {
            path = 'item.' + path.substring(dot+1);
            row.notifyPath(path, value, true);
          } else {
            row.item = value;
          }
        }
      }
    },

    _instanceForElement: function(el) {
      while (el && !el._templateInstance) {
        el = el.parentNode;
      }
      return el && el._templateInstance;
    },

    /**
     * Returns the item associated with a given element stamped by
     * this `x-repeat`.
     */
    itemForElement: function(el) {
      var instance = this._instanceForElement(el);
      return instance && instance.item;
    },

    /**
     * Returns the `Polymer.Collection` key associated with a given
     * element stamped by this `x-repeat`.
     */
    keyForElement: function(el) {
      var instance = this._instanceForElement(el);
      return instance && instance.key;
    },

    /**
     * Returns the index in `items` associated with a given element
     * stamped by this `x-repeat`.
     */
    indexForElement: function(el) {
      var instance = this._instanceForElement(el);
      return this.rows.indexOf(instance);
    }

  });



;

  Polymer({
    is: 'x-array-selector',
    
    properties: {

      /**
       * An array containing items from which selection will be made.
       */
      items: {
        type: Array,
        observer: '_itemsChanged'
      },

      /**
       * When `multi` is true, this is an array that contains any selected.
       * When `multi` is false, this is the currently selected item, or `null`
       * if no item is selected.
       */
      selected: {
        type: Object,
        notify: true
      },

      /**
       * When `true`, calling `select` on an item that is already selected
       * will deselect the item.
       */
      toggle: Boolean,

      /**
       * When `true`, multiple items may be selected at once (in this case,
       * `selected` is an array of currently selected items).  When `false`,
       * only one item may be selected at a time.
       */
      multi: Boolean
    },
    
    _itemsChanged: function() {
      // Unbind previous selection
      if (Array.isArray(this.selected)) {
        for (var i=0; i<this.selected.length; i++) {
          this.unbindPaths('selected.' + i);
        }
      } else {
        this.unbindPaths('selected');
      }
      // Initialize selection
      if (this.multi) {
        this.selected = [];
      } else {
        this.selected = null;
      }
    },

    /**
     * Deselects the given item if it is already selected.
     */
    deselect: function(item) {
      if (this.multi) {
        var scol = Polymer.Collection.get(this.selected);
        // var skey = scol.getKey(item);
        // if (skey >= 0) {
        var sidx = this.selected.indexOf(item);
        if (sidx >= 0) {
          var skey = scol.getKey(item);
          this.selected.splice(sidx, 1);
          // scol.remove(item);
          this.unbindPaths('selected.' + skey);
          return true;
        }
      } else {
        this.selected = null;
        this.unbindPaths('selected');
      }
    },

    /**
     * Selects the given item.  When `toggle` is true, this will automatically
     * deselect the item if already selected.
     */
    select: function(item) {
      var icol = Polymer.Collection.get(this.items);
      var key = icol.getKey(item);
      if (this.multi) {
        // var sidx = this.selected.indexOf(item);
        // if (sidx < 0) {
        var scol = Polymer.Collection.get(this.selected);
        var skey = scol.getKey(item);
        if (skey >= 0) {
          this.deselect(item);
        } else if (this.toggle) {
          this.selected.push(item);
          // this.bindPaths('selected.' + sidx, 'items.' + skey);
          // skey = Polymer.Collection.get(this.selected).add(item);
          this.async(function() {
            skey = scol.getKey(item);
            this.bindPaths('selected.' + skey, 'items.' + key);
          });
        }
      } else {
        if (this.toggle && item == this.selected) {
          this.deselect();
        } else {
          this.bindPaths('selected', 'items.' + key);
          this.selected = item;
        }
      }
    }

  });


;

  /**
   * Stamps the template iff the `if` property is truthy.
   *
   * When `if` becomes falsey, the stamped content is hidden but not
   * removed from dom. When `if` subsequently becomes truthy again, the content
   * is simply re-shown. This approach is used due to its favorable performance
   * characteristics: the expense of creating template content is paid only 
   * once and lazily.
   *
   * Set the `restamp` property to true to force the stamped content to be
   * created / destroyed when the `if` condition changes.
   */
  Polymer({

    is: 'x-if',
    extends: 'template',

    properties: {

      'if': {
        type: Boolean,
        value: false
      },

      restamp: {
        type: Boolean,
        value: false
      }

    },

    behaviors: [
      Polymer.Templatizer
    ],

    observers: [
      'render(if, restamp)'
    ],

    render: function() {
      this.debounce('render', function() {
        if (this.if) {
          if (!this.ctor) {
            this._wrapTextNodes(this._content);
            this.templatize(this);
          }
          this._ensureInstance();
        } else if (this.restamp) {
          this._teardownInstance();
        }
        if (!this.restamp && this._instance) {
          this._showHideInstance(this.if);
        }
      });
    },

    _ensureInstance: function() {
      if (!this._instance) {
        // TODO(sorvell): pickup stamping logic from x-repeat
        this._instance = this.stamp();
        var root = this._instance.root;
        this._instance._children = Array.prototype.slice.call(root.childNodes);
        // TODO(sorvell): this incantation needs to be simpler.
        var parent = Polymer.dom(Polymer.dom(this).parentNode);
        parent.insertBefore(root, this);
      }
    },

    _teardownInstance: function() {
      if (this._instance) {
        var parent = Polymer.dom(Polymer.dom(this).parentNode);
        this._instance._children.forEach(function(n) {
          parent.removeChild(n);
        });
        this._instance = null;
      }
    },

    _wrapTextNodes: function(root) {
      // wrap text nodes in span so they can be hidden.
      for (var n = root.firstChild; n; n=n.nextSibling) {
        if (n.nodeType === Node.TEXT_NODE) {
          var s = document.createElement('span');
          root.insertBefore(s, n);
          s.appendChild(n);
          n = s;
        }
      }
    },

    // Implements extension point from Templatizer mixin
    _getStampedChildren: function() {
      return this._instance._children;
    },

    _showHideInstance: function(showing) {
      this._getAllStampedChildren().forEach(function(n) {
        if (n.setAttribute) {
          this.serializeValueToAttribute(!showing, 'hidden', n);
        }
      }, this);
    },

    // Implements extension point from Templatizer mixin
    // Called as side-effect of a host property change, responsible for
    // notifying parent.<prop> path change on instance
    _forwardParentProp: function(prop, value) {
      if (this._instance) {
        this._instance.parent[prop] = value;
        this._instance.notifyPath('parent.' + prop, value, true);
      }
    },

    // Implements extension point from Templatizer
    // Called as side-effect of a host path change, responsible for
    // notifying parent.<path> path change on each row
    _forwardParentPath: function(path, value) {
      if (this._instance) {
        this._instance.notifyPath('parent.' + path, value, true);
      }
    }

  });

