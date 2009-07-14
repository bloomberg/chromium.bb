#!/bin/sh
perl -ane 'if ($_ =~ /real/ && $F[1] =~ /0m([\\.0-9]*)s/) { print $F[1], ",", $1, "\n"; $sum += $1; $n++ } END { print $sum, ",", $n, "\n"; print "avg ", $sum/$n, "\n";} '
