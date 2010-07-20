#!/bin/bash
dir=$(dirname $0)
perl -I ${dir}/bin/ ${dir}/bin/specdiff "$@"
