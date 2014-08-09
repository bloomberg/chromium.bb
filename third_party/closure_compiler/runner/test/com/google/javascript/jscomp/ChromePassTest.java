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

    public void testCrDefineReassignsExportedFunctionByQualifiedName() throws Exception {
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

}
