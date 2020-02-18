package # Hide from PAUSE
  DBIx::Class::Version::Table;
use base 'DBIx::Class::Core';
use strict;
use warnings;

__PACKAGE__->table('dbix_class_schema_versions');

__PACKAGE__->add_columns
    ( 'version' => {
        'data_type' => 'VARCHAR',
        'is_auto_increment' => 0,
        'default_value' => undef,
        'is_foreign_key' => 0,
        'name' => 'version',
        'is_nullable' => 0,
        'size' => '10'
        },
      'installed' => {
          'data_type' => 'VARCHAR',
          'is_auto_increment' => 0,
          'default_value' => undef,
          'is_foreign_key' => 0,
          'name' => 'installed',
          'is_nullable' => 0,
          'size' => '20'
          },
      );
__PACKAGE__->set_primary_key('version');

package # Hide from PAUSE
  DBIx::Class::Version::TableCompat;
use base 'DBIx::Class::Core';
__PACKAGE__->table('SchemaVersions');

__PACKAGE__->add_columns
    ( 'Version' => {
        'data_type' => 'VARCHAR',
        },
      'Installed' => {
          'data_type' => 'VARCHAR',
          },
      );
__PACKAGE__->set_primary_key('Version');

package # Hide from PAUSE
  DBIx::Class::Version;
use base 'DBIx::Class::Schema';
use strict;
use warnings;

__PACKAGE__->register_class('Table', 'DBIx::Class::Version::Table');

package # Hide from PAUSE
  DBIx::Class::VersionCompat;
use base 'DBIx::Class::Schema';
use strict;
use warnings;

__PACKAGE__->register_class('TableCompat', 'DBIx::Class::Version::TableCompat');


# ---------------------------------------------------------------------------

=head1 NAME

DBIx::Class::Schema::Versioned - DBIx::Class::Schema plugin for Schema upgrades

=head1 SYNOPSIS

  package MyApp::Schema;
  use base qw/DBIx::Class::Schema/;

  our $VERSION = 0.001;

  # load MyApp::Schema::CD, MyApp::Schema::Book, MyApp::Schema::DVD
  __PACKAGE__->load_classes(qw/CD Book DVD/);

  __PACKAGE__->load_components(qw/Schema::Versioned/);
  __PACKAGE__->upgrade_directory('/path/to/upgrades/');


=head1 DESCRIPTION

This module provides methods to apply DDL changes to your database using SQL
diff files. Normally these diff files would be created using
L<DBIx::Class::Schema/create_ddl_dir>.

A table called I<dbix_class_schema_versions> is created and maintained by the
module. This is used to determine which version your database is currently at.
Similarly the $VERSION in your DBIC schema class is used to determine the
current DBIC schema version.

The upgrade is initiated manually by calling C<upgrade> on your schema object,
this will attempt to upgrade the database from its current version to the current
schema version using a diff from your I<upgrade_directory>. If a suitable diff is
not found then no upgrade is possible.

=head1 SEE ALSO

L<DBIx::Class::DeploymentHandler> is a much more powerful alternative to this
module.  Examples of things it can do that this module cannot do include

=over

=item *

Downgrades in addition to upgrades

=item *

Multiple sql files per upgrade/downgrade/install

=item *

Perl scripts allowed for upgrade/downgrade/install

=item *

Just one set of files needed for upgrade, unlike this module where one might
need to generate C<factorial(scalar @versions)>

=back

=head1 GETTING STARTED

Firstly you need to setup your schema class as per the L</SYNOPSIS>, make sure
you have specified an upgrade_directory and an initial $VERSION.

Then you'll need two scripts, one to create DDL files and diffs and another to perform
upgrades. Your creation script might look like a bit like this:

  use strict;
  use Pod::Usage;
  use Getopt::Long;
  use MyApp::Schema;

  my ( $preversion, $help );
  GetOptions(
    'p|preversion:s'  => \$preversion,
  ) or die pod2usage;

  my $schema = MyApp::Schema->connect(
    $dsn,
    $user,
    $password,
  );
  my $sql_dir = './sql';
  my $version = $schema->schema_version();
  $schema->create_ddl_dir( 'MySQL', $version, $sql_dir, $preversion );

