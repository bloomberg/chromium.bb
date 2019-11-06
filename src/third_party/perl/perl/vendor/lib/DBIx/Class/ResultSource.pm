package DBIx::Class::ResultSource;

use strict;
use warnings;

use base qw/DBIx::Class::ResultSource::RowParser DBIx::Class/;

use DBIx::Class::ResultSet;
use DBIx::Class::ResultSourceHandle;

use DBIx::Class::Carp;
use DBIx::Class::_Util 'UNRESOLVABLE_CONDITION';
use SQL::Abstract 'is_literal_value';
use Devel::GlobalDestruction;
use Try::Tiny;
use List::Util 'first';
use Scalar::Util qw/blessed weaken isweak/;

use namespace::clean;

__PACKAGE__->mk_group_accessors(simple => qw/
  source_name name source_info
  _ordered_columns _columns _primaries _unique_constraints
  _relationships resultset_attributes
  column_info_from_storage
/);

__PACKAGE__->mk_group_accessors(component_class => qw/
  resultset_class
  result_class
/);

__PACKAGE__->mk_classdata( sqlt_deploy_callback => 'default_sqlt_deploy_hook' );

=head1 NAME

DBIx::Class::ResultSource - Result source object

=head1 SYNOPSIS

  # Create a table based result source, in a result class.

  package MyApp::Schema::Result::Artist;
  use base qw/DBIx::Class::Core/;

  __PACKAGE__->table('artist');
  __PACKAGE__->add_columns(qw/ artistid name /);
  __PACKAGE__->set_primary_key('artistid');
  __PACKAGE__->has_many(cds => 'MyApp::Schema::Result::CD');

  1;

  # Create a query (view) based result source, in a result class
  package MyApp::Schema::Result::Year2000CDs;
  use base qw/DBIx::Class::Core/;

  __PACKAGE__->load_components('InflateColumn::DateTime');
  __PACKAGE__->table_class('DBIx::Class::ResultSource::View');

  __PACKAGE__->table('year2000cds');
  __PACKAGE__->result_source_instance->is_virtual(1);
  __PACKAGE__->result_source_instance->view_definition(
      "SELECT cdid, artist, title FROM cd WHERE year ='2000'"
      );


=head1 DESCRIPTION

A ResultSource is an object that represents a source of data for querying.

This class is a base class for various specialised types of result
sources, for example L<DBIx::Class::ResultSource::Table>. Table is the
default result source type, so one is created for you when defining a
result class as described in the synopsis above.

More specifically, the L<DBIx::Class::Core> base class pulls in the
L<DBIx::Class::ResultSourceProxy::Table> component, which defines
the L<table|DBIx::Class::ResultSourceProxy::Table/table> method.
When called, C<table> creates and stores an instance of
L<DBIx::Class::ResultSource::Table>. Luckily, to use tables as result
sources, you don't need to remember any of this.

Result sources representing select queries, or views, can also be
created, see L<DBIx::Class::ResultSource::View> for full details.

=head2 Finding result source objects

As mentioned above, a result source instance is created and stored for
you when you define a
L<Result Class|DBIx::Class::Manual::Glossary/Result Class>.

You can retrieve the result source at runtime in the following ways:

=over

=item From a Schema object:

   $schema->source($source_name);

=item From a Result object:

   $result->result_source;

=item From a ResultSet object:

   $rs->result_source;

=back

=head1 METHODS

=head2 new

  $class->new();

  $class->new({attribute_name => value});

Creates a new ResultSource object.  Not normally called directly by end users.

=cut

sub new {
  my ($class, $attrs) = @_;
  $class = ref $class if ref $class;

  my $new = bless { %{$attrs || {}} }, $class;
  $new->{resultset_class} ||= 'DBIx::Class::ResultSet';
  $new->{resultset_attributes} = { %{$new->{resultset_attributes} || {}} };
  $new->{_ordered_columns} = [ @{$new->{_ordered_columns}||[]}];
  $new->{_columns} = { %{$new->{_columns}||{}} };
  $new->{_relationships} = { %{$new->{_relationships}||{}} };
  $new->{name} ||= "!!NAME NOT SET!!";
  $new->{_columns_info_loaded} ||= 0;
  return $new;
}

=pod

=head2 add_columns

=over

=item Arguments: @columns

=item Return Value: L<$result_source|/new>

=back

  $source->add_columns(qw/col1 col2 col3/);

  $source->add_columns('col1' => \%col1_info, 'col2' => \%col2_info, ...);

  $source->add_columns(
    'col1' => { data_type => 'integer', is_nullable => 1, ... },
    'col2' => { data_type => 'text',    is_auto_increment => 1, ... },
  );

Adds columns to the result source. If supplied colname => hashref
pairs, uses the hashref as the L</column_info> for that column. Repeated
calls of this method will add more columns, not replace them.

The column names given will be created as accessor methods on your
L<Result|DBIx::Class::Manual::ResultClass> objects. You can change the name of the accessor
by supplying an L</accessor> in the column_info hash.

If a column name beginning with a plus sign ('+col1') is provided, the
attributes provided will be merged with any existing attributes for the
column, with the new attributes taking precedence in the case that an
attribute already exists. Using this without a hashref
(C<< $source->add_columns(qw/+col1 +col2/) >>) is legal, but useless --
it does the same thing it would do without the plus.

The contents of the column_info are not set in stone. The following
keys are currently recognised/used by DBIx::Class:

=over 4

=item accessor

   { accessor => '_name' }

   # example use, replace standard accessor with one of your own:
   sub name {
       my ($self, $value) = @_;

       die "Name cannot contain digits!" if($value =~ /\d/);
       $self->_name($value);

       return $self->_name();
   }

Use this to set the name of the accessor method for this column. If unset,
the name of the column will be used.

=item data_type

   { data_type => 'integer' }

This contains the column type. It is automatically filled if you use the
L<SQL::Translator::Producer::DBIx::Class::File> producer, or the
L<DBIx::Class::Schema::Loader> module.

Currently there is no standard set of values for the data_type. Use
whatever your database supports.

=item size

   { size => 20 }

The length of your column, if it is a column type that can have a size
restriction. This is currently only used to create tables from your
schema, see L<DBIx::Class::Schema/deploy>.

   { size => [ 9, 6 ] }

For decimal or float values you can specify an ArrayRef in order to
control precision, assuming your database's
L<SQL::Translator::Producer> supports it.

=item is_nullable

   { is_nullable => 1 }

Set this to a true value for a column that is allowed to contain NULL
values, default is false. This is currently only used to create tables
from your schema, see L<DBIx::Class::Schema/deploy>.

=item is_auto_increment

   { is_auto_increment => 1 }

Set this to a true value for a column whose value is somehow
automatically set, defaults to false. This is used to determine which
columns to empty when cloning objects using
L<DBIx::Class::Row/copy>. It is also used by
L<DBIx::Class::Schema/deploy>.

=item is_numeric

   { is_numeric => 1 }

Set this to a true or false value (not C<undef>) to explicitly specify
if this column contains numeric data. This controls how set_column
decides whether to consider a column dirty after an update: if
C<is_numeric> is true a numeric comparison C<< != >> will take place
instead of the usual C<eq>

If not specified the storage class will attempt to figure this out on
first access to the column, based on the column C<data_type>. The
result will be cached in this attribute.

=item is_foreign_key

   { is_foreign_key => 1 }

Set this to a true value for a column that contains a key from a
foreign table, defaults to false. This is currently only used to
create tables from your schema, see L<DBIx::Class::Schema/deploy>.

=item default_value

   { default_value => \'now()' }

Set this to the default value which will be inserted into a column by
the database. Can contain either a value or a function (use a
reference to a scalar e.g. C<\'now()'> if you want a function). This
is currently only used to create tables from your schema, see
L<DBIx::Class::Schema/deploy>.

See the note on L<DBIx::Class::Row/new> for more information about possible
issues related to db-side default values.

=item sequence

   { sequence => 'my_table_seq' }

Set this on a primary key column to the name of the sequence used to
generate a new key value. If not specified, L<DBIx::Class::PK::Auto>
will attempt to retrieve the name of the sequence from the database
automatically.

=item retrieve_on_insert

  { retrieve_on_insert => 1 }

For every column where this is set to true, DBIC will retrieve the RDBMS-side
value upon a new row insertion (normally only the autoincrement PK is
retrieved on insert). C<INSERT ... RETURNING> is used automatically if
supported by the underlying storage, otherwise an extra SELECT statement is
executed to retrieve the missing data.

=item auto_nextval

   { auto_nextval => 1 }

