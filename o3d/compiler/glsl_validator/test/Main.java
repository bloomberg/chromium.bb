/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

package test;

import java.io.*;
import org.antlr.runtime.*;
import glsl_es.*;

public class Main {
  public static void main(String[] args) {
    for (String str : args) {
      parseFile(str);
    }
  }

  private static void parseFile(String f) {
    try {
      long startTime = System.currentTimeMillis();
      GLSL_ESLexer lexer = new GLSL_ESLexer(new ANTLRFileStream(f, "iso8859-1"));
      TokenRewriteStream tokens = new TokenRewriteStream(lexer);
      tokens.LT(1);	
      // Create a parser that reads from the scanner
      GLSL_ESParser parser = new GLSL_ESParser(tokens);
      // Parse the translation unit
      parser.translation_unit();
      long endTime = System.currentTimeMillis();
      System.out.println("Parsing " + f + " took " + (endTime - startTime) + " ms");
    } catch (Exception e) {
      System.out.println("While parsing " + f + ":");
      e.printStackTrace();
    }
  }
}
