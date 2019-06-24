(function webpackUniversalModuleDefinition(root, factory) {
	if(typeof exports === 'object' && typeof module === 'object')
		module.exports = factory();
	else if(typeof define === 'function' && define.amd)
		define([], factory);
	else if(typeof exports === 'object')
		exports["WebIDL2"] = factory();
	else
		root["WebIDL2"] = factory();
})(this, function() {
return /******/ (function(modules) { // webpackBootstrap
/******/ 	// The module cache
/******/ 	var installedModules = {};
/******/
/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {
/******/
/******/ 		// Check if module is in cache
/******/ 		if(installedModules[moduleId]) {
/******/ 			return installedModules[moduleId].exports;
/******/ 		}
/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = installedModules[moduleId] = {
/******/ 			i: moduleId,
/******/ 			l: false,
/******/ 			exports: {}
/******/ 		};
/******/
/******/ 		// Execute the module function
/******/ 		modules[moduleId].call(module.exports, module, module.exports, __webpack_require__);
/******/
/******/ 		// Flag the module as loaded
/******/ 		module.l = true;
/******/
/******/ 		// Return the exports of the module
/******/ 		return module.exports;
/******/ 	}
/******/
/******/
/******/ 	// expose the modules object (__webpack_modules__)
/******/ 	__webpack_require__.m = modules;
/******/
/******/ 	// expose the module cache
/******/ 	__webpack_require__.c = installedModules;
/******/
/******/ 	// define getter function for harmony exports
/******/ 	__webpack_require__.d = function(exports, name, getter) {
/******/ 		if(!__webpack_require__.o(exports, name)) {
/******/ 			Object.defineProperty(exports, name, { enumerable: true, get: getter });
/******/ 		}
/******/ 	};
/******/
/******/ 	// define __esModule on exports
/******/ 	__webpack_require__.r = function(exports) {
/******/ 		if(typeof Symbol !== 'undefined' && Symbol.toStringTag) {
/******/ 			Object.defineProperty(exports, Symbol.toStringTag, { value: 'Module' });
/******/ 		}
/******/ 		Object.defineProperty(exports, '__esModule', { value: true });
/******/ 	};
/******/
/******/ 	// create a fake namespace object
/******/ 	// mode & 1: value is a module id, require it
/******/ 	// mode & 2: merge all properties of value into the ns
/******/ 	// mode & 4: return value when already ns object
/******/ 	// mode & 8|1: behave like require
/******/ 	__webpack_require__.t = function(value, mode) {
/******/ 		if(mode & 1) value = __webpack_require__(value);
/******/ 		if(mode & 8) return value;
/******/ 		if((mode & 4) && typeof value === 'object' && value && value.__esModule) return value;
/******/ 		var ns = Object.create(null);
/******/ 		__webpack_require__.r(ns);
/******/ 		Object.defineProperty(ns, 'default', { enumerable: true, value: value });
/******/ 		if(mode & 2 && typeof value != 'string') for(var key in value) __webpack_require__.d(ns, key, function(key) { return value[key]; }.bind(null, key));
/******/ 		return ns;
/******/ 	};
/******/
/******/ 	// getDefaultExport function for compatibility with non-harmony modules
/******/ 	__webpack_require__.n = function(module) {
/******/ 		var getter = module && module.__esModule ?
/******/ 			function getDefault() { return module['default']; } :
/******/ 			function getModuleExports() { return module; };
/******/ 		__webpack_require__.d(getter, 'a', getter);
/******/ 		return getter;
/******/ 	};
/******/
/******/ 	// Object.prototype.hasOwnProperty.call
/******/ 	__webpack_require__.o = function(object, property) { return Object.prototype.hasOwnProperty.call(object, property); };
/******/
/******/ 	// __webpack_public_path__
/******/ 	__webpack_require__.p = "";
/******/
/******/
/******/ 	// Load entry module and return exports
/******/ 	return __webpack_require__(__webpack_require__.s = 0);
/******/ })
/************************************************************************/
/******/ ([
/* 0 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony import */ var _lib_webidl2_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(1);
/* harmony reexport (safe) */ __webpack_require__.d(__webpack_exports__, "parse", function() { return _lib_webidl2_js__WEBPACK_IMPORTED_MODULE_0__["parse"]; });

/* harmony import */ var _lib_writer_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(11);
/* harmony reexport (safe) */ __webpack_require__.d(__webpack_exports__, "write", function() { return _lib_writer_js__WEBPACK_IMPORTED_MODULE_1__["write"]; });

/* harmony import */ var _lib_validator_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(12);
/* harmony reexport (safe) */ __webpack_require__.d(__webpack_exports__, "validate", function() { return _lib_validator_js__WEBPACK_IMPORTED_MODULE_2__["validate"]; });






/***/ }),
/* 1 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "parse", function() { return parse; });
/* harmony import */ var _productions_helpers_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(2);
/* harmony import */ var _tokeniser_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(3);
/* harmony import */ var _productions_array_base_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(5);
/* harmony import */ var _productions_base_js__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(6);
/* harmony import */ var _productions_token_js__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(7);
/* harmony import */ var _productions_default_js__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(8);
/* harmony import */ var _productions_enum_js__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(9);
/* harmony import */ var _productions_includes_js__WEBPACK_IMPORTED_MODULE_7__ = __webpack_require__(10);











/**
 * @param {Tokeniser} tokeniser
 * @param {object} options
 * @param {boolean} [options.concrete]
 */
function parseByTokens(tokeniser, options) {
  const source = tokeniser.source;

  const DECIMAL = "decimal";
  const INT = "integer";
  const ID = "identifier";
  const STR = "string";

  function error(str) {
    tokeniser.error(str);
  }

  function probe(type) {
    return tokeniser.probe(type);
  }

  function consume(...candidates) {
    return tokeniser.consume(...candidates);
  }

  function unconsume(position) {
    return tokeniser.unconsume(position);
  }

  function integer_type() {
    const prefix = consume("unsigned");
    const base = consume("short", "long");
    if (base) {
      const postfix = consume("long");
      return new Type({ source, tokens: { prefix, base, postfix } });
    }
    if (prefix) error("Failed to parse integer type");
  }

  function float_type() {
    const prefix = consume("unrestricted");
    const base = consume("float", "double");
    if (base) {
      return new Type({ source, tokens: { prefix, base } });
    }
    if (prefix) error("Failed to parse float type");
  }

  function primitive_type() {
    const num_type = integer_type() || float_type();
    if (num_type) return num_type;
    const base = consume("boolean", "byte", "octet");
    if (base) {
      return new Type({ source, tokens: { base } });
    }
  }

  function type_suffix(obj) {
    const nullable = consume("?");
    if (nullable) {
      obj.tokens.nullable = nullable;
    }
    if (probe("?")) error("Can't nullable more than once");
  }

  class Type extends _productions_base_js__WEBPACK_IMPORTED_MODULE_3__["Base"] {
    constructor({ source, tokens }) {
      super({ source, tokens });
      Object.defineProperty(this, "subtype", { value: [] });
      this.extAttrs = [];
    }

    get generic() {
      return "";
    }
    get nullable() {
      return !!this.tokens.nullable;
    }
    get union() {
      return false;
    }
    get idlType() {
      if (this.subtype.length) {
        return this.subtype;
      }
      // Adding prefixes/postfixes for "unrestricted float", etc.
      const name = [
        this.tokens.prefix,
        this.tokens.base,
        this.tokens.postfix
      ].filter(t => t).map(t => t.value).join(" ");
      return Object(_productions_helpers_js__WEBPACK_IMPORTED_MODULE_0__["unescape"])(name);
    }
  }

  class GenericType extends Type {
    static parse(typeName) {
      const base = consume("FrozenArray", "Promise", "sequence", "record");
      if (!base) {
        return;
      }
      const ret = new GenericType({ source, tokens: { base } });
      ret.tokens.open = consume("<") || error(`No opening bracket after ${base.type}`);
      switch (base.type) {
        case "Promise": {
          if (probe("[")) error("Promise type cannot have extended attribute");
          const subtype = return_type(typeName) || error("Missing Promise subtype");
          ret.subtype.push(subtype);
          break;
        }
        case "sequence":
        case "FrozenArray": {
          const subtype = type_with_extended_attributes(typeName) || error(`Missing ${base.type} subtype`);
          ret.subtype.push(subtype);
          break;
        }
        case "record": {
          if (probe("[")) error("Record key cannot have extended attribute");
          const keyType = consume(..._tokeniser_js__WEBPACK_IMPORTED_MODULE_1__["stringTypes"]) || error(`Record key must be one of: ${_tokeniser_js__WEBPACK_IMPORTED_MODULE_1__["stringTypes"].join(", ")}`);
          const keyIdlType = new Type({ source, tokens: { base: keyType }});
          keyIdlType.tokens.separator = consume(",") || error("Missing comma after record key type");
          keyIdlType.type = typeName;
          const valueType = type_with_extended_attributes(typeName) || error("Error parsing generic type record");
          ret.subtype.push(keyIdlType, valueType);
          break;
        }
      }
      if (!ret.idlType) error(`Error parsing generic type ${base.type}`);
      ret.tokens.close = consume(">") || error(`Missing closing bracket after ${base.type}`);
      return ret;
    }

    get generic() {
      return this.tokens.base.value;
    }
  }

  function single_type(typeName) {
    let ret = GenericType.parse(typeName) || primitive_type();
    if (!ret) {
      const base = consume(ID, ..._tokeniser_js__WEBPACK_IMPORTED_MODULE_1__["stringTypes"]);
      if (!base) {
        return;
      }
      ret = new Type({ source, tokens: { base } });
      if (probe("<")) error(`Unsupported generic type ${base.value}`);
    }
    if (ret.generic === "Promise" && probe("?")) {
      error("Promise type cannot be nullable");
    }
    ret.type = typeName || null;
    type_suffix(ret);
    if (ret.nullable && ret.idlType === "any") error("Type `any` cannot be made nullable");
    return ret;
  }

  class UnionType extends Type {
    static parse(type) {
      const tokens = {};
      tokens.open = consume("(");
      if (!tokens.open) return;
      const ret = new UnionType({ source, tokens });
      ret.type = type || null;
      while (true) {
        const typ = type_with_extended_attributes() || error("No type after open parenthesis or 'or' in union type");
        if (typ.idlType === "any") error("Type `any` cannot be included in a union type");
        ret.subtype.push(typ);
        const or = consume("or");
        if (or) {
          typ.tokens.separator = or;
        }
        else break;
      }
      if (ret.idlType.length < 2) {
        error("At least two types are expected in a union type but found less");
      }
      tokens.close = consume(")") || error("Unterminated union type");
      type_suffix(ret);
      return ret;
    }

    get union() {
      return true;
    }
  }

  function type(typeName) {
    return single_type(typeName) || UnionType.parse(typeName);
  }

  function type_with_extended_attributes(typeName) {
    const extAttrs = ExtendedAttributes.parse();
    const ret = type(typeName);
    if (ret) ret.extAttrs = extAttrs;
    return ret;
  }

  class Argument extends _productions_base_js__WEBPACK_IMPORTED_MODULE_3__["Base"] {
    static parse() {
      const start_position = tokeniser.position;
      const tokens = {};
      const ret = new Argument({ source, tokens });
      ret.extAttrs = ExtendedAttributes.parse();
      tokens.optional = consume("optional");
      ret.idlType = type_with_extended_attributes("argument-type");
      if (!ret.idlType) {
        return unconsume(start_position);
      }
      if (!tokens.optional) {
        tokens.variadic = consume("...");
      }
      tokens.name = consume(ID, ..._tokeniser_js__WEBPACK_IMPORTED_MODULE_1__["argumentNameKeywords"]);
      if (!tokens.name) {
        return unconsume(start_position);
      }
      ret.default = tokens.optional ? _productions_default_js__WEBPACK_IMPORTED_MODULE_5__["Default"].parse(tokeniser) : null;
      return ret;
    }

    get optional() {
      return !!this.tokens.optional;
    }
    get variadic() {
      return !!this.tokens.variadic;
    }
    get name() {
      return Object(_productions_helpers_js__WEBPACK_IMPORTED_MODULE_0__["unescape"])(this.tokens.name.value);
    }
  }

  function argument_list() {
    return Object(_productions_helpers_js__WEBPACK_IMPORTED_MODULE_0__["list"])(tokeniser, { parser: Argument.parse, listName: "arguments list" });
  }

  function identifiers() {
    const ids = Object(_productions_helpers_js__WEBPACK_IMPORTED_MODULE_0__["list"])(tokeniser, { parser: _productions_token_js__WEBPACK_IMPORTED_MODULE_4__["Token"].parser(tokeniser, ID), listName: "identifier list" });
    if (!ids.length) {
      error("Expected identifiers but none found");
    }
    return ids;
  }

  class ExtendedAttributeParameters extends _productions_base_js__WEBPACK_IMPORTED_MODULE_3__["Base"] {
    static parse() {
      const tokens = { assign: consume("=") };
      const ret = new ExtendedAttributeParameters({ source, tokens });
      if (tokens.assign) {
        tokens.secondaryName = consume(ID, DECIMAL, INT, STR);
      }
      tokens.open = consume("(");
      if (tokens.open) {
        ret.list = ret.rhsType === "identifier-list" ?
          // [Exposed=(Window,Worker)]
          identifiers() :
          // [NamedConstructor=Audio(DOMString src)] or [Constructor(DOMString str)]
          argument_list();
        tokens.close = consume(")") || error("Unexpected token in extended attribute argument list");
      } else if (ret.hasRhs && !tokens.secondaryName) {
        error("No right hand side to extended attribute assignment");
      }
      return ret;
    }

    get rhsType() {
      return !this.tokens.assign ? null :
        !this.tokens.secondaryName ? "identifier-list" :
        this.tokens.secondaryName.type;
    }
  }

  class SimpleExtendedAttribute extends _productions_base_js__WEBPACK_IMPORTED_MODULE_3__["Base"] {
    static parse() {
      const name = consume(ID);
      if (name) {
        return new SimpleExtendedAttribute({
          tokens: { name },
          params: ExtendedAttributeParameters.parse()
        });
      }
    }

    constructor({ source, tokens, params }) {
      super({ source, tokens });
      Object.defineProperty(this, "params", { value: params });
    }

    get type() {
      return "extended-attribute";
    }
    get name() {
      return this.tokens.name.value;
    }
    get rhs() {
      const { rhsType: type, tokens, list } = this.params;
      if (!type) {
        return null;
      }
      const value = type === "identifier-list" ? list : tokens.secondaryName.value;
      return { type, value };
    }
    get arguments() {
      const { rhsType, list } = this.params;
      if (!list || rhsType === "identifier-list") {
        return [];
      }
      return list;
    }
  }

  // Note: we parse something simpler than the official syntax. It's all that ever
  // seems to be used
  class ExtendedAttributes extends _productions_array_base_js__WEBPACK_IMPORTED_MODULE_2__["ArrayBase"] {
    static parse() {
      const tokens = {};
      tokens.open = consume("[");
      if (!tokens.open) return [];
      const ret = new ExtendedAttributes({ source, tokens });
      ret.push(...Object(_productions_helpers_js__WEBPACK_IMPORTED_MODULE_0__["list"])(tokeniser, {
        parser: SimpleExtendedAttribute.parse,
        listName: "extended attribute"
      }));
      tokens.close = consume("]") || error("Unexpected form of extended attribute");
      if (!ret.length) {
        error("Found an empty extended attribute");
      }
      if (probe("[")) {
        error("Illegal double extended attribute lists, consider merging them");
      }
      return ret;
    }
  }

  class Constant extends _productions_base_js__WEBPACK_IMPORTED_MODULE_3__["Base"] {
    static parse() {
      const tokens = {};
      tokens.base = consume("const");
      if (!tokens.base) {
        return;
      }
      let idlType = primitive_type();
      if (!idlType) {
        const base = consume(ID) || error("No type for const");
        idlType = new Type({ source, tokens: { base } });
      }
      if (probe("?")) {
        error("Unexpected nullable constant type");
      }
      idlType.type = "const-type";
      tokens.name = consume(ID) || error("No name for const");
      tokens.assign = consume("=") || error("No value assignment for const");
      tokens.value = Object(_productions_helpers_js__WEBPACK_IMPORTED_MODULE_0__["const_value"])(tokeniser) || error("No value for const");
      tokens.termination = consume(";") || error("Unterminated const");
      const ret = new Constant({ source, tokens });
      ret.idlType = idlType;
      return ret;
    }

    get type() {
      return "const";
    }
    get name() {
      return Object(_productions_helpers_js__WEBPACK_IMPORTED_MODULE_0__["unescape"])(this.tokens.name.value);
    }
    get value() {
      return Object(_productions_helpers_js__WEBPACK_IMPORTED_MODULE_0__["const_data"])(this.tokens.value);
    }
  }

  class CallbackFunction extends _productions_base_js__WEBPACK_IMPORTED_MODULE_3__["Base"] {
    static parse(base) {
      const tokens = { base };
      const ret = new CallbackFunction({ source, tokens });
      tokens.name = consume(ID) || error("No name for callback");
      tokeniser.current = ret;
      tokens.assign = consume("=") || error("No assignment in callback");
      ret.idlType = return_type() || error("Missing return type");
      tokens.open = consume("(") || error("No arguments in callback");
      ret.arguments = argument_list();
      tokens.close = consume(")") || error("Unterminated callback");
      tokens.termination = consume(";") || error("Unterminated callback");
      return ret;
    }

    get type() {
      return "callback";
    }
    get name() {
      return Object(_productions_helpers_js__WEBPACK_IMPORTED_MODULE_0__["unescape"])(this.tokens.name.value);
    }
  }

  function callback() {
    const callback = consume("callback");
    if (!callback) return;
    const tok = consume("interface");
    if (tok) {
      return Interface.parse(tok, { callback });
    }
    return CallbackFunction.parse(callback);
  }

  class Attribute extends _productions_base_js__WEBPACK_IMPORTED_MODULE_3__["Base"] {
    static parse({ special, noInherit = false, readonly = false } = {}) {
      const start_position = tokeniser.position;
      const tokens = { special };
      const ret = new Attribute({ source, tokens });
      if (!special && !noInherit) {
        tokens.special = consume("inherit");
      }
      if (ret.special === "inherit" && probe("readonly")) {
        error("Inherited attributes cannot be read-only");
      }
      tokens.readonly = consume("readonly");
      if (readonly && !tokens.readonly && probe("attribute")) {
        error("Attributes must be readonly in this context");
      }
      tokens.base = consume("attribute");
      if (!tokens.base) {
        unconsume(start_position);
        return;
      }
      ret.idlType = type_with_extended_attributes("attribute-type") || error("No type in attribute");
      switch (ret.idlType.generic) {
        case "sequence":
        case "record": error(`Attributes cannot accept ${ret.idlType.generic} types`);
      }
      tokens.name = consume(ID, "required") || error("No name in attribute");
      tokens.termination = consume(";") || error("Unterminated attribute");
      return ret;
    }

    get type() {
      return "attribute";
    }
    get special() {
      if (!this.tokens.special) {
        return "";
      }
      return this.tokens.special.value;
    }
    get readonly() {
      return !!this.tokens.readonly;
    }
    get name() {
      return Object(_productions_helpers_js__WEBPACK_IMPORTED_MODULE_0__["unescape"])(this.tokens.name.value);
    }
  }

  function return_type(typeName) {
    const typ = type(typeName || "return-type");
    if (typ) {
      return typ;
    }
    const voidToken = consume("void");
    if (voidToken) {
      const ret = new Type({ source, tokens: { base: voidToken } });
      ret.type = "return-type";
      return ret;
    }
  }

  class Operation extends _productions_base_js__WEBPACK_IMPORTED_MODULE_3__["Base"] {
    static parse({ special, regular } = {}) {
      const tokens = { special };
      const ret = new Operation({ source, tokens });
      if (special && special.value === "stringifier") {
        tokens.termination = consume(";");
        if (tokens.termination) {
          ret.arguments = [];
          return ret;
        }
      }
      if (!special && !regular) {
        tokens.special = consume("getter", "setter", "deleter");
      }
      ret.idlType = return_type() || error("Missing return type");
      tokens.name = consume(ID);
      tokens.open = consume("(") || error("Invalid operation");
      ret.arguments = argument_list();
      tokens.close = consume(")") || error("Unterminated operation");
      tokens.termination = consume(";") || error("Unterminated attribute");
      return ret;
    }

    get type() {
      return "operation";
    }
    get name() {
      const { name } = this.tokens;
      if (!name) {
        return "";
      }
      return Object(_productions_helpers_js__WEBPACK_IMPORTED_MODULE_0__["unescape"])(name.value);
    }
    get special() {
      if (!this.tokens.special) {
        return "";
      }
      return this.tokens.special.value;
    }
  }

  function static_member() {
    const special = consume("static");
    if (!special) return;
    const member = Attribute.parse({ special }) ||
      Operation.parse({ special }) ||
      error("No body in static member");
    return member;
  }

  function stringifier() {
    const special = consume("stringifier");
    if (!special) return;
    const member = Attribute.parse({ special }) ||
      Operation.parse({ special }) ||
      error("Unterminated stringifier");
    return member;
  }

  class IterableLike extends _productions_base_js__WEBPACK_IMPORTED_MODULE_3__["Base"] {
    static parse() {
      const start_position = tokeniser.position;
      const tokens = {};
      const ret = new IterableLike({ source, tokens });
      tokens.readonly = consume("readonly");
      tokens.base = tokens.readonly ?
        consume("maplike", "setlike") :
        consume("iterable", "maplike", "setlike");
      if (!tokens.base) {
        unconsume(start_position);
        return;
      }

      const { type } = ret;
      const secondTypeRequired = type === "maplike";
      const secondTypeAllowed = secondTypeRequired || type === "iterable";

      tokens.open = consume("<") || error(`Error parsing ${type} declaration`);
      const first = type_with_extended_attributes() || error(`Error parsing ${type} declaration`);
      ret.idlType = [first];
      if (secondTypeAllowed) {
        first.tokens.separator = consume(",");
        if (first.tokens.separator) {
          ret.idlType.push(type_with_extended_attributes());
        }
        else if (secondTypeRequired)
          error(`Missing second type argument in ${type} declaration`);
      }
      tokens.close = consume(">") || error(`Unterminated ${type} declaration`);
      tokens.termination = consume(";") || error(`Missing semicolon after ${type} declaration`);

      return ret;
    }

    get type() {
      return this.tokens.base.value;
    }
    get readonly() {
      return !!this.tokens.readonly;
    }
  }

  function inheritance() {
    const colon = consume(":");
    if (!colon) {
      return {};
    }
    const inheritance = consume(ID) || error("No type in inheritance");
    return { colon, inheritance };
  }

  class Container extends _productions_base_js__WEBPACK_IMPORTED_MODULE_3__["Base"] {
    static parse(instance, { type, inheritable, allowedMembers }) {
      const { tokens } = instance;
      tokens.name = consume(ID) || error("No name for interface");
      tokeniser.current = instance;
      if (inheritable) {
        Object.assign(tokens, inheritance());
      }
      tokens.open = consume("{") || error(`Bodyless ${type}`);
      instance.members = [];
      while (true) {
        tokens.close = consume("}");
        if (tokens.close) {
          tokens.termination = consume(";") || error(`Missing semicolon after ${type}`);
          return instance;
        }
        const ea = ExtendedAttributes.parse();
        let mem;
        for (const [parser, ...args] of allowedMembers) {
          mem = parser(...args);
          if (mem) {
            break;
          }
        }
        if (!mem) {
          error("Unknown member");
        }
        mem.extAttrs = ea;
        instance.members.push(mem);
      }
    }

    get partial() {
      return !!this.tokens.partial;
    }
    get name() {
      return Object(_productions_helpers_js__WEBPACK_IMPORTED_MODULE_0__["unescape"])(this.tokens.name.value);
    }
    get inheritance() {
      if (!this.tokens.inheritance) {
        return null;
      }
      return Object(_productions_helpers_js__WEBPACK_IMPORTED_MODULE_0__["unescape"])(this.tokens.inheritance.value);
    }
  }

  class Interface extends Container {
    static parse(base, { callback = null, partial = null } = {}) {
      const tokens = { callback, partial, base };
      return Container.parse(new Interface({ source, tokens }), {
        type: "interface",
        inheritable: !partial,
        allowedMembers: [
          [Constant.parse],
          [static_member],
          [stringifier],
          [IterableLike.parse],
          [Attribute.parse],
          [Operation.parse]
        ]
      });
    }

    get type() {
      if (this.tokens.callback) {
        return "callback interface";
      }
      return "interface";
    }
  }

  class Mixin extends Container {
    static parse(base, { partial } = {}) {
      const tokens = { partial, base };
      tokens.mixin = consume("mixin");
      if (!tokens.mixin) {
        return;
      }
      return Container.parse(new Mixin({ source, tokens }), {
        type: "interface mixin",
        allowedMembers: [
          [Constant.parse],
          [stringifier],
          [Attribute.parse, { noInherit: true }],
          [Operation.parse, { regular: true }]
        ]
      });
    }

    get type() {
      return "interface mixin";
    }
  }

  function interface_(opts) {
    const base = consume("interface");
    if (!base) return;
    const ret = Mixin.parse(base, opts) ||
      Interface.parse(base, opts) ||
      error("Interface has no proper body");
    return ret;
  }

  class Namespace extends Container {
    static parse({ partial } = {}) {
      const tokens = { partial };
      tokens.base = consume("namespace");
      if (!tokens.base) {
        return;
      }
      return Container.parse(new Namespace({ source, tokens }), {
        type: "namespace",
        allowedMembers: [
          [Attribute.parse, { noInherit: true, readonly: true }],
          [Operation.parse, { regular: true }]
        ]
      });
    }

    get type() {
      return "namespace";
    }
  }

  function partial() {
    const partial = consume("partial");
    if (!partial) return;
    return Dictionary.parse({ partial }) ||
      interface_({ partial }) ||
      Namespace.parse({ partial }) ||
      error("Partial doesn't apply to anything");
  }

  class Dictionary extends Container {
    static parse({ partial } = {}) {
      const tokens = { partial };
      tokens.base = consume("dictionary");
      if (!tokens.base) {
        return;
      }
      return Container.parse(new Dictionary({ source, tokens }), {
        type: "dictionary",
        inheritable: !partial,
        allowedMembers: [
          [Field.parse],
        ]
      });
    }

    get type() {
      return "dictionary";
    }
  }

  class Field extends _productions_base_js__WEBPACK_IMPORTED_MODULE_3__["Base"] {
    static parse() {
      const tokens = {};
      const ret = new Field({ source, tokens });
      ret.extAttrs = ExtendedAttributes.parse();
      tokens.required = consume("required");
      ret.idlType = type_with_extended_attributes("dictionary-type") || error("No type for dictionary member");
      tokens.name = consume(ID) || error("No name for dictionary member");
      ret.default = _productions_default_js__WEBPACK_IMPORTED_MODULE_5__["Default"].parse(tokeniser);
      if (tokens.required && ret.default) error("Required member must not have a default");
      tokens.termination = consume(";") || error("Unterminated dictionary member");
      return ret;
    }

    get type() {
      return "field";
    }
    get name() {
      return Object(_productions_helpers_js__WEBPACK_IMPORTED_MODULE_0__["unescape"])(this.tokens.name.value);
    }
    get required() {
      return !!this.tokens.required;
    }
  }

  class Typedef extends _productions_base_js__WEBPACK_IMPORTED_MODULE_3__["Base"] {
    static parse() {
      const tokens = {};
      const ret = new Typedef({ source, tokens });
      tokens.base = consume("typedef");
      if (!tokens.base) {
        return;
      }
      ret.idlType = type_with_extended_attributes("typedef-type") || error("No type in typedef");
      tokens.name = consume(ID) || error("No name in typedef");
      tokeniser.current = ret;
      tokens.termination = consume(";") || error("Unterminated typedef");
      return ret;
    }

    get type() {
      return "typedef";
    }
    get name() {
      return Object(_productions_helpers_js__WEBPACK_IMPORTED_MODULE_0__["unescape"])(this.tokens.name.value);
    }
  }

  function definition() {
    return callback() ||
      interface_() ||
      partial() ||
      Dictionary.parse() ||
      _productions_enum_js__WEBPACK_IMPORTED_MODULE_6__["Enum"].parse(tokeniser) ||
      Typedef.parse() ||
      _productions_includes_js__WEBPACK_IMPORTED_MODULE_7__["Includes"].parse(tokeniser) ||
      Namespace.parse();
  }

  function definitions() {
    if (!source.length) return [];
    const defs = [];
    while (true) {
      const ea = ExtendedAttributes.parse();
      const def = definition();
      if (!def) {
        if (ea.length) error("Stray extended attributes");
        break;
      }
      def.extAttrs = ea;
      defs.push(def);
    }
    const eof = consume("eof");
    if (options.concrete) {
      defs.push(eof);
    }
    return defs;
  }
  const res = definitions();
  if (tokeniser.position < source.length) error("Unrecognised tokens");
  return res;
}

function parse(str, options = {}) {
  const tokeniser = new _tokeniser_js__WEBPACK_IMPORTED_MODULE_1__["Tokeniser"](str);
  return parseByTokens(tokeniser, options);
}


/***/ }),
/* 2 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "unescape", function() { return unescape; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "list", function() { return list; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "const_value", function() { return const_value; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "const_data", function() { return const_data; });
/**
 * @param {string} identifier
 */