Set this to a true value for a column whose value is retrieved automatically
from a sequence or function (if supported by your Storage driver.) For a
sequence, if you do not use a trigger to get the nextval, you have to set the
L</sequence> value as well.

Also set this for MSSQL columns with the 'uniqueidentifier'
L<data_type|DBIx::Class::ResultSource/data_type> whose values you want to
automatically generate using C<NEWID()>, unless they are a primary key in which
case this will be done anyway.

=item extra

This is used by L<DBIx::Class::Schema/deploy> and L<SQL::Translator>
to add extra non-generic data to the column. For example: C<< extra
=> { unsigned => 1} >> is used by the MySQL producer to set an integer
column to unsigned. For more details, see
L<SQL::Translator::Producer::MySQL>.

=back

=head2 add_column

=over

=item Arguments: $colname, \%columninfo?

=item Return Value: 1/0 (true/false)

=back

  $source->add_column('col' => \%info);

Add a single column and optional column info. Uses the same column
info keys as L</add_columns>.

=cut

sub add_columns {
  my ($self, @cols) = @_;
  $self->_ordered_columns(\@cols) unless $self->_ordered_columns;

  my @added;
  my $columns = $self->_columns;
  while (my $col = shift @cols) {
    my $column_info = {};
    if ($col =~ s/^\+//) {
      $column_info = $self->column_info($col);
    }

    # If next entry is { ... } use that for the column info, if not
    # use an empty hashref
    if (ref $cols[0]) {
      my $new_info = shift(@cols);
      %$column_info = (%$column_info, %$new_info);
    }
    push(@added, $col) unless exists $columns->{$col};
    $columns->{$col} = $column_info;
  }
  push @{ $self->_ordered_columns }, @added;
  return $self;
}

sub add_column { shift->add_columns(@_); } # DO NOT CHANGE THIS TO GLOB

=head2 has_column

=over

=item Arguments: $colname

=item Return Value: 1/0 (true/false)

=back

  if ($source->has_column($colname)) { ... }

Returns true if the source has a column of this name, false otherwise.

=cut

sub has_column {
  my ($self, $column) = @_;
  return exists $self->_columns->{$column};
}

=head2 column_info

=over

=item Arguments: $colname

=item Return Value: Hashref of info

=back

  my $info = $source->column_info($col);

Returns the column metadata hashref for a column, as originally passed
to L</add_columns>. See L</add_columns> above for information on the
contents of the hashref.

=cut

sub column_info {
  my ($self, $column) = @_;
  $self->throw_exception("No such column $column")
    unless exists $self->_columns->{$column};

  if ( ! $self->_columns->{$column}{data_type}
       and ! $self->{_columns_info_loaded}
       and $self->column_info_from_storage
       and my $stor = try { $self->storage } )
  {
    $self->{_columns_info_loaded}++;

    # try for the case of storage without table
    try {
      my $info = $stor->columns_info_for( $self->from );
      my $lc_info = { map
        { (lc $_) => $info->{$_} }
        ( keys %$info )
      };

      foreach my $col ( keys %{$self->_columns} ) {
        $self->_columns->{$col} = {
          %{ $self->_columns->{$col} },
          %{ $info->{$col} || $lc_info->{lc $col} || {} }
        };
      }
    };
  }

  return $self->_columns->{$column};
}

=head2 columns

=over

=item Arguments: none

=item Return Value: Ordered list of column names

=back

  my @column_names = $source->columns;

Returns all column names in the order they were declared to L</add_columns>.

=cut

sub columns {
  my $self = shift;
  $self->throw_exception(
    "columns() is a read-only accessor, did you mean add_columns()?"
  ) if @_;
  return @{$self->{_ordered_columns}||[]};
}

=head2 columns_info

=over

=item Arguments: \@colnames ?

=item Return Value: Hashref of column name/info pairs

=back

  my $columns_info = $source->columns_info;

Like L</column_info> but returns information for the requested columns. If
the optional column-list arrayref is omitted it returns info on all columns
currently defined on the ResultSource via L</add_columns>.

=cut

sub columns_info {
  my ($self, $columns) = @_;

  my $colinfo = $self->_columns;

  if (
    first { ! $_->{data_type} } values %$colinfo
      and
    ! $self->{_columns_info_loaded}
      and
    $self->column_info_from_storage
      and
    my $stor = try { $self->storage }
  ) {
    $self->{_columns_info_loaded}++;

    # try for the case of storage without table
    try {
      my $info = $stor->columns_info_for( $self->from );
      my $lc_info = { map
        { (lc $_) => $info->{$_} }
        ( keys %$info )
      };

      foreach my $col ( keys %$colinfo ) {
        $colinfo->{$col} = {
          %{ $colinfo->{$col} },
          %{ $info->{$col} || $lc_info->{lc $col} || {} }
        };
      }
    };
  }

  my %ret;

  if ($columns) {
    for (@$columns) {
      if (my $inf = $colinfo->{$_}) {
        $ret{$_} = $inf;
      }
      else {
        $self->throw_exception( sprintf (
          "No such column '%s' on source '%s'",
          $_,
          $self->source_name || $self->name || 'Unknown source...?',
        ));
      }
    }
  }
  else {
    %ret = %$colinfo;
  }

  return \%ret;
}

=head2 remove_columns

=over

=item Arguments: @colnames

=item Return Value: not defined

=back

  $source->remove_columns(qw/col1 col2 col3/);

Removes the given list of columns by name, from the result source.

B<Warning>: Removing a column that is also used in the sources primary
key, or in one of the sources unique constraints, B<will> result in a
broken result source.

=head2 remove_column

=over

=item Arguments: $colname

=item Return Value: not defined

=back

  $source->remove_column('col');

Remove a single column by name from the result source, similar to
L</remove_columns>.

B<Warning>: Removing a column that is also used in the sources primary
key, or in one of the sources unique constraints, B<will> result in a
broken result source.

=cut

sub remove_columns {
  my ($self, @to_remove) = @_;

  my $columns = $self->_columns
    or return;

  my %to_remove;
  for (@to_remove) {
    delete $columns->{$_};
    ++$to_remove{$_};
  }

  $self->_ordered_columns([ grep { not $to_remove{$_} } @{$self->_ordered_columns} ]);
}

sub remove_column { shift->remove_columns(@_); } # DO NOT CHANGE THIS TO GLOB

=head2 set_primary_key

=over 4

=item Arguments: @cols

=item Return Value: not defined

=back

Defines one or more columns as primary key for this source. Must be
called after L</add_columns>.

Additionally, defines a L<unique constraint|/add_unique_constraint>
named C<primary>.

Note: you normally do want to define a primary key on your sources
B<even if the underlying database table does not have a primary key>.
See
L<DBIx::Class::Manual::Intro/The Significance and Importance of Primary Keys>
for more info.

=cut

sub set_primary_key {
  my ($self, @cols) = @_;

  my $colinfo = $self->columns_info(\@cols);
  for my $col (@cols) {
    carp_unique(sprintf (
      "Primary key of source '%s' includes the column '%s' which has its "
    . "'is_nullable' attribute set to true. This is a mistake and will cause "
    . 'various Result-object operations to fail',
      $self->source_name || $self->name || 'Unknown source...?',
      $col,
    )) if $colinfo->{$col}{is_nullable};
  }

  $self->_primaries(\@cols);

  $self->add_unique_constraint(primary => \@cols);
}

=head2 primary_columns

=over 4

=item Arguments: none

=item Return Value: Ordered list of primary column names

=back

Read-only accessor which returns the list of primary keys, supplied by
L</set_primary_key>.

=cut

sub primary_columns {
  return @{shift->_primaries||[]};
}

# a helper method that will automatically die with a descriptive message if
# no pk is defined on the source in question. For internal use to save
# on if @pks... boilerplate
sub _pri_cols_or_die {
  my $self = shift;
  my @pcols = $self->primary_columns
    or $self->throw_exception (sprintf(
      "Operation requires a primary key to be declared on '%s' via set_primary_key",
      # source_name is set only after schema-registration
      $self->source_name || $self->result_class || $self->name || 'Unknown source...?',
    ));
  return @pcols;
}

# same as above but mandating single-column PK (used by relationship condition
# inference)
sub _single_pri_col_or_die {
  my $self = shift;
  my ($pri, @too_many) = $self->_pri_cols_or_die;

  $self->throw_exception( sprintf(
    "Operation requires a single-column primary key declared on '%s'",
    $self->source_name || $self->result_class || $self->name || 'Unknown source...?',
  )) if @too_many;
  return $pri;
}


