// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.testing.robolectric.template;

import org.apache.velocity.Template;
import org.apache.velocity.VelocityContext;
import org.apache.velocity.app.Velocity;
import org.apache.velocity.exception.VelocityException;
import org.apache.velocity.runtime.RuntimeConstants;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.StringWriter;
import java.nio.file.PathMatcher;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.FileSystems;
import java.nio.file.SimpleFileVisitor;
import java.nio.file.attribute.BasicFileAttributes;
import java.nio.file.FileVisitResult;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.List;

/**
 * Class to process Robolectric template (*.vm) files using Apache Velocity.
 */
public final class ProcessRobolectricTemplate {

    private ProcessRobolectricTemplate() {
    }

    public static void main(String[] args) {
        final ProcessTemplateArgParser parser = ProcessTemplateArgParser.parse(args);

        Velocity.setProperty(RuntimeConstants.RESOURCE_LOADER, "file");
        Velocity.setProperty(RuntimeConstants.FILE_RESOURCE_LOADER_PATH,
                parser.getBaseTemplateDir().toString());
        Velocity.init();

        final VelocityContext context = new VelocityContext();
        final int api = parser.getApiLevel();
        context.put("api", api);
        if (api >= 21) {
            context.put("ptrClass", "long");
            context.put("ptrClassBoxed", "Long");
        } else {
            context.put("ptrClass", "int");
            context.put("ptrClassBoxed", "Integer");
        }

        try {
            final PathMatcher templatePathMatcher =
                    FileSystems.getDefault().getPathMatcher("glob:" + "**/*.vm");
            Files.walkFileTree(parser.getBaseTemplateDir(), new SimpleFileVisitor<Path>() {
                @Override
                public FileVisitResult visitFile(
                        Path path, BasicFileAttributes attrs) throws IOException {
                    if (templatePathMatcher.matches(path)) {
                        processTemplate(context, path, parser.getBaseTemplateDir(), parser.getOutputDir(), api);
                    }
                    return FileVisitResult.CONTINUE;
                }
            });
        } catch (IOException e) {
            System.err.println("Error processing template files for Robolectric! " + e.toString());
        }
    }

    private static void processTemplate(VelocityContext context, Path templateFile, Path baseTemplateDir, Path outputDir, int api_level) throws IOException {
        final StringWriter stringWriter = new StringWriter();
        Template template = Velocity.getTemplate(baseTemplateDir.relativize(templateFile).toString(), "UTF-8");
        template.merge(context, stringWriter);

        String templateFilename = templateFile.getFileName().toString();
        String processedFilename = "" + api_level + File.separator + templateFilename.replace(".vm", "");

        String relativeOutputFile = templateFile.toString().replace(baseTemplateDir.toString(), "").replace(templateFilename, processedFilename);

        if (relativeOutputFile.startsWith("/")) {
            relativeOutputFile = relativeOutputFile.substring(1);
        }
        Path outputFile = outputDir.resolve(relativeOutputFile);
        if (!Files.exists(outputFile.getParent())) {
            Files.createDirectories(outputFile.getParent());
        }
        Files.write(outputFile, stringWriter.toString().getBytes("UTF-8"));
    }
}