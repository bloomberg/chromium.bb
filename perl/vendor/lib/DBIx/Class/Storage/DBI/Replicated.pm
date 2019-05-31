package DBIx::Class::Storage::DBI::Replicated;

BEGIN {
  use DBIx::Class;
  die('The following modules are required for Replication ' . DBIx::Class::Optional::Dependencies->req_missing_for ('replicated') . "\n" )
    unless DBIx::Class::Optional::Dependencies->req_ok_for ('replicated');
}

use Moose;
use DBIx::Class::Storage::DBI;
use DBIx::Class::Storage::DBI::Replicated::Pool;
use DBIx::Class::Storage::DBI::Replicated::Balancer;
use DBIx::Class::Storage::DBI::Replicated::Types qw/BalancerClassNamePart DBICSchema DBICStorageDBI/;
use MooseX::Types::Moose qw/ClassName HashRef Object/;
use Scalar::Util 'reftype';
use Hash::Merge;
use List::Util qw/min max reduce/;
use Context::Preserve 'preserve_context';
use Try::Tiny;

use namespace::clean -except => 'meta';

=head1 NAME

DBIx::Class::Storage::DBI::Replicated - BETA Replicated database support

=head1 SYNOPSIS

The Following example shows how to change an existing $schema to a replicated
storage type, add some replicated (read-only) databases, and perform reporting
tasks.

You should set the 'storage_type attribute to a replicated type.  You should
also define your arguments, such as which balancer you want and any arguments
that the Pool object should get.

  my $schema = Schema::Class->clone;
  $schema->storage_type(['::DBI::Replicated', { balancer_type => '::Random' }]);
  $schema->connection(...);

Next, you need to add in the Replicants.  Basically this is an array of
arrayrefs, where each arrayref is database connect information.  Think of these
arguments as what you'd pass to the 'normal' $schema->connect method.

  $schema->storage->connect_replicants(
    [$dsn1, $user, $pass, \%opts],
    [$dsn2, $user, $pass, \%opts],
    [$dsn3, $user, $pass, \%opts],
  );

Now, just use the $schema as you normally would.  Automatically all reads will
be delegated to the replicants, while writes to the master.

  $schema->resultset('Source')->search({name=>'etc'});

You can force a given query to use a particular storage using the search
attribute 'force_pool'.  For example:

  my $rs = $schema->resultset('Source')->search(undef, {force_pool=>'master'});

Now $rs will force everything (both reads and writes) to use whatever was setup
as the master storage.  'master' is hardcoded to always point to the Master,
but you can also use any Replicant name.  Please see:
L<DBIx::Class::Storage::DBI::Replicated::Pool> and the replicants attribute for more.

Also see transactions and L</execute_reliably> for alternative ways to
force read traffic to the master.  In general, you should wrap your statements
in a transaction when you are reading and writing to the same tables at the
same time, since your replicants will often lag a bit behind the master.

If you have a multi-statement read only transaction you can force it to select
a random server in the pool by:

  my $rs = $schema->resultset('Source')->search( undef,
    { force_pool => $db->storage->read_handler->next_storage }
  );

=head1 DESCRIPTION

Warning: This class is marked BETA.  This has been running a production
website using MySQL native replication as its backend and we have some decent
test coverage but the code hasn't yet been stressed by a variety of databases.
Individual DBs may have quirks we are not aware of.  Please use this in first
development and pass along your experiences/bug fixes.

This class implements replicated data store for DBI. Currently you can define
one master and numerous slave database connections. All write-type queries
(INSERT, UPDATE, DELETE and even LAST_INSERT_ID) are routed to master
database, all read-type queries (SELECTs) go to the slave database.

Basically, any method request that L<DBIx::Class::Storage::DBI> would normally
handle gets delegated to one of the two attributes: L</read_handler> or to
L</write_handler>.  Additionally, some methods need to be distributed
to all existing storages.  This way our storage class is a drop in replacement
for L<DBIx::Class::Storage::DBI>.