Then your upgrade script might look like so:

  use strict;
  use MyApp::Schema;

  my $schema = MyApp::Schema->connect(
    $dsn,
    $user,
    $password,
  );

  if (!$schema->get_db_version()) {
    # schema is unversioned
    $schema->deploy();
  } else {
    $schema->upgrade();
  }

The script above assumes that if the database is unversioned then it is empty
and we can safely deploy the DDL to it. However things are not always so simple.

if you want to initialise a pre-existing database where the DDL is not the same
as the DDL for your current schema version then you will need a diff which
converts the database's DDL to the current DDL. The best way to do this is
to get a dump of the database schema (without data) and save that in your
SQL directory as version 0.000 (the filename must be as with
L<DBIx::Class::Schema/ddl_filename>) then create a diff using your create DDL
script given above from version 0.000 to the current version. Then hand check
and if necessary edit the resulting diff to ensure that it will apply. Once you have
done all that you can do this:

  if (!$schema->get_db_version()) {
    # schema is unversioned
    $schema->install("0.000");
  }

  # this will now apply the 0.000 to current version diff
  $schema->upgrade();

In the case of an unversioned database the above code will create the
dbix_class_schema_versions table and write version 0.000 to it, then
upgrade will then apply the diff we talked about creating in the previous paragraph
and then you're good to go.

=cut

package DBIx::Class::Schema::Versioned;

use strict;
use warnings;
use base 'DBIx::Class::Schema';

use DBIx::Class::Carp;
use Time::HiRes qw/gettimeofday/;
use Try::Tiny;
use Scalar::Util 'weaken';
use namespace::clean;

__PACKAGE__->mk_classdata('_filedata');
__PACKAGE__->mk_classdata('upgrade_directory');
__PACKAGE__->mk_classdata('backup_directory');
__PACKAGE__->mk_classdata('do_backup');
__PACKAGE__->mk_classdata('do_diff_on_init');


=head1 METHODS

=head2 upgrade_directory

Use this to set the directory your upgrade files are stored in.

=head2 backup_directory

Use this to set the directory you want your backups stored in (note that backups
are disabled by default).

=cut

=head2 install

=over 4

=item Arguments: $db_version

=back

Call this to initialise a previously unversioned database. The table 'dbix_class_schema_versions' will be created which will be used to store the database version.

Takes one argument which should be the version that the database is currently at. Defaults to the return value of L</schema_version>.

See L</GETTING STARTED> for more details.

=cut

sub install
{
  my ($self, $new_version) = @_;

  # must be called on a fresh database
  if ($self->get_db_version()) {
      $self->throw_exception("A versioned schema has already been deployed, try upgrade instead.\n");
  }

  # default to current version if none passed
  $new_version ||= $self->schema_version();

  if ($new_version) {
    # create versions table and version row
    $self->{vschema}->deploy;
    $self->_set_db_version({ version => $new_version });
  }
}

=head2 deploy

Same as L<DBIx::Class::Schema/deploy> but also calls C<install>.

=cut

sub deploy {
  my $self = shift;
  $self->next::method(@_);
  $self->install();
}

=head2 create_upgrade_path

=over 4

=item Arguments: { upgrade_file => $file }

=back

Virtual method that should be overridden to create an upgrade file.
This is useful in the case of upgrading across multiple versions
to concatenate several files to create one upgrade file.

You'll probably want the db_version retrieved via $self->get_db_version
and the schema_version which is retrieved via $self->schema_version

=cut

sub create_upgrade_path {
  ## override this method
}

=head2 ordered_schema_versions

=over 4

=item Return Value: a list of version numbers, ordered from lowest to highest

=back

Virtual method that should be overridden to return an ordered list
of schema versions. This is then used to produce a set of steps to
upgrade through to achieve the required schema version.

You may want the db_version retrieved via $self->get_db_version
and the schema_version which is retrieved via $self->schema_version

=cut

sub ordered_schema_versions {
  ## override this method
}

=head2 upgrade

Call this to attempt to upgrade your database from the version it
is at to the version this DBIC schema is at. If they are the same
it does nothing.