=head2 sequence

Manually define the correct sequence for your table, to avoid the overhead
associated with looking up the sequence automatically. The supplied sequence
will be applied to the L</column_info> of each L<primary_key|/set_primary_key>

=over 4

=item Arguments: $sequence_name

=item Return Value: not defined

=back

=cut

sub sequence {
  my ($self,$seq) = @_;

  my @pks = $self->primary_columns
    or return;

  $_->{sequence} = $seq
    for values %{ $self->columns_info (\@pks) };
}


=head2 add_unique_constraint

=over 4

=item Arguments: $name?, \@colnames

=item Return Value: not defined

=back

Declare a unique constraint on this source. Call once for each unique
constraint.

  # For UNIQUE (column1, column2)
  __PACKAGE__->add_unique_constraint(
    constraint_name => [ qw/column1 column2/ ],
  );

Alternatively, you can specify only the columns:

  __PACKAGE__->add_unique_constraint([ qw/column1 column2/ ]);

This will result in a unique constraint named
C<table_column1_column2>, where C<table> is replaced with the table
name.

Unique constraints are used, for example, when you pass the constraint
name as the C<key> attribute to L<DBIx::Class::ResultSet/find>. Then
only columns in the constraint are searched.

Throws an error if any of the given column names do not yet exist on
the result source.

=cut

sub add_unique_constraint {
  my $self = shift;

  if (@_ > 2) {
    $self->throw_exception(
        'add_unique_constraint() does not accept multiple constraints, use '
      . 'add_unique_constraints() instead'
    );
  }

  my $cols = pop @_;
  if (ref $cols ne 'ARRAY') {
    $self->throw_exception (
      'Expecting an arrayref of constraint columns, got ' . ($cols||'NOTHING')
    );
  }

  my $name = shift @_;

  $name ||= $self->name_unique_constraint($cols);

  foreach my $col (@$cols) {
    $self->throw_exception("No such column $col on table " . $self->name)
      unless $self->has_column($col);
  }

  my %unique_constraints = $self->unique_constraints;
  $unique_constraints{$name} = $cols;
  $self->_unique_constraints(\%unique_constraints);
}

=head2 add_unique_constraints

=over 4

=item Arguments: @constraints

=item Return Value: not defined

=back

Declare multiple unique constraints on this source.

  __PACKAGE__->add_unique_constraints(
    constraint_name1 => [ qw/column1 column2/ ],
    constraint_name2 => [ qw/column2 column3/ ],
  );

Alternatively, you can specify only the columns:

  __PACKAGE__->add_unique_constraints(
    [ qw/column1 column2/ ],
    [ qw/column3 column4/ ]
  );

This will result in unique constraints named C<table_column1_column2> and
C<table_column3_column4>, where C<table> is replaced with the table name.

Throws an error if any of the given column names do not yet exist on
the result source.

See also L</add_unique_constraint>.

=cut

sub add_unique_constraints {
  my $self = shift;
  my @constraints = @_;

  if ( !(@constraints % 2) && first { ref $_ ne 'ARRAY' } @constraints ) {
    # with constraint name
    while (my ($name, $constraint) = splice @constraints, 0, 2) {
      $self->add_unique_constraint($name => $constraint);
    }
  }
  else {
    # no constraint name
    foreach my $constraint (@constraints) {
      $self->add_unique_constraint($constraint);
    }
  }
}

=head2 name_unique_constraint

=over 4

=item Arguments: \@colnames

=item Return Value: Constraint name

=back

  $source->table('mytable');
  $source->name_unique_constraint(['col1', 'col2']);
  # returns
  'mytable_col1_col2'

Return a name for a unique constraint containing the specified
columns. The name is created by joining the table name and each column
name, using an underscore character.

For example, a constraint on a table named C<cd> containing the columns
C<artist> and C<title> would result in a constraint name of C<cd_artist_title>.

This is used by L</add_unique_constraint> if you do not specify the
optional constraint name.

=cut

sub name_unique_constraint {
  my ($self, $cols) = @_;

  my $name = $self->name;
  $name = $$name if (ref $name eq 'SCALAR');
  $name =~ s/ ^ [^\.]+ \. //x;  # strip possible schema qualifier

  return join '_', $name, @$cols;
}

=head2 unique_constraints

=over 4

=item Arguments: none

=item Return Value: Hash of unique constraint data

=back

  $source->unique_constraints();

Read-only accessor which returns a hash of unique constraints on this
source.

The hash is keyed by constraint name, and contains an arrayref of
column names as values.

=cut

sub unique_constraints {
  return %{shift->_unique_constraints||{}};
}

=head2 unique_constraint_names

=over 4

=item Arguments: none

=item Return Value: Unique constraint names

=back

  $source->unique_constraint_names();

Returns the list of unique constraint names defined on this source.

=cut

sub unique_constraint_names {
  my ($self) = @_;

  my %unique_constraints = $self->unique_constraints;

  return keys %unique_constraints;
}

=head2 unique_constraint_columns

=over 4

=item Arguments: $constraintname

=item Return Value: List of constraint columns

=back

  $source->unique_constraint_columns('myconstraint');

Returns the list of columns that make up the specified unique constraint.

=cut

sub unique_constraint_columns {
  my ($self, $constraint_name) = @_;

  my %unique_constraints = $self->unique_constraints;

  $self->throw_exception(
    "Unknown unique constraint $constraint_name on '" . $self->name . "'"
  ) unless exists $unique_constraints{$constraint_name};

  return @{ $unique_constraints{$constraint_name} };
}

=head2 sqlt_deploy_callback

=over

=item Arguments: $callback_name | \&callback_code

=item Return Value: $callback_name | \&callback_code

=back

  __PACKAGE__->sqlt_deploy_callback('mycallbackmethod');

   or

  __PACKAGE__->sqlt_deploy_callback(sub {
    my ($source_instance, $sqlt_table) = @_;
    ...
  } );

An accessor to set a callback to be called during deployment of
the schema via L<DBIx::Class::Schema/create_ddl_dir> or
L<DBIx::Class::Schema/deploy>.

The callback can be set as either a code reference or the name of a
method in the current result class.

Defaults to L</default_sqlt_deploy_hook>.

Your callback will be passed the $source object representing the
ResultSource instance being deployed, and the
L<SQL::Translator::Schema::Table> object being created from it. The
callback can be used to manipulate the table object or add your own
customised indexes. If you need to manipulate a non-table object, use
the L<DBIx::Class::Schema/sqlt_deploy_hook>.

See L<DBIx::Class::Manual::Cookbook/Adding Indexes And Functions To
Your SQL> for examples.

This sqlt deployment callback can only be used to manipulate
SQL::Translator objects as they get turned into SQL. To execute
post-deploy statements which SQL::Translator does not currently
handle, override L<DBIx::Class::Schema/deploy> in your Schema class
and call L<dbh_do|DBIx::Class::Storage::DBI/dbh_do>.

=head2 default_sqlt_deploy_hook

This is the default deploy hook implementation which checks if your
current Result class has a C<sqlt_deploy_hook> method, and if present
invokes it B<on the Result class directly>. This is to preserve the
semantics of C<sqlt_deploy_hook> which was originally designed to expect
the Result class name and the
L<$sqlt_table instance|SQL::Translator::Schema::Table> of the table being
deployed.

=cut

sub default_sqlt_deploy_hook {
  my $self = shift;

  my $class = $self->result_class;

  if ($class and $class->can('sqlt_deploy_hook')) {
    $class->sqlt_deploy_hook(@_);
  }
}

sub _invoke_sqlt_deploy_hook {
  my $self = shift;
  if ( my $hook = $self->sqlt_deploy_callback) {
    $self->$hook(@_);
  }
}

=head2 result_class

=over 4

=item Arguments: $classname

=item Return Value: $classname

=back

 use My::Schema::ResultClass::Inflator;
 ...

 use My::Schema::Artist;
 ...
 __PACKAGE__->result_class('My::Schema::ResultClass::Inflator');

Set the default result class for this source. You can use this to create
and use your own result inflator. See L<DBIx::Class::ResultSet/result_class>
for more details.

Please note that setting this to something like
L<DBIx::Class::ResultClass::HashRefInflator> will make every result unblessed
and make life more difficult.  Inflators like those are better suited to
temporary usage via L<DBIx::Class::ResultSet/result_class>.

=head2 resultset

=over 4

=item Arguments: none

=item Return Value: L<$resultset|DBIx::Class::ResultSet>

=back

Returns a resultset for the given source. This will initially be created
on demand by calling

  $self->resultset_class->new($self, $self->resultset_attributes)

