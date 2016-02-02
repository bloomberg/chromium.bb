#!/bin/bash

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

if [ "$#" -ne 2 ]
then
  echo "Usage: $0 <src_dir> <dst_dir>"
  echo
  echo "Extracts inlined scripts from <src_dir> to <dst_dir>" \
       "Polymer HTML files found in the destination directory to separate JS" \
       "files. A JS file extracted from the file with name 'foo.html' will" \
       "have a name 'foo-extracted.js'. Inclusion of the script file will be" \
       "added to 'foo.html': '<script src=\"foo-extracted.js\"></script>'."
  exit 1
fi

src="${1%/}"
dst="${2%/}"

rsync -c -r -v --exclude="compiled_resources*.gyp" "$src/" "$dst/"

find "$dst" -name "*.html" \
            -not -path "*/demos/*" \
            -not -path "*/test/*" \
            -not -path "*/tests/*" \
            -not -name "demo*.html" \
            -not -name "index.html" \
            -not -name "metadata.html" | \
xargs grep -l "<script>" | \
while read original_html_name
do
  dir=$(dirname "$original_html_name")
  name=$(basename "$original_html_name" .html)

  html_without_js="$dir/$name-extracted.html"
  extracted_js="$dir/$name-extracted.js"
  echo "Crisping $original_html_name"
  crisper --script-in-head=false --source "$original_html_name" \
      --html "$html_without_js" --js "$extracted_js"
  mv "$html_without_js" "$original_html_name"
done
