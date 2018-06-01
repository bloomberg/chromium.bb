The certificate message files (\*.msg) and the signed exchange files
(\*.htxg) in this directory are generated using the following commands.

gen-certurl and gen-signedexchange are available in [webpackage repository][1].
Revision 01e618f6af is used to generate these files.

[1]: https://github.com/WICG/webpackage

```
# Install gen-certurl command.
go get github.com/WICG/webpackage/go/signedexchange/cmd/gen-certurl

# Install gen-signedexchange command.
go get github.com/WICG/webpackage/go/signedexchange/cmd/gen-signedexchange

# Make dummy OCSP data for cbor certificate chains.
echo -n OCSP >/tmp/ocsp

# Generate the certificate chain of "127.0.0.1.pem".
gen-certurl  \
  -pem ../../../../../../../blink/tools/blinkpy/third_party/wpt/certs/127.0.0.1.pem \
  -ocsp /tmp/ocsp \
  > 127.0.0.1.pem.cbor

# Generate the signed exchange file.
gen-signedexchange \
  -uri https://www.127.0.0.1/test.html \
  -status 200 \
  -content htxg-location.html \
  -certificate ../../../../../../../blink/tools/blinkpy/third_party/wpt/certs/127.0.0.1.pem \
  -certUrl http://localhost:8000/loading/htxg/resources/127.0.0.1.pem.cbor \
  -validityUrl http://localhost:8000/loading/htxg/resources/resource.validity.msg \
  -privateKey ../../../../../../../blink/tools/blinkpy/third_party/wpt/certs/127.0.0.1.key \
  -date 2018-04-01T00:00:00Z \
  -expire 168h \
  -o htxg-location.sxg \
  -miRecordSize 100

# Generate the signed exchange file which certificate file is not available.
gen-signedexchange \
  -uri https://www.127.0.0.1/not_found_cert.html \
  -status 200 \
  -content htxg-location.html \
  -certificate ../../../../../../../blink/tools/blinkpy/third_party/wpt/certs/127.0.0.1.pem \
  -certUrl http://localhost:8000/loading/htxg/resources/not_found_cert.pem.cbor \
  -validityUrl http://localhost:8000/loading/htxg/resources/not_found_cert.validity.msg \
  -privateKey ../../../../../../../blink/tools/blinkpy/third_party/wpt/certs/127.0.0.1.key \
  -date 2018-04-01T00:00:00Z \
  -expire 168h \
  -o htxg-cert-not-found.sxg \
  -miRecordSize 100
```
