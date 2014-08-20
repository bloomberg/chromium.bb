// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.javascript.jscomp;

/**
 * Tests {@link ChromePass}.
 */
public class ChromePassTest extends CompilerTestCase {

    @Override
    protected CompilerPass getProcessor(Compiler compiler) {
      return new ChromePass(compiler);
    }

    @Override
    protected int getNumRepetitions() {
      // This pass isn't idempotent and only runs once.
      return 1;
    }

    public void testCrDefineCreatesObjectsForQualifiedName() throws Exception {
        test(
            "cr.define('my.namespace.name', function() {\n" +
            "  return {};\n" +
            "});",
            "var my = my || {};\n" +
            "my.namespace = my.namespace || {};\n" +
            "my.namespace.name = my.namespace.name || {};\n" +
            "cr.define('my.namespace.name', function() {\n" +
            "  return {};\n" +
            "});");
    }

    public void testCrDefineAssignsExportedFunctionByQualifiedName() throws Exception {
        test(
            "cr.define('namespace', function() {\n" +
            "  function internalStaticMethod() {\n" +
            "    alert(42);\n" +
            "  }\n" +
            "  return {\n" +
            "    externalStaticMethod: internalStaticMethod\n" +
            "  };\n" +
            "});",
            "var namespace = namespace || {};\n" +
            "cr.define('namespace', function() {\n" +
            "  namespace.externalStaticMethod = function internalStaticMethod() {\n" +
            "    alert(42);\n" +
            "  }\n" +
            "  return {\n" +
            "    externalStaticMethod: namespace.externalStaticMethod\n" +
            "  };\n" +
            "});");
    }

    public void testCrDefineCopiesJSDocForExportedFunction() throws Exception {
        test("cr.define('namespace', function() {\n" +
            "  /** I'm function's JSDoc */\n" +
            "  function internalStaticMethod() {\n" +
            "    alert(42);\n" +
            "  }\n" +
            "  return {\n" +
            "    externalStaticMethod: internalStaticMethod\n" +
            "  };\n" +
            "});",
            "var namespace = namespace || {};\n" +
            "cr.define('namespace', function() {\n" +
            "  /** I'm function's JSDoc */\n" +
            "  namespace.externalStaticMethod = function internalStaticMethod() {\n" +
            "    alert(42);\n" +
            "  }\n" +
            "  return {\n" +
            "    externalStaticMethod: namespace.externalStaticMethod\n" +
            "  };\n" +
            "});");
    }

    public void testCrDefineReassignsExportedVarByQualifiedName() throws Exception {
        test(
            "cr.define('namespace', function() {\n" +
            "  var internalStaticMethod = function() {\n" +
            "    alert(42);\n" +
            "  }\n" +
            "  return {\n" +
            "    externalStaticMethod: internalStaticMethod\n" +
            "  };\n" +
            "});",
            "var namespace = namespace || {};\n" +
            "cr.define('namespace', function() {\n" +
            "  namespace.externalStaticMethod = function() {\n" +
            "    alert(42);\n" +
            "  }\n" +
            "  return {\n" +
            "    externalStaticMethod: namespace.externalStaticMethod\n" +
            "  };\n" +
            "});");
    }

    public void testCrDefineExportsVarsWithoutAssignment() throws Exception {
        test(
            "cr.define('namespace', function() {\n" +
            "  var a;\n" +
            "  return {\n" +
            "    a: a\n" +
            "  };\n" +
            "});\n",
            "var namespace = namespace || {};\n" +
            "cr.define('namespace', function() {\n" +
            "  namespace.a;\n" +
            "  return {\n" +
            "    a: namespace.a\n" +
            "  };\n" +
            "});\n");
    }

    public void testCrDefineExportsVarsWithoutAssignmentWithJSDoc() throws Exception {
        test(
            "cr.define('namespace', function() {\n" +
            "  /** @type {number} */\n" +
            "  var a;\n" +
            "  return {\n" +
            "    a: a\n" +
            "  };\n" +
            "});\n",
            "var namespace = namespace || {};\n" +
            "cr.define('namespace', function() {\n" +
            "  /** @type {number} */\n" +
            "  namespace.a;\n" +
            "  return {\n" +
            "    a: namespace.a\n" +
            "  };\n" +
            "});\n");
    }