but is cached from then on unless resultset_class changes.

=head2 resultset_class

=over 4

=item Arguments: $classname

=item Return Value: $classname

=back

  package My::Schema::ResultSet::Artist;
  use base 'DBIx::Class::ResultSet';
  ...

  # In the result class
  __PACKAGE__->resultset_class('My::Schema::ResultSet::Artist');

  # Or in code
  $source->resultset_class('My::Schema::ResultSet::Artist');

Set the class of the resultset. This is useful if you want to create your
own resultset methods. Create your own class derived from
L<DBIx::Class::ResultSet>, and set it here. If called with no arguments,
this method returns the name of the existing resultset class, if one
exists.

=head2 resultset_attributes

=over 4

=item Arguments: L<\%attrs|DBIx::Class::ResultSet/ATTRIBUTES>

=item Return Value: L<\%attrs|DBIx::Class::ResultSet/ATTRIBUTES>

=back

  # In the result class
  __PACKAGE__->resultset_attributes({ order_by => [ 'id' ] });

  # Or in code
  $source->resultset_attributes({ order_by => [ 'id' ] });

Store a collection of resultset attributes, that will be set on every
L<DBIx::Class::ResultSet> produced from this result source.

B<CAVEAT>: C<resultset_attributes> comes with its own set of issues and
bugs! While C<resultset_attributes> isn't deprecated per se, its usage is
not recommended!

Since relationships use attributes to link tables together, the "default"
attributes you set may cause unpredictable and undesired behavior.  Furthermore,
the defaults cannot be turned off, so you are stuck with them.

In most cases, what you should actually be using are project-specific methods:

  package My::Schema::ResultSet::Artist;
  use base 'DBIx::Class::ResultSet';
  ...

  # BAD IDEA!
  #__PACKAGE__->resultset_attributes({ prefetch => 'tracks' });

  # GOOD IDEA!
  sub with_tracks { shift->search({}, { prefetch => 'tracks' }) }

  # in your code
  $schema->resultset('Artist')->with_tracks->...

This gives you the flexibility of not using it when you don't need it.

For more complex situations, another solution would be to use a virtual view
via L<DBIx::Class::ResultSource::View>.

=cut

sub resultset {
  my $self = shift;
  $self->throw_exception(
    'resultset does not take any arguments. If you want another resultset, '.
    'call it on the schema instead.'
  ) if scalar @_;

  $self->resultset_class->new(
    $self,
    {
      try { %{$self->schema->default_resultset_attributes} },
      %{$self->{resultset_attributes}},
    },
  );
}

=head2 name

=over 4

=item Arguments: none

=item Result value: $name

=back

Returns the name of the result source, which will typically be the table
name. This may be a scalar reference if the result source has a non-standard
name.

=head2 source_name

=over 4

=item Arguments: $source_name

=item Result value: $source_name

=back

Set an alternate name for the result source when it is loaded into a schema.
This is useful if you want to refer to a result source by a name other than
its class name.

  package ArchivedBooks;
  use base qw/DBIx::Class/;
  __PACKAGE__->table('books_archive');
  __PACKAGE__->source_name('Books');

  # from your schema...
  $schema->resultset('Books')->find(1);

=head2 from

=over 4

=item Arguments: none

=item Return Value: FROM clause

=back

  my $from_clause = $source->from();

Returns an expression of the source to be supplied to storage to specify
retrieval from this source. In the case of a database, the required FROM
clause contents.

=cut

sub from { die 'Virtual method!' }

=head2 source_info

Stores a hashref of per-source metadata.  No specific key names
have yet been standardized, the examples below are purely hypothetical
and don't actually accomplish anything on their own:

  __PACKAGE__->source_info({
    "_tablespace" => 'fast_disk_array_3',
    "_engine" => 'InnoDB',
  });

=head2 schema

=over 4

=item Arguments: L<$schema?|DBIx::Class::Schema>

=item Return Value: L<$schema|DBIx::Class::Schema>

=back

  my $schema = $source->schema();

Sets and/or returns the L<DBIx::Class::Schema> object to which this
result source instance has been attached to.

=cut

sub schema {
  if (@_ > 1) {
    $_[0]->{schema} = $_[1];
  }
  else {
    $_[0]->{schema} || do {
      my $name = $_[0]->{source_name} || '_unnamed_';
      my $err = 'Unable to perform storage-dependent operations with a detached result source '
              . "(source '$name' is not associated with a schema).";

      $err .= ' You need to use $schema->thaw() or manually set'
            . ' $DBIx::Class::ResultSourceHandle::thaw_schema while thawing.'
        if $_[0]->{_detached_thaw};

      DBIx::Class::Exception->throw($err);
    };
  }
}

=head2 storage

=over 4

=item Arguments: none

=item Return Value: L<$storage|DBIx::Class::Storage>

=back

  $source->storage->debug(1);

Returns the L<storage handle|DBIx::Class::Storage> for the current schema.

=cut

sub storage { shift->schema->storage; }

=head2 add_relationship

=over 4

=item Arguments: $rel_name, $related_source_name, \%cond, \%attrs?

=item Return Value: 1/true if it succeeded

=back

  $source->add_relationship('rel_name', 'related_source', $cond, $attrs);

L<DBIx::Class::Relationship> describes a series of methods which
create pre-defined useful types of relationships. Look there first
before using this method directly.

The relationship name can be arbitrary, but must be unique for each
relationship attached to this result source. 'related_source' should
be the name with which the related result source was registered with
the current schema. For example:

  $schema->source('Book')->add_relationship('reviews', 'Review', {
    'foreign.book_id' => 'self.id',
  });

The condition C<$cond> needs to be an L<SQL::Abstract>-style
representation of the join between the tables. For example, if you're
creating a relation from Author to Book,

  { 'foreign.author_id' => 'self.id' }

will result in the JOIN clause

  author me JOIN book foreign ON foreign.author_id = me.id

You can specify as many foreign => self mappings as necessary.

Valid attributes are as follows:

=over 4

=item join_type

Explicitly specifies the type of join to use in the relationship. Any
SQL join type is valid, e.g. C<LEFT> or C<RIGHT>. It will be placed in
the SQL command immediately before C<JOIN>.

=item proxy

An arrayref containing a list of accessors in the foreign class to proxy in
the main class. If, for example, you do the following:

  CD->might_have(liner_notes => 'LinerNotes', undef, {
    proxy => [ qw/notes/ ],
  });

Then, assuming LinerNotes has an accessor named notes, you can do:

  my $cd = CD->find(1);
  # set notes -- LinerNotes object is created if it doesn't exist
  $cd->notes('Notes go here');

=item accessor

Specifies the type of accessor that should be created for the
relationship. Valid values are C<single> (for when there is only a single
related object), C<multi> (when there can be many), and C<filter> (for
when there is a single related object, but you also want the relationship
accessor to double as a column accessor). For C<multi> accessors, an
add_to_* method is also created, which calls C<create_related> for the
relationship.

=back

Throws an exception if the condition is improperly supplied, or cannot
be resolved.

=cut

sub add_relationship {
  my ($self, $rel, $f_source_name, $cond, $attrs) = @_;
  $self->throw_exception("Can't create relationship without join condition")
    unless $cond;
  $attrs ||= {};

  # Check foreign and self are right in cond
  if ( (ref $cond ||'') eq 'HASH') {
    $_ =~ /^foreign\./ or $self->throw_exception("Malformed relationship condition key '$_': must be prefixed with 'foreign.'")
      for keys %$cond;

    $_ =~ /^self\./ or $self->throw_exception("Malformed relationship condition value '$_': must be prefixed with 'self.'")
      for values %$cond;
  }

  my %rels = %{ $self->_relationships };
  $rels{$rel} = { class => $f_source_name,
                  source => $f_source_name,
                  cond  => $cond,
                  attrs => $attrs };
  $self->_relationships(\%rels);

  return $self;

# XXX disabled. doesn't work properly currently. skip in tests.

  my $f_source = $self->schema->source($f_source_name);
  unless ($f_source) {
    $self->ensure_class_loaded($f_source_name);
    $f_source = $f_source_name->result_source;
    #my $s_class = ref($self->schema);
    #$f_source_name =~ m/^${s_class}::(.*)$/;
    #$self->schema->register_class(($1 || $f_source_name), $f_source_name);
    #$f_source = $self->schema->source($f_source_name);
  }
  return unless $f_source; # Can't test rel without f_source

  try { $self->_resolve_join($rel, 'me', {}, []) }
  catch {
    # If the resolve failed, back out and re-throw the error
    delete $rels{$rel};
    $self->_relationships(\%rels);
    $self->throw_exception("Error creating relationship $rel: $_");
  };

  1;
}