function unescape(identifier) {
  return identifier.startsWith('_') ? identifier.slice(1) : identifier;
}

/**
 * Parses comma-separated list
 * @param {import("../tokeniser").Tokeniser} tokeniser
 * @param {object} args
 * @param {Function} args.parser parser function for each item
 * @param {boolean} [args.allowDangler] whether to allow dangling comma
 * @param {string} [args.listName] the name to be shown on error messages
 */
function list(tokeniser, { parser, allowDangler, listName = "list" }) {
  const first = parser(tokeniser);
  if (!first) {
    return [];
  }
  first.tokens.separator = tokeniser.consume(",");
  const items = [first];
  while (first.tokens.separator) {
    const item = parser(tokeniser);
    if (!item) {
      if (!allowDangler) {
        tokeniser.error(`Trailing comma in ${listName}`);
      }
      break;
    }
    item.tokens.separator = tokeniser.consume(",");
    items.push(item);
    if (!item.tokens.separator) break;
  }
  return items;
}

/**
 * @param {import("../tokeniser").Tokeniser} tokeniser
 */
function const_value(tokeniser) {
  return tokeniser.consume("true", "false", "Infinity", "-Infinity", "NaN", "decimal", "integer");
}

/**
 * @param {object} token
 * @param {string} token.type
 * @param {string} token.value
 */
