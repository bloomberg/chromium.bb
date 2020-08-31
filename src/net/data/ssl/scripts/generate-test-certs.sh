#!/bin/sh

# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script generates a set of test (end-entity, intermediate, root)
# certificates that can be used to test fetching of an intermediate via AIA.
set -e -x

# The maximum lifetime for any certificates that may go through a "real"
# cert verifier. This is effectively:
# min(OS verifier max lifetime for local certs, built-in verifier max lifetime
#     for local certs)
#
# The current built-in verifier max lifetime is 39 months
# The current OS verifier max lifetime is 825 days, which comes from
#   iOS 13/macOS 10.15 - https://support.apple.com/en-us/HT210176
# 731 is used here as just a short-hand for 2 years
CERT_LIFETIME=730

rm -rf out
mkdir out
mkdir out/int

openssl rand -hex -out out/2048-sha256-root-serial 16
touch out/2048-sha256-root-index.txt

# Generate the key or copy over the existing one if present.
if [ -f ../certificates/root_ca_cert.pem ]; then
  openssl rsa -in ../certificates/root_ca_cert.pem -out out/2048-sha256-root.key
else
  openssl genrsa -out out/2048-sha256-root.key 2048
fi

# Generate the root certificate
CA_NAME="req_ca_dn" \
  openssl req \
    -new \
    -key out/2048-sha256-root.key \
    -out out/2048-sha256-root.req \
    -config ca.cnf

CA_NAME="req_ca_dn" \
  openssl x509 \
    -req -days 3650 \
    -in out/2048-sha256-root.req \
    -signkey out/2048-sha256-root.key \
    -extfile ca.cnf \
    -extensions ca_cert \
    -text > out/2048-sha256-root.pem

# Generate the test intermediate
openssl rand -hex -out out/int/2048-sha256-int-serial 16
touch out/int/2048-sha256-int-index.txt

# Copy over an existing key if present.
if [ -f ../certificates/intermediate_ca_cert.pem ]; then
  openssl rsa -in ../certificates/intermediate_ca_cert.pem \
    -out out/int/2048-sha256-int.key
else
  openssl genrsa -out out/int/2048-sha256-int.key 2048
fi

CA_NAME="req_intermediate_dn" \
  openssl req \
    -new \
    -key out/int/2048-sha256-int.key \
    -out out/int/2048-sha256-int.req \
    -config ca.cnf

CA_NAME="req_intermediate_dn" \
  openssl ca \
    -batch \
    -extensions ca_cert \
    -days 3650 \
    -in out/int/2048-sha256-int.req \
    -out out/int/2048-sha256-int.pem \
    -config ca.cnf

# Generate the leaf certificate requests
openssl req \
  -new \
  -keyout out/expired_cert.key \
  -out out/expired_cert.req \
  -config ee.cnf

openssl req \
  -new \
  -keyout out/ok_cert.key \
  -out out/ok_cert.req \
  -config ee.cnf

openssl req \
  -new \
  -keyout out/wildcard.key \
  -out out/wildcard.req \
  -reqexts req_wildcard \
  -config ee.cnf

SUBJECT_NAME="req_localhost_cn" \
openssl req \
  -new \
  -keyout out/localhost_cert.key \
  -out out/localhost_cert.req \
  -reqexts req_localhost_san \
  -config ee.cnf

openssl req \
  -new \
  -keyout out/test_names.key \
  -out out/test_names.req \
  -reqexts req_test_names \
  -config ee.cnf

# Generate the leaf certificates
CA_NAME="req_ca_dn" \
  openssl ca \
    -batch \
    -extensions user_cert \
    -startdate 060101000000Z \
    -enddate 070101000000Z \
    -in out/expired_cert.req \
    -out out/expired_cert.pem \
    -config ca.cnf

CA_NAME="req_ca_dn" \
  openssl ca \
    -batch \
    -extensions user_cert \
    -days ${CERT_LIFETIME} \
    -in out/ok_cert.req \
    -out out/ok_cert.pem \
    -config ca.cnf

CA_DIR="out/int" \
CERT_TYPE="int" \
CA_NAME="req_intermediate_dn" \
  openssl ca \
    -batch \
    -extensions user_cert \
    -days ${CERT_LIFETIME} \
    -in out/ok_cert.req \
    -out out/int/ok_cert.pem \
    -config ca.cnf

