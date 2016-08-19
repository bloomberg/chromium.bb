// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.testing.robolectric.template;

import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.LinkedList;
import java.util.List;

/**
 *  Parses command line arguments for ProcessRobolectricTemplate.
 */
public class ProcessTemplateArgParser {

    private List<TemplateFileInfo> mTemplateFileInfoList;
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
                    if ("process-file".equals(argName)) {
                        // Read the two command line arguments after the flag.
                        // Format is --process-file <template file> <output file>.
                        // Argument can be passed multiple times.
                        Path templatePath = Paths.get(args[++i]);
                        Path outputPath = Paths.get(args[++i]);
                        parsed.addTemplateFileInfo(
                                new TemplateFileInfo(templatePath, outputPath));
                    } else if ("api-level".equals(argName)) {
                        // Read the command line argument after the flag.
                        parsed.setApiLevel(args[++i]);
                    } else if ("base-template-dir".equals(argName)) {
                        // Read the command line argument after the flag.
                        parsed.setBaseTemplateDir(args[++i]);
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
        mApiLevel = null;
        mBaseTemplateDir = null;
        mOutputDir = null;
        mTemplateFileInfoList = new LinkedList<TemplateFileInfo>();
    }

    public Integer getApiLevel() {
        return mApiLevel;
    }

    public Path getBaseTemplateDir() {
        return mBaseTemplateDir;
    }

    public List<TemplateFileInfo> getTemplateFileInfoList() {
        return mTemplateFileInfoList;
    }

    private void setApiLevel(String integer) {
        mApiLevel = Integer.parseInt(integer);
    }

    private void setBaseTemplateDir(String path) {
        mBaseTemplateDir = Paths.get(path);
    }

    private void addTemplateFileInfo(TemplateFileInfo templateFileInfo) {
        mTemplateFileInfoList.add(templateFileInfo);
    }
}
