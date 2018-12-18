#!/bin/sh

ret=0
fail() {
    echo "FAILED: $1"
    ret=1
}
check() {
    grep -q "$2" "$1" || fail "$2"
}

tempfile=$(mktemp)
npm run cts > $tempfile

check $tempfile THIS_LINE_WAS_HIT:69078bb3-72c8-4d17-9937-aef9158bfef9
check $tempfile THIS_LINE_WAS_HIT:b7782904-3770-49f3-99c0-f461b557eea6
check $tempfile THIS_LINE_WAS_HIT:a25d4b04-b6c5-4fca-aff8-80ff632bf887
check $tempfile THIS_LINE_WAS_HIT:d194f5ea-c4b3-4eb4-a283-30690834cf22
check $tempfile THIS_LINE_WAS_HIT:452a6787-ba57-4d43-a02e-6201875afb0d
check $tempfile THIS_LINE_WAS_HIT:4829aefc-5908-4439-88cf-bb5fc5d7a9fe
check $tempfile THIS_LINE_WAS_HIT:38442515-c553-4dc6-9cfb-03e0c82cda33
check $tempfile THIS_LINE_WAS_HIT:34e4ea2b-5115-438b-a52e-d32cbbe3b6c3
check $tempfile THIS_LINE_WAS_HIT:6d73faad-ca77-426a-8dc6-900cca1523eb
check $tempfile THIS_LINE_WAS_HIT:bf0cd187-6ec7-4f8b-b6c8-7acd27db00ee
check $tempfile THIS_LINE_WAS_HIT:73d58cf6-bbbb-401b-b686-612e89cd9967
check $tempfile THIS_LINE_WAS_HIT:915d4cc4-1a20-4c2c-abfe-630cc033108c

rm $tempfile
exit $ret