=head2 relationships

=over 4

=item Arguments: none

=item Return Value: L<@rel_names|DBIx::Class::Relationship>

=back

  my @rel_names = $source->relationships();

Returns all relationship names for this source.

=cut

sub relationships {
  return keys %{shift->_relationships};
}

=head2 relationship_info

=over 4

=item Arguments: L<$rel_name|DBIx::Class::Relationship>

=item Return Value: L<\%rel_data|DBIx::Class::Relationship::Base/add_relationship>

=back

Returns a hash of relationship information for the specified relationship
name. The keys/values are as specified for L<DBIx::Class::Relationship::Base/add_relationship>.

=cut

sub relationship_info {
  #my ($self, $rel) = @_;
  return shift->_relationships->{+shift};
}

=head2 has_relationship

=over 4

=item Arguments: L<$rel_name|DBIx::Class::Relationship>

=item Return Value: 1/0 (true/false)

=back

Returns true if the source has a relationship of this name, false otherwise.

=cut

sub has_relationship {
  #my ($self, $rel) = @_;
  return exists shift->_relationships->{+shift};
}

=head2 reverse_relationship_info

=over 4

=item Arguments: L<$rel_name|DBIx::Class::Relationship>

=item Return Value: L<\%rel_data|DBIx::Class::Relationship::Base/add_relationship>

=back

Looks through all the relationships on the source this relationship
points to, looking for one whose condition is the reverse of the
condition on this relationship.

A common use of this is to find the name of the C<belongs_to> relation
opposing a C<has_many> relation. For definition of these look in
L<DBIx::Class::Relationship>.

The returned hashref is keyed by the name of the opposing
relationship, and contains its data in the same manner as
L</relationship_info>.

=cut

sub reverse_relationship_info {
  my ($self, $rel) = @_;

  my $rel_info = $self->relationship_info($rel)
    or $self->throw_exception("No such relationship '$rel'");

  my $ret = {};

  return $ret unless ((ref $rel_info->{cond}) eq 'HASH');

  my $stripped_cond = $self->__strip_relcond ($rel_info->{cond});

  my $registered_source_name = $self->source_name;

  # this may be a partial schema or something else equally esoteric
  my $other_rsrc = $self->related_source($rel);

  # Get all the relationships for that source that related to this source
  # whose foreign column set are our self columns on $rel and whose self
  # columns are our foreign columns on $rel
  foreach my $other_rel ($other_rsrc->relationships) {

    # only consider stuff that points back to us
    # "us" here is tricky - if we are in a schema registration, we want
    # to use the source_names, otherwise we will use the actual classes

    # the schema may be partial
    my $roundtrip_rsrc = try { $other_rsrc->related_source($other_rel) }
      or next;

    if ($registered_source_name) {
      next if $registered_source_name ne ($roundtrip_rsrc->source_name || '')
    }
    else {
      next if $self->result_class ne $roundtrip_rsrc->result_class;
    }

    my $other_rel_info = $other_rsrc->relationship_info($other_rel);

    # this can happen when we have a self-referential class
    next if $other_rel_info eq $rel_info;

    next unless ref $other_rel_info->{cond} eq 'HASH';
    my $other_stripped_cond = $self->__strip_relcond($other_rel_info->{cond});

    $ret->{$other_rel} = $other_rel_info if (
      $self->_compare_relationship_keys (
        [ keys %$stripped_cond ], [ values %$other_stripped_cond ]
      )
        and
      $self->_compare_relationship_keys (
        [ values %$stripped_cond ], [ keys %$other_stripped_cond ]
      )
    );
  }

  return $ret;
}

# all this does is removes the foreign/self prefix from a condition
sub __strip_relcond {
  +{
    map
      { map { /^ (?:foreign|self) \. (\w+) $/x } ($_, $_[1]{$_}) }
      keys %{$_[1]}
  }
}

sub compare_relationship_keys {
  carp 'compare_relationship_keys is a private method, stop calling it';
  my $self = shift;
  $self->_compare_relationship_keys (@_);
}

# Returns true if both sets of keynames are the same, false otherwise.
sub _compare_relationship_keys {
#  my ($self, $keys1, $keys2) = @_;
  return
    join ("\x00", sort @{$_[1]})
      eq
    join ("\x00", sort @{$_[2]})
  ;
}

# optionally takes either an arrayref of column names, or a hashref of already
# retrieved colinfos
# returns an arrayref of column names of the shortest unique constraint
# (matching some of the input if any), giving preference to the PK
sub _identifying_column_set {
  my ($self, $cols) = @_;

  my %unique = $self->unique_constraints;
  my $colinfos = ref $cols eq 'HASH' ? $cols : $self->columns_info($cols||());

  # always prefer the PK first, and then shortest constraints first
  USET:
  for my $set (delete $unique{primary}, sort { @$a <=> @$b } (values %unique) ) {
    next unless $set && @$set;

    for (@$set) {
      next USET unless ($colinfos->{$_} && !$colinfos->{$_}{is_nullable} );
    }

    # copy so we can mangle it at will
    return [ @$set ];
  }

  return undef;
}

sub _minimal_valueset_satisfying_constraint {
  my $self = shift;
  my $args = { ref $_[0] eq 'HASH' ? %{ $_[0] } : @_ };

  $args->{columns_info} ||= $self->columns_info;

  my $vals = $self->storage->_extract_fixed_condition_columns(
    $args->{values},
    ($args->{carp_on_nulls} ? 'consider_nulls' : undef ),
  );

  my $cols;
  for my $col ($self->unique_constraint_columns($args->{constraint_name}) ) {
    if( ! exists $vals->{$col} or ( $vals->{$col}||'' ) eq UNRESOLVABLE_CONDITION ) {
      $cols->{missing}{$col} = undef;
    }
    elsif( ! defined $vals->{$col} ) {
      $cols->{$args->{carp_on_nulls} ? 'undefined' : 'missing'}{$col} = undef;
    }
    else {
      # we need to inject back the '=' as _extract_fixed_condition_columns
      # will strip it from literals and values alike, resulting in an invalid
      # condition in the end
      $cols->{present}{$col} = { '=' => $vals->{$col} };
    }

    $cols->{fc}{$col} = 1 if (
      ( ! $cols->{missing} or ! exists $cols->{missing}{$col} )
        and
      keys %{ $args->{columns_info}{$col}{_filter_info} || {} }
    );
  }

  $self->throw_exception( sprintf ( "Unable to satisfy requested constraint '%s', missing values for column(s): %s",
    $args->{constraint_name},
    join (', ', map { "'$_'" } sort keys %{$cols->{missing}} ),
  ) ) if $cols->{missing};

  $self->throw_exception( sprintf (
    "Unable to satisfy requested constraint '%s', FilterColumn values not usable for column(s): %s",
    $args->{constraint_name},
    join (', ', map { "'$_'" } sort keys %{$cols->{fc}}),
  )) if $cols->{fc};

  if (
    $cols->{undefined}
      and
    !$ENV{DBIC_NULLABLE_KEY_NOWARN}
  ) {
    carp_unique ( sprintf (
      "NULL/undef values supplied for requested unique constraint '%s' (NULL "
    . 'values in column(s): %s). This is almost certainly not what you wanted, '
    . 'though you can set DBIC_NULLABLE_KEY_NOWARN to disable this warning.',
      $args->{constraint_name},
      join (', ', map { "'$_'" } sort keys %{$cols->{undefined}}),
    ));
  }

  return { map { %{ $cols->{$_}||{} } } qw(present undefined) };
}

