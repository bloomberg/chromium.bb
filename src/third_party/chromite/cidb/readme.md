### [Deployment Docs](go/cros-cidb-admin)

### update schema.dump

If you add new CIDB scema migrations, you'll need to update the schema.dump file.

With a full ChromeOS (not ChromiumOS) checkout, inside the chroot:
  $ lib/cidb_integration_test --update

### Local, interactiveCIDB instance.

Create a DB with a full ChromeOS (not ChromiumOS) checkout, inside the chroot:
  $ lib/cidb_integration_test --debug --no-wipe

The test logs the path to the temporary working directory at the end.

You can launch the mysqld server again to play with the database in its final
state. You'll have to fish out the temp directory that it created -- it will
be inside the logged directory (above) and will look something like:

  /tmp/chromite.test_no_cleanup3WqzmO/chromite.testYypd_c/

Set your local tmpdir variable to the path that you found, and run (inside the
chroot):

  $ tmpdir=/tmp/....
  $ /usr/sbin/mysqld --no-defaults --datadir ${tmpdir}/mysqld_dir --socket \
      ${tmpdir}/mysqld_dir/mysqld.socket --port 8440 --pid-file \
      ${tmpdir}/mysqld_dir/mysqld.pid --tmpdir ${tmpdir}/mysqld_dir/tmp &

You can connect to this instance using mysql client.

  $ mysql -u root -S ${tmpdir}/mysqld_dir/mysqld.socket

At this point you can run normal SQL. To double check, run:

   > use cidb;
   > show tables;

You can then use the data here to create your own integration test to test
something you added to CIDB.

When you're done, remember to shutdown the mysqld instance, and delete the
temporary directory.

$ mysqladmin -u root -S ${tmpdir}/mysqld_dir/mysqld.socket shutdown
$ rm -rf ${tmpdir}