function const_data({ type, value }) {
  switch (type) {
    case "true":
    case "false":
      return { type: "boolean", value: type === "true" };
    case "Infinity":
    case "-Infinity":
      return { type: "Infinity", negative: type.startsWith("-") };
    case "[":
      return { type: "sequence", value: [] };
    case "decimal":
    case "integer":
        return { type: "number", value };
    case "string":
      return { type: "string", value: value.slice(1, -1) };
    default:
      return { type };
  }
}


/***/ }),
/* 3 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "stringTypes", function() { return stringTypes; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "argumentNameKeywords", function() { return argumentNameKeywords; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Tokeniser", function() { return Tokeniser; });
/* harmony import */ var _error_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(4);


// These regular expressions use the sticky flag so they will only match at
// the current location (ie. the offset of lastIndex).
const tokenRe = {
  // This expression uses a lookahead assertion to catch false matches
  // against integers early.
  "decimal": /-?(?=[0-9]*\.|[0-9]+[eE])(([0-9]+\.[0-9]*|[0-9]*\.[0-9]+)([Ee][-+]?[0-9]+)?|[0-9]+[Ee][-+]?[0-9]+)/y,
  "integer": /-?(0([Xx][0-9A-Fa-f]+|[0-7]*)|[1-9][0-9]*)/y,
  "identifier": /[_-]?[A-Za-z][0-9A-Z_a-z-]*/y,
  "string": /"[^"]*"/y,
  "whitespace": /[\t\n\r ]+/y,
  "comment": /((\/(\/.*|\*([^*]|\*[^/])*\*\/)[\t\n\r ]*)+)/y,
  "other": /[^\t\n\r 0-9A-Za-z]/y
};

