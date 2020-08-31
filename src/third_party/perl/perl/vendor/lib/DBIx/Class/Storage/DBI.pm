package DBIx::Class::Storage::DBI;
# -*- mode: cperl; cperl-indent-level: 2 -*-

use strict;
use warnings;

use base qw/DBIx::Class::Storage::DBIHacks DBIx::Class::Storage/;
use mro 'c3';

use DBIx::Class::Carp;
use Scalar::Util qw/refaddr weaken reftype blessed/;
use List::Util qw/first/;
use Context::Preserve 'preserve_context';
use Try::Tiny;
use SQL::Abstract qw(is_plain_value is_literal_value);
use DBIx::Class::_Util qw(quote_sub perlstring serialize detected_reinvoked_destructor);
use namespace::clean;

# default cursor class, overridable in connect_info attributes
__PACKAGE__->cursor_class('DBIx::Class::Storage::DBI::Cursor');

__PACKAGE__->mk_group_accessors('inherited' => qw/
  sql_limit_dialect sql_quote_char sql_name_sep
/);

__PACKAGE__->mk_group_accessors('component_class' => qw/sql_maker_class datetime_parser_type/);

__PACKAGE__->sql_maker_class('DBIx::Class::SQLMaker');
__PACKAGE__->datetime_parser_type('DateTime::Format::MySQL'); # historic default

__PACKAGE__->sql_name_sep('.');

__PACKAGE__->mk_group_accessors('simple' => qw/
  _connect_info _dbic_connect_attributes _driver_determined
  _dbh _dbh_details _conn_pid _sql_maker _sql_maker_opts _dbh_autocommit
  _perform_autoinc_retrieval _autoinc_supplied_for_op
/);

# the values for these accessors are picked out (and deleted) from
# the attribute hashref passed to connect_info
my @storage_options = qw/
  on_connect_call on_disconnect_call on_connect_do on_disconnect_do
  disable_sth_caching unsafe auto_savepoint
/;
__PACKAGE__->mk_group_accessors('simple' => @storage_options);


# capability definitions, using a 2-tiered accessor system
# The rationale is:
#
# A driver/user may define _use_X, which blindly without any checks says:
# "(do not) use this capability", (use_dbms_capability is an "inherited"
# type accessor)
#
# If _use_X is undef, _supports_X is then queried. This is a "simple" style
# accessor, which in turn calls _determine_supports_X, and stores the return
# in a special slot on the storage object, which is wiped every time a $dbh
# reconnection takes place (it is not guaranteed that upon reconnection we
# will get the same rdbms version). _determine_supports_X does not need to
# exist on a driver, as we ->can for it before calling.

my @capabilities = (qw/
  insert_returning
  insert_returning_bound

  multicolumn_in

  placeholders
  typeless_placeholders

  join_optimizer
/);
__PACKAGE__->mk_group_accessors( dbms_capability => map { "_supports_$_" } @capabilities );
__PACKAGE__->mk_group_accessors( use_dbms_capability => map { "_use_$_" } (@capabilities ) );

# on by default, not strictly a capability (pending rewrite)
__PACKAGE__->_use_join_optimizer (1);
sub _determine_supports_join_optimizer { 1 };

# Each of these methods need _determine_driver called before itself
# in order to function reliably. We also need to separate accessors
# from plain old method calls, since an accessor called as a setter
# does *not* need the driver determination loop fired (and in fact
# can produce hard to find bugs, like e.g. losing on_connect_*
# semantics on fresh connections)
#
# The construct below is simply a parameterized around()
my $storage_accessor_idx = { map { $_ => 1 } qw(
  sqlt_type
  datetime_parser_type

  sql_maker
  cursor_class
)};
for my $meth (keys %$storage_accessor_idx, qw(
  deployment_statements

  build_datetime_parser

  txn_begin

  insert
  update
  delete
  select
  select_single

  _insert_bulk

  with_deferred_fk_checks

  get_use_dbms_capability
  get_dbms_capability

  _server_info
  _get_server_version
)) {

  my $orig = __PACKAGE__->can ($meth)
    or die "$meth is not a ::Storage::DBI method!";

  my $is_getter = $storage_accessor_idx->{$meth} ? 0 : 1;

  quote_sub
    __PACKAGE__ ."::$meth", sprintf( <<'EOC', $is_getter, perlstring $meth ), { '$orig' => \$orig };

    if (
      # only fire when invoked on an instance, a valid class-based invocation
      # would e.g. be setting a default for an inherited accessor
      ref $_[0]
        and
      ! $_[0]->{_driver_determined}
        and
      ! $_[0]->{_in_determine_driver}
        and
      # if this is a known *setter* - just set it, no need to connect
      # and determine the driver
      ( %1$s or @_ <= 1 )
        and
      # Only try to determine stuff if we have *something* that either is or can
      # provide a DSN. Allows for bare $schema's generated with a plain ->connect()
      # to still be marginally useful
      $_[0]->_dbi_connect_info->[0]
    ) {
      $_[0]->_determine_driver;

      # work around http://rt.perl.org/rt3/Public/Bug/Display.html?id=35878
      goto $_[0]->can(%2$s) unless DBIx::Class::_ENV_::BROKEN_GOTO;

      my $cref = $_[0]->can(%2$s);
      goto $cref;
    }

    goto $orig;
EOC
}

=head1 NAME

DBIx::Class::Storage::DBI - DBI storage handler

=head1 SYNOPSIS

  my $schema = MySchema->connect('dbi:SQLite:my.db');

  $schema->storage->debug(1);

  my @stuff = $schema->storage->dbh_do(
    sub {
      my ($storage, $dbh, @args) = @_;
      $dbh->do("DROP TABLE authors");
    },
    @column_list
  );

  $schema->resultset('Book')->search({
     written_on => $schema->storage->datetime_parser->format_datetime(DateTime->now)
  });

=head1 DESCRIPTION

This class represents the connection to an RDBMS via L<DBI>.  See
L<DBIx::Class::Storage> for general information.  This pod only
documents DBI-specific methods and behaviors.

=head1 METHODS

=cut

sub new {
  my $new = shift->next::method(@_);

  $new->_sql_maker_opts({});
  $new->_dbh_details({});
  $new->{_in_do_block} = 0;

  # read below to see what this does
  $new->_arm_global_destructor;

  $new;
}

# This is hack to work around perl shooting stuff in random
# order on exit(). If we do not walk the remaining storage
# objects in an END block, there is a *small but real* chance
# of a fork()ed child to kill the parent's shared DBI handle,
# *before perl reaches the DESTROY in this package*
# Yes, it is ugly and effective.
# Additionally this registry is used by the CLONE method to
# make sure no handles are shared between threads
{
  my %seek_and_destroy;

  sub _arm_global_destructor {

    # quick "garbage collection" pass - prevents the registry
    # from slowly growing with a bunch of undef-valued keys
    defined $seek_and_destroy{$_} or delete $seek_and_destroy{$_}
      for keys %seek_and_destroy;

    weaken (
      $seek_and_destroy{ refaddr($_[0]) } = $_[0]
    );
  }

  END {
    local $?; # just in case the DBI destructor changes it somehow

    # destroy just the object if not native to this process
    $_->_verify_pid for (grep
      { defined $_ }
      values %seek_and_destroy
    );
  }

  sub CLONE {
    # As per DBI's recommendation, DBIC disconnects all handles as
    # soon as possible (DBIC will reconnect only on demand from within
    # the thread)
    my @instances = grep { defined $_ } values %seek_and_destroy;
    %seek_and_destroy = ();

    for (@instances) {
      $_->_dbh(undef);

      $_->transaction_depth(0);
      $_->savepoints([]);

      # properly renumber existing refs
      $_->_arm_global_destructor
    }
  }
}

sub DESTROY {
  return if &detected_reinvoked_destructor;

  $_[0]->_verify_pid unless DBIx::Class::_ENV_::BROKEN_FORK;
  # some databases spew warnings on implicit disconnect
  local $SIG{__WARN__} = sub {};
  $_[0]->_dbh(undef);

  # this op is necessary, since the very last perl runtime statement
  # triggers a global destruction shootout, and the $SIG localization
  # may very well be destroyed before perl actually gets to do the
  # $dbh undef
  1;
}

# handle pid changes correctly - do not destroy parent's connection
sub _verify_pid {

  my $pid = $_[0]->_conn_pid;

  if( defined $pid and $pid != $$ and my $dbh = $_[0]->_dbh ) {
    $dbh->{InactiveDestroy} = 1;
    $_[0]->_dbh(undef);
    $_[0]->transaction_depth(0);
    $_[0]->savepoints([]);
  }

  return;
}

=head2 connect_info

This method is normally called by L<DBIx::Class::Schema/connection>, which
encapsulates its argument list in an arrayref before passing them here.

The argument list may contain:

=over

=item *

The same 4-element argument set one would normally pass to
L<DBI/connect>, optionally followed by
L<extra attributes|/DBIx::Class specific connection attributes>
recognized by DBIx::Class:

  $connect_info_args = [ $dsn, $user, $password, \%dbi_attributes?, \%extra_attributes? ];

=item *

A single code reference which returns a connected
L<DBI database handle|DBI/connect> optionally followed by
L<extra attributes|/DBIx::Class specific connection attributes> recognized
by DBIx::Class:

  $connect_info_args = [ sub { DBI->connect (...) }, \%extra_attributes? ];

=item *

A single hashref with all the attributes and the dsn/user/password
mixed together:

  $connect_info_args = [{
    dsn => $dsn,
    user => $user,
    password => $pass,
    %dbi_attributes,
    %extra_attributes,
  }];

  $connect_info_args = [{
    dbh_maker => sub { DBI->connect (...) },
    %dbi_attributes,
    %extra_attributes,
  }];

This is particularly useful for L<Catalyst> based applications, allowing the
following config (L<Config::General> style):

  <Model::DB>
    schema_class   App::DB
    <connect_info>
      dsn          dbi:mysql:database=test
      user         testuser
      password     TestPass
      AutoCommit   1
    </connect_info>
  </Model::DB>

The C<dsn>/C<user>/C<password> combination can be substituted by the
C<dbh_maker> key whose value is a coderef that returns a connected
L<DBI database handle|DBI/connect>

=back

Please note that the L<DBI> docs recommend that you always explicitly
set C<AutoCommit> to either I<0> or I<1>.  L<DBIx::Class> further
recommends that it be set to I<1>, and that you perform transactions
via our L<DBIx::Class::Schema/txn_do> method.  L<DBIx::Class> will set it
to I<1> if you do not do explicitly set it to zero.  This is the default
for most DBDs. See L</DBIx::Class and AutoCommit> for details.

=head3 DBIx::Class specific connection attributes

In addition to the standard L<DBI|DBI/ATTRIBUTES COMMON TO ALL HANDLES>
L<connection|DBI/Database Handle Attributes> attributes, DBIx::Class recognizes
the following connection options. These options can be mixed in with your other
L<DBI> connection attributes, or placed in a separate hashref
(C<\%extra_attributes>) as shown above.

Every time C<connect_info> is invoked, any previous settings for
these options will be cleared before setting the new ones, regardless of
whether any options are specified in the new C<connect_info>.


=over

=item on_connect_do

Specifies things to do immediately after connecting or re-connecting to
the database.  Its value may contain:

=over

=item a scalar

This contains one SQL statement to execute.

=item an array reference

This contains SQL statements to execute in order.  Each element contains
a string or a code reference that returns a string.

=item a code reference

This contains some code to execute.  Unlike code references within an
array reference, its return value is ignored.

=back

=item on_disconnect_do

Takes arguments in the same form as L</on_connect_do> and executes them
immediately before disconnecting from the database.

Note, this only runs if you explicitly call L</disconnect> on the
storage object.

=item on_connect_call

A more generalized form of L</on_connect_do> that calls the specified
C<connect_call_METHOD> methods in your storage driver.

  on_connect_do => 'select 1'

is equivalent to:

  on_connect_call => [ [ do_sql => 'select 1' ] ]

Its values may contain:

=over

=item a scalar

Will call the C<connect_call_METHOD> method.

=item a code reference

Will execute C<< $code->($storage) >>

=item an array reference

Each value can be a method name or code reference.

=item an array of arrays

For each array, the first item is taken to be the C<connect_call_> method name
or code reference, and the rest are parameters to it.

=back

Some predefined storage methods you may use:

=over

=item do_sql

Executes a SQL string or a code reference that returns a SQL string. This is
what L</on_connect_do> and L</on_disconnect_do> use.

It can take:

=over