CA_NAME="req_ca_dn" \
  openssl ca \
    -batch \
    -extensions user_cert \
    -in out/wildcard.req \
    -out out/wildcard.pem \
    -config ca.cnf

CA_NAME="req_ca_dn" \
  openssl ca \
    -batch \
    -extensions name_constraint_bad \
    -subj "/CN=Leaf certificate/" \
    -days ${CERT_LIFETIME} \
    -in out/ok_cert.req \
    -out out/name_constraint_bad.pem \
    -config ca.cnf

CA_NAME="req_ca_dn" \
  openssl ca \
    -batch \
    -extensions name_constraint_good \
    -subj "/CN=Leaf Certificate/" \
    -days ${CERT_LIFETIME} \
    -in out/ok_cert.req \
    -out out/name_constraint_good.pem \
    -config ca.cnf

CA_NAME="req_ca_dn" \
  openssl ca \
    -batch \
    -extensions user_cert \
    -days ${CERT_LIFETIME} \
    -in out/localhost_cert.req \
    -out out/localhost_cert.pem \
    -config ca.cnf

CA_NAME="req_ca_dn" \
  openssl ca \
    -batch \
    -extensions user_cert \
    -subj "/CN=Leaf Certificate/" \
    -startdate 00010101000000Z \
    -enddate   00010101000000Z \
    -in out/ok_cert.req \
    -out out/bad_validity.pem \
    -config ca.cnf

CA_NAME="req_ca_dn" \
  openssl ca \
    -batch \
    -extensions user_cert \
    -days ${CERT_LIFETIME} \
    -in out/test_names.req \
    -out out/test_names.pem \
    -config ca.cnf

/bin/sh -c "cat out/ok_cert.key out/ok_cert.pem \
    > ../certificates/ok_cert.pem"
/bin/sh -c "cat out/wildcard.key out/wildcard.pem \
    > ../certificates/wildcard.pem"
/bin/sh -c "cat out/localhost_cert.key out/localhost_cert.pem \
    > ../certificates/localhost_cert.pem"
/bin/sh -c "cat out/expired_cert.key out/expired_cert.pem \
    > ../certificates/expired_cert.pem"
/bin/sh -c "cat out/2048-sha256-root.key out/2048-sha256-root.pem \
    > ../certificates/root_ca_cert.pem"
/bin/sh -c "cat out/ok_cert.key out/name_constraint_bad.pem \
    > ../certificates/name_constraint_bad.pem"
/bin/sh -c "cat out/ok_cert.key out/name_constraint_good.pem \
    > ../certificates/name_constraint_good.pem"
/bin/sh -c "cat out/ok_cert.key out/bad_validity.pem \
    > ../certificates/bad_validity.pem"
/bin/sh -c "cat out/ok_cert.key out/int/ok_cert.pem \
    out/int/2048-sha256-int.pem \
    > ../certificates/ok_cert_by_intermediate.pem"
/bin/sh -c "cat out/int/2048-sha256-int.key out/int/2048-sha256-int.pem \
    > ../certificates/intermediate_ca_cert.pem"
/bin/sh -c "cat out/int/ok_cert.pem out/int/2048-sha256-int.pem \
    out/2048-sha256-root.pem \
    > ../certificates/x509_verify_results.chain.pem"
/bin/sh -c "cat out/test_names.key out/test_names.pem \
    > ../certificates/test_names.pem"

# Now generate the one-off certs
## Self-signed cert for SPDY/QUIC/HTTP2 pooling testing
openssl req -x509 -days 3650 -extensions req_spdy_pooling \
    -config ../scripts/ee.cnf -newkey rsa:2048 -text \
    -out ../certificates/spdy_pooling.pem

## SubjectAltName parsing
openssl req -x509 -days 3650 -extensions req_san_sanity \
    -config ../scripts/ee.cnf -newkey rsa:2048 -text \
    -out ../certificates/subjectAltName_sanity_check.pem

## SubjectAltName containing www.example.com
openssl req -x509 -days 3650 -extensions req_san_example \
    -config ../scripts/ee.cnf -newkey rsa:2048 -text \
    -out ../certificates/subjectAltName_www_example_com.pem

## Punycode handling
SUBJECT_NAME="req_punycode_dn" \
  openssl req -x509 -days 3650 -extensions req_punycode \
    -config ../scripts/ee.cnf -newkey rsa:2048 -text \
    -out ../certificates/punycodetest.pem