# Returns the {from} structure used to express JOIN conditions
sub _resolve_join {
  my ($self, $join, $alias, $seen, $jpath, $parent_force_left) = @_;

  # we need a supplied one, because we do in-place modifications, no returns
  $self->throw_exception ('You must supply a seen hashref as the 3rd argument to _resolve_join')
    unless ref $seen eq 'HASH';

  $self->throw_exception ('You must supply a joinpath arrayref as the 4th argument to _resolve_join')
    unless ref $jpath eq 'ARRAY';

  $jpath = [@$jpath]; # copy

  if (not defined $join or not length $join) {
    return ();
  }
  elsif (ref $join eq 'ARRAY') {
    return
      map {
        $self->_resolve_join($_, $alias, $seen, $jpath, $parent_force_left);
      } @$join;
  }
  elsif (ref $join eq 'HASH') {

    my @ret;
    for my $rel (keys %$join) {

      my $rel_info = $self->relationship_info($rel)
        or $self->throw_exception("No such relationship '$rel' on " . $self->source_name);

      my $force_left = $parent_force_left;
      $force_left ||= lc($rel_info->{attrs}{join_type}||'') eq 'left';

      # the actual seen value will be incremented by the recursion
      my $as = $self->storage->relname_to_table_alias(
        $rel, ($seen->{$rel} && $seen->{$rel} + 1)
      );

      push @ret, (
        $self->_resolve_join($rel, $alias, $seen, [@$jpath], $force_left),
        $self->related_source($rel)->_resolve_join(
          $join->{$rel}, $as, $seen, [@$jpath, { $rel => $as }], $force_left
        )
      );
    }
    return @ret;

  }
  elsif (ref $join) {
    $self->throw_exception("No idea how to resolve join reftype ".ref $join);
  }
  else {
    my $count = ++$seen->{$join};
    my $as = $self->storage->relname_to_table_alias(
      $join, ($count > 1 && $count)
    );

    my $rel_info = $self->relationship_info($join)
      or $self->throw_exception("No such relationship $join on " . $self->source_name);

    my $rel_src = $self->related_source($join);
    return [ { $as => $rel_src->from,
               -rsrc => $rel_src,
               -join_type => $parent_force_left
                  ? 'left'
                  : $rel_info->{attrs}{join_type}
                ,
               -join_path => [@$jpath, { $join => $as } ],
               -is_single => (
                  (! $rel_info->{attrs}{accessor})
                    or
                  first { $rel_info->{attrs}{accessor} eq $_ } (qw/single filter/)
                ),
               -alias => $as,
               -relation_chain_depth => ( $seen->{-relation_chain_depth} || 0 ) + 1,
             },
             scalar $self->_resolve_condition($rel_info->{cond}, $as, $alias, $join)
          ];
  }
}

sub pk_depends_on {
  carp 'pk_depends_on is a private method, stop calling it';
  my $self = shift;
  $self->_pk_depends_on (@_);
}

