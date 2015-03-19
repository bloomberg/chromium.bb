#!/bin/sh
readonly LIGHT_OPTS="\
    --closure-count=10 \
    --function-count=10 \
    --inner-function-count=10 \
    --function-call-count=1 \
    --closure-call-count=1 \
    --inner-function-line-count=1 \
    --loop-count=5"
readonly MEDIUM_OPTS="\
    --closure-count=50 \
    --function-count=50 \
    --inner-function-count=20 \
    --function-call-count=5 \
    --closure-call-count=5 \
    --inner-function-line-count=2 \
    --loop-count=5"
readonly HEAVY_OPTS="\
    --closure-count=200 \
    --function-count=200 \
    --inner-function-count=40 \
    --function-call-count=10 \
    --closure-call-count=10 \
    --inner-function-line-count=3 \
    --loop-count=5"

function generate {
  local generator="./second_batch_js_generator.py"
  local header="// Generated with $generator $@"
  {
    echo $header;
    $generator $@ | yui-compressor --type js;
  }
}

generate $LIGHT_OPTS > second_batch_js_light.min.js
generate $MEDIUM_OPTS > second_batch_js_medium.min.js
generate $HEAVY_OPTS > second_batch_js_heavy.min.js