    public void testCrDefineCopiesJSDocForExportedVariable() throws Exception {
        test(
            "cr.define('namespace', function() {\n" +
            "  /** I'm function's JSDoc */\n" +
            "  var internalStaticMethod = function() {\n" +
            "    alert(42);\n" +
            "  }\n" +
            "  return {\n" +
            "    externalStaticMethod: internalStaticMethod\n" +
            "  };\n" +
            "});",
            "var namespace = namespace || {};\n" +
            "cr.define('namespace', function() {\n" +
            "  /** I'm function's JSDoc */\n" +
            "  namespace.externalStaticMethod = function() {\n" +
            "    alert(42);\n" +
            "  }\n" +
            "  return {\n" +
            "    externalStaticMethod: namespace.externalStaticMethod\n" +
            "  };\n" +
            "});");
    }

    public void testCrDefineDoesNothingWithNonExportedFunction() throws Exception {
        test(
            "cr.define('namespace', function() {\n" +
            "  function internalStaticMethod() {\n" +
            "    alert(42);\n" +
            "  }\n" +
            "  return {};\n" +
            "});",
            "var namespace = namespace || {};\n" +
            "cr.define('namespace', function() {\n" +
            "  function internalStaticMethod() {\n" +
            "    alert(42);\n" +
            "  }\n" +
            "  return {};\n" +
            "});");
    }

    public void testCrDefineDoesNothingWithNonExportedVar() throws Exception {
        test(
            "cr.define('namespace', function() {\n" +
            "  var a;\n" +
            "  var b;\n" +
            "  return {\n" +
            "    a: a\n" +
            "  };\n" +
            "});\n",
            "var namespace = namespace || {};\n" +
            "cr.define('namespace', function() {\n" +
            "  namespace.a;\n" +
            "  var b;\n" +
            "  return {\n" +
            "    a: namespace.a\n" +
            "  };\n" +
            "});\n");
    }

    public void testCrDefineChangesReferenceToExportedFunction() throws Exception {
        test(
            "cr.define('namespace', function() {\n" +
            "  function internalStaticMethod() {\n" +
            "    alert(42);\n" +
            "  }\n" +
            "  function letsUseIt() {\n" +
            "    internalStaticMethod();\n" +
            "  }\n" +
            "  return {\n" +
            "    externalStaticMethod: internalStaticMethod\n" +
            "  };\n" +
            "});",
            "var namespace = namespace || {};\n" +
            "cr.define('namespace', function() {\n" +
            "  namespace.externalStaticMethod = function internalStaticMethod() {\n" +
            "    alert(42);\n" +
            "  }\n" +
            "  function letsUseIt() {\n" +
            "    namespace.externalStaticMethod();\n" +
            "  }\n" +
            "  return {\n" +
            "    externalStaticMethod: namespace.externalStaticMethod\n" +
            "  };\n" +
            "});");
    }

    public void testCrDefineWrongNumberOfArguments() throws Exception {
        test("cr.define('namespace', function() { return {}; }, 'invalid argument')\n",
            null, ChromePass.CR_DEFINE_WRONG_NUMBER_OF_ARGUMENTS);
    }

    public void testCrDefineInvalidFirstArgument() throws Exception {
        test("cr.define(42, function() { return {}; })\n",
            null, ChromePass.CR_DEFINE_INVALID_FIRST_ARGUMENT);
    }

    public void testCrDefineInvalidSecondArgument() throws Exception {
        test("cr.define('namespace', 42)\n",
            null, ChromePass.CR_DEFINE_INVALID_SECOND_ARGUMENT);
    }

    public void testCrDefineInvalidReturnInFunction() throws Exception {
        test("cr.define('namespace', function() {})\n",
            null, ChromePass.CR_DEFINE_INVALID_RETURN_IN_FUNCTION);
    }

    public void testObjectDefinePropertyDefinesUnquotedProperty() throws Exception {
        test(
            "Object.defineProperty(a.b, 'c', {});",
            "Object.defineProperty(a.b, 'c', {});\n" +
            "/** @type {?} */\n" +
            "a.b.c;");
    }

    public void testCrDefinePropertyDefinesUnquotedPropertyWithStringTypeForPropertyKindAttr()
            throws Exception {
        test(
            "cr.defineProperty(a.prototype, 'c', cr.PropertyKind.ATTR);",
            "cr.defineProperty(a.prototype, 'c', cr.PropertyKind.ATTR);\n" +
            "/** @type {string} */\n" +
            "a.prototype.c;");
    }

