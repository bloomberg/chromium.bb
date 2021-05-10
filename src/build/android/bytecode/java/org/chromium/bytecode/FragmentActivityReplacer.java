// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.bytecode;

import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Opcodes;
import org.objectweb.asm.commons.MethodRemapper;
import org.objectweb.asm.commons.Remapper;

import java.io.File;
import java.io.IOException;

/**
 * Java application that modifies Fragment.getActivity() to return an Activity instead of a
 * FragmentActivity, and updates any existing getActivity() calls to reference the updated method.
 *
 * See crbug.com/1144345 for more context.
 */
public class FragmentActivityReplacer extends ByteCodeRewriter {
    private static final String GET_ACTIVITY_METHOD_NAME = "getActivity";
    private static final String GET_LIFECYCLE_ACTIVITY_METHOD_NAME = "getLifecycleActivity";
    private static final String NEW_METHOD_DESCRIPTOR = "()Landroid/app/Activity;";
    private static final String OLD_METHOD_DESCRIPTOR =
            "()Landroidx/fragment/app/FragmentActivity;";
    private static final String REQUIRE_ACTIVITY_METHOD_NAME = "requireActivity";

    public static void main(String[] args) throws IOException {
        // Invoke this script using //build/android/gyp/bytecode_processor.py
        if (args.length != 2) {
            System.err.println("Expected 2 arguments: [input.jar] [output.jar]");
            System.exit(1);
        }

        FragmentActivityReplacer rewriter = new FragmentActivityReplacer();
        rewriter.rewrite(new File(args[0]), new File(args[1]));
    }

    @Override
    protected boolean shouldRewriteClass(String classPath) {
        return true;
    }

    @Override
    protected ClassVisitor getClassVisitorForClass(String classPath, ClassVisitor delegate) {
        ClassVisitor invocationVisitor = new InvocationReplacer(delegate);
        switch (classPath) {
            case "androidx/fragment/app/Fragment.class":
                return new FragmentClassVisitor(invocationVisitor);
            case "com/google/android/gms/common/api/internal/SupportLifecycleFragmentImpl.class":
                return new SupportLifecycleFragmentImplClassVisitor(invocationVisitor);
            default:
                return invocationVisitor;
        }
    }

    /**
     * Updates any Fragment.getActivity/requireActivity() or getLifecycleActivity() calls to call
     * the replaced method.
     */
    private static class InvocationReplacer extends ClassVisitor {
        private InvocationReplacer(ClassVisitor baseVisitor) {
            super(Opcodes.ASM7, baseVisitor);
        }

        @Override
        public MethodVisitor visitMethod(
                int access, String name, String descriptor, String signature, String[] exceptions) {
            MethodVisitor base = super.visitMethod(access, name, descriptor, signature, exceptions);
            return new MethodVisitor(Opcodes.ASM7, base) {
                @Override
                public void visitMethodInsn(int opcode, String owner, String name,
                        String descriptor, boolean isInterface) {
                    if ((opcode == Opcodes.INVOKEVIRTUAL || opcode == Opcodes.INVOKESPECIAL)
                            && descriptor.equals(OLD_METHOD_DESCRIPTOR)
                            && (name.equals(GET_ACTIVITY_METHOD_NAME)
                                    || name.equals(REQUIRE_ACTIVITY_METHOD_NAME)
                                    || name.equals(GET_LIFECYCLE_ACTIVITY_METHOD_NAME))) {
                        super.visitMethodInsn(
                                opcode, owner, name, NEW_METHOD_DESCRIPTOR, isInterface);
                    } else {
                        super.visitMethodInsn(opcode, owner, name, descriptor, isInterface);
                    }
                }
            };
        }
    }

    /**
     * Updates the implementation of Fragment.getActivity() and Fragment.requireActivity().
     */
    private static class FragmentClassVisitor extends ClassVisitor {
        private FragmentClassVisitor(ClassVisitor baseVisitor) {
            super(Opcodes.ASM7, baseVisitor);
        }

        @Override
        public MethodVisitor visitMethod(
                int access, String name, String descriptor, String signature, String[] exceptions) {
            // Update the descriptor of getActivity() and requireActivity().
            MethodVisitor baseVisitor;
            if (descriptor.equals(OLD_METHOD_DESCRIPTOR)
                    && (name.equals(GET_ACTIVITY_METHOD_NAME)
                            || name.equals(REQUIRE_ACTIVITY_METHOD_NAME))) {
                // Some Fragments in a Clank library implement an interface that defines an
                // `Activity getActivity()` method. Fragment.getActivity() is considered its
                // implementation from a typechecking perspective, but javac still generates a
                // getActivity() method in these Fragments that call Fragment.getActivity(). This
                // isn't an issue when the methods return different types, but after changing
                // Fragment.getActivity() to return an Activity, this generated implementation is
                // now overriding Fragment's, which it can't do because Fragment.getActivity() is
                // final. We make it non-final here to avoid this issue.
                baseVisitor = super.visitMethod(
                        access & ~Opcodes.ACC_FINAL, name, NEW_METHOD_DESCRIPTOR, null, exceptions);
            } else {
                baseVisitor = super.visitMethod(access, name, descriptor, signature, exceptions);
            }

            // Replace getActivity() with `return ContextUtils.activityFromContext(getContext());`
            if (name.equals(GET_ACTIVITY_METHOD_NAME) && descriptor.equals(OLD_METHOD_DESCRIPTOR)) {
                baseVisitor.visitVarInsn(Opcodes.ALOAD, 0);
                baseVisitor.visitMethodInsn(Opcodes.INVOKEVIRTUAL, "androidx/fragment/app/Fragment",
                        "getContext", "()Landroid/content/Context;", false);
                baseVisitor.visitMethodInsn(Opcodes.INVOKESTATIC, "org/chromium/utils/ContextUtils",
                        "activityFromContext", "(Landroid/content/Context;)Landroid/app/Activity;",
                        false);
                baseVisitor.visitInsn(Opcodes.ARETURN);
                return null;
            }

            return new MethodRemapper(baseVisitor, new Remapper() {
                @Override
                public String mapType(String internalName) {
                    if (internalName.equals("androidx/fragment/app/FragmentActivity")) {
                        return "android/app/Activity";
                    }
                    return internalName;
                }
            });
        }
    }

    /**
     * Update SupportLifecycleFragmentImpl.getLifecycleActivity().
     */
    private static class SupportLifecycleFragmentImplClassVisitor extends ClassVisitor {
        private SupportLifecycleFragmentImplClassVisitor(ClassVisitor baseVisitor) {
            super(Opcodes.ASM7, baseVisitor);
        }

        @Override
        public MethodVisitor visitMethod(
                int access, String name, String descriptor, String signature, String[] exceptions) {
            // SupportLifecycleFragmentImpl has two getActivity methods:
            //   1. public FragmentActivity getLifecycleActivity():
            //      This is what you'll see in the source. This delegates to Fragment.getActivity().
            //   2. public Activity getLifecycleActivity():
            //      This is generated because the class implements LifecycleFragment, which
            //      declares this method, and delegates to #1.
            //
            // Here we change the return type of #1 and delete #2.
            if (name.equals(GET_LIFECYCLE_ACTIVITY_METHOD_NAME)) {
                if (descriptor.equals(OLD_METHOD_DESCRIPTOR)) {
                    return super.visitMethod(
                            access, name, NEW_METHOD_DESCRIPTOR, signature, exceptions);
                }
                return null;
            }
            return super.visitMethod(access, name, descriptor, signature, exceptions);
        }
    }
}