Read traffic is spread across the replicants (slaves) occurring to a user
selected algorithm.  The default algorithm is random weighted.

=head1 NOTES

The consistency between master and replicants is database specific.  The Pool
gives you a method to validate its replicants, removing and replacing them
when they fail/pass predefined criteria.  Please make careful use of the ways
to force a query to run against Master when needed.

=head1 REQUIREMENTS

Replicated Storage has additional requirements not currently part of
L<DBIx::Class>. See L<DBIx::Class::Optional::Dependencies> for more details.

=head1 ATTRIBUTES

This class defines the following attributes.

=head2 schema

The underlying L<DBIx::Class::Schema> object this storage is attaching

=cut

has 'schema' => (
    is=>'rw',
    isa=>DBICSchema,
    weak_ref=>1,
    required=>1,
);

=head2 pool_type

Contains the classname which will instantiate the L</pool> object.  Defaults
to: L<DBIx::Class::Storage::DBI::Replicated::Pool>.

=cut

has 'pool_type' => (
  is=>'rw',
  isa=>ClassName,
  default=>'DBIx::Class::Storage::DBI::Replicated::Pool',
  handles=>{
    'create_pool' => 'new',
  },
);

=head2 pool_args

Contains a hashref of initialized information to pass to the Balancer object.
See L<DBIx::Class::Storage::DBI::Replicated::Pool> for available arguments.

=cut

has 'pool_args' => (
  is=>'rw',
  isa=>HashRef,
  lazy=>1,
  default=>sub { {} },
);


=head2 balancer_type

The replication pool requires a balance class to provider the methods for
choose how to spread the query load across each replicant in the pool.

=cut

has 'balancer_type' => (
  is=>'rw',
  isa=>BalancerClassNamePart,
  coerce=>1,
  required=>1,
  default=> 'DBIx::Class::Storage::DBI::Replicated::Balancer::First',
  handles=>{
    'create_balancer' => 'new',
  },
);

=head2 balancer_args

Contains a hashref of initialized information to pass to the Balancer object.
See L<DBIx::Class::Storage::DBI::Replicated::Balancer> for available arguments.

=cut

has 'balancer_args' => (
  is=>'rw',
  isa=>HashRef,
  lazy=>1,
  required=>1,
  default=>sub { {} },
);

=head2 pool

Is a L<DBIx::Class::Storage::DBI::Replicated::Pool> or derived class.  This is a
container class for one or more replicated databases.

=cut

has 'pool' => (
  is=>'ro',
  isa=>'DBIx::Class::Storage::DBI::Replicated::Pool',
  lazy_build=>1,
  handles=>[qw/
    connect_replicants
    replicants
    has_replicants
  /],
);

=head2 balancer

Is a L<DBIx::Class::Storage::DBI::Replicated::Balancer> or derived class.  This
is a class that takes a pool (L<DBIx::Class::Storage::DBI::Replicated::Pool>)

=cut

has 'balancer' => (
  is=>'rw',
  isa=>'DBIx::Class::Storage::DBI::Replicated::Balancer',
  lazy_build=>1,
  handles=>[qw/auto_validate_every/],
);

=head2 master

The master defines the canonical state for a pool of connected databases.  All
the replicants are expected to match this databases state.  Thus, in a classic
Master / Slaves distributed system, all the slaves are expected to replicate
the Master's state as quick as possible.  This is the only database in the
pool of databases that is allowed to handle write traffic.

=cut

has 'master' => (
  is=> 'ro',
  isa=>DBICStorageDBI,
  lazy_build=>1,
);

=head1 ATTRIBUTES IMPLEMENTING THE DBIx::Storage::DBI INTERFACE

The following methods are delegated all the methods required for the
L<DBIx::Class::Storage::DBI> interface.

=cut