    public void testCrDefinePropertyDefinesUnquotedPropertyWithBooleanTypeForPropertyKindBoolAttr()
            throws Exception {
        test(
            "cr.defineProperty(a.prototype, 'c', cr.PropertyKind.BOOL_ATTR);",
            "cr.defineProperty(a.prototype, 'c', cr.PropertyKind.BOOL_ATTR);\n" +
            "/** @type {boolean} */\n" +
            "a.prototype.c;");
    }

    public void testCrDefinePropertyDefinesUnquotedPropertyWithAnyTypeForPropertyKindJs()
            throws Exception {
        test(
            "cr.defineProperty(a.prototype, 'c', cr.PropertyKind.JS);",
            "cr.defineProperty(a.prototype, 'c', cr.PropertyKind.JS);\n" +
            "/** @type {?} */\n" +
            "a.prototype.c;");
    }

    public void testCrDefinePropertyCalledWithouthThirdArgumentMeansCrPropertyKindJs()
            throws Exception {
        test(
            "cr.defineProperty(a.prototype, 'c');",
            "cr.defineProperty(a.prototype, 'c');\n" +
            "/** @type {?} */\n" +
            "a.prototype.c;");
    }

    public void testCrDefinePropertyDefinesUnquotedPropertyOnPrototypeWhenFunctionIsPassed()
            throws Exception {
        test(
            "cr.defineProperty(a, 'c', cr.PropertyKind.JS);",
            "cr.defineProperty(a, 'c', cr.PropertyKind.JS);\n" +
            "/** @type {?} */\n" +
            "a.prototype.c;");
    }

    public void testCrDefinePropertyInvalidPropertyKind()
            throws Exception {
        test(
            "cr.defineProperty(a.b, 'c', cr.PropertyKind.INEXISTENT_KIND);",
            null, ChromePass.CR_DEFINE_PROPERTY_INVALID_PROPERTY_KIND);
    }

    public void testCrExportPath() throws Exception {
        test(
            "cr.exportPath('a.b.c');",
            "var a = a || {};\n" +
            "a.b = a.b || {};\n" +
            "a.b.c = a.b.c || {};\n" +
            "cr.exportPath('a.b.c');");
    }

    public void testCrDefineCreatesEveryObjectOnlyOnce() throws Exception {
        test(
            "cr.define('a.b.c.d', function() {\n" +
            "  return {};\n" +
            "});\n" +
            "cr.define('a.b.e.f', function() {\n" +
            "  return {};\n" +
            "});",
            "var a = a || {};\n" +
            "a.b = a.b || {};\n" +
            "a.b.c = a.b.c || {};\n" +
            "a.b.c.d = a.b.c.d || {};\n" +
            "cr.define('a.b.c.d', function() {\n" +
            "  return {};\n" +
            "});\n" +
            "a.b.e = a.b.e || {};\n" +
            "a.b.e.f = a.b.e.f || {};\n" +
            "cr.define('a.b.e.f', function() {\n" +
            "  return {};\n" +
            "});");
    }

    public void testCrDefineAndCrExportPathCreateEveryObjectOnlyOnce() throws Exception {
        test(
            "cr.exportPath('a.b.c.d');\n" +
            "cr.define('a.b.e.f', function() {\n" +
            "  return {};\n" +
            "});",
            "var a = a || {};\n" +
            "a.b = a.b || {};\n" +
            "a.b.c = a.b.c || {};\n" +
            "a.b.c.d = a.b.c.d || {};\n" +
            "cr.exportPath('a.b.c.d');\n" +
            "a.b.e = a.b.e || {};\n" +
            "a.b.e.f = a.b.e.f || {};\n" +
            "cr.define('a.b.e.f', function() {\n" +
            "  return {};\n" +
            "});");
    }

    public void testCrDefineDoesntRedefineCrVar() throws Exception {
        test(
            "cr.define('cr.ui', function() {\n" +
            "  return {};\n" +
            "});",
            "cr.ui = cr.ui || {};\n" +
            "cr.define('cr.ui', function() {\n" +
            "  return {};\n" +
            "});");
    }

    public void testCrExportPathInvalidNumberOfArguments() throws Exception {
        test("cr.exportPath();", null, ChromePass.CR_EXPORT_PATH_WRONG_NUMBER_OF_ARGUMENTS);
    }

}