It will call L</ordered_schema_versions> to retrieve an ordered
list of schema versions (if ordered_schema_versions returns nothing
then it is assumed you can do the upgrade as a single step). It
then iterates through the list of versions between the current db
version and the schema version applying one update at a time until
all relevant updates are applied.

The individual update steps are performed by using
L</upgrade_single_step>, which will apply the update and also
update the dbix_class_schema_versions table.

=cut

sub upgrade {
    my ($self) = @_;
    my $db_version = $self->get_db_version();

    # db unversioned
    unless ($db_version) {
        carp 'Upgrade not possible as database is unversioned. Please call install first.';
        return;
    }

    # db and schema at same version. do nothing
    if ( $db_version eq $self->schema_version ) {
        carp 'Upgrade not necessary';
        return;
    }

    my @version_list = $self->ordered_schema_versions;

    # if nothing returned then we preload with min/max
    @version_list = ( $db_version, $self->schema_version )
      unless ( scalar(@version_list) );

    # catch the case of someone returning an arrayref
    @version_list = @{ $version_list[0] }
      if ( ref( $version_list[0] ) eq 'ARRAY' );

    # remove all versions in list above the required version
    while ( scalar(@version_list)
        && ( $version_list[-1] ne $self->schema_version ) )
    {
        pop @version_list;
    }

    # remove all versions in list below the current version
    while ( scalar(@version_list) && ( $version_list[0] ne $db_version ) ) {
        shift @version_list;
    }

    # check we have an appropriate list of versions
    if ( scalar(@version_list) < 2 ) {
        die;
    }

    # do sets of upgrade
    while ( scalar(@version_list) >= 2 ) {
        $self->upgrade_single_step( $version_list[0], $version_list[1] );
        shift @version_list;
    }
}

=head2 upgrade_single_step

=over 4

=item Arguments: db_version - the version currently within the db

=item Arguments: target_version - the version to upgrade to

=back

Call this to attempt to upgrade your database from the
I<db_version> to the I<target_version>. If they are the same it
does nothing.

It requires an SQL diff file to exist in your I<upgrade_directory>,
normally you will have created this using L<DBIx::Class::Schema/create_ddl_dir>.

If successful the dbix_class_schema_versions table is updated with
the I<target_version>.

This method may be called repeatedly by the upgrade method to
upgrade through a series of updates.

=cut

sub upgrade_single_step
{
  my ($self,
      $db_version,
      $target_version) = @_;

  # db and schema at same version. do nothing
  if ($db_version eq $target_version) {
    carp 'Upgrade not necessary';
    return;
  }

  # strangely the first time this is called can
  # differ to subsequent times. so we call it
  # here to be sure.
  # XXX - just fix it
  $self->storage->sqlt_type;

  my $upgrade_file = $self->ddl_filename(
                                         $self->storage->sqlt_type,
                                         $target_version,
                                         $self->upgrade_directory,
                                         $db_version,
                                        );

  $self->create_upgrade_path({ upgrade_file => $upgrade_file });

  unless (-f $upgrade_file) {
    carp "Upgrade not possible, no upgrade file found ($upgrade_file), please create one";
    return;
  }

  carp "DB version ($db_version) is lower than the schema version (".$self->schema_version."). Attempting upgrade.\n";

  # backup if necessary then apply upgrade
  $self->_filedata($self->_read_sql_file($upgrade_file));
  $self->backup() if($self->do_backup);
  $self->txn_do(sub { $self->do_upgrade() });

  # set row in dbix_class_schema_versions table
  $self->_set_db_version({version => $target_version});
}

=head2 do_upgrade

This is an overwritable method used to run your upgrade. The freeform method
allows you to run your upgrade any way you please, you can call C<run_upgrade>
any number of times to run the actual SQL commands, and in between you can
sandwich your data upgrading. For example, first run all the B<CREATE>
commands, then migrate your data from old to new tables/formats, then
issue the DROP commands when you are finished. Will run the whole file as it is by default.

=cut

sub do_upgrade
{
  my ($self) = @_;

  # just run all the commands (including inserts) in order
  $self->run_upgrade(qr/.*?/);
}

=head2 run_upgrade

 $self->run_upgrade(qr/create/i);