## Reject intranet hostnames in "publicly" trusted certs
SUBJECT_NAME="req_intranet_dn" \
  openssl req -x509 -days ${CERT_LIFETIME} -extensions req_intranet_san \
    -config ../scripts/ee.cnf -newkey rsa:2048 -text \
    -out ../certificates/reject_intranet_hosts.pem

## Leaf certificate with a large key; Apple's certificate verifier rejects with
## a fatal error if the key is bigger than 8192 bits.
openssl req -x509 -days 3650 \
    -config ../scripts/ee.cnf -newkey rsa:8200 -text \
    -sha256 \
    -out ../certificates/large_key.pem

## SHA1 certificate expiring in 2016.
openssl req -config ../scripts/ee.cnf \
  -newkey rsa:2048 -text -out out/sha1_2016.req
CA_NAME="req_ca_dn" \
  openssl ca \
    -batch \
    -extensions user_cert \
    -startdate 081030000000Z \
    -enddate   161230000000Z \
    -in out/sha1_2016.req \
    -out ../certificates/sha1_2016.pem \
    -config ca.cnf \
    -md sha1

## Validity too long unit test support.
openssl req -config ../scripts/ee.cnf \
  -newkey rsa:2048 -text -out out/10_year_validity.req
CA_NAME="req_ca_dn" \
  openssl ca \
    -batch \
    -extensions user_cert \
    -startdate 081030000000Z \
    -enddate   181029000000Z \
    -in out/10_year_validity.req \
    -out ../certificates/10_year_validity.pem \
    -config ca.cnf
openssl req -config ../scripts/ee.cnf \
  -newkey rsa:2048 -text -out out/11_year_validity.req
CA_NAME="req_ca_dn" \
  openssl ca \
    -batch \
    -extensions user_cert \
    -startdate 141030000000Z \
    -enddate   251030000000Z \
    -in out/11_year_validity.req \
    -out ../certificates/11_year_validity.pem \
    -config ca.cnf
openssl req -config ../scripts/ee.cnf \
  -newkey rsa:2048 -text -out out/39_months_after_2015_04.req
CA_NAME="req_ca_dn" \
  openssl ca \
    -batch \
    -extensions user_cert \
    -startdate 150402000000Z \
    -enddate   180702000000Z \
    -in out/39_months_after_2015_04.req \
    -out ../certificates/39_months_after_2015_04.pem \
    -config ca.cnf
openssl req -config ../scripts/ee.cnf \
  -newkey rsa:2048 -text -out out/40_months_after_2015_04.req
CA_NAME="req_ca_dn" \
  openssl ca \
    -batch \
    -extensions user_cert \
    -startdate 150402000000Z \
    -enddate   180801000000Z \
    -in out/40_months_after_2015_04.req \
    -out ../certificates/40_months_after_2015_04.pem \
    -config ca.cnf
openssl req -config ../scripts/ee.cnf \
  -newkey rsa:2048 -text -out out/60_months_after_2012_07.req
CA_NAME="req_ca_dn" \
  openssl ca \
    -batch \
    -extensions user_cert \
    -startdate 141030000000Z \
    -enddate   190930000000Z \
    -in out/60_months_after_2012_07.req \
    -out ../certificates/60_months_after_2012_07.pem \
    -config ca.cnf
openssl req -config ../scripts/ee.cnf \
  -newkey rsa:2048 -text -out out/61_months_after_2012_07.req
CA_NAME="req_ca_dn" \
  openssl ca \
    -batch \
    -extensions user_cert \
    -startdate 141030000000Z \
    -enddate   191103000000Z \
    -in out/61_months_after_2012_07.req \
    -out ../certificates/61_months_after_2012_07.pem \
    -config ca.cnf
# 39 months, based on a CA calculating one month as 'last day of Month 0' to
# last day of 'Month 1'.
openssl req -config ../scripts/ee.cnf \
  -newkey rsa:2048 -text -out out/39_months_based_on_last_day.req
CA_NAME="req_ca_dn" \
  openssl ca \
    -batch \
    -extensions user_cert \
    -startdate 170228000000Z \
    -enddate   200530000000Z \
    -in out/39_months_based_on_last_day.req \
    -out ../certificates/39_months_based_on_last_day.pem \
    -config ca.cnf
# start date after expiry date
openssl req -config ../scripts/ee.cnf \
  -newkey rsa:2048 -text -out out/start_after_expiry.req
