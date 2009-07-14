#!/bin/bash
program=${1:-switch}
(i=0; while [ $i -lt 100 ]; do time ./$program; i=$(($i + 1)); done) > timing.$program.stdout.txt 2> timing.$program.stderr.txt