Runs a set of SQL statements matching a passed in regular expression. The
idea is that this method can be called any number of times from your
C<do_upgrade> method, running whichever commands you specify via the
regex in the parameter. Probably won't work unless called from the overridable
do_upgrade method.

=cut

sub run_upgrade
{
    my ($self, $stm) = @_;

    return unless ($self->_filedata);
    my @statements = grep { $_ =~ $stm } @{$self->_filedata};
    $self->_filedata([ grep { $_ !~ /$stm/i } @{$self->_filedata} ]);

    for (@statements)
    {
        $self->storage->debugobj->query_start($_) if $self->storage->debug;
        $self->apply_statement($_);
        $self->storage->debugobj->query_end($_) if $self->storage->debug;
    }

    return 1;
}

=head2 apply_statement

Takes an SQL statement and runs it. Override this if you want to handle errors
differently.

=cut

sub apply_statement {
    my ($self, $statement) = @_;

    $self->storage->dbh->do($_) or carp "SQL was: $_";
}

=head2 get_db_version

Returns the version that your database is currently at. This is determined by the values in the
dbix_class_schema_versions table that C<upgrade> and C<install> write to.

=cut

sub get_db_version
{
    my ($self, $rs) = @_;

    my $vtable = $self->{vschema}->resultset('Table');
    my $version = try {
      $vtable->search({}, { order_by => { -desc => 'installed' }, rows => 1 } )
              ->get_column ('version')
               ->next;
    };
    return $version || 0;
}

=head2 schema_version

Returns the current schema class' $VERSION

=cut

=head2 backup

This is an overwritable method which is called just before the upgrade, to
allow you to make a backup of the database. Per default this method attempts
to call C<< $self->storage->backup >>, to run the standard backup on each
database type.

This method should return the name of the backup file, if appropriate..

This method is disabled by default. Set $schema->do_backup(1) to enable it.

=cut

sub backup
{
    my ($self) = @_;
    ## Make each ::DBI::Foo do this
    $self->storage->backup($self->backup_directory());
}

=head2 connection

Overloaded method. This checks the DBIC schema version against the DB version and
warns if they are not the same or if the DB is unversioned. It also provides
compatibility between the old versions table (SchemaVersions) and the new one
(dbix_class_schema_versions).

To avoid the checks on connect, set the environment var DBIC_NO_VERSION_CHECK or alternatively you can set the ignore_version attr in the forth argument like so:

  my $schema = MyApp::Schema->connect(
    $dsn,
    $user,
    $password,
    { ignore_version => 1 },
  );

=cut

sub connection {
  my $self = shift;
  $self->next::method(@_);
  $self->_on_connect();
  return $self;
}

sub _on_connect
{
  my ($self) = @_;

  weaken (my $w_storage = $self->storage );

  $self->{vschema} = DBIx::Class::Version->connect(
    sub { $w_storage->dbh },

    # proxy some flags from the main storage
    { map { $_ => $w_storage->$_ } qw( unsafe ) },
  );
  my $conn_attrs = $w_storage->_dbic_connect_attributes || {};

  my $vtable = $self->{vschema}->resultset('Table');

  # useful when connecting from scripts etc
  return if ($conn_attrs->{ignore_version} || ($ENV{DBIC_NO_VERSION_CHECK} && !exists $conn_attrs->{ignore_version}));

  # check for legacy versions table and move to new if exists
  unless ($self->_source_exists($vtable)) {
    my $vtable_compat = DBIx::Class::VersionCompat->connect(sub { $w_storage->dbh })->resultset('TableCompat');
    if ($self->_source_exists($vtable_compat)) {
      $self->{vschema}->deploy;
      map { $vtable->new_result({ installed => $_->Installed, version => $_->Version })->insert } $vtable_compat->all;
      $self->storage->_get_dbh->do("DROP TABLE " . $vtable_compat->result_source->from);
    }
  }

  my $pversion = $self->get_db_version();

  if($pversion eq $self->schema_version)
    {
        #carp "This version is already installed";
        return 1;
    }

  if(!$pversion)
    {
        carp "Your DB is currently unversioned. Please call upgrade on your schema to sync the DB.";
        return 1;
    }

  carp "Versions out of sync. This is " . $self->schema_version .
    ", your database contains version $pversion, please call upgrade on your Schema.";
}