=item a scalar

Will execute the scalar as SQL.

=item an arrayref

Taken to be arguments to L<DBI/do>, the SQL string optionally followed by the
attributes hashref and bind values.

=item a code reference

Will execute C<< $code->($storage) >> and execute the return array refs as
above.

=back

=item datetime_setup

Execute any statements necessary to initialize the database session to return
and accept datetime/timestamp values used with
L<DBIx::Class::InflateColumn::DateTime>.

Only necessary for some databases, see your specific storage driver for
implementation details.

=back

=item on_disconnect_call

Takes arguments in the same form as L</on_connect_call> and executes them
immediately before disconnecting from the database.

Calls the C<disconnect_call_METHOD> methods as opposed to the
C<connect_call_METHOD> methods called by L</on_connect_call>.

Note, this only runs if you explicitly call L</disconnect> on the
storage object.

=item disable_sth_caching

If set to a true value, this option will disable the caching of
statement handles via L<DBI/prepare_cached>.

=item limit_dialect

Sets a specific SQL::Abstract::Limit-style limit dialect, overriding the
default L</sql_limit_dialect> setting of the storage (if any). For a list
of available limit dialects see L<DBIx::Class::SQLMaker::LimitDialects>.

=item quote_names

When true automatically sets L</quote_char> and L</name_sep> to the characters
appropriate for your particular RDBMS. This option is preferred over specifying
L</quote_char> directly.

=item quote_char

Specifies what characters to use to quote table and column names.

C<quote_char> expects either a single character, in which case is it
is placed on either side of the table/column name, or an arrayref of length
2 in which case the table/column name is placed between the elements.

For example under MySQL you should use C<< quote_char => '`' >>, and for
SQL Server you should use C<< quote_char => [qw/[ ]/] >>.

=item name_sep

This parameter is only useful in conjunction with C<quote_char>, and is used to
specify the character that separates elements (schemas, tables, columns) from
each other. If unspecified it defaults to the most commonly used C<.>.

=item unsafe

This Storage driver normally installs its own C<HandleError>, sets
C<RaiseError> and C<ShowErrorStatement> on, and sets C<PrintError> off on
all database handles, including those supplied by a coderef.  It does this
so that it can have consistent and useful error behavior.

If you set this option to a true value, Storage will not do its usual
modifications to the database handle's attributes, and instead relies on
the settings in your connect_info DBI options (or the values you set in
your connection coderef, in the case that you are connecting via coderef).

Note that your custom settings can cause Storage to malfunction,
especially if you set a C<HandleError> handler that suppresses exceptions
and/or disable C<RaiseError>.

=item auto_savepoint

If this option is true, L<DBIx::Class> will use savepoints when nesting
transactions, making it possible to recover from failure in the inner
transaction without having to abort all outer transactions.

=item cursor_class

Use this argument to supply a cursor class other than the default
L<DBIx::Class::Storage::DBI::Cursor>.

=back

