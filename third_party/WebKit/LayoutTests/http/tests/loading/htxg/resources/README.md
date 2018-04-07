The certificate message files (\*.msg) and the signed exchange files
(\*.htxg) in this directory are generated using the following commands.

gen-certurl and gen-signedexchange are available in [webpackage repository][1].
We're using a fork of [this repository][2] that implements the "Implementation
Checkpoints" of [the signed-exchange spec][3].

[1]: https://github.com/WICG/webpackage
[2]: https://github.com/nyaxt/webpackage
[3]: https://wicg.github.io/webpackage/draft-yasskin-httpbis-origin-signed-exchanges-impl.html

```
# Install gen-certurl command
go get github.com/nyaxt/webpackage/go/signedexchange/cmd/gen-certurl

# Install gen-signedexchange command
go get github.com/nyaxt/webpackage/go/signedexchange/cmd/gen-signedexchange

# Generate the certificate message file of "127.0.0.1.pem".
gen-certurl  \
  ../../../../../../Tools/Scripts/webkitpy/thirdparty/wpt/certs/127.0.0.1.pem \
  > 127.0.0.1.pem.msg

# Generate the signed exchange file.
gen-signedexchange \
  -uri https://www.127.0.0.1/test.html \
  -status 200 \
  -content htxg-location.html \
  -certificate ../../../../../../Tools/Scripts/webkitpy/thirdparty/wpt/certs/127.0.0.1.pem \
  -certUrl http://localhost:8000/loading/htxg/resources/127.0.0.1.pem.msg \
  -validityUrl http://localhost:8000/loading/htxg/resources/resource.validity.msg \
  -privateKey ../../../../../../Tools/Scripts/webkitpy/thirdparty/wpt/certs/127.0.0.1.key\
  -date 2018-04-01T00:00:00Z \
  -expire 168h \
  -o htxg-location.htxg \
  -miRecordSize 100
```