# Determines whether a relation is dependent on an object from this source
# having already been inserted. Takes the name of the relationship and a
# hashref of columns of the related object.
sub _pk_depends_on {
  my ($self, $rel_name, $rel_data) = @_;

  my $relinfo = $self->relationship_info($rel_name);

  # don't assume things if the relationship direction is specified
  return $relinfo->{attrs}{is_foreign_key_constraint}
    if exists ($relinfo->{attrs}{is_foreign_key_constraint});

  my $cond = $relinfo->{cond};
  return 0 unless ref($cond) eq 'HASH';

  # map { foreign.foo => 'self.bar' } to { bar => 'foo' }
  my $keyhash = { map { my $x = $_; $x =~ s/.*\.//; $x; } reverse %$cond };

  # assume anything that references our PK probably is dependent on us
  # rather than vice versa, unless the far side is (a) defined or (b)
  # auto-increment
  my $rel_source = $self->related_source($rel_name);

  foreach my $p ($self->primary_columns) {
    if (exists $keyhash->{$p}) {
      unless (defined($rel_data->{$keyhash->{$p}})
              || $rel_source->column_info($keyhash->{$p})
                            ->{is_auto_increment}) {
        return 0;
      }
    }
  }

  return 1;
}

sub resolve_condition {
  carp 'resolve_condition is a private method, stop calling it';
  shift->_resolve_condition (@_);
}

sub _resolve_condition {
#  carp_unique sprintf
#    '_resolve_condition is a private method, and moreover is about to go '
#  . 'away. Please contact the development team at %s if you believe you '
#  . 'have a genuine use for this method, in order to discuss alternatives.',
#    DBIx::Class::_ENV_::HELP_URL,
#  ;

#######################
### API Design? What's that...? (a backwards compatible shim, kill me now)

  my ($self, $cond, @res_args, $rel_name);

  # we *SIMPLY DON'T KNOW YET* which arg is which, yay
  ($self, $cond, $res_args[0], $res_args[1], $rel_name) = @_;

  # assume that an undef is an object-like unset (set_from_related(undef))
  my @is_objlike = map { ! defined $_ or length ref $_ } (@res_args);

  # turn objlike into proper objects for saner code further down
  for (0,1) {
    next unless $is_objlike[$_];

    if ( defined blessed $res_args[$_] ) {

      # but wait - there is more!!! WHAT THE FUCK?!?!?!?!
      if ($res_args[$_]->isa('DBIx::Class::ResultSet')) {
        carp('Passing a resultset for relationship resolution makes no sense - invoking __gremlins__');
        $is_objlike[$_] = 0;
        $res_args[$_] = '__gremlins__';
      }
    }
    else {
      $res_args[$_] ||= {};

      # hate everywhere - have to pass in as a plain hash
      # pretending to be an object at least for now
      $self->throw_exception("Unsupported object-like structure encountered: $res_args[$_]")
        unless ref $res_args[$_] eq 'HASH';
    }
  }

  my $args = {
    condition => $cond,

    # where-is-waldo block guesses relname, then further down we override it if available
    (
      $is_objlike[1] ? ( rel_name => $res_args[0], self_alias => $res_args[0], foreign_alias => 'me',         self_result_object  => $res_args[1] )
    : $is_objlike[0] ? ( rel_name => $res_args[1], self_alias => 'me',         foreign_alias => $res_args[1], foreign_values      => $res_args[0] )
    :                  ( rel_name => $res_args[0], self_alias => $res_args[1], foreign_alias => $res_args[0]                                      )
    ),

    ( $rel_name ? ( rel_name => $rel_name ) : () ),
  };
#######################

  # now it's fucking easy isn't it?!
  my $rc = $self->_resolve_relationship_condition( $args );

  my @res = (
    ( $rc->{join_free_condition} || $rc->{condition} ),
    ! $rc->{join_free_condition},
  );

  # _resolve_relationship_condition always returns qualified cols even in the
  # case of join_free_condition, but nothing downstream expects this
  if ($rc->{join_free_condition} and ref $res[0] eq 'HASH') {
    $res[0] = { map
      { ($_ =~ /\.(.+)/) => $res[0]{$_} }
      keys %{$res[0]}
    };
  }

  # and more legacy
  return wantarray ? @res : $res[0];
}

# Keep this indefinitely. There is evidence of both CPAN and
# darkpan using it, and there isn't much harm in an extra var
# anyway.
our $UNRESOLVABLE_CONDITION = UNRESOLVABLE_CONDITION;
# YES I KNOW THIS IS EVIL
# it is there to save darkpan from themselves, since internally
# we are moving to a constant
Internals::SvREADONLY($UNRESOLVABLE_CONDITION => 1);

# Resolves the passed condition to a concrete query fragment and extra
# metadata
#
## self-explanatory API, modeled on the custom cond coderef:
# rel_name              => (scalar)
# foreign_alias         => (scalar)
# foreign_values        => (either not supplied, or a hashref, or a foreign ResultObject (to be ->get_columns()ed), or plain undef )
# self_alias            => (scalar)
# self_result_object    => (either not supplied or a result object)
# require_join_free_condition => (boolean, throws on failure to construct a JF-cond)
# infer_values_based_on => (either not supplied or a hashref, implies require_join_free_condition)
# condition             => (sqla cond struct, optional, defeaults to from $self->rel_info(rel_name)->{cond})
#
## returns a hash
# condition           => (a valid *likely fully qualified* sqla cond structure)
# identity_map        => (a hashref of foreign-to-self *unqualified* column equality names)
# join_free_condition => (a valid *fully qualified* sqla cond structure, maybe unset)
# inferred_values     => (in case of an available join_free condition, this is a hashref of
#                         *unqualified* column/value *EQUALITY* pairs, representing an amalgamation
#                         of the JF-cond parse and infer_values_based_on
#                         always either complete or unset)
#
sub _resolve_relationship_condition {
  my $self = shift;

  my $args = { ref $_[0] eq 'HASH' ? %{ $_[0] } : @_ };

  for ( qw( rel_name self_alias foreign_alias ) ) {
    $self->throw_exception("Mandatory argument '$_' to _resolve_relationship_condition() is not a plain string")
      if !defined $args->{$_} or length ref $args->{$_};
  }

  $self->throw_exception("Arguments 'self_alias' and 'foreign_alias' may not be identical")
    if $args->{self_alias} eq $args->{foreign_alias};

# TEMP
  my $exception_rel_id = "relationship '$args->{rel_name}' on source '@{[ $self->source_name ]}'";

  my $rel_info = $self->relationship_info($args->{rel_name})
# TEMP
#    or $self->throw_exception( "No such $exception_rel_id" );
    or carp_unique("Requesting resolution on non-existent relationship '$args->{rel_name}' on source '@{[ $self->source_name ]}': fix your code *soon*, as it will break with the next major version");

# TEMP
  $exception_rel_id = "relationship '$rel_info->{_original_name}' on source '@{[ $self->source_name ]}'"
    if $rel_info and exists $rel_info->{_original_name};

  $self->throw_exception("No practical way to resolve $exception_rel_id between two data structures")
    if exists $args->{self_result_object} and exists $args->{foreign_values};

  $self->throw_exception( "Argument to infer_values_based_on must be a hash" )
    if exists $args->{infer_values_based_on} and ref $args->{infer_values_based_on} ne 'HASH';

  $args->{require_join_free_condition} ||= !!$args->{infer_values_based_on};

  $args->{condition} ||= $rel_info->{cond};

  $self->throw_exception( "Argument 'self_result_object' must be an object inheriting from DBIx::Class::Row" )
    if (
      exists $args->{self_result_object}
        and
      ( ! defined blessed $args->{self_result_object} or ! $args->{self_result_object}->isa('DBIx::Class::Row') )
    )
  ;

#TEMP
  my $rel_rsrc;# = $self->related_source($args->{rel_name});

  if (exists $args->{foreign_values}) {
# TEMP
    $rel_rsrc ||= $self->related_source($args->{rel_name});

    if (defined blessed $args->{foreign_values}) {

      $self->throw_exception( "Objects supplied as 'foreign_values' ($args->{foreign_values}) must inherit from DBIx::Class::Row" )
        unless $args->{foreign_values}->isa('DBIx::Class::Row');

      carp_unique(
        "Objects supplied as 'foreign_values' ($args->{foreign_values}) "
      . "usually should inherit from the related ResultClass ('@{[ $rel_rsrc->result_class ]}'), "
      . "perhaps you've made a mistake invoking the condition resolver?"
      ) unless $args->{foreign_values}->isa($rel_rsrc->result_class);

      $args->{foreign_values} = { $args->{foreign_values}->get_columns };
    }
    elsif (! defined $args->{foreign_values} or ref $args->{foreign_values} eq 'HASH') {
      my $ri = { map { $_ => 1 } $rel_rsrc->relationships };
      my $ci = $rel_rsrc->columns_info;
      ! exists $ci->{$_} and ! exists $ri->{$_} and $self->throw_exception(
        "Key '$_' supplied as 'foreign_values' is not a column on related source '@{[ $rel_rsrc->source_name ]}'"
      ) for keys %{ $args->{foreign_values} ||= {} };
    }
    else {
      $self->throw_exception(
        "Argument 'foreign_values' must be either an object inheriting from '@{[ $rel_rsrc->result_class ]}', "
      . "or a hash reference, or undef"
      );
    }
  }

  my $ret;

  if (ref $args->{condition} eq 'CODE') {

    my $cref_args = {
      rel_name => $args->{rel_name},
      self_resultsource => $self,
      self_alias => $args->{self_alias},
      foreign_alias => $args->{foreign_alias},
      ( map
        { (exists $args->{$_}) ? ( $_ => $args->{$_} ) : () }
        qw( self_result_object foreign_values )
      ),
    };

    # legacy - never remove these!!!
    $cref_args->{foreign_relname} = $cref_args->{rel_name};

    $cref_args->{self_rowobj} = $cref_args->{self_result_object}
      if exists $cref_args->{self_result_object};

    ($ret->{condition}, $ret->{join_free_condition}, my @extra) = $args->{condition}->($cref_args);

    # sanity check
    $self->throw_exception("A custom condition coderef can return at most 2 conditions, but $exception_rel_id returned extra values: @extra")
      if @extra;

    if (my $jfc = $ret->{join_free_condition}) {

      $self->throw_exception (
        "The join-free condition returned for $exception_rel_id must be a hash reference"
      ) unless ref $jfc eq 'HASH';

# TEMP
      $rel_rsrc ||= $self->related_source($args->{rel_name});

      my ($joinfree_alias, $joinfree_source);
      if (defined $args->{self_result_object}) {
        $joinfree_alias = $args->{foreign_alias};
        $joinfree_source = $rel_rsrc;
      }
      elsif (defined $args->{foreign_values}) {
        $joinfree_alias = $args->{self_alias};
        $joinfree_source = $self;
      }

      # FIXME sanity check until things stabilize, remove at some point
      $self->throw_exception (
        "A join-free condition returned for $exception_rel_id without a result object to chain from"
      ) unless $joinfree_alias;

      my $fq_col_list = { map
        { ( "$joinfree_alias.$_" => 1 ) }
        $joinfree_source->columns
      };

      exists $fq_col_list->{$_} or $self->throw_exception (
        "The join-free condition returned for $exception_rel_id may only "
      . 'contain keys that are fully qualified column names of the corresponding source '
      . "(it returned '$_')"
      ) for keys %$jfc;

      (
        length ref $_
          and
        defined blessed($_)
          and
        $_->isa('DBIx::Class::Row')
          and
        $self->throw_exception (
          "The join-free condition returned for $exception_rel_id may not "
        . 'contain result objects as values - perhaps instead of invoking '
        . '->$something you meant to return ->get_column($something)'
        )
      ) for values %$jfc;

    }
  }
  elsif (ref $args->{condition} eq 'HASH') {

    # the condition is static - use parallel arrays
    # for a "pivot" depending on which side of the
    # rel did we get as an object
    my (@f_cols, @l_cols);
    for my $fc (keys %{$args->{condition}}) {
      my $lc = $args->{condition}{$fc};

      # FIXME STRICTMODE should probably check these are valid columns
      $fc =~ s/^foreign\.// ||
        $self->throw_exception("Invalid rel cond key '$fc'");

      $lc =~ s/^self\.// ||
        $self->throw_exception("Invalid rel cond val '$lc'");

      push @f_cols, $fc;
      push @l_cols, $lc;
    }

    # construct the crosstable condition and the identity map
    for  (0..$#f_cols) {
      $ret->{condition}{"$args->{foreign_alias}.$f_cols[$_]"} = { -ident => "$args->{self_alias}.$l_cols[$_]" };
      $ret->{identity_map}{$l_cols[$_]} = $f_cols[$_];
    };

    if ($args->{foreign_values}) {
      $ret->{join_free_condition}{"$args->{self_alias}.$l_cols[$_]"} = $args->{foreign_values}{$f_cols[$_]}
        for 0..$#f_cols;
    }
    elsif (defined $args->{self_result_object}) {

      for my $i (0..$#l_cols) {
        if ( $args->{self_result_object}->has_column_loaded($l_cols[$i]) ) {
          $ret->{join_free_condition}{"$args->{foreign_alias}.$f_cols[$i]"} = $args->{self_result_object}->get_column($l_cols[$i]);
        }
        else {
          $self->throw_exception(sprintf
            "Unable to resolve relationship '%s' from object '%s': column '%s' not "
          . 'loaded from storage (or not passed to new() prior to insert()). You '
          . 'probably need to call ->discard_changes to get the server-side defaults '
          . 'from the database.',
            $args->{rel_name},
            $args->{self_result_object},
            $l_cols[$i],
          ) if $args->{self_result_object}->in_storage;

          # FIXME - temporarly force-override
          delete $args->{require_join_free_condition};
          $ret->{join_free_condition} = UNRESOLVABLE_CONDITION;
          last;
        }
      }
    }
  }
  elsif (ref $args->{condition} eq 'ARRAY') {
    if (@{$args->{condition}} == 0) {
      $ret = {
        condition => UNRESOLVABLE_CONDITION,
        join_free_condition => UNRESOLVABLE_CONDITION,
      };
    }
    elsif (@{$args->{condition}} == 1) {
      $ret = $self->_resolve_relationship_condition({
        %$args,
        condition => $args->{condition}[0],
      });
    }
    else {
      # we are discarding inferred values here... likely incorrect...
      # then again - the entire thing is an OR, so we *can't* use them anyway
      for my $subcond ( map
        { $self->_resolve_relationship_condition({ %$args, condition => $_ }) }
        @{$args->{condition}}
      ) {
        $self->throw_exception('Either all or none of the OR-condition members must resolve to a join-free condition')
          if ( $ret and ( $ret->{join_free_condition} xor $subcond->{join_free_condition} ) );

        $subcond->{$_} and push @{$ret->{$_}}, $subcond->{$_} for (qw(condition join_free_condition));
      }
    }
  }
  else {
    $self->throw_exception ("Can't handle condition $args->{condition} for $exception_rel_id yet :(");
  }

  $self->throw_exception(ucfirst "$exception_rel_id does not resolve to a join-free condition fragment") if (
    $args->{require_join_free_condition}
      and
    ( ! $ret->{join_free_condition} or $ret->{join_free_condition} eq UNRESOLVABLE_CONDITION )
  );

  my $storage = $self->schema->storage;

  # we got something back - sanity check and infer values if we can
  my @nonvalues;
  if ( my $jfc = $ret->{join_free_condition} and $ret->{join_free_condition} ne UNRESOLVABLE_CONDITION ) {

    my $jfc_eqs = $storage->_extract_fixed_condition_columns($jfc, 'consider_nulls');

    if (keys %$jfc_eqs) {

      for (keys %$jfc) {
        # $jfc is fully qualified by definition
        my ($col) = $_ =~ /\.(.+)/;

        if (exists $jfc_eqs->{$_} and ($jfc_eqs->{$_}||'') ne UNRESOLVABLE_CONDITION) {
          $ret->{inferred_values}{$col} = $jfc_eqs->{$_};
        }
        elsif ( !$args->{infer_values_based_on} or ! exists $args->{infer_values_based_on}{$col} ) {
          push @nonvalues, $col;
        }
      }

      # all or nothing
      delete $ret->{inferred_values} if @nonvalues;
    }
  }

  # did the user explicitly ask
  if ($args->{infer_values_based_on}) {

    $self->throw_exception(sprintf (
      "Unable to complete value inferrence - custom $exception_rel_id returns conditions instead of values for column(s): %s",
      map { "'$_'" } @nonvalues
    )) if @nonvalues;


    $ret->{inferred_values} ||= {};

    $ret->{inferred_values}{$_} = $args->{infer_values_based_on}{$_}
      for keys %{$args->{infer_values_based_on}};
  }

  # add the identities based on the main condition
  # (may already be there, since easy to calculate on the fly in the HASH case)
  if ( ! $ret->{identity_map} ) {

    my $col_eqs = $storage->_extract_fixed_condition_columns($ret->{condition});

    my $colinfos;
    for my $lhs (keys %$col_eqs) {

      next if $col_eqs->{$lhs} eq UNRESOLVABLE_CONDITION;

# TEMP
      $rel_rsrc ||= $self->related_source($args->{rel_name});

      # there is no way to know who is right and who is left in a cref
      # therefore a full blown resolution call, and figure out the
      # direction a bit further below
      $colinfos ||= $storage->_resolve_column_info([
        { -alias => $args->{self_alias}, -rsrc => $self },
        { -alias => $args->{foreign_alias}, -rsrc => $rel_rsrc },
      ]);

      next unless $colinfos->{$lhs};  # someone is engaging in witchcraft

      if ( my $rhs_ref = is_literal_value( $col_eqs->{$lhs} ) ) {

        if (
          $colinfos->{$rhs_ref->[0]}
            and
          $colinfos->{$lhs}{-source_alias} ne $colinfos->{$rhs_ref->[0]}{-source_alias}
        ) {
          ( $colinfos->{$lhs}{-source_alias} eq $args->{self_alias} )
            ? ( $ret->{identity_map}{$colinfos->{$lhs}{-colname}} = $colinfos->{$rhs_ref->[0]}{-colname} )
            : ( $ret->{identity_map}{$colinfos->{$rhs_ref->[0]}{-colname}} = $colinfos->{$lhs}{-colname} )
          ;
        }
      }
      elsif (
        $col_eqs->{$lhs} =~ /^ ( \Q$args->{self_alias}\E \. .+ ) /x
          and
        ($colinfos->{$1}||{})->{-result_source} == $rel_rsrc
      ) {
        my ($lcol, $rcol) = map
          { $colinfos->{$_}{-colname} }
          ( $lhs, $1 )
        ;
        carp_unique(
          "The $exception_rel_id specifies equality of column '$lcol' and the "
        . "*VALUE* '$rcol' (you did not use the { -ident => ... } operator)"
        );
      }
    }
  }

  # FIXME - temporary, to fool the idiotic check in SQLMaker::_join_condition
  $ret->{condition} = { -and => [ $ret->{condition} ] }
    unless $ret->{condition} eq UNRESOLVABLE_CONDITION;

  $ret;
}

=head2 related_source

=over 4

=item Arguments: $rel_name

=item Return Value: $source

=back

Returns the result source object for the given relationship.

=cut

sub related_source {
  my ($self, $rel) = @_;
  if( !$self->has_relationship( $rel ) ) {
    $self->throw_exception("No such relationship '$rel' on " . $self->source_name);
  }

  # if we are not registered with a schema - just use the prototype
  # however if we do have a schema - ask for the source by name (and
  # throw in the process if all fails)
  if (my $schema = try { $self->schema }) {
    $schema->source($self->relationship_info($rel)->{source});
  }
  else {
    my $class = $self->relationship_info($rel)->{class};
    $self->ensure_class_loaded($class);
    $class->result_source_instance;
  }
}

=head2 related_class

=over 4

=item Arguments: $rel_name

=item Return Value: $classname

=back

Returns the class name for objects in the given relationship.

=cut

sub related_class {
  my ($self, $rel) = @_;
  if( !$self->has_relationship( $rel ) ) {
    $self->throw_exception("No such relationship '$rel' on " . $self->source_name);
  }
  return $self->schema->class($self->relationship_info($rel)->{source});
}

=head2 handle

=over 4

=item Arguments: none

=item Return Value: L<$source_handle|DBIx::Class::ResultSourceHandle>

=back

Obtain a new L<result source handle instance|DBIx::Class::ResultSourceHandle>
for this source. Used as a serializable pointer to this resultsource, as it is not
easy (nor advisable) to serialize CODErefs which may very well be present in e.g.
relationship definitions.

=cut

sub handle {
  return DBIx::Class::ResultSourceHandle->new({
    source_moniker => $_[0]->source_name,

    # so that a detached thaw can be re-frozen
    $_[0]->{_detached_thaw}
      ? ( _detached_source  => $_[0]          )
      : ( schema            => $_[0]->schema  )
    ,
  });
}

my $global_phase_destroy;
sub DESTROY {
  ### NO detected_reinvoked_destructor check
  ### This code very much relies on being called multuple times

  return if $global_phase_destroy ||= in_global_destruction;

######
# !!! ACHTUNG !!!!
######
#
# Under no circumstances shall $_[0] be stored anywhere else (like copied to
# a lexical variable, or shifted, or anything else). Doing so will mess up
# the refcount of this particular result source, and will allow the $schema
# we are trying to save to reattach back to the source we are destroying.
# The relevant code checking refcounts is in ::Schema::DESTROY()

  # if we are not a schema instance holder - we don't matter
  return if(
    ! ref $_[0]->{schema}
      or
    isweak $_[0]->{schema}
  );

  # weaken our schema hold forcing the schema to find somewhere else to live
  # during global destruction (if we have not yet bailed out) this will throw
  # which will serve as a signal to not try doing anything else
  # however beware - on older perls the exception seems randomly untrappable
  # due to some weird race condition during thread joining :(((
  local $@;
  eval {
    weaken $_[0]->{schema};

    # if schema is still there reintroduce ourselves with strong refs back to us
    if ($_[0]->{schema}) {
      my $srcregs = $_[0]->{schema}->source_registrations;
      for (keys %$srcregs) {
        next unless $srcregs->{$_};
        $srcregs->{$_} = $_[0] if $srcregs->{$_} == $_[0];
      }
    }

    1;
  } or do {
    $global_phase_destroy = 1;
  };

  return;
}

sub STORABLE_freeze { Storable::nfreeze($_[0]->handle) }

sub STORABLE_thaw {
  my ($self, $cloning, $ice) = @_;
  %$self = %{ (Storable::thaw($ice))->resolve };
}

=head2 throw_exception

See L<DBIx::Class::Schema/"throw_exception">.

=cut

sub throw_exception {
  my $self = shift;

  $self->{schema}
    ? $self->{schema}->throw_exception(@_)
    : DBIx::Class::Exception->throw(@_)
  ;
}

=head2 column_info_from_storage

=over

=item Arguments: 1/0 (default: 0)

=item Return Value: 1/0

=back

  __PACKAGE__->column_info_from_storage(1);

Enables the on-demand automatic loading of the above column
metadata from storage as necessary.  This is *deprecated*, and
should not be used.  It will be removed before 1.0.

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.

=cut

1;