CA_NAME="req_ca_dn" \
  openssl ca \
    -batch \
    -extensions user_cert \
    -startdate 180901000000Z \
    -enddate   150402000000Z \
    -in out/start_after_expiry.req \
    -out ../certificates/start_after_expiry.pem \
    -config ca.cnf
openssl req -config ../scripts/ee.cnf \
  -newkey rsa:2048 -text -out out/start_after_expiry.req
# Issued pre-BRs, lifetime < 120 months, expires before 2019-07-01
openssl req -config ../scripts/ee.cnf \
  -newkey rsa:2048 -text -out out/pre_br_validity_ok.req
CA_NAME="req_ca_dn" \
  openssl ca \
    -batch \
    -extensions user_cert \
    -startdate 080101000000Z \
    -enddate   150101000000Z \
    -in out/pre_br_validity_ok.req \
    -out ../certificates/pre_br_validity_ok.pem \
    -config ca.cnf
openssl req -config ../scripts/ee.cnf \
  -newkey rsa:2048 -text -out out/pre_br_validity_ok.req
# Issued pre-BRs, lifetime > 120 months, expires before 2019-07-01
openssl req -config ../scripts/ee.cnf \
  -newkey rsa:2048 -text -out out/pre_br_validity_bad_121.req
CA_NAME="req_ca_dn" \
  openssl ca \
    -batch \
    -extensions user_cert \
    -startdate 080101000000Z \
    -enddate   180501000000Z \
    -in out/pre_br_validity_bad_121.req \
    -out ../certificates/pre_br_validity_bad_121.pem \
    -config ca.cnf
openssl req -config ../scripts/ee.cnf \
  -newkey rsa:2048 -text -out out/pre_br_validity_bad_121.req
# Issued pre-BRs, lifetime < 120 months, expires after 2019-07-01
openssl req -config ../scripts/ee.cnf \
  -newkey rsa:2048 -text -out out/pre_br_validity_bad_2020.req
CA_NAME="req_ca_dn" \
  openssl ca \
    -batch \
    -extensions user_cert \
    -startdate 120501000000Z \
    -enddate   190703000000Z \
    -in out/pre_br_validity_bad_2020.req \
    -out ../certificates/pre_br_validity_bad_2020.pem \
    -config ca.cnf
# Issued after 2018-03-01, lifetime == 826 days (bad)
openssl req -config ../scripts/ee.cnf \
  -newkey rsa:2048 -text -out out/826_days_after_2018_03_01.req
CA_NAME="req_ca_dn" \
  openssl ca \
    -batch \
    -extensions user_cert \
    -startdate 180302000000Z \
    -enddate   200605000000Z \
    -in out/826_days_after_2018_03_01.req \
    -out ../certificates/826_days_after_2018_03_01.pem \
    -config ca.cnf
# Issued after 2018-03-01, lifetime == 825 days (good)
openssl req -config ../scripts/ee.cnf \
  -newkey rsa:2048 -text -out out/825_days_after_2018_03_01.req
CA_NAME="req_ca_dn" \
  openssl ca \
    -batch \
    -extensions user_cert \
    -startdate 180302000000Z \
    -enddate   200604000000Z \
    -in out/825_days_after_2018_03_01.req \
    -out ../certificates/825_days_after_2018_03_01.pem \
    -config ca.cnf
# Issued after 2018-03-01, lifetime == 825 days and one second (bad)
openssl req -config ../scripts/ee.cnf \
  -newkey rsa:2048 -text -out out/825_days_1_second_after_2018_03_01.req
CA_NAME="req_ca_dn" \
  openssl ca \
    -batch \
    -extensions user_cert \
    -startdate 180302000000Z \
    -enddate   200604000001Z \
    -in out/825_days_1_second_after_2018_03_01.req \
    -out ../certificates/825_days_1_second_after_2018_03_01.pem \
    -config ca.cnf

# Issued prior to 1 June 2016 (Symantec CT Enforcement Date)
openssl req -config ../scripts/ee.cnf \
  -newkey rsa:2048 -text -out out/pre_june_2016.req
CA_NAME="req_ca_dn" \
  openssl ca \
    -batch \
    -extensions user_cert \
    -startdate 160501000000Z \
    -enddate   170703000000Z \
    -in out/pre_june_2016.req \
    -out ../certificates/pre_june_2016.pem \
    -config ca.cnf

# Issued after 1 June 2016 (Symantec CT Enforcement Date)
openssl req -config ../scripts/ee.cnf \
  -newkey rsa:2048 -text -out out/post_june_2016.req