const stringTypes = [
  "ByteString",
  "DOMString",
  "USVString"
];

const argumentNameKeywords = [
  "attribute",
  "callback",
  "const",
  "deleter",
  "dictionary",
  "enum",
  "getter",
  "includes",
  "inherit",
  "interface",
  "iterable",
  "maplike",
  "namespace",
  "partial",
  "required",
  "setlike",
  "setter",
  "static",
  "stringifier",
  "typedef",
  "unrestricted"
];

const nonRegexTerminals = [
  "-Infinity",
  "FrozenArray",
  "Infinity",
  "NaN",
  "Promise",
  "boolean",
  "byte",
  "double",
  "false",
  "float",
  "implements",
  "legacyiterable",
  "long",
  "mixin",
  "null",
  "octet",
  "optional",
  "or",
  "readonly",
  "record",
  "sequence",
  "short",
  "true",
  "unsigned",
  "void"
].concat(argumentNameKeywords, stringTypes);

const punctuations = [
  "(",
  ")",
  ",",
  "...",
  ":",
  ";",
  "<",
  "=",
  ">",
  "?",
  "[",
  "]",
  "{",
  "}"
];

/**
 * @param {string} str
 */
function tokenise(str) {
  const tokens = [];
  let lastCharIndex = 0;
  let trivia = "";
  let line = 1;
  let index = 0;
  while (lastCharIndex < str.length) {
    const nextChar = str.charAt(lastCharIndex);
    let result = -1;

    if (/[\t\n\r ]/.test(nextChar)) {
      result = attemptTokenMatch("whitespace", { noFlushTrivia: true });
    } else if (nextChar === '/') {
      result = attemptTokenMatch("comment", { noFlushTrivia: true });
    }

    if (result !== -1) {
      const currentTrivia = tokens.pop().value;
      line += (currentTrivia.match(/\n/g) || []).length;
      trivia += currentTrivia;
      index -= 1;
    } else if (/[-0-9.A-Z_a-z]/.test(nextChar)) {
      result = attemptTokenMatch("decimal");
      if (result === -1) {
        result = attemptTokenMatch("integer");
      }
      if (result === -1) {
        result = attemptTokenMatch("identifier");
        const token = tokens[tokens.length - 1];
        if (result !== -1 && nonRegexTerminals.includes(token.value)) {
          token.type = token.value;
        }
      }
    } else if (nextChar === '"') {
      result = attemptTokenMatch("string");
    }

    for (const punctuation of punctuations) {
      if (str.startsWith(punctuation, lastCharIndex)) {
        tokens.push({ type: punctuation, value: punctuation, trivia, line, index });
        trivia = "";
        lastCharIndex += punctuation.length;
        result = lastCharIndex;
        break;
      }
    }

    // other as the last try
    if (result === -1) {
      result = attemptTokenMatch("other");
    }
    if (result === -1) {
      throw new Error("Token stream not progressing");
    }
    lastCharIndex = result;
    index += 1;
  }

  // remaining trivia as eof
  tokens.push({
    type: "eof",
    value: "",
    trivia
  });

  return tokens;

  /**
   * @param {keyof tokenRe} type
   * @param {object} [options]
   * @param {boolean} [options.noFlushTrivia]
   */
  function attemptTokenMatch(type, { noFlushTrivia } = {}) {
    const re = tokenRe[type];
    re.lastIndex = lastCharIndex;
    const result = re.exec(str);
    if (result) {
      tokens.push({ type, value: result[0], trivia, line, index });
      if (!noFlushTrivia) {
        trivia = "";
      }
      return re.lastIndex;
    }
    return -1;
  }
}