my $method_dispatch = {
  writer => [qw/
    on_connect_do
    on_disconnect_do
    on_connect_call
    on_disconnect_call
    connect_info
    _connect_info
    throw_exception
    sql_maker
    sqlt_type
    create_ddl_dir
    deployment_statements
    datetime_parser
    datetime_parser_type
    build_datetime_parser
    last_insert_id
    insert
    update
    delete
    dbh
    txn_begin
    txn_do
    txn_commit
    txn_rollback
    txn_scope_guard
    _exec_txn_rollback
    _exec_txn_begin
    _exec_txn_commit
    deploy
    with_deferred_fk_checks
    dbh_do
    _prep_for_execute
    is_datatype_numeric
    _count_select
    svp_rollback
    svp_begin
    svp_release
    relname_to_table_alias
    _dbh_last_insert_id
    _default_dbi_connect_attributes
    _dbi_connect_info
    _dbic_connect_attributes
    auto_savepoint
    _query_start
    _query_end
    _format_for_trace
    _dbi_attrs_for_bind
    bind_attribute_by_data_type
    transaction_depth
    _dbh
    _select_args
    _dbh_execute_for_fetch
    _sql_maker
    _dbh_execute_inserts_with_no_binds
    _select_args_to_query
    _gen_sql_bind
    _svp_generate_name
    _normalize_connect_info
    _parse_connect_do
    savepoints
    _sql_maker_opts
    _use_multicolumn_in
    _conn_pid
    _dbh_autocommit
    _native_data_type
    _get_dbh
    sql_maker_class
    insert_bulk
    _insert_bulk
    _execute
    _do_query
    _dbh_execute
  /, Class::MOP::Class->initialize('DBIx::Class::Storage::DBIHacks')->get_method_list ],
  reader => [qw/
    select
    select_single
    columns_info_for
    _dbh_columns_info_for
    _select
  /],
  unimplemented => [qw/
    _arm_global_destructor
    _verify_pid

    get_use_dbms_capability
    set_use_dbms_capability
    get_dbms_capability
    set_dbms_capability
    _dbh_details
    _dbh_get_info

    _determine_connector_driver
    _extract_driver_from_connect_info
    _describe_connection
    _warn_undetermined_driver

    sql_limit_dialect
    sql_quote_char
    sql_name_sep

    _prefetch_autovalues
    _perform_autoinc_retrieval
    _autoinc_supplied_for_op

    _resolve_bindattrs

    _max_column_bytesize
    _is_lob_type
    _is_binary_lob_type
    _is_binary_type
    _is_text_lob_type

    _prepare_sth
    _bind_sth_params
  /,(
    # the capability framework
    # not sure if CMOP->initialize does evil things to DBIC::S::DBI, fix if a problem
    grep
      { $_ =~ /^ _ (?: use | supports | determine_supports ) _ /x and $_ ne '_use_multicolumn_in' }
      ( Class::MOP::Class->initialize('DBIx::Class::Storage::DBI')->get_all_method_names )
  )],
};

if (DBIx::Class::_ENV_::DBICTEST) {

  my $seen;
  for my $type (keys %$method_dispatch) {
    for (@{$method_dispatch->{$type}}) {
      push @{$seen->{$_}}, $type;
    }
  }

  if (my @dupes = grep { @{$seen->{$_}} > 1 } keys %$seen) {
    die(join "\n", '',
      'The following methods show up multiple times in ::Storage::DBI::Replicated handlers:',
      (map { "$_: " . (join ', ', @{$seen->{$_}}) } sort @dupes),
      '',
    );
  }

  if (my @cant = grep { ! DBIx::Class::Storage::DBI->can($_) } keys %$seen) {
    die(join "\n", '',
      '::Storage::DBI::Replicated specifies handling of the following *NON EXISTING* ::Storage::DBI methods:',
      @cant,
      '',
    );
  }
}

for my $method (@{$method_dispatch->{unimplemented}}) {
  __PACKAGE__->meta->add_method($method, sub {
    my $self = shift;
    $self->throw_exception("$method() must not be called on ".(blessed $self).' objects');
  });
}

=head2 read_handler

