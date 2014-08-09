// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.javascript.jscomp;

import com.google.javascript.jscomp.NodeTraversal.AbstractPostOrderCallback;
import com.google.javascript.rhino.IR;
import com.google.javascript.rhino.Node;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Compiler pass for Chrome-specific needs. Right now it allows the compiler to check types with
 * methods defined inside Chrome namespaces.
 *
 * <p>The namespaces in Chrome JS are declared as follows:
 * <pre>
 * cr.define('my.namespace.name', function() {
 *   /** @param {number} arg
 *   function internalStaticMethod(arg) {}
 *
 *   /** @constructor
 *   function InternalClass() {}
 *
 *   InternalClass.prototype = {
 *     /** @param {number} arg
 *     method: function(arg) {
 *       internalStaticMethod(arg);  // let's demonstrate the change of local names after our pass
 *     }
 *   };
 *
 *   return {
 *     externalStaticMethod: internalStaticMethod,
 *     ExternalClass: InternalClass
 *   }
 * });
 * </pre>
 *
 * <p>Outside of cr.define() statement the function can be called like this:
 * {@code my.namespace.name.externalStaticMethod(42);}.
 *
 * <p>We need to check the types of arguments and return values of such functions. However, the
 * function is assigned to its namespace dictionary only at run-time and the original Closure
 * Compiler isn't smart enough to predict behavior in that case. Therefore, we need to modify the
 * AST before any compiler checks. That's how we modify it to tell the compiler what's going on:
 *
 * <pre>
 * var my = my || {};
 * my.namespace = my.namespace || {};
 * my.namespace.name = my.namespace.name || {};
 *
 * cr.define('my.namespace.name', function() {
 *   /** @param {number} arg
 *   my.namespace.name.externalStaticMethod(arg) {}
 *
 *   /** @constructor
 *   my.namespace.name.ExternalClass = function() {};
 *
 *   my.namespace.name.ExternalClass.prototype = {
 *     /** @param {number} arg
 *     method: function(arg) {
 *       my.namespace.name.externalStaticMethod(arg);  // see, it has been changed
 *     }
 *   };
 *
 *   return {
 *     externalStaticMethod: my.namespace.name.externalStaticMethod,
 *     ExternalClass: my.namespace.name.ExternalClass
 *   }
 * });
 * </pre>
 */
public class ChromePass extends AbstractPostOrderCallback implements CompilerPass {
    final AbstractCompiler compiler;

    private static final String CR_DEFINE = "cr.define";

    private static final String CR_DEFINE_COMMON_EXPLANATION = "It should be called like this: " +
            "cr.define('name.space', function() '{ ... return {Export: Internal}; }');";

    static final DiagnosticType CR_DEFINE_WRONG_NUMBER_OF_ARGUMENTS =
            DiagnosticType.error("JSC_CR_DEFINE_WRONG_NUMBER_OF_ARGUMENTS",
                    "cr.define() should have exactly 2 arguments. " + CR_DEFINE_COMMON_EXPLANATION);

    static final DiagnosticType CR_DEFINE_INVALID_FIRST_ARGUMENT =
            DiagnosticType.error("JSC_CR_DEFINE_INVALID_FIRST_ARGUMENT",
                    "Invalid first argument for cr.define(). " + CR_DEFINE_COMMON_EXPLANATION);

    static final DiagnosticType CR_DEFINE_INVALID_SECOND_ARGUMENT =
            DiagnosticType.error("JSC_CR_DEFINE_INVALID_SECOND_ARGUMENT",
                    "Invalid second argument for cr.define(). " + CR_DEFINE_COMMON_EXPLANATION);

    static final DiagnosticType CR_DEFINE_INVALID_RETURN_IN_FUNCTION =
            DiagnosticType.error("JSC_CR_DEFINE_INVALID_RETURN_IN_SECOND_ARGUMENT",
                    "Function passed as second argument of cr.define() should return the " +
                    "dictionary in its last statement. " + CR_DEFINE_COMMON_EXPLANATION);

    public ChromePass(AbstractCompiler compiler) {
        this.compiler = compiler;
    }

    @Override
    public void process(Node externs, Node root) {
        NodeTraversal.traverse(compiler, root, this);
    }

    @Override
    public void visit(NodeTraversal t, Node node, Node parent) {
        if (node.isCall()) {
            Node callee = node.getFirstChild();
            if (callee.matchesQualifiedName(CR_DEFINE)) {
                visitNamespaceDefinition(node, parent);
                compiler.reportCodeChange();
            }
        }
    }

    private void visitNamespaceDefinition(Node crDefineCallNode, Node parent) {
        if (crDefineCallNode.getChildCount() != 3) {
            compiler.report(JSError.make(crDefineCallNode, CR_DEFINE_WRONG_NUMBER_OF_ARGUMENTS));
        }

        Node namespaceArg = crDefineCallNode.getChildAtIndex(1);
        Node function = crDefineCallNode.getChildAtIndex(2);

        if (!namespaceArg.isString()) {
            compiler.report(JSError.make(namespaceArg, CR_DEFINE_INVALID_FIRST_ARGUMENT));
            return;
        }

        // TODO(vitalyp): Check namespace name for validity here. It should be a valid chain of
        // identifiers.
        String namespace = namespaceArg.getString();

        List<Node> objectsForQualifiedName = createObjectsForQualifiedName(namespace);
        for (Node n : objectsForQualifiedName) {
            parent.getParent().addChildBefore(n, parent);
        }

        Node returnNode, functionBlock, objectLit;
        if (!function.isFunction()) {
            compiler.report(JSError.make(namespaceArg, CR_DEFINE_INVALID_SECOND_ARGUMENT));
            return;
        }

        if ((functionBlock = function.getLastChild()) == null ||
                (returnNode = functionBlock.getLastChild()) == null ||
                !returnNode.isReturn() ||
                (objectLit = returnNode.getFirstChild()) == null ||
                !objectLit.isObjectLit()) {
            compiler.report(JSError.make(namespaceArg, CR_DEFINE_INVALID_RETURN_IN_FUNCTION));
            return;
        }

        Map<String, String> exports = objectLitToMap(objectLit);

        NodeTraversal.traverse(compiler, functionBlock, new RenameInternalsToExternalsCallback(
                namespace, exports, functionBlock));
    }

    private Map<String, String> objectLitToMap(Node objectLit) {
        Map<String, String> res = new HashMap<String, String>();

        for (Node keyNode : objectLit.children()) {
            String key = keyNode.getString();

            // TODO(vitalyp): Can dict value be other than a simple NAME? What if NAME doesn't
            // refer to a function/constructor?
            String value = keyNode.getFirstChild().getString();

            res.put(value, key);
        }

        return res;
    }

    /**
     * For a string "a.b.c" produce the following JS IR:
     *
     * <p><pre>
     * var a = a || {};
     * a.b = a.b || {};
     * a.b.c = a.b.c || {};</pre>
     */
    private List<Node> createObjectsForQualifiedName(String namespace) {
        List<Node> objects = new ArrayList<>();
        String[] parts = namespace.split("\\.");

        objects.add(createJsNode("var " + parts[0] + " = " + parts[0] + " || {};"));

        if (parts.length >= 2) {
            StringBuilder currPrefix = new StringBuilder().append(parts[0]);
            for (int i = 1; i < parts.length; ++i) {
                currPrefix.append(".").append(parts[i]);
                String code = currPrefix + " = " + currPrefix + " || {};";
                objects.add(createJsNode(code));
            }
        }

        return objects;
    }

    private Node createJsNode(String code) {
        // The parent node after parseSyntheticCode() is SCRIPT node, we need to get rid of it.
        return compiler.parseSyntheticCode(code).removeFirstChild();
    }

    private class RenameInternalsToExternalsCallback extends AbstractPostOrderCallback {
        final private String namespaceName;
        final private Map<String, String> exports;
        final private Node namespaceBlock;

        public RenameInternalsToExternalsCallback(String namespaceName,
                Map<String, String> exports, Node namespaceBlock) {
            this.namespaceName = namespaceName;
            this.exports = exports;
            this.namespaceBlock = namespaceBlock;
        }

        @Override
        public void visit(NodeTraversal t, Node n, Node parent) {
            if (n == null) {
                return;
            }

            if (n.isFunction() && parent == this.namespaceBlock &&
                    this.exports.containsKey(n.getFirstChild().getString())) {
                // It's a top-level function/constructor definition.
                //
                // Change
                //
                //   /** Some doc */
                //   function internalName() {}
                //
                // to
                //
                //   /** Some doc */
                //   my.namespace.name.externalName = function internalName() {};
                //
                // by looking up in this.exports for internalName to find the correspondent
                // externalName.
                Node functionTree = n.cloneTree();
                Node exprResult = IR.exprResult(
                        IR.assign(buildQualifiedName(n.getFirstChild()), functionTree));
                NodeUtil.setDebugInformation(exprResult, n, n.getFirstChild().getString());
                if (n.getJSDocInfo() != null) {
                    exprResult.getFirstChild().setJSDocInfo(n.getJSDocInfo());
                    functionTree.removeProp(Node.JSDOC_INFO_PROP);
                }
                this.namespaceBlock.replaceChild(n, exprResult);
            } else if (n.isName() && this.exports.containsKey(n.getString()) &&
                    !parent.isFunction()) {
                if (parent.isVar()) {
                    if (parent.getParent() == this.namespaceBlock) {
                        // It's a top-level exported variable definition.
                        // Change
                        //
                        //   var enum = { 'one': 1, 'two': 2 };
                        //
                        // to
                        //
                        //   my.namespace.name.enum = { 'one': 1, 'two': 2 };
                        Node varContent = n.removeFirstChild();
                        Node exprResult = IR.exprResult(
                                IR.assign(buildQualifiedName(n), varContent));
                        NodeUtil.setDebugInformation(exprResult, parent, n.getString());
                        if (parent.getJSDocInfo() != null) {
                            exprResult.getFirstChild().setJSDocInfo(parent.getJSDocInfo().clone());
                        }
                        this.namespaceBlock.replaceChild(parent, exprResult);
                    }
                } else {
                    // It's a local name referencing exported entity. Change to its global name.
                    Node newNode = buildQualifiedName(n);
                    if (n.getJSDocInfo() != null) {
                        newNode.setJSDocInfo(n.getJSDocInfo().clone());
                    }

                    // If we alter the name of a called function, then it gets an explicit "this"
                    // value.
                    if (parent.isCall()) {
                        parent.putBooleanProp(Node.FREE_CALL, false);
                    }

                    parent.replaceChild(n, newNode);
                }
            }
        }

        private Node buildQualifiedName(Node internalName) {
            String externalName = this.exports.get(internalName.getString());
            return NodeUtil.newQualifiedNameNode(compiler.getCodingConvention(),
                                                 this.namespaceName + "." + externalName,
                                                 internalName,
                                                 internalName.getString());
        }
    }
}