# is this just a waste of time? if not then merge with DBI.pm
sub _create_db_to_schema_diff {
  my $self = shift;

  my %driver_to_db_map = (
                          'mysql' => 'MySQL'
                         );

  my $db = $driver_to_db_map{$self->storage->dbh->{Driver}->{Name}};
  unless ($db) {
    print "Sorry, this is an unsupported DB\n";
    return;
  }

  unless (DBIx::Class::Optional::Dependencies->req_ok_for ('deploy')) {
    $self->throw_exception("Unable to proceed without " . DBIx::Class::Optional::Dependencies->req_missing_for ('deploy') );
  }

  my $db_tr = SQL::Translator->new({
                                    add_drop_table => 1,
                                    parser => 'DBI',
                                    parser_args => { dbh => $self->storage->dbh }
                                   });

  $db_tr->producer($db);
  my $dbic_tr = SQL::Translator->new;
  $dbic_tr->parser('SQL::Translator::Parser::DBIx::Class');
  $dbic_tr->data($self);
  $dbic_tr->producer($db);

  $db_tr->schema->name('db_schema');
  $dbic_tr->schema->name('dbic_schema');

  # is this really necessary?
  foreach my $tr ($db_tr, $dbic_tr) {
    my $data = $tr->data;
    $tr->parser->($tr, $$data);
  }

  my $diff = SQL::Translator::Diff::schema_diff($db_tr->schema, $db,
                                                $dbic_tr->schema, $db,
                                                { ignore_constraint_names => 1, ignore_index_names => 1, caseopt => 1 });

  my $filename = $self->ddl_filename(
                                         $db,
                                         $self->schema_version,
                                         $self->upgrade_directory,
                                         'PRE',
                                    );
  my $file;
  if(!open($file, ">$filename"))
    {
      $self->throw_exception("Can't open $filename for writing ($!)");
      next;
    }
  print $file $diff;
  close($file);

  carp "WARNING: There may be differences between your DB and your DBIC schema. Please review and if necessary run the SQL in $filename to sync your DB.";
}


sub _set_db_version {
  my $self = shift;
  my ($params) = @_;
  $params ||= {};

  my $version = $params->{version} ? $params->{version} : $self->schema_version;
  my $vtable = $self->{vschema}->resultset('Table');

  ##############################################################################
  #                             !!! NOTE !!!
  ##############################################################################
  #
  # The travesty below replaces the old nice timestamp format of %Y-%m-%d %H:%M:%S
  # This is necessary since there are legitimate cases when upgrades can happen
  # back to back within the same second. This breaks things since we relay on the
  # ability to sort by the 'installed' value. The logical choice of an autoinc
  # is not possible, as it will break multiple legacy installations. Also it is
  # not possible to format the string sanely, as the column is a varchar(20).
  # The 'v' character is added to the front of the string, so that any version
  # formatted by this new function will sort _after_ any existing 200... strings.
  my @tm = gettimeofday();
  my @dt = gmtime ($tm[0]);
  my $o = $vtable->new_result({
    version => $version,
    installed => sprintf("v%04d%02d%02d_%02d%02d%02d.%03.0f",
      $dt[5] + 1900,
      $dt[4] + 1,
      $dt[3],
      $dt[2],
      $dt[1],
      $dt[0],
      int($tm[1] / 1000), # convert to millisecs
    ),
  })->insert;
}

sub _read_sql_file {
  my $self = shift;
  my $file = shift || return;

  open my $fh, '<', $file or carp("Can't open upgrade file, $file ($!)");
  my @data = split /\n/, join '', <$fh>;
  close $fh;

  @data = split /;/,
     join '',
       grep { $_ &&
              !/^--/  &&
              !/^(BEGIN|BEGIN TRANSACTION|COMMIT)/mi }
         @data;

  return \@data;
}

sub _source_exists
{
    my ($self, $rs) = @_;

    return try {
      $rs->search(\'1=0')->cursor->next;
      1;
    } catch {
      0;
    };
}

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.

=cut

1;
