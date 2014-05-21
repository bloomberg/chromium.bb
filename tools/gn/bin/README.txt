This folder contains GN binaries. They should be automatically downloaded from
Google Storage by gclient runhooks for the current platform.


To upload a file:
  python ~/depot_tools/upload_to_google_storage.py -b chromium-gn <FILENAME>

To download a file given a .sha1 file:
  python ~/depot_tools/download_from_google_storage.py -b chromium-gn -s <FILENAME>.sha1

List the contents of GN's Google Storage bucket:
  python ~/depot_tools/third_party/gsutil/gsutil ls gs://chromium-gn/

To initialize gsutil's credentials:
  python ~/depot_tools/third_party/gsutil/gsutil config

  That will give a URL which you should log into with your web browser. The
  username should be the one that is on the ACL for the "chromium-gn" bucket
  (probably your @google.com address). Contact the build team for help getting
  access if necessary.

  Copy the code back to the command line util. Ignore the project ID (it's OK
  to just leave blank when prompted).

gsutil documentation:
  https://developers.google.com/storage/docs/gsutil
