# Benchmarks for Open Web Platform Storage.

These benchmarks exercise storage apis in a real-life usage way (avoiding microbenchmarks).

# IDB Docs Load

This models an offline load of a Google doc. See [this document](https://docs.google.com/document/d/1JC1RgMyxBAjUPSHjm2Bd1KPzcqpPPvxRomKevOkMPm0/edit) for a breakdown of the database and the transactions, along with the traces used to extract this information.

# Blob Build All Then Read Serially

This benchmark models the creation and reading of a large number of blobs. The blobs are created first, then read one at a time.

# Blob Build All Then Read in Parallel

This benchmark models the creation and reading of a large number of blobs. The blobs are created first, then read all at once.

# Blob Build And Read Immediately

This benchmark models the creation and reading of a large number of blobs. Each blob is read immediately after creation.
