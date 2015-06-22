// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.errorprone;

import com.google.errorprone.BugPattern;
import com.google.errorprone.ErrorProneOptions;
import com.google.errorprone.ErrorProneScanner;
import com.google.errorprone.ErrorReportingJavaCompiler;
import com.google.errorprone.JDKCompatible;
import com.google.errorprone.Scanner;
import com.google.errorprone.bugpatterns.BugChecker;

import com.sun.tools.javac.file.JavacFileManager;
import com.sun.tools.javac.main.Main;
import com.sun.tools.javac.util.Context;
import com.sun.tools.javac.util.List;

import java.io.PrintWriter;
import java.util.HashSet;
import java.util.Set;

import javax.tools.JavaFileObject;

/**
 * Configures (and compiles with) the error-prone java compiler.
 */
public class ChromiumErrorProneCompiler {

    public static void main(String[] args) {
        System.exit(compile(args));
    }

    private static int compile(String[] args) {
        PrintWriter printWriter = new PrintWriter(System.err, true);
        Main main = new Main("javac (chromium-error-prone)", printWriter);
        Context context = new Context();
        JavacFileManager.preRegister(context);

        ErrorProneOptions epOptions = ErrorProneOptions.processArgs(args);
        final Set<String> disabledChecks = new HashSet<String>(epOptions.getDisabledChecks());

        Scanner scannerInContext = new ErrorProneScanner(new ErrorProneScanner.EnabledPredicate() {
            @Override
            public boolean isEnabled(Class<? extends BugChecker> check, BugPattern annotation) {
                return !disabledChecks.contains(check.getCanonicalName());
            }
        });
        context.put(Scanner.class, scannerInContext);

        ErrorReportingJavaCompiler.preRegister(context);
        return JDKCompatible.runCompile(
                main, epOptions.getRemainingArgs(), context, List.<JavaFileObject>nil(), null);
    }

}
