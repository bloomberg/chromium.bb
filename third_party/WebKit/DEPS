vars = {
  "chromium_git": "https://chromium.googlesource.com",
}

deps = {
  # FIXME, for now we checkout out these repos at empty revisions so that
  # we don't run any tests in them.
  # Once we can drop the repos pulled in directly through Chromium's src/DEPS,
  # we can move to r6bed4516fe8522d65512c76ef02e4f0ae8234395.
  "src/third_party/WebKit/LayoutTests/third_party/web-platform-tests":
    Var("chromium_git") +
     "/external/w3c/web-platform-tests.git@35a9c0f1348052303a03523781c26ca98572ffa7",

  "src/third_party/WebKit/LayoutTests/third_party/csswg-test":
    Var("chromium_git") +
    "/external/w3c/csswg-test.git@bacbb4a8dca702cd86646761fde96793db13d4f1",
}