Some real-life examples of arguments to L</connect_info> and
L<DBIx::Class::Schema/connect>

  # Simple SQLite connection
  ->connect_info([ 'dbi:SQLite:./foo.db' ]);

  # Connect via subref
  ->connect_info([ sub { DBI->connect(...) } ]);

  # Connect via subref in hashref
  ->connect_info([{
    dbh_maker => sub { DBI->connect(...) },
    on_connect_do => 'alter session ...',
  }]);

  # A bit more complicated
  ->connect_info(
    [
      'dbi:Pg:dbname=foo',
      'postgres',
      'my_pg_password',
      { AutoCommit => 1 },
      { quote_char => q{"} },
    ]
  );

  # Equivalent to the previous example
  ->connect_info(
    [
      'dbi:Pg:dbname=foo',
      'postgres',
      'my_pg_password',
      { AutoCommit => 1, quote_char => q{"}, name_sep => q{.} },
    ]
  );

  # Same, but with hashref as argument
  # See parse_connect_info for explanation
  ->connect_info(
    [{
      dsn         => 'dbi:Pg:dbname=foo',
      user        => 'postgres',
      password    => 'my_pg_password',
      AutoCommit  => 1,
      quote_char  => q{"},
      name_sep    => q{.},
    }]
  );

  # Subref + DBIx::Class-specific connection options
  ->connect_info(
    [
      sub { DBI->connect(...) },
      {
          quote_char => q{`},
          name_sep => q{@},
          on_connect_do => ['SET search_path TO myschema,otherschema,public'],
          disable_sth_caching => 1,
      },
    ]
  );



=cut

sub connect_info {
  my ($self, $info) = @_;

  return $self->_connect_info if !$info;

  $self->_connect_info($info); # copy for _connect_info

  $info = $self->_normalize_connect_info($info)
    if ref $info eq 'ARRAY';

  my %attrs = (
    %{ $self->_default_dbi_connect_attributes || {} },
    %{ $info->{attributes} || {} },
  );

  my @args = @{ $info->{arguments} };

  if (keys %attrs and ref $args[0] ne 'CODE') {
    carp_unique (
        'You provided explicit AutoCommit => 0 in your connection_info. '
      . 'This is almost universally a bad idea (see the footnotes of '
      . 'DBIx::Class::Storage::DBI for more info). If you still want to '
      . 'do this you can set $ENV{DBIC_UNSAFE_AUTOCOMMIT_OK} to disable '
      . 'this warning.'
    ) if ! $attrs{AutoCommit} and ! $ENV{DBIC_UNSAFE_AUTOCOMMIT_OK};

    push @args, \%attrs if keys %attrs;
  }

  # this is the authoritative "always an arrayref" thing fed to DBI->connect
  # OR a single-element coderef-based $dbh factory
  $self->_dbi_connect_info(\@args);

  # extract the individual storage options
  for my $storage_opt (keys %{ $info->{storage_options} }) {
    my $value = $info->{storage_options}{$storage_opt};

    $self->$storage_opt($value);
  }

  # Extract the individual sqlmaker options
  #
  # Kill sql_maker/_sql_maker_opts, so we get a fresh one with only
  #  the new set of options
  $self->_sql_maker(undef);
  $self->_sql_maker_opts({});

  for my $sql_maker_opt (keys %{ $info->{sql_maker_options} }) {
    my $value = $info->{sql_maker_options}{$sql_maker_opt};

    $self->_sql_maker_opts->{$sql_maker_opt} = $value;
  }

  # FIXME - dirty:
  # save attributes in a separate accessor so they are always
  # introspectable, even in case of a CODE $dbhmaker
  $self->_dbic_connect_attributes (\%attrs);

  return $self->_connect_info;
}

sub _dbi_connect_info {
  my $self = shift;

  return $self->{_dbi_connect_info} = $_[0]
    if @_;

  my $conninfo = $self->{_dbi_connect_info} || [];

  # last ditch effort to grab a DSN
  if ( ! defined $conninfo->[0] and $ENV{DBI_DSN} ) {
    my @new_conninfo = @$conninfo;
    $new_conninfo[0] = $ENV{DBI_DSN};
    $conninfo = \@new_conninfo;
  }

  return $conninfo;
}


sub _normalize_connect_info {
  my ($self, $info_arg) = @_;
  my %info;

  my @args = @$info_arg;  # take a shallow copy for further mutilation

  # combine/pre-parse arguments depending on invocation style

  my %attrs;
  if (ref $args[0] eq 'CODE') {     # coderef with optional \%extra_attributes
    %attrs = %{ $args[1] || {} };
    @args = $args[0];
  }
  elsif (ref $args[0] eq 'HASH') { # single hashref (i.e. Catalyst config)
    %attrs = %{$args[0]};
    @args = ();
    if (my $code = delete $attrs{dbh_maker}) {
      @args = $code;

      my @ignored = grep { delete $attrs{$_} } (qw/dsn user password/);
      if (@ignored) {
        carp sprintf (
            'Attribute(s) %s in connect_info were ignored, as they can not be applied '
          . "to the result of 'dbh_maker'",

          join (', ', map { "'$_'" } (@ignored) ),
        );
      }
    }
    else {
      @args = delete @attrs{qw/dsn user password/};
    }
  }
  else {                # otherwise assume dsn/user/password + \%attrs + \%extra_attrs
    %attrs = (
      % { $args[3] || {} },
      % { $args[4] || {} },
    );
    @args = @args[0,1,2];
  }

  $info{arguments} = \@args;

  my @storage_opts = grep exists $attrs{$_},
    @storage_options, 'cursor_class';

  @{ $info{storage_options} }{@storage_opts} =
    delete @attrs{@storage_opts} if @storage_opts;

  my @sql_maker_opts = grep exists $attrs{$_},
    qw/limit_dialect quote_char name_sep quote_names/;

  @{ $info{sql_maker_options} }{@sql_maker_opts} =
    delete @attrs{@sql_maker_opts} if @sql_maker_opts;

  $info{attributes} = \%attrs if %attrs;

  return \%info;
}

sub _default_dbi_connect_attributes () {
  +{
    AutoCommit => 1,
    PrintError => 0,
    RaiseError => 1,
    ShowErrorStatement => 1,
  };
}

=head2 on_connect_do

This method is deprecated in favour of setting via L</connect_info>.

=cut

=head2 on_disconnect_do

This method is deprecated in favour of setting via L</connect_info>.

=cut

sub _parse_connect_do {
  my ($self, $type) = @_;

  my $val = $self->$type;
  return () if not defined $val;

  my @res;

  if (not ref($val)) {
    push @res, [ 'do_sql', $val ];
  } elsif (ref($val) eq 'CODE') {
    push @res, $val;
  } elsif (ref($val) eq 'ARRAY') {
    push @res, map { [ 'do_sql', $_ ] } @$val;
  } else {
    $self->throw_exception("Invalid type for $type: ".ref($val));
  }

  return \@res;
}

=head2 dbh_do

Arguments: ($subref | $method_name), @extra_coderef_args?

Execute the given $subref or $method_name using the new exception-based
connection management.

The first two arguments will be the storage object that C<dbh_do> was called
on and a database handle to use.  Any additional arguments will be passed
verbatim to the called subref as arguments 2 and onwards.

Using this (instead of $self->_dbh or $self->dbh) ensures correct
exception handling and reconnection (or failover in future subclasses).

Your subref should have no side-effects outside of the database, as
there is the potential for your subref to be partially double-executed
if the database connection was stale/dysfunctional.

Example:

  my @stuff = $schema->storage->dbh_do(
    sub {
      my ($storage, $dbh, @cols) = @_;
      my $cols = join(q{, }, @cols);
      $dbh->selectrow_array("SELECT $cols FROM foo");
    },
    @column_list
  );

=cut

sub dbh_do {
  my $self = shift;
  my $run_target = shift; # either a coderef or a method name

  # short circuit when we know there is no need for a runner
  #
  # FIXME - assumption may be wrong
  # the rationale for the txn_depth check is that if this block is a part
  # of a larger transaction, everything up to that point is screwed anyway
  return $self->$run_target($self->_get_dbh, @_)
    if $self->{_in_do_block} or $self->transaction_depth;

  # take a ref instead of a copy, to preserve @_ aliasing
  # semantics within the coderef, but only if needed
  # (pseudoforking doesn't like this trick much)
  my $args = @_ ? \@_ : [];

  DBIx::Class::Storage::BlockRunner->new(
    storage => $self,
    wrap_txn => 0,
    retry_handler => sub {
      $_[0]->failed_attempt_count == 1
        and
      ! $_[0]->storage->connected
    },
  )->run(sub {
    $self->$run_target ($self->_get_dbh, @$args )
  });
}

sub txn_do {
  $_[0]->_get_dbh; # connects or reconnects on pid change, necessary to grab correct txn_depth
  shift->next::method(@_);
}

=head2 disconnect

Our C<disconnect> method also performs a rollback first if the
database is not in C<AutoCommit> mode.

=cut

sub disconnect {

  if( my $dbh = $_[0]->_dbh ) {

    $_[0]->_do_connection_actions(disconnect_call_ => $_) for (
      ( $_[0]->on_disconnect_call || () ),
      $_[0]->_parse_connect_do ('on_disconnect_do')
    );

    # stops the "implicit rollback on disconnect" warning
    $_[0]->_exec_txn_rollback unless $_[0]->_dbh_autocommit;

    %{ $dbh->{CachedKids} } = ();
    $dbh->disconnect;
    $_[0]->_dbh(undef);
  }
}

=head2 with_deferred_fk_checks

=over 4

=item Arguments: C<$coderef>

=item Return Value: The return value of $coderef

=back

Storage specific method to run the code ref with FK checks deferred or
in MySQL's case disabled entirely.

=cut

# Storage subclasses should override this
sub with_deferred_fk_checks {
  #my ($self, $sub) = @_;
  $_[1]->();
}

=head2 connected

=over

=item Arguments: none

=item Return Value: 1|0

=back

Verifies that the current database handle is active and ready to execute
an SQL statement (e.g. the connection did not get stale, server is still
answering, etc.) This method is used internally by L</dbh>.

=cut

sub connected {
  return 0 unless $_[0]->_seems_connected;

  #be on the safe side
  local $_[0]->_dbh->{RaiseError} = 1;

  return $_[0]->_ping;
}

sub _seems_connected {
  $_[0]->_verify_pid unless DBIx::Class::_ENV_::BROKEN_FORK;

  ($_[0]->_dbh || return 0)->FETCH('Active');
}

sub _ping {
  ($_[0]->_dbh || return 0)->ping;
}

sub ensure_connected {
  $_[0]->connected || ( $_[0]->_populate_dbh && 1 );
}

=head2 dbh

Returns a C<$dbh> - a data base handle of class L<DBI>. The returned handle
is guaranteed to be healthy by implicitly calling L</connected>, and if
necessary performing a reconnection before returning. Keep in mind that this
is very B<expensive> on some database engines. Consider using L</dbh_do>
instead.

=cut

sub dbh {
  # maybe save a ping call
  $_[0]->_dbh
    ? ( $_[0]->ensure_connected and $_[0]->_dbh )
    : $_[0]->_populate_dbh
  ;
}

# this is the internal "get dbh or connect (don't check)" method
sub _get_dbh {
  $_[0]->_verify_pid unless DBIx::Class::_ENV_::BROKEN_FORK;
  $_[0]->_dbh || $_[0]->_populate_dbh;
}

# *DELIBERATELY* not a setter (for the time being)
# Too intertwined with everything else for any kind of sanity
sub sql_maker {
  my $self = shift;

  $self->throw_exception('sql_maker() is not a setter method') if @_;

  unless ($self->_sql_maker) {
    my $sql_maker_class = $self->sql_maker_class;

    my %opts = %{$self->_sql_maker_opts||{}};
    my $dialect =
      $opts{limit_dialect}
        ||
      $self->sql_limit_dialect
        ||
      do {
        my $s_class = (ref $self) || $self;
        carp_unique (
          "Your storage class ($s_class) does not set sql_limit_dialect and you "
        . 'have not supplied an explicit limit_dialect in your connection_info. '
        . 'DBIC will attempt to use the GenericSubQ dialect, which works on most '
        . 'databases but can be (and often is) painfully slow. '
        . "Please file an RT ticket against '$s_class'"
        ) if $self->_dbi_connect_info->[0];

        'GenericSubQ';
      }
    ;

    my ($quote_char, $name_sep);

    if ($opts{quote_names}) {
      $quote_char = (delete $opts{quote_char}) || $self->sql_quote_char || do {
        my $s_class = (ref $self) || $self;
        carp_unique (
          "You requested 'quote_names' but your storage class ($s_class) does "
        . 'not explicitly define a default sql_quote_char and you have not '
        . 'supplied a quote_char as part of your connection_info. DBIC will '
        .q{default to the ANSI SQL standard quote '"', which works most of }
        . "the time. Please file an RT ticket against '$s_class'."
        );

        '"'; # RV
      };

      $name_sep = (delete $opts{name_sep}) || $self->sql_name_sep;
    }

    $self->_sql_maker($sql_maker_class->new(
      bindtype=>'columns',
      array_datatypes => 1,
      limit_dialect => $dialect,
      ($quote_char ? (quote_char => $quote_char) : ()),
      name_sep => ($name_sep || '.'),
      %opts,
    ));
  }
  return $self->_sql_maker;
}

# nothing to do by default
sub _rebless {}
sub _init {}

sub _populate_dbh {

  $_[0]->_dbh(undef); # in case ->connected failed we might get sent here

  $_[0]->_dbh_details({}); # reset everything we know

  # FIXME - this needs reenabling with the proper "no reset on same DSN" check
  #$_[0]->_sql_maker(undef); # this may also end up being different

  $_[0]->_dbh($_[0]->_connect);

  $_[0]->_conn_pid($$) unless DBIx::Class::_ENV_::BROKEN_FORK; # on win32 these are in fact threads

  $_[0]->_determine_driver;

  # Always set the transaction depth on connect, since
  #  there is no transaction in progress by definition
  $_[0]->{transaction_depth} = $_[0]->_dbh_autocommit ? 0 : 1;

  $_[0]->_run_connection_actions unless $_[0]->{_in_determine_driver};

  $_[0]->_dbh;
}

sub _run_connection_actions {

  $_[0]->_do_connection_actions(connect_call_ => $_) for (
    ( $_[0]->on_connect_call || () ),
    $_[0]->_parse_connect_do ('on_connect_do'),
  );
}



sub set_use_dbms_capability {
  $_[0]->set_inherited ($_[1], $_[2]);
}

sub get_use_dbms_capability {
  my ($self, $capname) = @_;

  my $use = $self->get_inherited ($capname);
  return defined $use
    ? $use
    : do { $capname =~ s/^_use_/_supports_/; $self->get_dbms_capability ($capname) }
  ;
}

sub set_dbms_capability {
  $_[0]->_dbh_details->{capability}{$_[1]} = $_[2];
}

sub get_dbms_capability {
  my ($self, $capname) = @_;

  my $cap = $self->_dbh_details->{capability}{$capname};

  unless (defined $cap) {
    if (my $meth = $self->can ("_determine$capname")) {
      $cap = $self->$meth ? 1 : 0;
    }
    else {
      $cap = 0;
    }

    $self->set_dbms_capability ($capname, $cap);
  }

  return $cap;
}

sub _server_info {
  my $self = shift;

  my $info;
  unless ($info = $self->_dbh_details->{info}) {

    $info = {};
    # this guarantees that problematic conninfo won't be hidden
    # by the try{} below
    $self->ensure_connected;

    my $server_version = try {
      $self->_get_server_version
    } catch {
      # driver determination *may* use this codepath
      # in which case we must rethrow
      $self->throw_exception($_) if $self->{_in_determine_driver};

      # $server_version on failure
      undef;
    };

    if (defined $server_version) {
      $info->{dbms_version} = $server_version;

      my ($numeric_version) = $server_version =~ /^([\d\.]+)/;
      my @verparts = split (/\./, $numeric_version);
      if (
        @verparts
          &&
        $verparts[0] <= 999
      ) {
        # consider only up to 3 version parts, iff not more than 3 digits
        my @use_parts;
        while (@verparts && @use_parts < 3) {
          my $p = shift @verparts;
          last if $p > 999;
          push @use_parts, $p;
        }
        push @use_parts, 0 while @use_parts < 3;

        $info->{normalized_dbms_version} = sprintf "%d.%03d%03d", @use_parts;
      }
    }

    $self->_dbh_details->{info} = $info;
  }

  return $info;
}

sub _get_server_version {
  shift->_dbh_get_info('SQL_DBMS_VER');
}

sub _dbh_get_info {
  my ($self, $info) = @_;

  if ($info =~ /[^0-9]/) {
    require DBI::Const::GetInfoType;
    $info = $DBI::Const::GetInfoType::GetInfoType{$info};
    $self->throw_exception("Info type '$_[1]' not provided by DBI::Const::GetInfoType")
      unless defined $info;
  }

  $self->_get_dbh->get_info($info);
}

sub _describe_connection {
  require DBI::Const::GetInfoReturn;

  my $self = shift;

  my $drv;
  try {
    $drv = $self->_extract_driver_from_connect_info;
    $self->ensure_connected;
  };

  $drv = "DBD::$drv" if $drv;

  my $res = {
    DBIC_DSN => $self->_dbi_connect_info->[0],
    DBI_VER => DBI->VERSION,
    DBIC_VER => DBIx::Class->VERSION,
    DBIC_DRIVER => ref $self,
    $drv ? (
      DBD => $drv,
      DBD_VER => try { $drv->VERSION },
    ) : (),
  };

  # try to grab data even if we never managed to connect
  # will cover us in cases of an oddly broken half-connect
  for my $inf (
    #keys %DBI::Const::GetInfoType::GetInfoType,
    qw/
      SQL_CURSOR_COMMIT_BEHAVIOR
      SQL_CURSOR_ROLLBACK_BEHAVIOR
      SQL_CURSOR_SENSITIVITY
      SQL_DATA_SOURCE_NAME
      SQL_DBMS_NAME
      SQL_DBMS_VER
      SQL_DEFAULT_TXN_ISOLATION
      SQL_DM_VER
      SQL_DRIVER_NAME
      SQL_DRIVER_ODBC_VER
      SQL_DRIVER_VER
      SQL_EXPRESSIONS_IN_ORDERBY
      SQL_GROUP_BY
      SQL_IDENTIFIER_CASE
      SQL_IDENTIFIER_QUOTE_CHAR
      SQL_MAX_CATALOG_NAME_LEN
      SQL_MAX_COLUMN_NAME_LEN
      SQL_MAX_IDENTIFIER_LEN
      SQL_MAX_TABLE_NAME_LEN
      SQL_MULTIPLE_ACTIVE_TXN
      SQL_MULT_RESULT_SETS
      SQL_NEED_LONG_DATA_LEN
      SQL_NON_NULLABLE_COLUMNS
      SQL_ODBC_VER
      SQL_QUALIFIER_NAME_SEPARATOR
      SQL_QUOTED_IDENTIFIER_CASE
      SQL_TXN_CAPABLE
      SQL_TXN_ISOLATION_OPTION
    /
  ) {
    # some drivers barf on things they do not know about instead
    # of returning undef
    my $v = try { $self->_dbh_get_info($inf) };
    next unless defined $v;

    #my $key = sprintf( '%s(%s)', $inf, $DBI::Const::GetInfoType::GetInfoType{$inf} );
    my $expl = DBI::Const::GetInfoReturn::Explain($inf, $v);
    $res->{$inf} = DBI::Const::GetInfoReturn::Format($inf, $v) . ( $expl ? " ($expl)" : '' );
  }

  $res;
}

sub _determine_driver {
  my ($self) = @_;

  if ((not $self->_driver_determined) && (not $self->{_in_determine_driver})) {
    my $started_connected = 0;
    local $self->{_in_determine_driver} = 1;

    if (ref($self) eq __PACKAGE__) {
      my $driver;
      if ($self->_dbh) { # we are connected
        $driver = $self->_dbh->{Driver}{Name};
        $started_connected = 1;
      }
      else {
        $driver = $self->_extract_driver_from_connect_info;
      }

      if ($driver) {
        my $storage_class = "DBIx::Class::Storage::DBI::${driver}";
        if ($self->load_optional_class($storage_class)) {
          mro::set_mro($storage_class, 'c3');
          bless $self, $storage_class;
          $self->_rebless();
        }
        else {
          $self->_warn_undetermined_driver(
            'This version of DBIC does not yet seem to supply a driver for '
          . "your particular RDBMS and/or connection method ('$driver')."
          );
        }
      }
      else {
        $self->_warn_undetermined_driver(
          'Unable to extract a driver name from connect info - this '
        . 'should not have happened.'
        );
      }
    }

    $self->_driver_determined(1);

    Class::C3->reinitialize() if DBIx::Class::_ENV_::OLD_MRO;

    if ($self->can('source_bind_attributes')) {
      $self->throw_exception(
        "Your storage subclass @{[ ref $self ]} provides (or inherits) the method "
      . 'source_bind_attributes() for which support has been removed as of Jan 2013. '
      . 'If you are not sure how to proceed please contact the development team via '
      . DBIx::Class::_ENV_::HELP_URL
      );
    }

    $self->_init; # run driver-specific initializations

    $self->_run_connection_actions
        if !$started_connected && defined $self->_dbh;
  }
}

sub _extract_driver_from_connect_info {
  my $self = shift;

  my $drv;

  # if connect_info is a CODEREF, we have no choice but to connect
  if (
    ref $self->_dbi_connect_info->[0]
      and
    reftype $self->_dbi_connect_info->[0] eq 'CODE'
  ) {
    $self->_populate_dbh;
    $drv = $self->_dbh->{Driver}{Name};
  }
  else {
    # try to use dsn to not require being connected, the driver may still
    # force a connection later in _rebless to determine version
    # (dsn may not be supplied at all if all we do is make a mock-schema)
    #
    # Use the same regex as the one used by DBI itself (even if the use of
    # \w is odd given unicode):
    # https://metacpan.org/source/TIMB/DBI-1.634/DBI.pm#L621
    #
    # DO NOT use https://metacpan.org/source/TIMB/DBI-1.634/DBI.pm#L559-566
    # as there is a long-standing precedent of not loading DBI.pm until the
    # very moment we are actually connecting
    #
    ($drv) = ($self->_dbi_connect_info->[0] || '') =~ /^dbi:(\w*)/i;
    $drv ||= $ENV{DBI_DRIVER};
  }

  return $drv;
}

sub _determine_connector_driver {
  my ($self, $conn) = @_;

  my $dbtype = $self->_dbh_get_info('SQL_DBMS_NAME');

  if (not $dbtype) {
    $self->_warn_undetermined_driver(
      'Unable to retrieve RDBMS type (SQL_DBMS_NAME) of the engine behind your '
    . "$conn connector - this should not have happened."
    );
    return;
  }

  $dbtype =~ s/\W/_/gi;

  my $subclass = "DBIx::Class::Storage::DBI::${conn}::${dbtype}";
  return if $self->isa($subclass);

  if ($self->load_optional_class($subclass)) {
    bless $self, $subclass;
    $self->_rebless;
  }
  else {
    $self->_warn_undetermined_driver(
      'This version of DBIC does not yet seem to supply a driver for '
    . "your particular RDBMS and/or connection method ('$conn/$dbtype')."
    );
  }
}

sub _warn_undetermined_driver {
  my ($self, $msg) = @_;

  require Data::Dumper::Concise;

  carp_once ($msg . ' While we will attempt to continue anyway, the results '
  . 'are likely to be underwhelming. Please upgrade DBIC, and if this message '
  . "does not go away, file a bugreport including the following info:\n"
  . Data::Dumper::Concise::Dumper($self->_describe_connection)
  );
}

sub _do_connection_actions {
  my ($self, $method_prefix, $call, @args) = @_;

  try {
    if (not ref($call)) {
      my $method = $method_prefix . $call;
      $self->$method(@args);
    }
    elsif (ref($call) eq 'CODE') {
      $self->$call(@args);
    }
    elsif (ref($call) eq 'ARRAY') {
      if (ref($call->[0]) ne 'ARRAY') {
        $self->_do_connection_actions($method_prefix, $_) for @$call;
      }
      else {
        $self->_do_connection_actions($method_prefix, @$_) for @$call;
      }
    }
    else {
      $self->throw_exception (sprintf ("Don't know how to process conection actions of type '%s'", ref($call)) );
    }
  }
  catch {
    if ( $method_prefix =~ /^connect/ ) {
      # this is an on_connect cycle - we can't just throw while leaving
      # a handle in an undefined state in our storage object
      # kill it with fire and rethrow
      $self->_dbh(undef);
      $self->throw_exception( $_[0] );
    }
    else {
      carp "Disconnect action failed: $_[0]";
    }
  };

  return $self;
}

sub connect_call_do_sql {
  my $self = shift;
  $self->_do_query(@_);
}

sub disconnect_call_do_sql {
  my $self = shift;
  $self->_do_query(@_);
}

=head2 connect_call_datetime_setup

A no-op stub method, provided so that one can always safely supply the
L<connection option|/DBIx::Class specific connection attributes>

 on_connect_call => 'datetime_setup'

This way one does not need to know in advance whether the underlying
storage requires any sort of hand-holding when dealing with calendar
data.

=cut

sub connect_call_datetime_setup { 1 }

sub _do_query {
  my ($self, $action) = @_;

  if (ref $action eq 'CODE') {
    $action = $action->($self);
    $self->_do_query($_) foreach @$action;
  }
  else {
    # Most debuggers expect ($sql, @bind), so we need to exclude
    # the attribute hash which is the second argument to $dbh->do
    # furthermore the bind values are usually to be presented
    # as named arrayref pairs, so wrap those here too
    my @do_args = (ref $action eq 'ARRAY') ? (@$action) : ($action);
    my $sql = shift @do_args;
    my $attrs = shift @do_args;
    my @bind = map { [ undef, $_ ] } @do_args;

    $self->dbh_do(sub {
      $_[0]->_query_start($sql, \@bind);
      $_[1]->do($sql, $attrs, @do_args);
      $_[0]->_query_end($sql, \@bind);
    });
  }

  return $self;
}

sub _connect {
  my $self = shift;

  my $info = $self->_dbi_connect_info;

  $self->throw_exception("You did not provide any connection_info")
    unless defined $info->[0];

  my ($old_connect_via, $dbh);

  local $DBI::connect_via = 'connect' if $INC{'Apache/DBI.pm'} && $ENV{MOD_PERL};

  # this odd anonymous coderef dereference is in fact really
  # necessary to avoid the unwanted effect described in perl5
  # RT#75792
  #
  # in addition the coderef itself can't reside inside the try{} block below
  # as it somehow triggers a leak under perl -d
  my $dbh_error_handler_installer = sub {
    weaken (my $weak_self = $_[0]);

    # the coderef is blessed so we can distinguish it from externally
    # supplied handles (which must be preserved)
    $_[1]->{HandleError} = bless sub {
      if ($weak_self) {
        $weak_self->throw_exception("DBI Exception: $_[0]");
      }
      else {
        # the handler may be invoked by something totally out of
        # the scope of DBIC
        DBIx::Class::Exception->throw("DBI Exception (unhandled by DBIC, ::Schema GCed): $_[0]");
      }
    }, '__DBIC__DBH__ERROR__HANDLER__';
  };

  try {
    if(ref $info->[0] eq 'CODE') {
      $dbh = $info->[0]->();
    }
    else {
      require DBI;
      $dbh = DBI->connect(@$info);
    }

    die $DBI::errstr unless $dbh;

    die sprintf ("%s fresh DBI handle with a *false* 'Active' attribute. "
      . 'This handle is disconnected as far as DBIC is concerned, and we can '
      . 'not continue',
      ref $info->[0] eq 'CODE'
        ? "Connection coderef $info->[0] returned a"
        : 'DBI->connect($schema->storage->connect_info) resulted in a'
    ) unless $dbh->FETCH('Active');

    # sanity checks unless asked otherwise
    unless ($self->unsafe) {

      $self->throw_exception(
        'Refusing clobbering of {HandleError} installed on externally supplied '
       ."DBI handle $dbh. Either remove the handler or use the 'unsafe' attribute."
      ) if $dbh->{HandleError} and ref $dbh->{HandleError} ne '__DBIC__DBH__ERROR__HANDLER__';

      # Default via _default_dbi_connect_attributes is 1, hence it was an explicit
      # request, or an external handle. Complain and set anyway
      unless ($dbh->{RaiseError}) {
        carp( ref $info->[0] eq 'CODE'

          ? "The 'RaiseError' of the externally supplied DBI handle is set to false. "
           ."DBIx::Class will toggle it back to true, unless the 'unsafe' connect "
           .'attribute has been supplied'

          : 'RaiseError => 0 supplied in your connection_info, without an explicit '
           .'unsafe => 1. Toggling RaiseError back to true'
        );

        $dbh->{RaiseError} = 1;
      }

      $dbh_error_handler_installer->($self, $dbh);
    }
  }
  catch {
    $self->throw_exception("DBI Connection failed: $_")
  };

  $self->_dbh_autocommit($dbh->{AutoCommit});
  return $dbh;
}

sub txn_begin {
  # this means we have not yet connected and do not know the AC status
  # (e.g. coderef $dbh), need a full-fledged connection check
  if (! defined $_[0]->_dbh_autocommit) {
    $_[0]->ensure_connected;
  }
  # Otherwise simply connect or re-connect on pid changes
  else {
    $_[0]->_get_dbh;
  }

  shift->next::method(@_);
}

sub _exec_txn_begin {
  my $self = shift;

  # if the user is utilizing txn_do - good for him, otherwise we need to
  # ensure that the $dbh is healthy on BEGIN.
  # We do this via ->dbh_do instead of ->dbh, so that the ->dbh "ping"
  # will be replaced by a failure of begin_work itself (which will be
  # then retried on reconnect)
  if ($self->{_in_do_block}) {
    $self->_dbh->begin_work;
  } else {
    $self->dbh_do(sub { $_[1]->begin_work });
  }
}

sub txn_commit {
  my $self = shift;

  $self->throw_exception("Unable to txn_commit() on a disconnected storage")
    unless $self->_seems_connected;

  # esoteric case for folks using external $dbh handles
  if (! $self->transaction_depth and ! $self->_dbh->FETCH('AutoCommit') ) {
    carp "Storage transaction_depth 0 does not match "
        ."false AutoCommit of $self->{_dbh}, attempting COMMIT anyway";
    $self->transaction_depth(1);
  }

  $self->next::method(@_);

  # if AutoCommit is disabled txn_depth never goes to 0
  # as a new txn is started immediately on commit
  $self->transaction_depth(1) if (
    !$self->transaction_depth
      and
    defined $self->_dbh_autocommit
      and
    ! $self->_dbh_autocommit
  );
}

sub _exec_txn_commit {
  shift->_dbh->commit;
}

sub txn_rollback {
  my $self = shift;

  $self->throw_exception("Unable to txn_rollback() on a disconnected storage")
    unless $self->_seems_connected;

  # esoteric case for folks using external $dbh handles
  if (! $self->transaction_depth and ! $self->_dbh->FETCH('AutoCommit') ) {
    carp "Storage transaction_depth 0 does not match "
        ."false AutoCommit of $self->{_dbh}, attempting ROLLBACK anyway";
    $self->transaction_depth(1);
  }

  $self->next::method(@_);

  # if AutoCommit is disabled txn_depth never goes to 0
  # as a new txn is started immediately on commit
  $self->transaction_depth(1) if (
    !$self->transaction_depth
      and
    defined $self->_dbh_autocommit
      and
    ! $self->_dbh_autocommit
  );
}

sub _exec_txn_rollback {
  shift->_dbh->rollback;
}

# generate the DBI-specific stubs, which then fallback to ::Storage proper
quote_sub __PACKAGE__ . "::$_" => sprintf (<<'EOS', $_) for qw(svp_begin svp_release svp_rollback);
  $_[0]->throw_exception('Unable to %s() on a disconnected storage')
    unless $_[0]->_seems_connected;
  shift->next::method(@_);
EOS

# This used to be the top-half of _execute.  It was split out to make it
#  easier to override in NoBindVars without duping the rest.  It takes up
#  all of _execute's args, and emits $sql, @bind.
sub _prep_for_execute {
  #my ($self, $op, $ident, $args) = @_;
  return shift->_gen_sql_bind(@_)
}

sub _gen_sql_bind {
  my ($self, $op, $ident, $args) = @_;

  my ($colinfos, $from);
  if ( blessed($ident) ) {
    $from = $ident->from;
    $colinfos = $ident->columns_info;
  }

  my ($sql, $bind);
  ($sql, @$bind) = $self->sql_maker->$op( ($from || $ident), @$args );

  $bind = $self->_resolve_bindattrs(
    $ident, [ @{$args->[2]{bind}||[]}, @$bind ], $colinfos
  );

  if (
    ! $ENV{DBIC_DT_SEARCH_OK}
      and
    $op eq 'select'
      and
    first {
      length ref $_->[1]
        and
      blessed($_->[1])
        and
      $_->[1]->isa('DateTime')
    } @$bind
  ) {
    carp_unique 'DateTime objects passed to search() are not supported '
      . 'properly (InflateColumn::DateTime formats and settings are not '
      . 'respected.) See ".. format a DateTime object for searching?" in '
      . 'DBIx::Class::Manual::FAQ. To disable this warning for good '
      . 'set $ENV{DBIC_DT_SEARCH_OK} to true'
  }

  return( $sql, $bind );
}

sub _resolve_bindattrs {
  my ($self, $ident, $bind, $colinfos) = @_;

  my $resolve_bindinfo = sub {
    #my $infohash = shift;

    $colinfos ||= { %{ $self->_resolve_column_info($ident) } };

    my $ret;
    if (my $col = $_[0]->{dbic_colname}) {
      $ret = { %{$_[0]} };

      $ret->{sqlt_datatype} ||= $colinfos->{$col}{data_type}
        if $colinfos->{$col}{data_type};

      $ret->{sqlt_size} ||= $colinfos->{$col}{size}
        if $colinfos->{$col}{size};
    }

    $ret || $_[0];
  };

  return [ map {
      ( ref $_ ne 'ARRAY' or @$_ != 2 ) ? [ {}, $_ ]
    : ( ! defined $_->[0] )             ? [ {}, $_->[1] ]
    : (ref $_->[0] eq 'HASH')           ? [(
                                            ! keys %{$_->[0]}
                                              or
                                            exists $_->[0]{dbd_attrs}
                                              or
                                            $_->[0]{sqlt_datatype}
                                           ) ? $_->[0]
                                             : $resolve_bindinfo->($_->[0])
                                           , $_->[1]
                                          ]
    : (ref $_->[0] eq 'SCALAR')         ? [ { sqlt_datatype => ${$_->[0]} }, $_->[1] ]
    :                                     [ $resolve_bindinfo->(
                                              { dbic_colname => $_->[0] }
                                            ), $_->[1] ]
  } @$bind ];
}

sub _format_for_trace {
  #my ($self, $bind) = @_;

  ### Turn @bind from something like this:
  ###   ( [ "artist", 1 ], [ \%attrs, 3 ] )
  ### to this:
  ###   ( "'1'", "'3'" )

  map {
    defined( $_ && $_->[1] )
      ? qq{'$_->[1]'}
      : q{NULL}
  } @{$_[1] || []};
}

sub _query_start {
  my ( $self, $sql, $bind ) = @_;

  $self->debugobj->query_start( $sql, $self->_format_for_trace($bind) )
    if $self->debug;
}

sub _query_end {
  my ( $self, $sql, $bind ) = @_;

  $self->debugobj->query_end( $sql, $self->_format_for_trace($bind) )
    if $self->debug;
}

sub _dbi_attrs_for_bind {
  my ($self, $ident, $bind) = @_;

  my @attrs;

  for (map { $_->[0] } @$bind) {
    push @attrs, do {
      if (exists $_->{dbd_attrs}) {
        $_->{dbd_attrs}
      }
      elsif($_->{sqlt_datatype}) {
        # cache the result in the dbh_details hash, as it can not change unless
        # we connect to something else
        my $cache = $self->_dbh_details->{_datatype_map_cache} ||= {};
        if (not exists $cache->{$_->{sqlt_datatype}}) {
          $cache->{$_->{sqlt_datatype}} = $self->bind_attribute_by_data_type($_->{sqlt_datatype}) || undef;
        }
        $cache->{$_->{sqlt_datatype}};
      }
      else {
        undef;  # always push something at this position
      }
    }
  }

  return \@attrs;
}

sub _execute {
  my ($self, $op, $ident, @args) = @_;

  my ($sql, $bind) = $self->_prep_for_execute($op, $ident, \@args);

  # not even a PID check - we do not care about the state of the _dbh.
  # All we need is to get the appropriate drivers loaded if they aren't
  # already so that the assumption in ad7c50fc26e holds
  $self->_populate_dbh unless $self->_dbh;

  $self->dbh_do( _dbh_execute =>     # retry over disconnects
    $sql,
    $bind,
    $self->_dbi_attrs_for_bind($ident, $bind),
  );
}

sub _dbh_execute {
  my ($self, $dbh, $sql, $bind, $bind_attrs) = @_;

  $self->_query_start( $sql, $bind );

  my $sth = $self->_bind_sth_params(
    $self->_prepare_sth($dbh, $sql),
    $bind,
    $bind_attrs,
  );

  # Can this fail without throwing an exception anyways???
  my $rv = $sth->execute();
  $self->throw_exception(
    $sth->errstr || $sth->err || 'Unknown error: execute() returned false, but error flags were not set...'
  ) if !$rv;

  $self->_query_end( $sql, $bind );

  return (wantarray ? ($rv, $sth, @$bind) : $rv);
}

sub _prepare_sth {
  my ($self, $dbh, $sql) = @_;

  # 3 is the if_active parameter which avoids active sth re-use
  my $sth = $self->disable_sth_caching
    ? $dbh->prepare($sql)
    : $dbh->prepare_cached($sql, {}, 3);

  # XXX You would think RaiseError would make this impossible,
  #  but apparently that's not true :(
  $self->throw_exception(
    $dbh->errstr
      ||
    sprintf( "\$dbh->prepare() of '%s' through %s failed *silently* without "
            .'an exception and/or setting $dbh->errstr',
      length ($sql) > 20
        ? substr($sql, 0, 20) . '...'
        : $sql
      ,
      'DBD::' . $dbh->{Driver}{Name},
    )
  ) if !$sth;

  $sth;
}

sub _bind_sth_params {
  my ($self, $sth, $bind, $bind_attrs) = @_;

  for my $i (0 .. $#$bind) {
    if (ref $bind->[$i][1] eq 'SCALAR') {  # any scalarrefs are assumed to be bind_inouts
      $sth->bind_param_inout(
        $i + 1, # bind params counts are 1-based
        $bind->[$i][1],
        $bind->[$i][0]{dbd_size} || $self->_max_column_bytesize($bind->[$i][0]), # size
        $bind_attrs->[$i],
      );
    }
    else {
      # FIXME SUBOPTIMAL - DBI needs fixing to always stringify regardless of DBD
      my $v = ( length ref $bind->[$i][1] and is_plain_value $bind->[$i][1] )
        ? "$bind->[$i][1]"
        : $bind->[$i][1]
      ;

      $sth->bind_param(
        $i + 1,
        # The temp-var is CRUCIAL - DO NOT REMOVE IT, breaks older DBD::SQLite RT#79576
        $v,
        $bind_attrs->[$i],
      );
    }
  }

  $sth;
}

sub _prefetch_autovalues {
  my ($self, $source, $colinfo, $to_insert) = @_;

  my %values;
  for my $col (keys %$colinfo) {
    if (
      $colinfo->{$col}{auto_nextval}
        and
      (
        ! exists $to_insert->{$col}
          or
        is_literal_value($to_insert->{$col})
      )
    ) {
      $values{$col} = $self->_sequence_fetch(
        'NEXTVAL',
        ( $colinfo->{$col}{sequence} ||=
            $self->_dbh_get_autoinc_seq($self->_get_dbh, $source, $col)
        ),
      );
    }
  }

  \%values;
}

sub insert {
  my ($self, $source, $to_insert) = @_;

  my $col_infos = $source->columns_info;

  my $prefetched_values = $self->_prefetch_autovalues($source, $col_infos, $to_insert);

  # fuse the values, but keep a separate list of prefetched_values so that
  # they can be fused once again with the final return
  $to_insert = { %$to_insert, %$prefetched_values };

  # FIXME - we seem to assume undef values as non-supplied. This is wrong.
  # Investigate what does it take to s/defined/exists/
  my %pcols = map { $_ => 1 } $source->primary_columns;
  my (%retrieve_cols, $autoinc_supplied, $retrieve_autoinc_col);
  for my $col ($source->columns) {
    if ($col_infos->{$col}{is_auto_increment}) {
      $autoinc_supplied ||= 1 if defined $to_insert->{$col};
      $retrieve_autoinc_col ||= $col unless $autoinc_supplied;
    }

    # nothing to retrieve when explicit values are supplied
    next if (
      defined $to_insert->{$col} and ! is_literal_value($to_insert->{$col})
    );

    # the 'scalar keys' is a trick to preserve the ->columns declaration order
    $retrieve_cols{$col} = scalar keys %retrieve_cols if (
      $pcols{$col}
        or
      $col_infos->{$col}{retrieve_on_insert}
    );
  };

  local $self->{_autoinc_supplied_for_op} = $autoinc_supplied;
  local $self->{_perform_autoinc_retrieval} = $retrieve_autoinc_col;

  my ($sqla_opts, @ir_container);
  if (%retrieve_cols and $self->_use_insert_returning) {
    $sqla_opts->{returning_container} = \@ir_container
      if $self->_use_insert_returning_bound;

    $sqla_opts->{returning} = [
      sort { $retrieve_cols{$a} <=> $retrieve_cols{$b} } keys %retrieve_cols
    ];
  }

  my ($rv, $sth) = $self->_execute('insert', $source, $to_insert, $sqla_opts);

  my %returned_cols = %$to_insert;
  if (my $retlist = $sqla_opts->{returning}) {  # if IR is supported - we will get everything in one set

    unless( @ir_container ) {
      try {

        # FIXME - need to investigate why Caelum silenced this in 4d4dc518
        local $SIG{__WARN__} = sub {};

        @ir_container = $sth->fetchrow_array;
        $sth->finish;

      } catch {
        # Evict the $sth from the cache in case we got here, since the finish()
        # is crucial, at least on older Firebirds, possibly on other engines too
        #
        # It would be too complex to make this a proper subclass override,
        # and besides we already take the try{} penalty, adding a catch that
        # triggers infrequently is a no-brainer
        #
        if( my $kids = $self->_dbh->{CachedKids} ) {
          $kids->{$_} == $sth and delete $kids->{$_}
            for keys %$kids
        }
      };
    }

    @returned_cols{@$retlist} = @ir_container if @ir_container;
  }
  else {
    # pull in PK if needed and then everything else
    if (my @missing_pri = grep { $pcols{$_} } keys %retrieve_cols) {

      $self->throw_exception( "Missing primary key but Storage doesn't support last_insert_id" )
        unless $self->can('last_insert_id');

      my @pri_values = $self->last_insert_id($source, @missing_pri);

      $self->throw_exception( "Can't get last insert id" )
        unless (@pri_values == @missing_pri);

      @returned_cols{@missing_pri} = @pri_values;
      delete @retrieve_cols{@missing_pri};
    }

    # if there is more left to pull
    if (%retrieve_cols) {
      $self->throw_exception(
        'Unable to retrieve additional columns without a Primary Key on ' . $source->source_name
      ) unless %pcols;

      my @left_to_fetch = sort { $retrieve_cols{$a} <=> $retrieve_cols{$b} } keys %retrieve_cols;

      my $cur = DBIx::Class::ResultSet->new($source, {
        where => { map { $_ => $returned_cols{$_} } (keys %pcols) },
        select => \@left_to_fetch,
      })->cursor;

      @returned_cols{@left_to_fetch} = $cur->next;

      $self->throw_exception('Duplicate row returned for PK-search after fresh insert')
        if scalar $cur->next;
    }
  }

  return { %$prefetched_values, %returned_cols };
}

sub insert_bulk {
  carp_unique(
    'insert_bulk() should have never been exposed as a public method and '
  . 'calling it is depecated as of Aug 2014. If you believe having a genuine '
  . 'use for this method please contact the development team via '
  . DBIx::Class::_ENV_::HELP_URL
  );

  return '0E0' unless @{$_[3]||[]};

  shift->_insert_bulk(@_);
}

sub _insert_bulk {
  my ($self, $source, $cols, $data) = @_;

  $self->throw_exception('Calling _insert_bulk without a dataset to process makes no sense')
    unless @{$data||[]};

  my $colinfos = $source->columns_info($cols);

  local $self->{_autoinc_supplied_for_op} =
    (grep { $_->{is_auto_increment} } values %$colinfos)
      ? 1
      : 0
  ;

  # get a slice type index based on first row of data
  # a "column" in this context may refer to more than one bind value
  # e.g. \[ '?, ?', [...], [...] ]
  #
  # construct the value type index - a description of values types for every
  # per-column slice of $data:
  #
  # nonexistent - nonbind literal
  # 0 - regular value
  # [] of bindattrs - resolved attribute(s) of bind(s) passed via literal+bind \[] combo
  #
  # also construct the column hash to pass to the SQL generator. For plain
  # (non literal) values - convert the members of the first row into a
  # literal+bind combo, with extra positional info in the bind attr hashref.
  # This will allow us to match the order properly, and is so contrived
  # because a user-supplied literal/bind (or something else specific to a
  # resultsource and/or storage driver) can inject extra binds along the
  # way, so one can't rely on "shift positions" ordering at all. Also we
  # can't just hand SQLA a set of some known "values" (e.g. hashrefs that
  # can be later matched up by address), because we want to supply a real
  # value on which perhaps e.g. datatype checks will be performed
  my ($proto_data, $serialized_bind_type_by_col_idx);
  for my $col_idx (0..$#$cols) {
    my $colname = $cols->[$col_idx];
    if (ref $data->[0][$col_idx] eq 'SCALAR') {
      # no bind value at all - no type

      $proto_data->{$colname} = $data->[0][$col_idx];
    }
    elsif (ref $data->[0][$col_idx] eq 'REF' and ref ${$data->[0][$col_idx]} eq 'ARRAY' ) {
      # repack, so we don't end up mangling the original \[]
      my ($sql, @bind) = @${$data->[0][$col_idx]};

      # normalization of user supplied stuff
      my $resolved_bind = $self->_resolve_bindattrs(
        $source, \@bind, $colinfos,
      );

      # store value-less (attrs only) bind info - we will be comparing all
      # supplied binds against this for sanity
      $serialized_bind_type_by_col_idx->{$col_idx} = serialize [ map { $_->[0] } @$resolved_bind ];

      $proto_data->{$colname} = \[ $sql, map { [
        # inject slice order to use for $proto_bind construction
          { %{$resolved_bind->[$_][0]}, _bind_data_slice_idx => $col_idx, _literal_bind_subindex => $_+1 }
            =>
          $resolved_bind->[$_][1]
        ] } (0 .. $#bind)
      ];
    }
    else {
      $serialized_bind_type_by_col_idx->{$col_idx} = undef;

      $proto_data->{$colname} = \[ '?', [
        { dbic_colname => $colname, _bind_data_slice_idx => $col_idx }
          =>
        $data->[0][$col_idx]
      ] ];
    }
  }

  my ($sql, $proto_bind) = $self->_prep_for_execute (
    'insert',
    $source,
    [ $proto_data ],
  );

  if (! @$proto_bind and keys %$serialized_bind_type_by_col_idx) {
    # if the bindlist is empty and we had some dynamic binds, this means the
    # storage ate them away (e.g. the NoBindVars component) and interpolated
    # them directly into the SQL. This obviously can't be good for multi-inserts
    $self->throw_exception('Unable to invoke fast-path insert without storage placeholder support');
  }

  # sanity checks
  # FIXME - devise a flag "no babysitting" or somesuch to shut this off
  #
  # use an error reporting closure for convenience (less to pass)
  my $bad_slice_report_cref = sub {
    my ($msg, $r_idx, $c_idx) = @_;
    $self->throw_exception(sprintf "%s for column '%s' in populate slice:\n%s",
      $msg,
      $cols->[$c_idx],
      do {
        require Data::Dumper::Concise;
        local $Data::Dumper::Maxdepth = 5;
        Data::Dumper::Concise::Dumper ({
          map { $cols->[$_] =>
            $data->[$r_idx][$_]
          } 0..$#$cols
        }),
      }
    );
  };

  for my $col_idx (0..$#$cols) {
    my $reference_val = $data->[0][$col_idx];

    for my $row_idx (1..$#$data) {  # we are comparing against what we got from [0] above, hence start from 1
      my $val = $data->[$row_idx][$col_idx];

      if (! exists $serialized_bind_type_by_col_idx->{$col_idx}) { # literal no binds
        if (ref $val ne 'SCALAR') {
          $bad_slice_report_cref->(
            "Incorrect value (expecting SCALAR-ref \\'$$reference_val')",
            $row_idx,
            $col_idx,
          );
        }
        elsif ($$val ne $$reference_val) {
          $bad_slice_report_cref->(
            "Inconsistent literal SQL value (expecting \\'$$reference_val')",
            $row_idx,
            $col_idx,
          );
        }
      }
      elsif (! defined $serialized_bind_type_by_col_idx->{$col_idx} ) {  # regular non-literal value
        if (is_literal_value($val)) {
          $bad_slice_report_cref->("Literal SQL found where a plain bind value is expected", $row_idx, $col_idx);
        }
      }
      else {  # binds from a \[], compare type and attrs
        if (ref $val ne 'REF' or ref $$val ne 'ARRAY') {
          $bad_slice_report_cref->(
            "Incorrect value (expecting ARRAYREF-ref \\['${$reference_val}->[0]', ... ])",
            $row_idx,
            $col_idx,
          );
        }
        # start drilling down and bail out early on identical refs
        elsif (
          $reference_val != $val
            or
          $$reference_val != $$val
        ) {
          if (${$val}->[0] ne ${$reference_val}->[0]) {
            $bad_slice_report_cref->(
              "Inconsistent literal/bind SQL (expecting \\['${$reference_val}->[0]', ... ])",
              $row_idx,
              $col_idx,
            );
          }
          # need to check the bind attrs - a bind will happen only once for
          # the entire dataset, so any changes further down will be ignored.
          elsif (
            $serialized_bind_type_by_col_idx->{$col_idx}
              ne
            serialize [
              map
              { $_->[0] }
              @{$self->_resolve_bindattrs(
                $source, [ @{$$val}[1 .. $#$$val] ], $colinfos,
              )}
            ]
          ) {
            $bad_slice_report_cref->(
              'Differing bind attributes on literal/bind values not supported',
              $row_idx,
              $col_idx,
            );
          }
        }
      }
    }
  }

  # neither _dbh_execute_for_fetch, nor _dbh_execute_inserts_with_no_binds
  # are atomic (even if execute_for_fetch is a single call). Thus a safety
  # scope guard
  my $guard = $self->txn_scope_guard;

  $self->_query_start( $sql, @$proto_bind ? [[undef => '__BULK_INSERT__' ]] : () );
  my $sth = $self->_prepare_sth($self->_dbh, $sql);
  my $rv = do {
    if (@$proto_bind) {
      # proto bind contains the information on which pieces of $data to pull
      # $cols is passed in only for prettier error-reporting
      $self->_dbh_execute_for_fetch( $source, $sth, $proto_bind, $cols, $data );
    }
    else {
      # bind_param_array doesn't work if there are no binds
      $self->_dbh_execute_inserts_with_no_binds( $sth, scalar @$data );
    }
  };

  $self->_query_end( $sql, @$proto_bind ? [[ undef => '__BULK_INSERT__' ]] : () );

  $guard->commit;

  return wantarray ? ($rv, $sth, @$proto_bind) : $rv;
}

# execute_for_fetch is capable of returning data just fine (it means it
# can be used for INSERT...RETURNING and UPDATE...RETURNING. Since this
# is the void-populate fast-path we will just ignore this altogether
# for the time being.
sub _dbh_execute_for_fetch {
  my ($self, $source, $sth, $proto_bind, $cols, $data) = @_;

  # If we have any bind attributes to take care of, we will bind the
  # proto-bind data (which will never be used by execute_for_fetch)
  # However since column bindtypes are "sticky", this is sufficient
  # to get the DBD to apply the bindtype to all values later on
  my $bind_attrs = $self->_dbi_attrs_for_bind($source, $proto_bind);

  for my $i (0 .. $#$proto_bind) {
    $sth->bind_param (
      $i+1, # DBI bind indexes are 1-based
      $proto_bind->[$i][1],
      $bind_attrs->[$i],
    ) if defined $bind_attrs->[$i];
  }

  # At this point $data slots named in the _bind_data_slice_idx of
  # each piece of $proto_bind are either \[]s or plain values to be
  # passed in. Construct the dispensing coderef. *NOTE* the order
  # of $data will differ from this of the ?s in the SQL (due to
  # alphabetical ordering by colname). We actually do want to
  # preserve this behavior so that prepare_cached has a better
  # chance of matching on unrelated calls

  my $fetch_row_idx = -1; # saner loop this way
  my $fetch_tuple = sub {
    return undef if ++$fetch_row_idx > $#$data;

    return [ map {
      my $v = ! defined $_->{_literal_bind_subindex}

        ? $data->[ $fetch_row_idx ]->[ $_->{_bind_data_slice_idx} ]

        # There are no attributes to resolve here - we already did everything
        # when we constructed proto_bind. However we still want to sanity-check
        # what the user supplied, so pass stuff through to the resolver *anyway*
        : $self->_resolve_bindattrs (
            undef,  # a fake rsrc
            [ ${ $data->[ $fetch_row_idx ]->[ $_->{_bind_data_slice_idx} ]}->[ $_->{_literal_bind_subindex} ] ],
            {},     # a fake column_info bag
          )->[0][1]
      ;

      # FIXME SUBOPTIMAL - DBI needs fixing to always stringify regardless of DBD
      # For the time being forcibly stringify whatever is stringifiable
      (length ref $v and is_plain_value $v)
        ? "$v"
        : $v
      ;
    } map { $_->[0] } @$proto_bind ];
  };

  my $tuple_status = [];
  my ($rv, $err);
  try {
    $rv = $sth->execute_for_fetch(
      $fetch_tuple,
      $tuple_status,
    );
  }
  catch {
    $err = shift;
  };

  # Not all DBDs are create equal. Some throw on error, some return
  # an undef $rv, and some set $sth->err - try whatever we can
  $err = ($sth->errstr || 'UNKNOWN ERROR ($sth->errstr is unset)') if (
    ! defined $err
      and
    ( !defined $rv or $sth->err )
  );

  # Statement must finish even if there was an exception.
  try {
    $sth->finish
  }
  catch {
    $err = shift unless defined $err
  };

  if (defined $err) {
    my $i = 0;
    ++$i while $i <= $#$tuple_status && !ref $tuple_status->[$i];

    $self->throw_exception("Unexpected populate error: $err")
      if ($i > $#$tuple_status);

    require Data::Dumper::Concise;
    $self->throw_exception(sprintf "execute_for_fetch() aborted with '%s' at populate slice:\n%s",
      ($tuple_status->[$i][1] || $err),
      Data::Dumper::Concise::Dumper( { map { $cols->[$_] => $data->[$i][$_] } (0 .. $#$cols) } ),
    );
  }

  return $rv;
}

sub _dbh_execute_inserts_with_no_binds {
  my ($self, $sth, $count) = @_;

  my $err;
  try {
    my $dbh = $self->_get_dbh;
    local $dbh->{RaiseError} = 1;
    local $dbh->{PrintError} = 0;

    $sth->execute foreach 1..$count;
  }
  catch {
    $err = shift;
  };

  # Make sure statement is finished even if there was an exception.
  try {
    $sth->finish
  }
  catch {
    $err = shift unless defined $err;
  };

  $self->throw_exception($err) if defined $err;

  return $count;
}

sub update {
  #my ($self, $source, @args) = @_;
  shift->_execute('update', @_);
}


sub delete {
  #my ($self, $source, @args) = @_;
  shift->_execute('delete', @_);
}

sub _select {
  my $self = shift;
  $self->_execute($self->_select_args(@_));
}

sub _select_args_to_query {
  my $self = shift;

  $self->throw_exception(
    "Unable to generate limited query representation with 'software_limit' enabled"
  ) if ($_[3]->{software_limit} and ($_[3]->{offset} or $_[3]->{rows}) );

  # my ($op, $ident, $select, $cond, $rs_attrs, $rows, $offset)
  #  = $self->_select_args($ident, $select, $cond, $attrs);
  my ($op, $ident, @args) =
    $self->_select_args(@_);

  # my ($sql, $prepared_bind) = $self->_gen_sql_bind($op, $ident, [ $select, $cond, $rs_attrs, $rows, $offset ]);
  my ($sql, $bind) = $self->_gen_sql_bind($op, $ident, \@args);

  # reuse the bind arrayref
  unshift @{$bind}, "($sql)";
  \$bind;
}

sub _select_args {
  my ($self, $ident, $select, $where, $orig_attrs) = @_;

  # FIXME - that kind of caching would be nice to have
  # however currently we *may* pass the same $orig_attrs
  # with different ident/select/where
  # the whole interface needs to be rethought, since it
  # was centered around the flawed SQLA API. We can do
  # soooooo much better now. But that is also another
  # battle...
  #return (
  #  'select', $orig_attrs->{!args_as_stored_at_the_end_of_this_method!}
  #) if $orig_attrs->{!args_as_stored_at_the_end_of_this_method!};

  my $sql_maker = $self->sql_maker;

  my $attrs = {
    %$orig_attrs,
    select => $select,
    from => $ident,
    where => $where,
  };

  # Sanity check the attributes (SQLMaker does it too, but
  # in case of a software_limit we'll never reach there)
  if (defined $attrs->{offset}) {
    $self->throw_exception('A supplied offset attribute must be a non-negative integer')
      if ( $attrs->{offset} =~ /\D/ or $attrs->{offset} < 0 );
  }

  if (defined $attrs->{rows}) {
    $self->throw_exception("The rows attribute must be a positive integer if present")
      if ( $attrs->{rows} =~ /\D/ or $attrs->{rows} <= 0 );
  }
  elsif ($attrs->{offset}) {
    # MySQL actually recommends this approach.  I cringe.
    $attrs->{rows} = $sql_maker->__max_int;
  }

  # see if we will need to tear the prefetch apart to satisfy group_by == select
  # this is *extremely tricky* to get right, I am still not sure I did
  #
  my ($prefetch_needs_subquery, @limit_args);

  if ( $attrs->{_grouped_by_distinct} and $attrs->{collapse} ) {
    # we already know there is a valid group_by (we made it) and we know it is
    # intended to be based *only* on non-multi stuff
    # short circuit the group_by parsing below
    $prefetch_needs_subquery = 1;
  }
  elsif (
    # The rationale is that even if we do *not* have collapse, we still
    # need to wrap the core grouped select/group_by in a subquery
    # so that databases that care about group_by/select equivalence
    # are happy (this includes MySQL in strict_mode)
    # If any of the other joined tables are referenced in the group_by
    # however - the user is on their own
    ( $prefetch_needs_subquery or ! $attrs->{_simple_passthrough_construction} )
      and
    $attrs->{group_by}
      and
    @{$attrs->{group_by}}
      and
    my $grp_aliases = try { # try{} because $attrs->{from} may be unreadable
      $self->_resolve_aliastypes_from_select_args({ from => $attrs->{from}, group_by => $attrs->{group_by} })
    }
  ) {
    # no aliases other than our own in group_by
    # if there are - do not allow subquery even if limit is present
    $prefetch_needs_subquery = ! scalar grep { $_ ne $attrs->{alias} } keys %{ $grp_aliases->{grouping} || {} };
  }
  elsif ( $attrs->{rows} && $attrs->{collapse} ) {
    # active collapse with a limit - that one is a no-brainer unless
    # overruled by a group_by above
    $prefetch_needs_subquery = 1;
  }

  if ($prefetch_needs_subquery) {
    $attrs = $self->_adjust_select_args_for_complex_prefetch ($attrs);
  }
  elsif (! $attrs->{software_limit} ) {
    push @limit_args, (
      $attrs->{rows} || (),
      $attrs->{offset} || (),
    );
  }

  # try to simplify the joinmap further (prune unreferenced type-single joins)
  if (
    ! $prefetch_needs_subquery  # already pruned
      and
    ref $attrs->{from}
      and
    reftype $attrs->{from} eq 'ARRAY'
      and
    @{$attrs->{from}} != 1
  ) {
    ($attrs->{from}, $attrs->{_aliastypes}) = $self->_prune_unused_joins ($attrs);
  }

  # FIXME this is a gross, inefficient, largely incorrect and fragile hack
  # during the result inflation stage we *need* to know what was the aliastype
  # map as sqla saw it when the final pieces of SQL were being assembled
  # Originally we simply carried around the entirety of $attrs, but this
  # resulted in resultsets that are being reused growing continuously, as
  # the hash in question grew deeper and deeper.
  # Instead hand-pick what to take with us here (we actually don't need much
  # at this point just the map itself)
  $orig_attrs->{_last_sqlmaker_alias_map} = $attrs->{_aliastypes};

###
  #   my $alias2source = $self->_resolve_ident_sources ($ident);
  #
  # This would be the point to deflate anything found in $attrs->{where}
  # (and leave $attrs->{bind} intact). Problem is - inflators historically
  # expect a result object. And all we have is a resultsource (it is trivial
  # to extract deflator coderefs via $alias2source above).
  #
  # I don't see a way forward other than changing the way deflators are
  # invoked, and that's just bad...
###

  return ( 'select', @{$attrs}{qw(from select where)}, $attrs, @limit_args );
}

# Returns a counting SELECT for a simple count
# query. Abstracted so that a storage could override
# this to { count => 'firstcol' } or whatever makes
# sense as a performance optimization
sub _count_select {
  #my ($self, $source, $rs_attrs) = @_;
  return { count => '*' };
}

=head2 select

=over 4

=item Arguments: $ident, $select, $condition, $attrs

=back

Handle a SQL select statement.

=cut

sub select {
  my $self = shift;
  my ($ident, $select, $condition, $attrs) = @_;
  return $self->cursor_class->new($self, \@_, $attrs);
}

sub select_single {
  my $self = shift;
  my ($rv, $sth, @bind) = $self->_select(@_);
  my @row = $sth->fetchrow_array;
  my @nextrow = $sth->fetchrow_array if @row;
  if(@row && @nextrow) {
    carp "Query returned more than one row.  SQL that returns multiple rows is DEPRECATED for ->find and ->single";
  }
  # Need to call finish() to work round broken DBDs
  $sth->finish();
  return @row;
}

=head2 sql_limit_dialect

This is an accessor for the default SQL limit dialect used by a particular
storage driver. Can be overridden by supplying an explicit L</limit_dialect>
to L<DBIx::Class::Schema/connect>. For a list of available limit dialects
see L<DBIx::Class::SQLMaker::LimitDialects>.

=cut

sub _dbh_columns_info_for {
  my ($self, $dbh, $table) = @_;

  if ($dbh->can('column_info')) {
    my %result;
    my $caught;
    try {
      my ($schema,$tab) = $table =~ /^(.+?)\.(.+)$/ ? ($1,$2) : (undef,$table);
      my $sth = $dbh->column_info( undef,$schema, $tab, '%' );
      $sth->execute();
      while ( my $info = $sth->fetchrow_hashref() ){
        my %column_info;
        $column_info{data_type}   = $info->{TYPE_NAME};
        $column_info{size}      = $info->{COLUMN_SIZE};
        $column_info{is_nullable}   = $info->{NULLABLE} ? 1 : 0;
        $column_info{default_value} = $info->{COLUMN_DEF};
        my $col_name = $info->{COLUMN_NAME};
        $col_name =~ s/^\"(.*)\"$/$1/;

        $result{$col_name} = \%column_info;
      }
    } catch {
      $caught = 1;
    };
    return \%result if !$caught && scalar keys %result;
  }

  my %result;
  my $sth = $dbh->prepare($self->sql_maker->select($table, undef, \'1 = 0'));
  $sth->execute;
  my @columns = @{$sth->{NAME_lc}};
  for my $i ( 0 .. $#columns ){
    my %column_info;
    $column_info{data_type} = $sth->{TYPE}->[$i];
    $column_info{size} = $sth->{PRECISION}->[$i];
    $column_info{is_nullable} = $sth->{NULLABLE}->[$i] ? 1 : 0;

    if ($column_info{data_type} =~ m/^(.*?)\((.*?)\)$/) {
      $column_info{data_type} = $1;
      $column_info{size}    = $2;
    }

    $result{$columns[$i]} = \%column_info;
  }
  $sth->finish;

  foreach my $col (keys %result) {
    my $colinfo = $result{$col};
    my $type_num = $colinfo->{data_type};
    my $type_name;
    if(defined $type_num && $dbh->can('type_info')) {
      my $type_info = $dbh->type_info($type_num);
      $type_name = $type_info->{TYPE_NAME} if $type_info;
      $colinfo->{data_type} = $type_name if $type_name;
    }
  }

  return \%result;
}

sub columns_info_for {
  my ($self, $table) = @_;
  $self->_dbh_columns_info_for ($self->_get_dbh, $table);
}

=head2 last_insert_id

Return the row id of the last insert.

=cut

sub _dbh_last_insert_id {
    my ($self, $dbh, $source, $col) = @_;

    my $id = try { $dbh->last_insert_id (undef, undef, $source->name, $col) };

    return $id if defined $id;

    my $class = ref $self;
    $self->throw_exception ("No storage specific _dbh_last_insert_id() method implemented in $class, and the generic DBI::last_insert_id() failed");
}

sub last_insert_id {
  my $self = shift;
  $self->_dbh_last_insert_id ($self->_dbh, @_);
}

=head2 _native_data_type

=over 4

=item Arguments: $type_name

=back

This API is B<EXPERIMENTAL>, will almost definitely change in the future, and
currently only used by L<::AutoCast|DBIx::Class::Storage::DBI::AutoCast> and
L<::Sybase::ASE|DBIx::Class::Storage::DBI::Sybase::ASE>.

The default implementation returns C<undef>, implement in your Storage driver if
you need this functionality.

Should map types from other databases to the native RDBMS type, for example
C<VARCHAR2> to C<VARCHAR>.

Types with modifiers should map to the underlying data type. For example,
C<INTEGER AUTO_INCREMENT> should become C<INTEGER>.

Composite types should map to the container type, for example
C<ENUM(foo,bar,baz)> becomes C<ENUM>.

=cut

sub _native_data_type {
  #my ($self, $data_type) = @_;
  return undef
}

# Check if placeholders are supported at all
sub _determine_supports_placeholders {
  my $self = shift;
  my $dbh  = $self->_get_dbh;

  # some drivers provide a $dbh attribute (e.g. Sybase and $dbh->{syb_dynamic_supported})
  # but it is inaccurate more often than not
  return try {
    local $dbh->{PrintError} = 0;
    local $dbh->{RaiseError} = 1;
    $dbh->do('select ?', {}, 1);
    1;
  }
  catch {
    0;
  };
}

# Check if placeholders bound to non-string types throw exceptions
#
sub _determine_supports_typeless_placeholders {
  my $self = shift;
  my $dbh  = $self->_get_dbh;

  return try {
    local $dbh->{PrintError} = 0;
    local $dbh->{RaiseError} = 1;
    # this specifically tests a bind that is NOT a string
    $dbh->do('select 1 where 1 = ?', {}, 1);
    1;
  }
  catch {
    0;
  };
}

=head2 sqlt_type

Returns the database driver name.

=cut

sub sqlt_type {
  shift->_get_dbh->{Driver}->{Name};
}

=head2 bind_attribute_by_data_type

Given a datatype from column info, returns a database specific bind
attribute for C<< $dbh->bind_param($val,$attribute) >> or nothing if we will
let the database planner just handle it.

This method is always called after the driver has been determined and a DBI
connection has been established. Therefore you can refer to C<DBI::$constant>
and/or C<DBD::$driver::$constant> directly, without worrying about loading
the correct modules.

=cut

sub bind_attribute_by_data_type {
    return;
}

=head2 is_datatype_numeric

Given a datatype from column_info, returns a boolean value indicating if
the current RDBMS considers it a numeric value. This controls how
L<DBIx::Class::Row/set_column> decides whether to mark the column as
dirty - when the datatype is deemed numeric a C<< != >> comparison will
be performed instead of the usual C<eq>.

=cut

sub is_datatype_numeric {
  #my ($self, $dt) = @_;

  return 0 unless $_[1];

  $_[1] =~ /^ (?:
    numeric | int(?:eger)? | (?:tiny|small|medium|big)int | dec(?:imal)? | real | float | double (?: \s+ precision)? | (?:big)?serial
  ) $/ix;
}


=head2 create_ddl_dir

=over 4

=item Arguments: $schema, \@databases, $version, $directory, $preversion, \%sqlt_args

=back

Creates a SQL file based on the Schema, for each of the specified
database engines in C<\@databases> in the given directory.
(note: specify L<SQL::Translator> names, not L<DBI> driver names).

Given a previous version number, this will also create a file containing
the ALTER TABLE statements to transform the previous schema into the
current one. Note that these statements may contain C<DROP TABLE> or
C<DROP COLUMN> statements that can potentially destroy data.

The file names are created using the C<ddl_filename> method below, please
override this method in your schema if you would like a different file
name format. For the ALTER file, the same format is used, replacing
$version in the name with "$preversion-$version".

See L<SQL::Translator/METHODS> for a list of values for C<\%sqlt_args>.
The most common value for this would be C<< { add_drop_table => 1 } >>
to have the SQL produced include a C<DROP TABLE> statement for each table
created. For quoting purposes supply C<quote_identifiers>.

If no arguments are passed, then the following default values are assumed:

=over 4

=item databases  - ['MySQL', 'SQLite', 'PostgreSQL']

=item version    - $schema->schema_version

=item directory  - './'

=item preversion - <none>

=back

By default, C<\%sqlt_args> will have

 { add_drop_table => 1, ignore_constraint_names => 1, ignore_index_names => 1 }

merged with the hash passed in. To disable any of those features, pass in a
hashref like the following

 { ignore_constraint_names => 0, # ... other options }


WARNING: You are strongly advised to check all SQL files created, before applying
them.

=cut

sub create_ddl_dir {
  my ($self, $schema, $databases, $version, $dir, $preversion, $sqltargs) = @_;

  unless ($dir) {
    carp "No directory given, using ./\n";
    $dir = './';
  } else {
      -d $dir
        or
      (require File::Path and File::Path::mkpath (["$dir"]))  # mkpath does not like objects (i.e. Path::Class::Dir)
        or
      $self->throw_exception(
        "Failed to create '$dir': " . ($! || $@ || 'error unknown')
      );
  }

  $self->throw_exception ("Directory '$dir' does not exist\n") unless(-d $dir);

  $databases ||= ['MySQL', 'SQLite', 'PostgreSQL'];
  $databases = [ $databases ] if(ref($databases) ne 'ARRAY');

  my $schema_version = $schema->schema_version || '1.x';
  $version ||= $schema_version;

  $sqltargs = {
    add_drop_table => 1,
    ignore_constraint_names => 1,
    ignore_index_names => 1,
    quote_identifiers => $self->sql_maker->_quoting_enabled,
    %{$sqltargs || {}}
  };

  unless (DBIx::Class::Optional::Dependencies->req_ok_for ('deploy')) {
    $self->throw_exception("Can't create a ddl file without " . DBIx::Class::Optional::Dependencies->req_missing_for ('deploy') );
  }

  my $sqlt = SQL::Translator->new( $sqltargs );

  $sqlt->parser('SQL::Translator::Parser::DBIx::Class');
  my $sqlt_schema = $sqlt->translate({ data => $schema })
    or $self->throw_exception ($sqlt->error);

  foreach my $db (@$databases) {
    $sqlt->reset();
    $sqlt->{schema} = $sqlt_schema;
    $sqlt->producer($db);

    my $file;
    my $filename = $schema->ddl_filename($db, $version, $dir);
    if (-e $filename && ($version eq $schema_version )) {
      # if we are dumping the current version, overwrite the DDL
      carp "Overwriting existing DDL file - $filename";
      unlink($filename);
    }

    my $output = $sqlt->translate;
    if(!$output) {
      carp("Failed to translate to $db, skipping. (" . $sqlt->error . ")");
      next;
    }
    if(!open($file, ">$filename")) {
      $self->throw_exception("Can't open $filename for writing ($!)");
      next;
    }
    print $file $output;
    close($file);

    next unless ($preversion);

    require SQL::Translator::Diff;

    my $prefilename = $schema->ddl_filename($db, $preversion, $dir);
    if(!-e $prefilename) {
      carp("No previous schema file found ($prefilename)");
      next;
    }

    my $difffile = $schema->ddl_filename($db, $version, $dir, $preversion);
    if(-e $difffile) {
      carp("Overwriting existing diff file - $difffile");
      unlink($difffile);
    }

    my $source_schema;
    {
      my $t = SQL::Translator->new($sqltargs);
      $t->debug( 0 );
      $t->trace( 0 );

      $t->parser( $db )
        or $self->throw_exception ($t->error);

      my $out = $t->translate( $prefilename )
        or $self->throw_exception ($t->error);

      $source_schema = $t->schema;

      $source_schema->name( $prefilename )
        unless ( $source_schema->name );
    }

    # The "new" style of producers have sane normalization and can support
    # diffing a SQL file against a DBIC->SQLT schema. Old style ones don't
    # And we have to diff parsed SQL against parsed SQL.
    my $dest_schema = $sqlt_schema;

    unless ( "SQL::Translator::Producer::$db"->can('preprocess_schema') ) {
      my $t = SQL::Translator->new($sqltargs);
      $t->debug( 0 );
      $t->trace( 0 );

      $t->parser( $db )
        or $self->throw_exception ($t->error);

      my $out = $t->translate( $filename )
        or $self->throw_exception ($t->error);

      $dest_schema = $t->schema;

      $dest_schema->name( $filename )
        unless $dest_schema->name;
    }

    my $diff = do {
      # FIXME - this is a terrible workaround for
      # https://github.com/dbsrgits/sql-translator/commit/2d23c1e
      # Fixing it in this sloppy manner so that we don't hve to
      # lockstep an SQLT release as well. Needs to be removed at
      # some point, and SQLT dep bumped
      local $SQL::Translator::Producer::SQLite::NO_QUOTES
        if $SQL::Translator::Producer::SQLite::NO_QUOTES;

      SQL::Translator::Diff::schema_diff($source_schema, $db,
                                         $dest_schema,   $db,
                                         $sqltargs
                                       );
    };

    if(!open $file, ">$difffile") {
      $self->throw_exception("Can't write to $difffile ($!)");
      next;
    }
    print $file $diff;
    close($file);
  }
}

=head2 deployment_statements

=over 4

=item Arguments: $schema, $type, $version, $directory, $sqlt_args

=back

Returns the statements used by L<DBIx::Class::Storage/deploy>
and L<DBIx::Class::Schema/deploy>.

The L<SQL::Translator> (not L<DBI>) database driver name can be explicitly
provided in C<$type>, otherwise the result of L</sqlt_type> is used as default.

C<$directory> is used to return statements from files in a previously created
L</create_ddl_dir> directory and is optional. The filenames are constructed
from L<DBIx::Class::Schema/ddl_filename>, the schema name and the C<$version>.

If no C<$directory> is specified then the statements are constructed on the
fly using L<SQL::Translator> and C<$version> is ignored.

See L<SQL::Translator/METHODS> for a list of values for C<$sqlt_args>.

=cut

sub deployment_statements {
  my ($self, $schema, $type, $version, $dir, $sqltargs) = @_;
  $type ||= $self->sqlt_type;
  $version ||= $schema->schema_version || '1.x';
  $dir ||= './';
  my $filename = $schema->ddl_filename($type, $version, $dir);
  if(-f $filename)
  {
      # FIXME replace this block when a proper sane sql parser is available
      my $file;
      open($file, "<$filename")
        or $self->throw_exception("Can't open $filename ($!)");
      my @rows = <$file>;
      close($file);
      return join('', @rows);
  }

  unless (DBIx::Class::Optional::Dependencies->req_ok_for ('deploy') ) {
    $self->throw_exception("Can't deploy without a ddl_dir or " . DBIx::Class::Optional::Dependencies->req_missing_for ('deploy') );
  }

  # sources needs to be a parser arg, but for simplicity allow at top level
  # coming in
  $sqltargs->{parser_args}{sources} = delete $sqltargs->{sources}
      if exists $sqltargs->{sources};

  $sqltargs->{quote_identifiers} = $self->sql_maker->_quoting_enabled
    unless exists $sqltargs->{quote_identifiers};

  my $tr = SQL::Translator->new(
    producer => "SQL::Translator::Producer::${type}",
    %$sqltargs,
    parser => 'SQL::Translator::Parser::DBIx::Class',
    data => $schema,
  );

  return preserve_context {
    $tr->translate
  } after => sub {
    $self->throw_exception( 'Unable to produce deployment statements: ' . $tr->error)
      unless defined $_[0];
  };
}

# FIXME deploy() currently does not accurately report sql errors
# Will always return true while errors are warned
sub deploy {
  my ($self, $schema, $type, $sqltargs, $dir) = @_;
  my $deploy = sub {
    my $line = shift;
    return if(!$line);
    return if($line =~ /^--/);
    # next if($line =~ /^DROP/m);
    return if($line =~ /^BEGIN TRANSACTION/m);
    return if($line =~ /^COMMIT/m);
    return if $line =~ /^\s+$/; # skip whitespace only
    $self->_query_start($line);
    try {
      # do a dbh_do cycle here, as we need some error checking in
      # place (even though we will ignore errors)
      $self->dbh_do (sub { $_[1]->do($line) });
    } catch {
      carp qq{$_ (running "${line}")};
    };
    $self->_query_end($line);
  };
  my @statements = $schema->deployment_statements($type, undef, $dir, { %{ $sqltargs || {} }, no_comments => 1 } );
  if (@statements > 1) {
    foreach my $statement (@statements) {
      $deploy->( $statement );
    }
  }
  elsif (@statements == 1) {
    # split on single line comments and end of statements
    foreach my $line ( split(/\s*--.*\n|;\n/, $statements[0])) {
      $deploy->( $line );
    }
  }
}

=head2 datetime_parser

Returns the datetime parser class

=cut

sub datetime_parser {
  my $self = shift;
  return $self->{datetime_parser} ||= do {
    $self->build_datetime_parser(@_);
  };
}

=head2 datetime_parser_type

Defines the datetime parser class - currently defaults to L<DateTime::Format::MySQL>

=head2 build_datetime_parser

See L</datetime_parser>

=cut

sub build_datetime_parser {
  my $self = shift;
  my $type = $self->datetime_parser_type(@_);
  return $type;
}


=head2 is_replicating

A boolean that reports if a particular L<DBIx::Class::Storage::DBI> is set to
replicate from a master database.  Default is undef, which is the result
returned by databases that don't support replication.

=cut

sub is_replicating {
    return;

}

=head2 lag_behind_master

Returns a number that represents a certain amount of lag behind a master db
when a given storage is replicating.  The number is database dependent, but
starts at zero and increases with the amount of lag. Default in undef

=cut

sub lag_behind_master {
    return;
}

=head2 relname_to_table_alias

=over 4

=item Arguments: $relname, $join_count

=item Return Value: $alias

=back

L<DBIx::Class> uses L<DBIx::Class::Relationship> names as table aliases in
queries.

This hook is to allow specific L<DBIx::Class::Storage> drivers to change the
way these aliases are named.

The default behavior is C<< "$relname_$join_count" if $join_count > 1 >>,
otherwise C<"$relname">.

=cut

sub relname_to_table_alias {
  my ($self, $relname, $join_count) = @_;

  my $alias = ($join_count && $join_count > 1 ?
    join('_', $relname, $join_count) : $relname);

  return $alias;
}

# The size in bytes to use for DBI's ->bind_param_inout, this is the generic
# version and it may be necessary to amend or override it for a specific storage
# if such binds are necessary.
sub _max_column_bytesize {
  my ($self, $attr) = @_;

  my $max_size;

  if ($attr->{sqlt_datatype}) {
    my $data_type = lc($attr->{sqlt_datatype});

    if ($attr->{sqlt_size}) {

      # String/sized-binary types
      if ($data_type =~ /^(?:
          l? (?:var)? char(?:acter)? (?:\s*varying)?
            |
          (?:var)? binary (?:\s*varying)?
            |
          raw
        )\b/x
      ) {
        $max_size = $attr->{sqlt_size};
      }
      # Other charset/unicode types, assume scale of 4
      elsif ($data_type =~ /^(?:
          national \s* character (?:\s*varying)?
            |
          nchar
            |
          univarchar
            |
          nvarchar
        )\b/x
      ) {
        $max_size = $attr->{sqlt_size} * 4;
      }
    }

    if (!$max_size and !$self->_is_lob_type($data_type)) {
      $max_size = 100 # for all other (numeric?) datatypes
    }
  }

  $max_size || $self->_dbic_connect_attributes->{LongReadLen} || $self->_get_dbh->{LongReadLen} || 8000;
}

# Determine if a data_type is some type of BLOB
sub _is_lob_type {
  my ($self, $data_type) = @_;
  $data_type && ($data_type =~ /lob|bfile|text|image|bytea|memo/i
    || $data_type =~ /^long(?:\s+(?:raw|bit\s*varying|varbit|binary
                                  |varchar|character\s*varying|nvarchar
                                  |national\s*character\s*varying))?\z/xi);
}

sub _is_binary_lob_type {
  my ($self, $data_type) = @_;
  $data_type && ($data_type =~ /blob|bfile|image|bytea/i
    || $data_type =~ /^long(?:\s+(?:raw|bit\s*varying|varbit|binary))?\z/xi);
}

sub _is_text_lob_type {
  my ($self, $data_type) = @_;
  $data_type && ($data_type =~ /^(?:clob|memo)\z/i
    || $data_type =~ /^long(?:\s+(?:varchar|character\s*varying|nvarchar
                        |national\s*character\s*varying))\z/xi);
}

# Determine if a data_type is some type of a binary type
sub _is_binary_type {
  my ($self, $data_type) = @_;
  $data_type && ($self->_is_binary_lob_type($data_type)
    || $data_type =~ /(?:var)?(?:binary|bit|graphic)(?:\s*varying)?/i);
}

1;

=head1 USAGE NOTES

=head2 DBIx::Class and AutoCommit

DBIx::Class can do some wonderful magic with handling exceptions,
disconnections, and transactions when you use C<< AutoCommit => 1 >>
(the default) combined with L<txn_do|DBIx::Class::Storage/txn_do> for
transaction support.

If you set C<< AutoCommit => 0 >> in your connect info, then you are always
in an assumed transaction between commits, and you're telling us you'd
like to manage that manually.  A lot of the magic protections offered by
this module will go away.  We can't protect you from exceptions due to database
disconnects because we don't know anything about how to restart your
transactions.  You're on your own for handling all sorts of exceptional
cases if you choose the C<< AutoCommit => 0 >> path, just as you would
be with raw DBI.

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.
