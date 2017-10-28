#!/usr/bin/env bash

yaml_test=$1

case $yaml_test in
    ./braille-specs/*)
        export LOUIS_TABLEPATH=${LOUIS_TABLEPATH}/tables
        ;;
    *)
        ;;
esac
${WINE} lou_checkyaml${EXEEXT} $yaml_test
