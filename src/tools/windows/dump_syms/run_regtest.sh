#!/bin/sh
Release/dump_syms.exe testdata/dump_syms_regtest.pdb > testdata/dump_syms_regtest.new
if diff -u testdata/dump_syms_regtest.new testdata/dump_syms_regtest.out >& testdata/dump_syms_regtest.diff; then
  rm testdata/dump_syms_regtest.diff testdata/dump_syms_regtest.new
  echo "PASS"
else
  echo "FAIL, see testdata/dump_syms_regtest.[new|diff]"
fi