class Tokeniser {
  /**
   * @param {string} idl
   */
  constructor(idl) {
    this.source = tokenise(idl);
    this.position = 0;
  }

  /**
   * @param {string} message
   */
  error(message) {
    throw new WebIDLParseError(Object(_error_js__WEBPACK_IMPORTED_MODULE_0__["syntaxError"])(this.source, this.position, this.current, message));
  }

  /**
   * @param {string} type
   */
  probe(type) {
    return this.source.length > this.position && this.source[this.position].type === type;
  }

  /**
   * @param  {...string} candidates
   */
  consume(...candidates) {
    for (const type of candidates) {
      if (!this.probe(type)) continue;
      const token = this.source[this.position];
      this.position++;
      return token;
    }
  }

  /**
   * @param {number} position
   */
  unconsume(position) {
    this.position = position;
  }
}

class WebIDLParseError extends Error {
  constructor({ message, line, input, tokens }) {
    super(message);
    this.name = "WebIDLParseError"; // not to be mangled
    this.line = line;
    this.input = input;
    this.tokens = tokens;
  }
}


/***/ }),
/* 4 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "syntaxError", function() { return syntaxError; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "validationError", function() { return validationError; });
/**
 * @param {string} text
 */
function lastLine(text) {
  const splitted = text.split("\n");
  return splitted[splitted.length - 1];
}

