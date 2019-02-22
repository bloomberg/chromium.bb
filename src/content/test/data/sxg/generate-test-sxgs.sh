#!/bin/sh

# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

for cmd in gen-signedexchange gen-certurl dump-signedexchange; do
    if ! command -v $cmd > /dev/null 2>&1; then
        echo "$cmd is not installed. Please run:"
        echo "  go get -u github.com/WICG/webpackage/go/signedexchange/cmd/..."
        exit 1
    fi
done

dumpSignature() {
  echo "constexpr char $1[] = R\"($(dump-signedexchange -signature -i $2))\";"
}

tmpdir=$(mktemp -d)
sctdir=$tmpdir/scts
mkdir $sctdir

# Make dummy OCSP and SCT data for cbor certificate chains.
echo -n OCSP >$tmpdir/ocsp; echo -n SCT >$sctdir/dummy.sct

# Generate the certificate chain of "*.example.org".
gen-certurl -pem prime256v1-sha256.public.pem \
  -ocsp $tmpdir/ocsp -sctDir $sctdir > test.example.org.public.pem.cbor

# Generate the certificate chain of "*.example.org", without
# CanSignHttpExchangesDraft extension.
gen-certurl -pem prime256v1-sha256-noext.public.pem \
  -ocsp $tmpdir/ocsp -sctDir $sctdir > test.example.org-noext.public.pem.cbor

# Generate the signed exchange file.
gen-signedexchange \
  -version 1b2 \
  -uri https://test.example.org/test/ \
  -status 200 \
  -content test.html \
  -certificate prime256v1-sha256.public.pem \
  -certUrl https://cert.example.org/cert.msg \
  -validityUrl https://test.example.org/resource.validity.msg \
  -privateKey prime256v1.key \
  -date 2018-03-12T05:53:20Z \
  -o test.example.org_test.sxg \
  -miRecordSize 100

# Generate the signed exchange for the invalid content-type test case.
cp test.example.org_test.sxg test.example.org_test_invalid_content_type.sxg

# Generate the signed exchange file with invalid magic string
xxd -p test.example.org_test.sxg |
  sed '1s/^737867312d623200/737867312d787800/' |
  xxd -r -p > test.example.org_test_invalid_magic_string.sxg

# Generate the signed exchange file with invalid cbor header.
# 0x82 : start array of 2 elements.
# 0xa1 : start map of 1 element -> 0xa4 : 4 elements.
xxd -p test.example.org_test.sxg |
  tr -d '\n' |
  sed 's/82a1/82a4/' |
  xxd -r -p > test.example.org_test_invalid_cbor_header.sxg

# Generate the signed exchange file with noext certificate
gen-signedexchange \
  -version 1b2 \
  -uri https://test.example.org/test/ \
  -status 200 \
  -content test.html \
  -certificate prime256v1-sha256-noext.public.pem \
  -certUrl https://cert.example.org/cert.msg \
  -validityUrl https://test.example.org/resource.validity.msg \
  -privateKey prime256v1.key \
  -date 2018-03-12T05:53:20Z \
  -o test.example.org_noext_test.sxg \
  -miRecordSize 100

# Generate the signed exchange file with invalid URL.
gen-signedexchange \
  -version 1b2 \
  -uri https://test.example.com/test/ \
  -status 200 \
  -content test.html \
  -certificate prime256v1-sha256.public.pem \
  -certUrl https://cert.example.org/cert.msg \
  -validityUrl https://test.example.com/resource.validity.msg \
  -privateKey prime256v1.key \
  -date 2018-03-12T05:53:20Z \
  -o test.example.com_invalid_test.sxg \
  -miRecordSize 100

# Generate the signed exchange for a plain text file.
gen-signedexchange \
  -version 1b2 \
  -uri https://test.example.org/hello.txt \
  -status 200 \
  -content hello.txt \
  -certificate prime256v1-sha256.public.pem \
  -certUrl https://cert.example.org/cert.msg \
  -validityUrl https://test.example.org/resource.validity.msg \
  -privateKey prime256v1.key \
  -responseHeader 'Content-Type: text/plain; charset=iso-8859-1' \
  -date 2018-03-12T05:53:20Z \
  -o test.example.org_hello.txt.sxg

echo "Update the test signatures in "
echo "signed_exchange_signature_verifier_unittest.cc with the followings:"
echo "===="

sed -ne '/-BEGIN PRIVATE KEY-/,/-END PRIVATE KEY-/p' \
  ../../../../net/data/ssl/certificates/wildcard.pem \
  > $tmpdir/wildcard_example.org.private.pem
sed -ne '/-BEGIN CERTIFICATE-/,/-END CERTIFICATE-/p' \
  ../../../../net/data/ssl/certificates/wildcard.pem \
  > $tmpdir/wildcard_example.org.public.pem
gen-signedexchange \
  -version 1b2 \
  -uri https://test.example.org/test/ \
  -content test.html \
  -certificate $tmpdir/wildcard_example.org.public.pem \
  -privateKey $tmpdir/wildcard_example.org.private.pem \
  -date 2018-02-06T04:45:41Z \
  -o $tmpdir/out.htxg

dumpSignature kSignatureHeaderRSA $tmpdir/out.htxg

gen-signedexchange \
  -version 1b2 \
  -uri https://test.example.org/test/ \
  -content test.html \
  -certificate ./prime256v1-sha256.public.pem \
  -privateKey ./prime256v1.key \
  -date 2018-02-06T04:45:41Z \
  -o $tmpdir/out.htxg \
  -dumpHeadersCbor $tmpdir/out.cborheader

dumpSignature kSignatureHeaderECDSAP256 $tmpdir/out.htxg

echo 'constexpr uint8_t kCborHeaderECDSAP256[] = {'
xxd --include $tmpdir/out.cborheader | sed '1d;$d'

gen-signedexchange \
  -version 1b2 \
  -uri https://test.example.org/test/ \
  -content test.html \
  -certificate ./secp384r1-sha256.public.pem \
  -privateKey ./secp384r1.key \
  -date 2018-02-06T04:45:41Z \
  -o $tmpdir/out.htxg

dumpSignature kSignatureHeaderECDSAP384 $tmpdir/out.htxg

echo "===="

rm -fr $tmpdir
