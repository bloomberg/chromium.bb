// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.testing.robolectric.template;

import java.nio.file.Path;
import java.nio.file.Paths;

/**
 *  Parses command line arguments for ProcessRobolectricTemplate.
 */
public class ProcessTemplateArgParser {

    private Path mBaseTemplateDir;
    private Path mOutputDir;
    private Integer mApiLevel;

    public static ProcessTemplateArgParser parse(String[] args) {

        ProcessTemplateArgParser parsed = new ProcessTemplateArgParser();

        for (int i = 0; i < args.length; ++i) {
            if (args[i].startsWith("-")) {
                String argName;
                if (args[i].startsWith("-", 1)) {
                    argName = args[i].substring(2, args[i].length());
                } else {
                    argName = args[i].substring(1, args[i].length());
                }
                try {
                    if ("output-dir".equals(argName)) {
                        // Read the command line argument after the flag.
                        parsed.setOutputDir(args[++i]);
                    } else if ("base-template-dir".equals(argName)) {
                        // Read the command line argument after the flag.
                        parsed.setBaseTemplateDir(args[++i]);
                    } else if ("api-level".equals(argName)) {
                        // Read the command line argument after the flag.
                        parsed.setApiLevel(args[++i]);
                    } else {
                        System.out.println("Ignoring flag: \"" + argName + "\"");
                    }
                } catch (ArrayIndexOutOfBoundsException e) {
                    System.err.println("No value specified for argument \"" + argName + "\"");
                    System.exit(1);
                }
            } else {
                System.out.println("Ignoring argument: \"" + args[i] + "\"");
            }
        }

        if (parsed.getOutputDir() == null) {
            System.err.println("--output-dir argument required.");
            System.exit(1);
        }

        if (parsed.getBaseTemplateDir() == null) {
            System.err.println("--base-template-dir argument required.");
            System.exit(1);
        }

        if (parsed.getApiLevel() == null) {
            System.err.println("--api-level argument required.");
            System.exit(1);
        }
        return parsed;
    }

    private ProcessTemplateArgParser() {
        mBaseTemplateDir = null;
        mOutputDir = null;
        mApiLevel = null;
    }

    public Path getBaseTemplateDir() {
        return mBaseTemplateDir;
    }

    public Path getOutputDir() {
        return mOutputDir;
    }

    public Integer getApiLevel() {
        return mApiLevel;
    }

    private void setBaseTemplateDir(String path) {
        mBaseTemplateDir = Paths.get(path);
    }

    private void setOutputDir(String path) {
        mOutputDir = Paths.get(path);
    }

    private void setApiLevel(String integer) {
        mApiLevel = Integer.parseInt(integer);
    }
}