/**
 * @param {string} message error message
 * @param {"Syntax" | "Validation"} type error type
 */
function error(source, position, current, message, type) {
  /**
   * @param {number} count
   */
  function sliceTokens(count) {
    return count > 0 ?
      source.slice(position, position + count) :
      source.slice(Math.max(position + count, 0), position);
  }

  function tokensToText(inputs, { precedes } = {}) {
    const text = inputs.map(t => t.trivia + t.value).join("");
    const nextToken = source[position];
    if (nextToken.type === "eof") {
      return text;
    }
    if (precedes) {
      return text + nextToken.trivia;
    }
    return text.slice(nextToken.trivia.length);
  }

  const maxTokens = 5; // arbitrary but works well enough
  const line =
    source[position].type !== "eof" ? source[position].line :
    source.length > 1 ? source[position - 1].line :
    1;

  const precedingLine = lastLine(
    tokensToText(sliceTokens(-maxTokens), { precedes: true })
  );

  const subsequentTokens = sliceTokens(maxTokens);
  const subsequentText = tokensToText(subsequentTokens);
  const sobsequentLine = subsequentText.split("\n")[0];

  const spaced = " ".repeat(precedingLine.length) + "^ " + message;
  const contextualMessage = precedingLine + sobsequentLine + "\n" + spaced;

  const contextType = type === "Syntax" ? "since" : "inside";
  const grammaticalContext = current ? `, ${contextType} \`${current.partial ? "partial " : ""}${current.type} ${current.name}\`` : "";
  return {
    message: `${type} error at line ${line}${grammaticalContext}:\n${contextualMessage}`,
    line,
    input: subsequentText,
    tokens: subsequentTokens
  };
}

/**
 * @param {string} message error message
 */
function syntaxError(source, position, current, message) {
  return error(source, position, current, message, "Syntax");
}

/**
 * @param {string} message error message
 */
function validationError(source, token, current, message) {
  return error(source, token.index, current, message, "Validation").message;
}


/***/ }),
/* 5 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "ArrayBase", function() { return ArrayBase; });
class ArrayBase extends Array {
  constructor({ source, tokens }) {
    super();
    Object.defineProperties(this, {
      source: { value: source },
      tokens: { value: tokens }
    });
  }
}


/***/ }),
/* 6 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Base", function() { return Base; });
class Base {
  constructor({ source, tokens }) {
    Object.defineProperties(this, {
      source: { value: source },
      tokens: { value: tokens }
    });
  }

  toJSON() {
    const json = { type: undefined, name: undefined, inheritance: undefined };
    let proto = this;
    while (proto !== Object.prototype) {
      const descMap = Object.getOwnPropertyDescriptors(proto);
      for (const [key, value] of Object.entries(descMap)) {
        if (value.enumerable || value.get) {
          json[key] = this[key];
        }
      }
      proto = Object.getPrototypeOf(proto);
    }
    return json;
  }
}


/***/ }),
/* 7 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Token", function() { return Token; });
/* harmony import */ var _base_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(6);


class Token extends _base_js__WEBPACK_IMPORTED_MODULE_0__["Base"] {
  /**
   * @param {import("../tokeniser").Tokeniser} tokeniser
   * @param {string} type
   */
  static parser(tokeniser, type) {
    return () => {
      const value = tokeniser.consume(type);
      if (value) {
        return new Token({ source: tokeniser.source, tokens: { value } });
      }
    };
  }

  get value() {
    return this.tokens.value.value;
  }
}


/***/ }),
/* 8 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Default", function() { return Default; });
/* harmony import */ var _base_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(6);
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(2);



class Default extends _base_js__WEBPACK_IMPORTED_MODULE_0__["Base"] {
  /**
   * @param {import("../tokeniser").Tokeniser} tokeniser
   */
  static parse(tokeniser) {
    const assign = tokeniser.consume("=");
    if (!assign) {
      return null;
    }
    const def = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["const_value"])(tokeniser) || tokeniser.consume("string", "null", "[") || tokeniser.error("No value for default");
    const expression = [def];
    if (def.type === "[") {
      const close = tokeniser.consume("]") || tokeniser.error("Default sequence value must be empty");
      expression.push(close);
    }
    return new Default({ source: tokeniser.source, tokens: { assign }, expression });
  }

  constructor({ source, tokens, expression }) {
    super({ source, tokens });
    Object.defineProperty(this, "expression", { value: expression });
  }

  get type() {
    return Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["const_data"])(this.expression[0]).type;
  }
  get value() {
    return Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["const_data"])(this.expression[0]).value;
  }
  get negative() {
    return Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["const_data"])(this.expression[0]).negative;
  }
}


/***/ }),
/* 9 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Enum", function() { return Enum; });
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(2);
/* harmony import */ var _token_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(7);
/* harmony import */ var _base_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(6);




class EnumValue extends _token_js__WEBPACK_IMPORTED_MODULE_1__["Token"] {
  /**
   * @param {import("../tokeniser").Tokeniser} tokeniser
   */
  static parse(tokeniser) {
    const value = tokeniser.consume("string");
    if (value) {
      return new EnumValue({ source: tokeniser.source, tokens: { value } });
    }
  }

