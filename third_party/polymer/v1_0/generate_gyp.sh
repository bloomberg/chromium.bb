#!/bin/sh

dirs=$(find components-chromium -type d | grep -v 'components-chromium/polymer')

for dir in $dirs; do
  htmls=$(\ls $dir/*.html 2>/dev/null | egrep -v '(demo|index)\.html')
  if [ "$htmls" ]; then
    echo "Creating $dir/compiled_resources2.gyp"
    ../../../tools/polymer/generate_compiled_resources_gyp.py $htmls > \
        "$dir/compiled_resources2.gyp"
  fi
done
