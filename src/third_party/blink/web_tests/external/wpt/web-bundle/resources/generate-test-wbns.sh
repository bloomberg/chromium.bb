#!/bin/sh

set -e

if ! command -v gen-bundle > /dev/null 2>&1; then
    echo "gen-bundle is not installed. Please run:"
    echo "  go get -u github.com/WICG/webpackage/go/bundle/cmd/..."
    exit 1
fi

# TODO: Stop hard-coding "web-platform.test" when generating Web Bundles on the
# fly.
wpt_test_https_origin=https://web-platform.test:8444
wpt_test_http_origin=http://web-platform.test:8001

gen-bundle \
  -version b1 \
  -baseURL $wpt_test_https_origin/web-bundle/resources/wbn/ \
  -primaryURL $wpt_test_https_origin/web-bundle/resources/wbn/location.html \
  -dir location/ \
  -o wbn/location.wbn

gen-bundle \
  -version b1 \
  -baseURL $wpt_test_http_origin/web-bundle/resources/wbn/ \
  -primaryURL $wpt_test_http_origin/web-bundle/resources/wbn/root.js \
  -dir subresource/ \
  -o wbn/subresource.wbn

gen-bundle \
  -version b1 \
  -baseURL $wpt_test_http_origin/web-bundle/resources/wbn/dynamic/ \
  -primaryURL $wpt_test_http_origin/web-bundle/resources/wbn/dynamic/resource1.js \
  -dir dynamic1/ \
  -o wbn/dynamic1.wbn

gen-bundle \
  -version b1 \
  -baseURL $wpt_test_http_origin/web-bundle/resources/wbn/dynamic/ \
  -primaryURL $wpt_test_http_origin/web-bundle/resources/wbn/dynamic/resource1.js \
  -dir dynamic2/ \
  -o wbn/dynamic2.wbn

gen-bundle \
  -version b1 \
  -baseURL $wpt_test_https_origin/web-bundle/resources/wbn/dynamic/ \
  -primaryURL $wpt_test_https_origin/web-bundle/resources/wbn/dynamic/resource1.js \
  -dir dynamic1/ \
  -o wbn/dynamic1-crossorigin.wbn

gen-bundle \
  -version b1 \
  -baseURL $wpt_test_http_origin/web-bundle/resources/ \
  -primaryURL $wpt_test_http_origin/web-bundle/resources/wbn/resource.js \
  -dir path-restriction/ \
  -o wbn/path-restriction.wbn