  get type() {
    return "enum-value";
  }
  get value() {
    return super.value.slice(1, -1);
  }
}

class Enum extends _base_js__WEBPACK_IMPORTED_MODULE_2__["Base"] {
  /**
   * @param {import("../tokeniser").Tokeniser} tokeniser
   */
  static parse(tokeniser) {
    const tokens = {};
    tokens.base = tokeniser.consume("enum");
    if (!tokens.base) {
      return;
    }
    tokens.name = tokeniser.consume("identifier") || tokeniser.error("No name for enum");
    const ret = tokeniser.current = new Enum({ source: tokeniser.source, tokens });
    tokens.open = tokeniser.consume("{") || tokeniser.error("Bodyless enum");
    ret.values = Object(_helpers_js__WEBPACK_IMPORTED_MODULE_0__["list"])(tokeniser, {
      parser: EnumValue.parse,
      allowDangler: true,
      listName: "enumeration"
    });
    if (tokeniser.probe("string")) {
      tokeniser.error("No comma between enum values");
    }
    tokens.close = tokeniser.consume("}") || tokeniser.error("Unexpected value in enum");
    if (!ret.values.length) {
      tokeniser.error("No value in enum");
    }
    tokens.termination = tokeniser.consume(";") || tokeniser.error("No semicolon after enum");
    return ret;
  }

  get type() {
    return "enum";
  }
  get name() {
    return Object(_helpers_js__WEBPACK_IMPORTED_MODULE_0__["unescape"])(this.tokens.name.value);
  }
}


/***/ }),
/* 10 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "Includes", function() { return Includes; });
/* harmony import */ var _base_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(6);
/* harmony import */ var _helpers_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(2);



class Includes extends _base_js__WEBPACK_IMPORTED_MODULE_0__["Base"] {
  /**
   * @param {import("../tokeniser").Tokeniser} tokeniser
   */
  static parse(tokeniser) {
    const target = tokeniser.consume("identifier");
    if (!target) {
      return;
    }
    const tokens = { target };
    tokens.includes = tokeniser.consume("includes");
    if (!tokens.includes) {
      tokeniser.unconsume(target.index);
      return;
    }
    tokens.mixin = tokeniser.consume("identifier") || tokeniser.error("Incomplete includes statement");
    tokens.termination = tokeniser.consume(";") || tokeniser.error("No terminating ; for includes statement");
    return new Includes({ source: tokeniser.source, tokens });
  }

  get type() {
    return "includes";
  }
  get target() {
    return Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["unescape"])(this.tokens.target.value);
  }
  get includes() {
    return Object(_helpers_js__WEBPACK_IMPORTED_MODULE_1__["unescape"])(this.tokens.mixin.value);
  }
}


/***/ }),
/* 11 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "write", function() { return write; });


function noop(arg) {
  return arg;
}

const templates = {
  wrap: items => items.join(""),
  trivia: noop,
  name: noop,
  reference: noop,
  type: noop,
  generic: noop,
  inheritance: noop,
  definition: noop,
  extendedAttribute: noop,
  extendedAttributeReference: noop
};

function write(ast, { templates: ts = templates } = {}) {
  ts = Object.assign({}, templates, ts);

  function reference(raw, { unescaped, context }) {
    if (!unescaped) {
      unescaped = raw.startsWith("_") ? raw.slice(1) : raw;
    }
    return ts.reference(raw, unescaped, context);
  }

  function token(t, wrapper = noop, ...args) {
    if (!t) {
      return "";
    }
    const value = wrapper(t.value, ...args);
    return ts.wrap([ts.trivia(t.trivia), value]);
  }

  function reference_token(t, context) {
    return token(t, reference, { context });
  }

  function name_token(t, arg) {
    return token(t, ts.name, arg);
  }

  function type_body(it) {
    if (it.union || it.generic) {
      return ts.wrap([
        token(it.tokens.base, ts.generic),
        token(it.tokens.open),
        ...it.subtype.map(type),
        token(it.tokens.close)
      ]);
    }
    const firstToken = it.tokens.prefix || it.tokens.base;
    const prefix = it.tokens.prefix ? [
      it.tokens.prefix.value,
      ts.trivia(it.tokens.base.trivia)
    ] : [];
    const ref = reference(ts.wrap([
      ...prefix,
      it.tokens.base.value,
      token(it.tokens.postfix)
    ]), { unescaped: it.idlType, context: it });
    return ts.wrap([ts.trivia(firstToken.trivia), ref]);
  }
  function type(it) {
    return ts.wrap([
      extended_attributes(it.extAttrs),
      type_body(it),
      token(it.tokens.nullable),
      token(it.tokens.separator)
    ]);
  }
  function default_(def) {
    if (!def) {
      return "";
    }
    return ts.wrap([
      token(def.tokens.assign),
      ...def.expression.map(t => token(t))
    ]);
  }
  function argument(arg) {
    return ts.wrap([
      extended_attributes(arg.extAttrs),
      token(arg.tokens.optional),
      ts.type(type(arg.idlType)),
      token(arg.tokens.variadic),
      name_token(arg.tokens.name, { data: arg }),
      default_(arg.default),
      token(arg.tokens.separator)
    ]);
  }
  function identifier(id, context) {
    return ts.wrap([
      reference_token(id.tokens.value, context),
      token(id.tokens.separator)
    ]);
  }
  function make_ext_at(it) {
    const { rhsType } = it.params;
    return ts.wrap([
      ts.trivia(it.tokens.name.trivia),
      ts.extendedAttribute(ts.wrap([
        ts.extendedAttributeReference(it.name),
        token(it.params.tokens.assign),
        reference_token(it.params.tokens.secondaryName, it),
        token(it.params.tokens.open),
        ...!it.params.list ? [] :
          it.params.list.map(
            rhsType === "identifier-list" ? id => identifier(id, it) : argument
          ),
        token(it.params.tokens.close)
      ])),
      token(it.tokens.separator)
    ]);
  }
  function extended_attributes(eats) {
    if (!eats.length) return "";
    return ts.wrap([
      token(eats.tokens.open),
      ...eats.map(make_ext_at),
      token(eats.tokens.close)
    ]);
  }

  function operation(it, parent) {
    const body = it.idlType ? [
      ts.type(type(it.idlType)),
      name_token(it.tokens.name, { data: it, parent }),
      token(it.tokens.open),
      ts.wrap(it.arguments.map(argument)),
      token(it.tokens.close),
    ] : [];
    return ts.definition(ts.wrap([
      extended_attributes(it.extAttrs),
      token(it.tokens.special),
      ...body,
      token(it.tokens.termination)
    ]), { data: it, parent });
  }

  function attribute(it, parent) {
    return ts.definition(ts.wrap([
      extended_attributes(it.extAttrs),
      token(it.tokens.special),
      token(it.tokens.readonly),
      token(it.tokens.base),
      ts.type(type(it.idlType)),
      name_token(it.tokens.name, { data: it, parent }),
      token(it.tokens.termination)
    ]), { data: it, parent });
  }

  function inheritance(inh) {
    if (!inh.tokens.inheritance) {
      return "";
    }
    return ts.wrap([
      token(inh.tokens.colon),
      ts.trivia(inh.tokens.inheritance.trivia),
      ts.inheritance(reference(inh.tokens.inheritance.value, { context: inh }))
    ]);
  }

  function container(it) {
    return ts.definition(ts.wrap([
      extended_attributes(it.extAttrs),
      token(it.tokens.callback),
      token(it.tokens.partial),
      token(it.tokens.base),
      token(it.tokens.mixin),
      name_token(it.tokens.name, { data: it }),
      inheritance(it),
      token(it.tokens.open),
      iterate(it.members, it),
      token(it.tokens.close),
      token(it.tokens.termination)
    ]), { data: it });
  }

  function field(it, parent) {
    return ts.definition(ts.wrap([
      extended_attributes(it.extAttrs),
      token(it.tokens.required),
      ts.type(type(it.idlType)),
      name_token(it.tokens.name, { data: it, parent }),
      default_(it.default),
      token(it.tokens.termination)
    ]), { data: it, parent });
  }
  function const_(it, parent) {
    return ts.definition(ts.wrap([
      extended_attributes(it.extAttrs),
      token(it.tokens.base),
      ts.type(type(it.idlType)),
      name_token(it.tokens.name, { data: it, parent }),
      token(it.tokens.assign),
      token(it.tokens.value),
      token(it.tokens.termination)
    ]), { data: it, parent });
  }
  function typedef(it) {
    return ts.definition(ts.wrap([
      extended_attributes(it.extAttrs),
      token(it.tokens.base),
      ts.type(type(it.idlType)),
      name_token(it.tokens.name, { data: it }),
      token(it.tokens.termination)
    ]), { data: it });
  }
  function includes(it) {
    return ts.definition(ts.wrap([
      extended_attributes(it.extAttrs),
      reference_token(it.tokens.target, it),
      token(it.tokens.includes),
      reference_token(it.tokens.mixin, it),
      token(it.tokens.termination)
    ]), { data: it });
  }
  function callback(it) {
    return ts.definition(ts.wrap([
      extended_attributes(it.extAttrs),
      token(it.tokens.base),
      name_token(it.tokens.name, { data: it }),
      token(it.tokens.assign),
      ts.type(type(it.idlType)),
      token(it.tokens.open),
      ...it.arguments.map(argument),
      token(it.tokens.close),
      token(it.tokens.termination),
    ]), { data: it });
  }
  function enum_(it) {
    return ts.definition(ts.wrap([
      extended_attributes(it.extAttrs),
      token(it.tokens.base),
      name_token(it.tokens.name, { data: it }),
      token(it.tokens.open),
      iterate(it.values, it),
      token(it.tokens.close),
      token(it.tokens.termination)
    ]), { data: it });
  }
  function enum_value(v, parent) {
    return ts.wrap([
      ts.trivia(v.tokens.value.trivia),
      ts.definition(
        ts.wrap(['"', ts.name(v.value, { data: v, parent }), '"']),
        { data: v, parent }
      ),
      token(v.tokens.separator)
    ]);
  }
  function iterable_like(it, parent) {
    return ts.definition(ts.wrap([
      extended_attributes(it.extAttrs),
      token(it.tokens.readonly),
      token(it.tokens.base, ts.generic),
      token(it.tokens.open),
      ts.wrap(it.idlType.map(type)),
      token(it.tokens.close),
      token(it.tokens.termination)
    ]), { data: it, parent });
  }
  function eof(it) {
    return ts.trivia(it.trivia);
  }

  const table = {
    interface: container,
    "interface mixin": container,
    namespace: container,
    operation,
    attribute,
    dictionary: container,
    field,
    const: const_,
    typedef,
    includes,
    callback,
    enum: enum_,
    "enum-value": enum_value,
    iterable: iterable_like,
    legacyiterable: iterable_like,
    maplike: iterable_like,
    setlike: iterable_like,
    "callback interface": container,
    eof
  };
  function dispatch(it, parent) {
    const dispatcher = table[it.type];
    if (!dispatcher) {
      throw new Error(`Type "${it.type}" is unsupported`);
    }
    return table[it.type](it, parent);
  }
  function iterate(things, parent) {
    if (!things) return;
    const results = things.map(thing => dispatch(thing, parent));
    return ts.wrap(results);
  }
  return iterate(ast);
}


/***/ }),
/* 12 */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "validate", function() { return validate; });
/* harmony import */ var _error_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(4);