Defines an object that implements the read side of L<DBIx::Class::Storage::DBI>.

=cut

has 'read_handler' => (
  is=>'rw',
  isa=>Object,
  lazy_build=>1,
  handles=>$method_dispatch->{reader},
);

=head2 write_handler

Defines an object that implements the write side of L<DBIx::Class::Storage::DBI>,
as well as methods that don't write or read that can be called on only one
storage, methods that return a C<$dbh>, and any methods that don't make sense to
run on a replicant.

=cut

has 'write_handler' => (
  is=>'ro',
  isa=>Object,
  lazy_build=>1,
  handles=>$method_dispatch->{writer},
);



has _master_connect_info_opts =>
  (is => 'rw', isa => HashRef, default => sub { {} });

=head2 around: connect_info

Preserves master's C<connect_info> options (for merging with replicants.)
Also sets any Replicated-related options from connect_info, such as
C<pool_type>, C<pool_args>, C<balancer_type> and C<balancer_args>.

=cut

around connect_info => sub {
  my ($next, $self, $info, @extra) = @_;

  $self->throw_exception(
    'connect_info can not be retrieved from a replicated storage - '
  . 'accessor must be called on a specific pool instance'
  ) unless defined $info;

  my $merge = Hash::Merge->new('LEFT_PRECEDENT');

  my %opts;
  for my $arg (@$info) {
    next unless (reftype($arg)||'') eq 'HASH';
    %opts = %{ $merge->merge($arg, \%opts) };
  }
  delete $opts{dsn};

  if (@opts{qw/pool_type pool_args/}) {
    $self->pool_type(delete $opts{pool_type})
      if $opts{pool_type};

    $self->pool_args(
      $merge->merge((delete $opts{pool_args} || {}), $self->pool_args)
    );

    ## Since we possibly changed the pool_args, we need to clear the current
    ## pool object so that next time it is used it will be rebuilt.
    $self->clear_pool;
  }

  if (@opts{qw/balancer_type balancer_args/}) {
    $self->balancer_type(delete $opts{balancer_type})
      if $opts{balancer_type};

    $self->balancer_args(
      $merge->merge((delete $opts{balancer_args} || {}), $self->balancer_args)
    );

    $self->balancer($self->_build_balancer)
      if $self->balancer;
  }

  $self->_master_connect_info_opts(\%opts);

  return preserve_context {
    $self->$next($info, @extra);
  } after => sub {
    # Make sure master is blessed into the correct class and apply role to it.
    my $master = $self->master;
    $master->_determine_driver;
    Moose::Meta::Class->initialize(ref $master);

    DBIx::Class::Storage::DBI::Replicated::WithDSN->meta->apply($master);

    # link pool back to master
    $self->pool->master($master);
  };
};

=head1 METHODS

This class defines the following methods.

=head2 BUILDARGS

L<DBIx::Class::Schema> when instantiating its storage passed itself as the
first argument.  So we need to massage the arguments a bit so that all the
bits get put into the correct places.

=cut

sub BUILDARGS {
  my ($class, $schema, $storage_type_args, @args) = @_;

  return {
    schema=>$schema,
    %$storage_type_args,
    @args
  }
}

=head2 _build_master

Lazy builder for the L</master> attribute.

=cut

sub _build_master {
  my $self = shift @_;
  my $master = DBIx::Class::Storage::DBI->new($self->schema);
  $master
}

=head2 _build_pool

Lazy builder for the L</pool> attribute.

=cut

sub _build_pool {
  my $self = shift @_;
  $self->create_pool(%{$self->pool_args});
}

=head2 _build_balancer

Lazy builder for the L</balancer> attribute.  This takes a Pool object so that
the balancer knows which pool it's balancing.

=cut

sub _build_balancer {
  my $self = shift @_;
  $self->create_balancer(
    pool=>$self->pool,
    master=>$self->master,
    %{$self->balancer_args},
  );
}

=head2 _build_write_handler

Lazy builder for the L</write_handler> attribute.  The default is to set this to
the L</master>.