CA_NAME="req_ca_dn" \
  openssl ca \
    -batch \
    -extensions user_cert \
    -startdate 160601000000Z \
    -enddate   170703000000Z \
    -in out/post_june_2016.req \
    -out ../certificates/post_june_2016.pem \
    -config ca.cnf

# Includes the TLS feature extension
openssl req -x509 -newkey rsa:2048 \
  -keyout out/tls_feature_extension.key \
  -out ../certificates/tls_feature_extension.pem \
  -days 365 \
  -extensions req_extensions_with_tls_feature \
  -nodes -config ee.cnf

# Includes the canSignHttpExchangesDraft extension
openssl req -x509 -newkey rsa:2048 \
  -keyout out/can_sign_http_exchanges_draft_extension.key \
  -out ../certificates/can_sign_http_exchanges_draft_extension.pem \
  -days 365 \
  -extensions req_extensions_with_can_sign_http_exchanges_draft \
  -nodes -config ee.cnf

# Includes the canSignHttpExchangesDraft extension, but with a SEQUENCE in the
# body rather than a NULL.
openssl req -x509 -newkey rsa:2048 \
  -keyout out/can_sign_http_exchanges_draft_extension_invalid.key \
  -out ../certificates/can_sign_http_exchanges_draft_extension_invalid.pem \
  -days 365 \
  -extensions req_extensions_with_can_sign_http_exchanges_draft_invalid \
  -nodes -config ee.cnf

# SHA-1 certificate issued by locally trusted CA
openssl req \
  -config ../scripts/ee.cnf \
  -newkey rsa:2048 \
  -text \
  -keyout out/sha1_leaf.key \
  -out out/sha1_leaf.req
CA_NAME="req_ca_dn" \
  openssl ca \
    -batch \
    -extensions user_cert \
    -days ${CERT_LIFETIME} \
    -in out/sha1_leaf.req \
    -out out/sha1_leaf.pem \
    -config ca.cnf \
    -md sha1
/bin/sh -c "cat out/sha1_leaf.key out/sha1_leaf.pem \
    > ../certificates/sha1_leaf.pem"

# Certificate with only a common name (no SAN) issued by a locally trusted CA
openssl req \
  -config ../scripts/ee.cnf \
  -reqexts req_no_san \
  -newkey rsa:2048 \
  -text \
  -keyout out/common_name_only.key \
  -out out/common_name_only.req
CA_NAME="req_ca_dn" \
  openssl ca \
    -batch \
    -extensions user_cert \
    -startdate 171220000000Z \
    -enddate   201220000000Z \
    -in out/common_name_only.req \
    -out out/common_name_only.pem \
    -config ca.cnf
/bin/sh -c "cat out/common_name_only.key out/common_name_only.pem \
    > ../certificates/common_name_only.pem"

# Issued after 1 Dec 2017 (Symantec Legacy Distrust Date)
openssl req \
  -config ../scripts/ee.cnf \
  -newkey rsa:2048 \
  -text \
  -out out/dec_2017.req
CA_NAME="req_ca_dn" \
  openssl ca \
    -batch \
    -extensions user_cert \
    -startdate 171220000000Z \
    -enddate   201220000000Z \
    -in out/dec_2017.req \
    -out ../certificates/dec_2017.pem \
    -config ca.cnf

# Issued on 1 May 2018 (after the 30 Apr 2018 CT Requirement date)
openssl req \
  -config ../scripts/ee.cnf \
  -newkey rsa:2048 \
  -text \
  -out out/may_2018.req
CA_NAME="req_ca_dn" \
  openssl ca \
    -batch \
    -extensions user_cert \
    -startdate 180501000000Z \
    -enddate   200803000000Z \
    -in out/may_2018.req \
    -out ../certificates/may_2018.pem \
    -config ca.cnf

# Issued after 1 July 2019 (The macOS 10.15+ date for additional
# policies for locally-trusted certificates - see
# https://support.apple.com/en-us/HT210176 ) and valid for >825
# days, even accounting for rounding issues.
openssl req \
  -config ../scripts/ee.cnf \
  -newkey rsa:2048 \
  -text \
  -out out/900_days_after_2019_07_01.req
CA_NAME="req_ca_dn" \
  openssl ca \
    -batch \
    -extensions user_cert \
    -startdate 190701000000Z \
    -enddate   211217000000Z \
    -in out/900_days_after_2019_07_01.req \
    -out ../certificates/900_days_after_2019_07_01.pem \
    -config ca.cnf