function groupDefinitions(all) {
  const unique = new Map();
  const duplicates = new Set();
  const partials = new Map();
  for (const def of all) {
    if (def.partial) {
      const array = partials.get(def.name);
      if (array) {
        array.push(def);
      } else {
        partials.set(def.name, [def]);
      }
      continue;
    }
    if (!def.name) {
      continue;
    }
    if (!unique.has(def.name)) {
      unique.set(def.name, def);
    } else {
      duplicates.add(def);
    }
  }
  return { all, unique, partials, duplicates };
}

function* checkDuplicatedNames({ unique, duplicates }) {
  for (const dup of duplicates) {
    const { name } = dup;
    const message = `The name "${name}" of type "${unique.get(name).type}" was already seen`;
    yield Object(_error_js__WEBPACK_IMPORTED_MODULE_0__["validationError"])(dup.source, dup.tokens.name, dup, message);
  }
}

function* checkInterfaceMemberDuplication(defs) {
  const interfaces = [...defs.unique.values()].filter(def => def.type === "interface");
  const includesMap = getIncludesMap();

  for (const i of interfaces) {
    yield* forEachInterface(i);
  }

  function* forEachInterface(i) {
    const opNames = new Set(getOperations(i).map(op => op.name));
    const partials = defs.partials.get(i.name) || [];
    const mixins = includesMap.get(i.name) || [];
    for (const ext of [...partials, ...mixins]) {
      const additions = getOperations(ext);
      yield* forEachExtension(additions, opNames, ext, i);
      for (const addition of additions) {
        opNames.add(addition.name);
      }
    }
  }

  function* forEachExtension(additions, existings, ext, base) {
    for (const addition of additions) {
      const { name } = addition;
      if (name && existings.has(name)) {
        const message = `The operation "${name}" has already been defined for the base interface "${base.name}" either in itself or in a mixin`;
        yield Object(_error_js__WEBPACK_IMPORTED_MODULE_0__["validationError"])(ext.source, addition.tokens.name, ext, message);
      }
    }
  }

  function getOperations(i) {
    return i.members
      .filter(({type}) => type === "operation");
  }

  function getIncludesMap() {
    const map = new Map();
    const includes = defs.all.filter(def => def.type === "includes");
    for (const include of includes) {
      const array = map.get(include.target);
      const mixin = defs.unique.get(include.includes);
      if (!mixin) {
        continue;
      }
      if (array) {
        array.push(mixin);
      } else {
        map.set(include.target, [mixin]);
      }
    }
    return map;
  }
}

function validate(ast) {
  const defs = groupDefinitions(ast);
  return [
    ...checkDuplicatedNames(defs),
    ...checkInterfaceMemberDuplication(defs)
  ];
}


/***/ })
/******/ ]);
});
//# sourceMappingURL=webidl2.js.map