=cut

sub _build_write_handler {
  return shift->master;
}

=head2 _build_read_handler

Lazy builder for the L</read_handler> attribute.  The default is to set this to
the L</balancer>.

=cut

sub _build_read_handler {
  return shift->balancer;
}

=head2 around: connect_replicants

All calls to connect_replicants needs to have an existing $schema tacked onto
top of the args, since L<DBIx::Class::Storage::DBI> needs it, and any
L<connect_info|DBIx::Class::Storage::DBI/connect_info>
options merged with the master, with replicant opts having higher priority.

=cut

around connect_replicants => sub {
  my ($next, $self, @args) = @_;

  for my $r (@args) {
    $r = [ $r ] unless reftype $r eq 'ARRAY';

    $self->throw_exception('coderef replicant connect_info not supported')
      if ref $r->[0] && reftype $r->[0] eq 'CODE';

# any connect_info options?
    my $i = 0;
    $i++ while $i < @$r && (reftype($r->[$i])||'') ne 'HASH';

# make one if none
    $r->[$i] = {} unless $r->[$i];

# merge if two hashes
    my @hashes = @$r[$i .. $#{$r}];

    $self->throw_exception('invalid connect_info options')
      if (grep { reftype($_) eq 'HASH' } @hashes) != @hashes;

    $self->throw_exception('too many hashrefs in connect_info')
      if @hashes > 2;

    my $merge = Hash::Merge->new('LEFT_PRECEDENT');
    my %opts = %{ $merge->merge(reverse @hashes) };

# delete them
    splice @$r, $i+1, ($#{$r} - $i), ();

# make sure master/replicants opts don't clash
    my %master_opts = %{ $self->_master_connect_info_opts };
    if (exists $opts{dbh_maker}) {
        delete @master_opts{qw/dsn user password/};
    }
    delete $master_opts{dbh_maker};

# merge with master
    %opts = %{ $merge->merge(\%opts, \%master_opts) };

# update
    $r->[$i] = \%opts;
  }

  $self->$next($self->schema, @args);
};

=head2 all_storages

Returns an array of all the connected storage backends.  The first element
in the returned array is the master, and the rest are each of the
replicants.

=cut

sub all_storages {
  my $self = shift @_;
  return grep {defined $_ && blessed $_} (
     $self->master,
     values %{ $self->replicants },
  );
}

=head2 execute_reliably ($coderef, ?@args)

Given a coderef, saves the current state of the L</read_handler>, forces it to
use reliable storage (e.g. sets it to the master), executes a coderef and then
restores the original state.

Example:

  my $reliably = sub {
    my $name = shift @_;
    $schema->resultset('User')->create({name=>$name});
    my $user_rs = $schema->resultset('User')->find({name=>$name});
    return $user_rs;
  };

  my $user_rs = $schema->storage->execute_reliably($reliably, 'John');

Use this when you must be certain of your database state, such as when you just
inserted something and need to get a resultset including it, etc.

=cut

sub execute_reliably {
  my $self = shift;
  my $coderef = shift;

  $self->throw_exception('Second argument must be a coderef')
    unless( ref $coderef eq 'CODE');

  ## replace the current read handler for the remainder of the scope
  local $self->{read_handler} = $self->master;

  &$coderef;
}

=head2 set_reliable_storage

Sets the current $schema to be 'reliable', that is all queries, both read and
write are sent to the master

=cut

sub set_reliable_storage {
  my $self = shift @_;
  my $schema = $self->schema;
  my $write_handler = $self->schema->storage->write_handler;

  $schema->storage->read_handler($write_handler);
}

=head2 set_balanced_storage

Sets the current $schema to be use the </balancer> for all reads, while all
writes are sent to the master only

=cut

sub set_balanced_storage {
  my $self = shift @_;
  my $schema = $self->schema;
  my $balanced_handler = $self->schema->storage->balancer;

  $schema->storage->read_handler($balanced_handler);
}

=head2 connected

Check that the master and at least one of the replicants is connected.

=cut

sub connected {
  my $self = shift @_;
  return
    $self->master->connected &&
    $self->pool->connected_replicants;
}

=head2 ensure_connected

Make sure all the storages are connected.

=cut

sub ensure_connected {
  my $self = shift @_;
  foreach my $source ($self->all_storages) {
    $source->ensure_connected(@_);
  }
}

=head2 limit_dialect

Set the limit_dialect for all existing storages

=cut

sub limit_dialect {
  my $self = shift @_;
  foreach my $source ($self->all_storages) {
    $source->limit_dialect(@_);
  }
  return $self->master->limit_dialect;
}

=head2 quote_char

Set the quote_char for all existing storages

=cut

sub quote_char {
  my $self = shift @_;
  foreach my $source ($self->all_storages) {
    $source->quote_char(@_);
  }
  return $self->master->quote_char;
}

=head2 name_sep

Set the name_sep for all existing storages

=cut

sub name_sep {
  my $self = shift @_;
  foreach my $source ($self->all_storages) {
    $source->name_sep(@_);
  }
  return $self->master->name_sep;
}

=head2 set_schema

Set the schema object for all existing storages

=cut

sub set_schema {
  my $self = shift @_;
  foreach my $source ($self->all_storages) {
    $source->set_schema(@_);
  }
}

=head2 debug

set a debug flag across all storages

=cut

sub debug {
  my $self = shift @_;
  if(@_) {
    foreach my $source ($self->all_storages) {
      $source->debug(@_);
    }
  }
  return $self->master->debug;
}

=head2 debugobj

set a debug object

=cut

sub debugobj {
  my $self = shift @_;
  return $self->master->debugobj(@_);
}

=head2 debugfh

set a debugfh object

=cut

sub debugfh {
  my $self = shift @_;
  return $self->master->debugfh(@_);
}

=head2 debugcb

set a debug callback

=cut

sub debugcb {
  my $self = shift @_;
  return $self->master->debugcb(@_);
}

=head2 disconnect

disconnect everything

=cut

sub disconnect {
  my $self = shift @_;
  foreach my $source ($self->all_storages) {
    $source->disconnect(@_);
  }
}

=head2 cursor_class

set cursor class on all storages, or return master's

=cut

sub cursor_class {
  my ($self, $cursor_class) = @_;

  if ($cursor_class) {
    $_->cursor_class($cursor_class) for $self->all_storages;
  }
  $self->master->cursor_class;
}

=head2 cursor

set cursor class on all storages, or return master's, alias for L</cursor_class>
above.

=cut

sub cursor {
  my ($self, $cursor_class) = @_;

  if ($cursor_class) {
    $_->cursor($cursor_class) for $self->all_storages;
  }
  $self->master->cursor;
}

=head2 unsafe

sets the L<DBIx::Class::Storage::DBI/unsafe> option on all storages or returns
master's current setting

=cut

sub unsafe {
  my $self = shift;

  if (@_) {
    $_->unsafe(@_) for $self->all_storages;
  }

  return $self->master->unsafe;
}

=head2 disable_sth_caching

sets the L<DBIx::Class::Storage::DBI/disable_sth_caching> option on all storages
or returns master's current setting

=cut

sub disable_sth_caching {
  my $self = shift;

  if (@_) {
    $_->disable_sth_caching(@_) for $self->all_storages;
  }

  return $self->master->disable_sth_caching;
}

=head2 lag_behind_master

returns the highest Replicant L<DBIx::Class::Storage::DBI/lag_behind_master>
setting

=cut

sub lag_behind_master {
  my $self = shift;

  return max map $_->lag_behind_master, $self->replicants;
}

=head2 is_replicating

returns true if all replicants return true for
L<DBIx::Class::Storage::DBI/is_replicating>

=cut

sub is_replicating {
  my $self = shift;

  return (grep $_->is_replicating, $self->replicants) == ($self->replicants);
}

=head2 connect_call_datetime_setup

calls L<DBIx::Class::Storage::DBI/connect_call_datetime_setup> for all storages

=cut

sub connect_call_datetime_setup {
  my $self = shift;
  $_->connect_call_datetime_setup for $self->all_storages;
}

sub _populate_dbh {
  my $self = shift;
  $_->_populate_dbh for $self->all_storages;
}

sub _connect {
  my $self = shift;
  $_->_connect for $self->all_storages;
}

sub _rebless {
  my $self = shift;
  $_->_rebless for $self->all_storages;
}

sub _determine_driver {
  my $self = shift;
  $_->_determine_driver for $self->all_storages;
}

sub _driver_determined {
  my $self = shift;

  if (@_) {
    $_->_driver_determined(@_) for $self->all_storages;
  }

  return $self->master->_driver_determined;
}

sub _init {
  my $self = shift;

  $_->_init for $self->all_storages;
}

sub _run_connection_actions {
  my $self = shift;

  $_->_run_connection_actions for $self->all_storages;
}

sub _do_connection_actions {
  my $self = shift;

  if (@_) {
    $_->_do_connection_actions(@_) for $self->all_storages;
  }
}

sub connect_call_do_sql {
  my $self = shift;
  $_->connect_call_do_sql(@_) for $self->all_storages;
}

sub disconnect_call_do_sql {
  my $self = shift;
  $_->disconnect_call_do_sql(@_) for $self->all_storages;
}

sub _seems_connected {
  my $self = shift;

  return min map $_->_seems_connected, $self->all_storages;
}

sub _ping {
  my $self = shift;

  return min map $_->_ping, $self->all_storages;
}

# not using the normalized_version, because we want to preserve
# version numbers much longer than the conventional xxx.yyyzzz
my $numify_ver = sub {
  my $ver = shift;
  my @numparts = split /\D+/, $ver;
  my $format = '%d.' . (join '', ('%06d') x (@numparts - 1));

  return sprintf $format, @numparts;
};
sub _server_info {
  my $self = shift;

  if (not $self->_dbh_details->{info}) {
    $self->_dbh_details->{info} = (
      reduce { $a->[0] < $b->[0] ? $a : $b }
      map [ $numify_ver->($_->{dbms_version}), $_ ],
      map $_->_server_info, $self->all_storages
    )->[1];
  }

  return $self->next::method;
}

sub _get_server_version {
  my $self = shift;

  return $self->_server_info->{dbms_version};
}

=head1 GOTCHAS

Due to the fact that replicants can lag behind a master, you must take care to
make sure you use one of the methods to force read queries to a master should
you need realtime data integrity.  For example, if you insert a row, and then
immediately re-read it from the database (say, by doing
L<< $result->discard_changes|DBIx::Class::Row/discard_changes >>)
or you insert a row and then immediately build a query that expects that row
to be an item, you should force the master to handle reads.  Otherwise, due to
the lag, there is no certainty your data will be in the expected state.

For data integrity, all transactions automatically use the master storage for
all read and write queries.  Using a transaction is the preferred and recommended
method to force the master to handle all read queries.

Otherwise, you can force a single query to use the master with the 'force_pool'
attribute:

  my $result = $resultset->search(undef, {force_pool=>'master'})->find($pk);

This attribute will safely be ignored by non replicated storages, so you can use
the same code for both types of systems.

Lastly, you can use the L</execute_reliably> method, which works very much like
a transaction.

For debugging, you can turn replication on/off with the methods L</set_reliable_storage>
and L</set_balanced_storage>, however this operates at a global level and is not
suitable if you have a shared Schema object being used by multiple processes,
such as on a web application server.  You can get around this limitation by
using the Schema clone method.

  my $new_schema = $schema->clone;
  $new_schema->set_reliable_storage;

  ## $new_schema will use only the Master storage for all reads/writes while
  ## the $schema object will use replicated storage.

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.

=cut

__PACKAGE__->meta->make_immutable;

1;