## Certificates for testing EV display (DN set with different variations)
SUBJECT_NAME="req_ev_dn" \
  openssl req -x509 -days ${CERT_LIFETIME} \
    --config ../scripts/ee.cnf -newkey rsa:2048 -text \
    -out ../certificates/ev_test.pem

SUBJECT_NAME="req_ev_state_only_dn" \
  openssl req -x509 -days ${CERT_LIFETIME} \
    --config ../scripts/ee.cnf -newkey rsa:2048 -text \
    -out ../certificates/ev_test_state_only.pem

# Regenerate CRLSets
## Block a leaf cert directly by SPKI
python crlsetutil.py -o ../certificates/crlset_by_leaf_spki.raw \
<<CRLBYLEAFSPKI
{
  "BlockedBySPKI": ["../certificates/ok_cert.pem"]
}
CRLBYLEAFSPKI

## Block a root cert directly by SPKI
python crlsetutil.py -o ../certificates/crlset_by_root_spki.raw \
<<CRLBYROOTSPKI
{
  "BlockedBySPKI": ["../certificates/root_ca_cert.pem"]
}
CRLBYROOTSPKI

## Block a leaf cert by issuer-hash-and-serial
python crlsetutil.py -o ../certificates/crlset_by_root_serial.raw \
<<CRLBYROOTSERIAL
{
  "BlockedByHash": {
    "../certificates/root_ca_cert.pem": [
      "../certificates/ok_cert.pem"
    ]
  }
}
CRLBYROOTSERIAL

## Block a leaf cert by issuer-hash-and-serial. However, this will be issued
## from an intermediate CA issued underneath a root.
python crlsetutil.py -o ../certificates/crlset_by_intermediate_serial.raw \
<<CRLSETBYINTERMEDIATESERIAL
{
  "BlockedByHash": {
    "../certificates/intermediate_ca_cert.pem": [
      "../certificates/ok_cert_by_intermediate.pem"
    ]
  }
}
CRLSETBYINTERMEDIATESERIAL

## Block a subject with a single-entry allowlist of SPKI hashes.
python crlsetutil.py -o ../certificates/crlset_by_root_subject.raw \
<<CRLSETBYROOTSUBJECT
{
  "LimitedSubjects": {
    "../certificates/root_ca_cert.pem": [
      "../certificates/root_ca_cert.pem"
    ]
  }
}
CRLSETBYROOTSUBJECT

## Block a subject with an empty allowlist of SPKI hashes.
python crlsetutil.py -o ../certificates/crlset_by_root_subject_no_spki.raw \
<<CRLSETBYROOTSUBJECTNOSPKI
{
  "LimitedSubjects": {
    "../certificates/root_ca_cert.pem": []
  },
  "Sequence": 1
}
CRLSETBYROOTSUBJECTNOSPKI

## Block a subject with an empty allowlist of SPKI hashes.
python crlsetutil.py -o ../certificates/crlset_by_leaf_subject_no_spki.raw \
<<CRLSETBYLEAFSUBJECTNOSPKI
{
  "LimitedSubjects": {
    "../certificates/ok_cert.pem": []
  }
}
CRLSETBYLEAFSUBJECTNOSPKI

## Mark a given root as blocked for interception.
python crlsetutil.py -o \
  ../certificates/crlset_blocked_interception_by_root.raw \
<<CRLSETINTERCEPTIONBYROOT
{
  "BlockedInterceptionSPKIs": [
    "../certificates/root_ca_cert.pem"
  ]
}
CRLSETINTERCEPTIONBYROOT

## Mark a given intermediate as blocked for interception.
python crlsetutil.py -o \
  ../certificates/crlset_blocked_interception_by_intermediate.raw \
<<CRLSETINTERCEPTIONBYINTERMEDIATE
{
  "BlockedInterceptionSPKIs": [
    "../certificates/intermediate_ca_cert.pem"
  ]
}
CRLSETINTERCEPTIONBYINTERMEDIATE

## Mark a given root as known for interception, but not blocked.
python crlsetutil.py -o \
  ../certificates/crlset_known_interception_by_root.raw \
<<CRLSETINTERCEPTIONBYROOT
{
  "KnownInterceptionSPKIs": [
    "../certificates/root_ca_cert.pem"
  ]
}
CRLSETINTERCEPTIONBYROOT
