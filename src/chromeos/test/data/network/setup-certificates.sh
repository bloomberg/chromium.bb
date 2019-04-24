#!/bin/bash

# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

INPUT_DIR=${1?first param missing - input directory}
OUTPUT_DIR=${2?second param missing - output directory}

# This script grabs certain certificates from ${INPUT_DIR} and places them under
# ${OUTPUT_DIR}. It uses openssl's x509 command to only take the certificate
# sections (and not e.g. private keys).
# Additionally, this script creates ONC files which contain some of the
# certificates to be used by tests.

openssl x509 -in "${INPUT_DIR}/root_ca_cert.pem" -inform PEM \
  > "${OUTPUT_DIR}/root_ca_cert.pem"
openssl x509 -in "${INPUT_DIR}/ok_cert.pem" -inform PEM \
  > "${OUTPUT_DIR}/ok_cert.pem"
openssl x509 -in "${INPUT_DIR}/intermediate_ca_cert.pem" -inform PEM \
  > "${OUTPUT_DIR}/intermediate_ca_cert.pem"
openssl x509 -in "${INPUT_DIR}/ok_cert_by_intermediate.pem" -inform PEM \
  > "${OUTPUT_DIR}/ok_cert_by_intermediate.pem"

# Read the root CA cert and interemdiate CA cert PEM files and replace newlines
# with \n literals. This is needed because the ONC JSON does not support
# multi-line strings. Note that replacement is done in two steps, using ',' as
# intermediate character. PEM files will not contain commas.
ROOT_CA_CERT_CONTENTS=$(cat root_ca_cert.pem \
  | tr '\n' ',' | sed 's/,/\\n/g')
INTERMEDIATE_CA_CERT_CONTENTS=$(cat intermediate_ca_cert.pem \
  | tr '\n' ',' | sed 's/,/\\n/g')

cat > "${OUTPUT_DIR}/root-ca-cert.onc" << EOL
{
  "Certificates": [
    {
      "GUID": "{b3aae353-cfa9-4093-9aff-9f8ee2bf8c29}",
      "TrustBits": [
        "Web"
      ],
      "Type": "Authority",
      "X509": "${ROOT_CA_CERT_CONTENTS}"
    }
  ],
  "Type": "UnencryptedConfiguration"
}
EOL

cat > "${OUTPUT_DIR}/root-and-intermediate-ca-certs.onc" << EOL
{
  "Certificates": [
    {
      "GUID": "{b3aae353-cfa9-4093-9aff-9f8ee2bf8c29}",
      "TrustBits": [
        "Web"
      ],
      "Type": "Authority",
      "X509": "${ROOT_CA_CERT_CONTENTS}"
    },
    {
      "GUID": "{ac861420-3342-4537-a20e-3c2ec0809b7a}",
      "TrustBits": [ ],
      "Type": "Authority",
      "X509": "${INTERMEDIATE_CA_CERT_CONTENTS}"
    }
  ],
  "Type": "UnencryptedConfiguration"
}
EOL
