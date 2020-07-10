package DBIx::Class::Row;

use strict;
use warnings;

use base qw/DBIx::Class/;

use Scalar::Util 'blessed';
use List::Util 'first';
use Try::Tiny;
use DBIx::Class::Carp;
use SQL::Abstract 'is_literal_value';

###
### Internal method
### Do not use
###
BEGIN {
  *MULTICREATE_DEBUG =
    $ENV{DBIC_MULTICREATE_DEBUG}
      ? sub () { 1 }
      : sub () { 0 };
}

use namespace::clean;

__PACKAGE__->mk_group_accessors ( simple => [ in_storage => '_in_storage' ] );

=head1 NAME

DBIx::Class::Row - Basic row methods

=head1 SYNOPSIS

=head1 DESCRIPTION

This class is responsible for defining and doing basic operations on rows
derived from L<DBIx::Class::ResultSource> objects.

Result objects are returned from L<DBIx::Class::ResultSet>s using the
L<create|DBIx::Class::ResultSet/create>, L<find|DBIx::Class::ResultSet/find>,
L<next|DBIx::Class::ResultSet/next> and L<all|DBIx::Class::ResultSet/all> methods,
as well as invocations of 'single' (
L<belongs_to|DBIx::Class::Relationship/belongs_to>,
L<has_one|DBIx::Class::Relationship/has_one> or
L<might_have|DBIx::Class::Relationship/might_have>)
relationship accessors of L<Result|DBIx::Class::Manual::ResultClass> objects.

=head1 NOTE

All "Row objects" derived from a Schema-attached L<DBIx::Class::ResultSet>
object (such as a typical C<< L<search|DBIx::Class::ResultSet/search>->
L<next|DBIx::Class::ResultSet/next> >> call) are actually Result
instances, based on your application's
L<Result Class|DBIx::Class::Manual::Glossary/Result Class>.

L<DBIx::Class::Row> implements most of the row-based communication with the
underlying storage, but a Result class B<should not inherit from it directly>.
Usually, Result classes inherit from L<DBIx::Class::Core>, which in turn
combines the methods from several classes, one of them being
L<DBIx::Class::Row>.  Therefore, while many of the methods available to a
L<DBIx::Class::Core>-derived Result class are described in the following
documentation, it does not detail all of the methods available to Result
objects.  Refer to L<DBIx::Class::Manual::ResultClass> for more info.

=head1 METHODS

=head2 new

  my $result = My::Class->new(\%attrs);

  my $result = $schema->resultset('MySource')->new(\%colsandvalues);

=over

=item Arguments: \%attrs or \%colsandvalues

=item Return Value: L<$result|DBIx::Class::Manual::ResultClass>

=back

While you can create a new result object by calling C<new> directly on
this class, you are better off calling it on a
L<DBIx::Class::ResultSet> object.

When calling it directly, you will not get a complete, usable row
object until you pass or set the C<result_source> attribute, to a
L<DBIx::Class::ResultSource> instance that is attached to a
L<DBIx::Class::Schema> with a valid connection.

C<$attrs> is a hashref of column name, value data. It can also contain
some other attributes such as the C<result_source>.

Passing an object, or an arrayref of objects as a value will call
L<DBIx::Class::Relationship::Base/set_from_related> for you. When
passed a hashref or an arrayref of hashrefs as the value, these will
be turned into objects via new_related, and treated as if you had
passed objects.

For a more involved explanation, see L<DBIx::Class::ResultSet/create>.

Please note that if a value is not passed to new, no value will be sent
in the SQL INSERT call, and the column will therefore assume whatever
default value was specified in your database. While DBIC will retrieve the
value of autoincrement columns, it will never make an explicit database
trip to retrieve default values assigned by the RDBMS. You can explicitly
request that all values be fetched back from the database by calling
L</discard_changes>, or you can supply an explicit C<undef> to columns
with NULL as the default, and save yourself a SELECT.

 CAVEAT:

 The behavior described above will backfire if you use a foreign key column
 with a database-defined default. If you call the relationship accessor on
 an object that doesn't have a set value for the FK column, DBIC will throw
 an exception, as it has no way of knowing the PK of the related object (if
 there is one).

=cut

## It needs to store the new objects somewhere, and call insert on that list later when insert is called on this object. We may need an accessor for these so the user can retrieve them, if just doing ->new().
## This only works because DBIC doesn't yet care to check whether the new_related objects have been passed all their mandatory columns
## When doing the later insert, we need to make sure the PKs are set.
## using _relationship_data in new and funky ways..
## check Relationship::CascadeActions and Relationship::Accessor for compat
## tests!

sub __new_related_find_or_new_helper {
  my ($self, $rel_name, $values) = @_;

  my $rsrc = $self->result_source;

  # create a mock-object so all new/set_column component overrides will run:
  my $rel_rs = $rsrc->related_source($rel_name)->resultset;
  my $new_rel_obj = $rel_rs->new_result($values);
  my $proc_data = { $new_rel_obj->get_columns };

  if ($self->__their_pk_needs_us($rel_name)) {
    MULTICREATE_DEBUG and print STDERR "MC $self constructing $rel_name via new_result\n";
    return $new_rel_obj;
  }
  elsif ($rsrc->_pk_depends_on($rel_name, $proc_data )) {
    if (! keys %$proc_data) {
      # there is nothing to search for - blind create
      MULTICREATE_DEBUG and print STDERR "MC $self constructing default-insert $rel_name\n";
    }
    else {
      MULTICREATE_DEBUG and print STDERR "MC $self constructing $rel_name via find_or_new\n";
      # this is not *really* find or new, as we don't want to double-new the
      # data (thus potentially double encoding or whatever)
      my $exists = $rel_rs->find ($proc_data);
      return $exists if $exists;
    }
    return $new_rel_obj;
  }
  else {
    my $us = $rsrc->source_name;
    $self->throw_exception (
      "Unable to determine relationship '$rel_name' direction from '$us', "
    . "possibly due to a missing reverse-relationship on '$rel_name' to '$us'."
    );
  }
}

sub __their_pk_needs_us { # this should maybe be in resultsource.
  my ($self, $rel_name) = @_;
  my $rsrc = $self->result_source;
  my $reverse = $rsrc->reverse_relationship_info($rel_name);
  my $rel_source = $rsrc->related_source($rel_name);
  my $us = { $self->get_columns };
  foreach my $key (keys %$reverse) {
    # if their primary key depends on us, then we have to
    # just create a result and we'll fill it out afterwards
    return 1 if $rel_source->_pk_depends_on($key, $us);
  }
  return 0;
}

sub new {
  my ($class, $attrs) = @_;
  $class = ref $class if ref $class;

  my $new = bless { _column_data => {}, _in_storage => 0 }, $class;

  if ($attrs) {
    $new->throw_exception("attrs must be a hashref")
      unless ref($attrs) eq 'HASH';

    my $rsrc = delete $attrs->{-result_source};
    if ( my $h = delete $attrs->{-source_handle} ) {
      $rsrc ||= $h->resolve;
    }

    $new->result_source($rsrc) if $rsrc;

    if (my $col_from_rel = delete $attrs->{-cols_from_relations}) {
      @{$new->{_ignore_at_insert}={}}{@$col_from_rel} = ();
    }

    my ($related,$inflated);

    foreach my $key (keys %$attrs) {
      if (ref $attrs->{$key} and ! is_literal_value($attrs->{$key}) ) {
        ## Can we extract this lot to use with update(_or .. ) ?
        $new->throw_exception("Can't do multi-create without result source")
          unless $rsrc;
        my $info = $rsrc->relationship_info($key);
        my $acc_type = $info->{attrs}{accessor} || '';
        if ($acc_type eq 'single') {
          my $rel_obj = delete $attrs->{$key};
          if(!blessed $rel_obj) {
            $rel_obj = $new->__new_related_find_or_new_helper($key, $rel_obj);
          }

          if ($rel_obj->in_storage) {
            $new->{_rel_in_storage}{$key} = 1;
            $new->set_from_related($key, $rel_obj);
          } else {
            MULTICREATE_DEBUG and print STDERR "MC $new uninserted $key $rel_obj\n";
          }

          $related->{$key} = $rel_obj;
          next;
        }
        elsif ($acc_type eq 'multi' && ref $attrs->{$key} eq 'ARRAY' ) {
          my $others = delete $attrs->{$key};
          my $total = @$others;
          my @objects;
          foreach my $idx (0 .. $#$others) {
            my $rel_obj = $others->[$idx];
            if(!blessed $rel_obj) {
              $rel_obj = $new->__new_related_find_or_new_helper($key, $rel_obj);
            }

            if ($rel_obj->in_storage) {
              $rel_obj->throw_exception ('A multi relationship can not be pre-existing when doing multicreate. Something went wrong');
            } else {
              MULTICREATE_DEBUG and
                print STDERR "MC $new uninserted $key $rel_obj (${\($idx+1)} of $total)\n";
            }
            push(@objects, $rel_obj);
          }
          $related->{$key} = \@objects;
          next;
        }
        elsif ($acc_type eq 'filter') {
          ## 'filter' should disappear and get merged in with 'single' above!
          my $rel_obj = delete $attrs->{$key};
          if(!blessed $rel_obj) {
            $rel_obj = $new->__new_related_find_or_new_helper($key, $rel_obj);
          }
          if ($rel_obj->in_storage) {
            $new->{_rel_in_storage}{$key} = 1;
          }
          else {
            MULTICREATE_DEBUG and print STDERR "MC $new uninserted $key $rel_obj\n";
          }
          $inflated->{$key} = $rel_obj;
          next;
        }
        elsif (
          $rsrc->has_column($key)
            and
          $rsrc->column_info($key)->{_inflate_info}
        ) {
          $inflated->{$key} = $attrs->{$key};
          next;
        }
      }
      $new->store_column($key => $attrs->{$key});
    }

    $new->{_relationship_data} = $related if $related;
    $new->{_inflated_column} = $inflated if $inflated;
  }

  return $new;
}

=head2 $column_accessor

  # Each pair does the same thing

  # (un-inflated, regular column)
  my $val = $result->get_column('first_name');
  my $val = $result->first_name;

  $result->set_column('first_name' => $val);
  $result->first_name($val);

  # (inflated column via DBIx::Class::InflateColumn::DateTime)
  my $val = $result->get_inflated_column('last_modified');
  my $val = $result->last_modified;

  $result->set_inflated_column('last_modified' => $val);
  $result->last_modified($val);

=over

=item Arguments: $value?

=item Return Value: $value

=back

A column accessor method is created for each column, which is used for
getting/setting the value for that column.

The actual method name is based on the
L<accessor|DBIx::Class::ResultSource/accessor> name given during the
L<Result Class|DBIx::Class::Manual::ResultClass> L<column definition
|DBIx::Class::ResultSource/add_columns>. Like L</set_column>, this
will not store the data in the database until L</insert> or L</update>
is called on the row.

=head2 insert

  $result->insert;

=over

=item Arguments: none

=item Return Value: L<$result|DBIx::Class::Manual::ResultClass>

=back

Inserts an object previously created by L</new> into the database if
it isn't already in there. Returns the object itself. To insert an
entirely new row into the database, use L<DBIx::Class::ResultSet/create>.

To fetch an uninserted result object, call
L<new_result|DBIx::Class::ResultSet/new_result> on a resultset.

This will also insert any uninserted, related objects held inside this
one, see L<DBIx::Class::ResultSet/create> for more details.

=cut

sub insert {
  my ($self) = @_;
  return $self if $self->in_storage;
  my $rsrc = $self->result_source;
  $self->throw_exception("No result_source set on this object; can't insert")
    unless $rsrc;

  my $storage = $rsrc->storage;

  my $rollback_guard;

  # Check if we stored uninserted relobjs here in new()
  my %related_stuff = (%{$self->{_relationship_data} || {}},
                       %{$self->{_inflated_column} || {}});

  # insert what needs to be inserted before us
  my %pre_insert;
  for my $rel_name (keys %related_stuff) {
    my $rel_obj = $related_stuff{$rel_name};

    if (! $self->{_rel_in_storage}{$rel_name}) {
      next unless (blessed $rel_obj && $rel_obj->isa('DBIx::Class::Row'));

      next unless $rsrc->_pk_depends_on(
                    $rel_name, { $rel_obj->get_columns }
                  );

      # The guard will save us if we blow out of this scope via die
      $rollback_guard ||= $storage->txn_scope_guard;

      MULTICREATE_DEBUG and print STDERR "MC $self pre-reconstructing $rel_name $rel_obj\n";

      my $them = { %{$rel_obj->{_relationship_data} || {} }, $rel_obj->get_columns };
      my $existing;

      # if there are no keys - nothing to search for
      if (keys %$them and $existing = $self->result_source
                                           ->related_source($rel_name)
                                           ->resultset
                                           ->find($them)
      ) {
        %{$rel_obj} = %{$existing};
      }
      else {
        $rel_obj->insert;
      }

      $self->{_rel_in_storage}{$rel_name} = 1;
    }

    $self->set_from_related($rel_name, $rel_obj);
    delete $related_stuff{$rel_name};
  }

  # start a transaction here if not started yet and there is more stuff
  # to insert after us
  if (keys %related_stuff) {
    $rollback_guard ||= $storage->txn_scope_guard
  }

  MULTICREATE_DEBUG and do {
    no warnings 'uninitialized';
    print STDERR "MC $self inserting (".join(', ', $self->get_columns).")\n";
  };

  # perform the insert - the storage will return everything it is asked to
  # (autoinc primary columns and any retrieve_on_insert columns)
  my %current_rowdata = $self->get_columns;
  my $returned_cols = $storage->insert(
    $rsrc,
    { %current_rowdata }, # what to insert, copy because the storage *will* change it
  );

  for (keys %$returned_cols) {
    $self->store_column($_, $returned_cols->{$_})
      # this ensures we fire store_column only once
      # (some asshats like overriding it)
      if (
        (!exists $current_rowdata{$_})
          or
        (defined $current_rowdata{$_} xor defined $returned_cols->{$_})
          or
        (defined $current_rowdata{$_} and $current_rowdata{$_} ne $returned_cols->{$_})
      );
  }

  delete $self->{_column_data_in_storage};
  $self->in_storage(1);

  $self->{_dirty_columns} = {};
  $self->{related_resultsets} = {};

  foreach my $rel_name (keys %related_stuff) {
    next unless $rsrc->has_relationship ($rel_name);

    my @cands = ref $related_stuff{$rel_name} eq 'ARRAY'
      ? @{$related_stuff{$rel_name}}
      : $related_stuff{$rel_name}
    ;

    if (@cands && blessed $cands[0] && $cands[0]->isa('DBIx::Class::Row')
    ) {
      my $reverse = $rsrc->reverse_relationship_info($rel_name);
      foreach my $obj (@cands) {
        $obj->set_from_related($_, $self) for keys %$reverse;
        if ($self->__their_pk_needs_us($rel_name)) {
          if (exists $self->{_ignore_at_insert}{$rel_name}) {
            MULTICREATE_DEBUG and print STDERR "MC $self skipping post-insert on $rel_name\n";
          }
          else {
            MULTICREATE_DEBUG and print STDERR "MC $self inserting $rel_name $obj\n";
            $obj->insert;
          }
        } else {
          MULTICREATE_DEBUG and print STDERR "MC $self post-inserting $obj\n";
          $obj->insert();
        }
      }
    }
  }

  delete $self->{_ignore_at_insert};

  $rollback_guard->commit if $rollback_guard;

  return $self;
}

=head2 in_storage

  $result->in_storage; # Get value
  $result->in_storage(1); # Set value

=over

=item Arguments: none or 1|0

=item Return Value: 1|0

=back

Indicates whether the object exists as a row in the database or
not. This is set to true when L<DBIx::Class::ResultSet/find>,
L<DBIx::Class::ResultSet/create> or L<DBIx::Class::Row/insert>
are invoked.

Creating a result object using L<DBIx::Class::ResultSet/new_result>, or
calling L</delete> on one, sets it to false.


=head2 update

  $result->update(\%columns?)

=over

=item Arguments: none or a hashref

=item Return Value: L<$result|DBIx::Class::Manual::ResultClass>

=back

Throws an exception if the result object is not yet in the database,
according to L</in_storage>. Returns the object itself.

This method issues an SQL UPDATE query to commit any changes to the
object to the database if required (see L</get_dirty_columns>).
It throws an exception if a proper WHERE clause uniquely identifying
the database row can not be constructed (see
L<significance of primary keys|DBIx::Class::Manual::Intro/The Significance and Importance of Primary Keys>
for more details).

Also takes an optional hashref of C<< column_name => value >> pairs
to update on the object first. Be aware that the hashref will be
passed to C<set_inflated_columns>, which might edit it in place, so
don't rely on it being the same after a call to C<update>.  If you
need to preserve the hashref, it is sufficient to pass a shallow copy
to C<update>, e.g. ( { %{ $href } } )

If the values passed or any of the column values set on the object
contain scalar references, e.g.:

  $result->last_modified(\'NOW()')->update();
  # OR
  $result->update({ last_modified => \'NOW()' });

The update will pass the values verbatim into SQL. (See
L<SQL::Abstract> docs).  The values in your Result object will NOT change
as a result of the update call, if you want the object to be updated
with the actual values from the database, call L</discard_changes>
after the update.

  $result->update()->discard_changes();

To determine before calling this method, which column values have
changed and will be updated, call L</get_dirty_columns>.

To check if any columns will be updated, call L</is_changed>.

To force a column to be updated, call L</make_column_dirty> before
this method.

=cut

sub update {
  my ($self, $upd) = @_;

  $self->set_inflated_columns($upd) if $upd;

  my %to_update = $self->get_dirty_columns
    or return $self;

  $self->throw_exception( "Not in database" ) unless $self->in_storage;

  my $rows = $self->result_source->storage->update(
    $self->result_source, \%to_update, $self->_storage_ident_condition
  );
  if ($rows == 0) {
    $self->throw_exception( "Can't update ${self}: row not found" );
  } elsif ($rows > 1) {
    $self->throw_exception("Can't update ${self}: updated more than one row");
  }
  $self->{_dirty_columns} = {};
  $self->{related_resultsets} = {};
  delete $self->{_column_data_in_storage};
  return $self;
}

=head2 delete

  $result->delete

=over

=item Arguments: none

=item Return Value: L<$result|DBIx::Class::Manual::ResultClass>

=back

Throws an exception if the object is not in the database according to
L</in_storage>. Also throws an exception if a proper WHERE clause
uniquely identifying the database row can not be constructed (see
L<significance of primary keys|DBIx::Class::Manual::Intro/The Significance and Importance of Primary Keys>
for more details).

The object is still perfectly usable, but L</in_storage> will
now return 0 and the object must be reinserted using L</insert>
before it can be used to L</update> the row again.

If you delete an object in a class with a C<has_many> relationship, an
attempt is made to delete all the related objects as well. To turn
this behaviour off, pass C<< cascade_delete => 0 >> in the C<$attr>
hashref of the relationship, see L<DBIx::Class::Relationship>. Any
database-level cascade or restrict will take precedence over a
DBIx-Class-based cascading delete, since DBIx-Class B<deletes the
main row first> and only then attempts to delete any remaining related
rows.

If you delete an object within a txn_do() (see L<DBIx::Class::Storage/txn_do>)
and the transaction subsequently fails, the result object will remain marked as
not being in storage. If you know for a fact that the object is still in
storage (i.e. by inspecting the cause of the transaction's failure), you can
use C<< $obj->in_storage(1) >> to restore consistency between the object and
the database. This would allow a subsequent C<< $obj->delete >> to work
as expected.

See also L<DBIx::Class::ResultSet/delete>.

=cut

sub delete {
  my $self = shift;
  if (ref $self) {
    $self->throw_exception( "Not in database" ) unless $self->in_storage;

    $self->result_source->storage->delete(
      $self->result_source, $self->_storage_ident_condition
    );

    delete $self->{_column_data_in_storage};
    $self->in_storage(0);
  }
  else {
    my $rsrc = try { $self->result_source_instance }
      or $self->throw_exception("Can't do class delete without a ResultSource instance");

    my $attrs = @_ > 1 && ref $_[$#_] eq 'HASH' ? { %{pop(@_)} } : {};
    my $query = ref $_[0] eq 'HASH' ? $_[0] : {@_};
    $rsrc->resultset->search(@_)->delete;
  }
  return $self;
}

=head2 get_column

  my $val = $result->get_column($col);

=over

=item Arguments: $columnname

=item Return Value: The value of the column

=back

Throws an exception if the column name given doesn't exist according
to L<has_column|DBIx::Class::ResultSource/has_column>.

Returns a raw column value from the result object, if it has already
been fetched from the database or set by an accessor.

If an L<inflated value|DBIx::Class::InflateColumn> has been set, it
will be deflated and returned.

Note that if you used the C<columns> or the C<select/as>
L<search attributes|DBIx::Class::ResultSet/ATTRIBUTES> on the resultset from
which C<$result> was derived, and B<did not include> C<$columnname> in the list,
this method will return C<undef> even if the database contains some value.

To retrieve all loaded column values as a hash, use L</get_columns>.

=cut

sub get_column {
  my ($self, $column) = @_;
  $self->throw_exception( "Can't fetch data as class method" ) unless ref $self;

  return $self->{_column_data}{$column}
    if exists $self->{_column_data}{$column};

  if (exists $self->{_inflated_column}{$column}) {
    # deflate+return cycle
    return $self->store_column($column, $self->_deflated_column(
      $column, $self->{_inflated_column}{$column}
    ));
  }

  $self->throw_exception( "No such column '${column}' on " . ref $self )
    unless $self->result_source->has_column($column);

  return undef;
}

=head2 has_column_loaded

  if ( $result->has_column_loaded($col) ) {
     print "$col has been loaded from db";
  }

=over

=item Arguments: $columnname

=item Return Value: 0|1

=back

Returns a true value if the column value has been loaded from the
database (or set locally).

=cut

sub has_column_loaded {
  my ($self, $column) = @_;
  $self->throw_exception( "Can't call has_column data as class method" ) unless ref $self;

  return (
    exists $self->{_inflated_column}{$column}
      or
    exists $self->{_column_data}{$column}
  ) ? 1 : 0;
}

=head2 get_columns

  my %data = $result->get_columns;

=over

=item Arguments: none

=item Return Value: A hash of columnname, value pairs.

=back

Returns all loaded column data as a hash, containing raw values. To
get just one value for a particular column, use L</get_column>.

See L</get_inflated_columns> to get the inflated values.

=cut

sub get_columns {
  my $self = shift;
  if (exists $self->{_inflated_column}) {
    # deflate cycle for each inflation, including filter rels
    foreach my $col (keys %{$self->{_inflated_column}}) {
      unless (exists $self->{_column_data}{$col}) {

        # if cached related_resultset is present assume this was a prefetch
        carp_unique(
          "Returning primary keys of prefetched 'filter' rels as part of get_columns() is deprecated and will "
        . 'eventually be removed entirely (set DBIC_COLUMNS_INCLUDE_FILTER_RELS to disable this warning)'
        ) if (
          ! $ENV{DBIC_COLUMNS_INCLUDE_FILTER_RELS}
            and
          defined $self->{related_resultsets}{$col}
            and
          defined $self->{related_resultsets}{$col}->get_cache
        );

        $self->store_column($col, $self->_deflated_column($col, $self->{_inflated_column}{$col}));
      }
    }
  }
  return %{$self->{_column_data}};
}

=head2 get_dirty_columns

  my %data = $result->get_dirty_columns;

=over

=item Arguments: none

=item Return Value: A hash of column, value pairs

=back

Only returns the column, value pairs for those columns that have been
changed on this object since the last L</update> or L</insert> call.

See L</get_columns> to fetch all column/value pairs.

=cut

sub get_dirty_columns {
  my $self = shift;
  return map { $_ => $self->{_column_data}{$_} }
           keys %{$self->{_dirty_columns}};
}

=head2 make_column_dirty

  $result->make_column_dirty($col)

=over

=item Arguments: $columnname

=item Return Value: not defined

=back

Throws an exception if the column does not exist.

Marks a column as having been changed regardless of whether it has
really changed.

=cut

sub make_column_dirty {
  my ($self, $column) = @_;

  $self->throw_exception( "No such column '${column}' on " . ref $self )
    unless exists $self->{_column_data}{$column} || $self->result_source->has_column($column);

  # the entire clean/dirty code relies on exists, not on true/false
  return 1 if exists $self->{_dirty_columns}{$column};

  $self->{_dirty_columns}{$column} = 1;

  # if we are just now making the column dirty, and if there is an inflated
  # value, force it over the deflated one
  if (exists $self->{_inflated_column}{$column}) {
    $self->store_column($column,
      $self->_deflated_column(
        $column, $self->{_inflated_column}{$column}
      )
    );
  }
}

=head2 get_inflated_columns

  my %inflated_data = $obj->get_inflated_columns;

=over

=item Arguments: none

=item Return Value: A hash of column, object|value pairs

=back

Returns a hash of all column keys and associated values. Values for any
columns set to use inflation will be inflated and returns as objects.

See L</get_columns> to get the uninflated values.

See L<DBIx::Class::InflateColumn> for how to setup inflation.

=cut

sub get_inflated_columns {
  my $self = shift;

  my $loaded_colinfo = $self->result_source->columns_info;
  $self->has_column_loaded($_) or delete $loaded_colinfo->{$_}
    for keys %$loaded_colinfo;

  my %cols_to_return = ( %{$self->{_column_data}}, %$loaded_colinfo );

  unless ($ENV{DBIC_COLUMNS_INCLUDE_FILTER_RELS}) {
    for (keys %$loaded_colinfo) {
      # if cached related_resultset is present assume this was a prefetch
      if (
        $loaded_colinfo->{$_}{_inflate_info}
          and
        defined $self->{related_resultsets}{$_}
          and
        defined $self->{related_resultsets}{$_}->get_cache
      ) {
        carp_unique(
          "Returning prefetched 'filter' rels as part of get_inflated_columns() is deprecated and will "
        . 'eventually be removed entirely (set DBIC_COLUMNS_INCLUDE_FILTER_RELS to disable this warning)'
        );
        last;
      }
    }
  }

  map { $_ => (
  (
    ! exists $loaded_colinfo->{$_}
      or
    (
      exists $loaded_colinfo->{$_}{accessor}
        and
      ! defined $loaded_colinfo->{$_}{accessor}
    )
  ) ? $self->get_column($_)
    : $self->${ \(
      defined $loaded_colinfo->{$_}{accessor}
        ? $loaded_colinfo->{$_}{accessor}
        : $_
      )}
  )} keys %cols_to_return;
}

sub _is_column_numeric {
    my ($self, $column) = @_;

    return undef unless $self->result_source->has_column($column);

    my $colinfo = $self->result_source->column_info ($column);

    # cache for speed (the object may *not* have a resultsource instance)
    if (
      ! defined $colinfo->{is_numeric}
        and
      my $storage = try { $self->result_source->schema->storage }
    ) {
      $colinfo->{is_numeric} =
        $storage->is_datatype_numeric ($colinfo->{data_type})
          ? 1
          : 0
        ;
    }

    return $colinfo->{is_numeric};
}

=head2 set_column

  $result->set_column($col => $val);

=over

=item Arguments: $columnname, $value

=item Return Value: $value

=back

Sets a raw column value. If the new value is different from the old one,
the column is marked as dirty for when you next call L</update>.

If passed an object or reference as a value, this method will happily
attempt to store it, and a later L</insert> or L</update> will try and
stringify/numify as appropriate. To set an object to be deflated
instead, see L</set_inflated_columns>, or better yet, use L</$column_accessor>.

=cut

sub set_column {
  my ($self, $column, $new_value) = @_;

  my $had_value = $self->has_column_loaded($column);
  my $old_value = $self->get_column($column);

  $new_value = $self->store_column($column, $new_value);

  my $dirty =
    $self->{_dirty_columns}{$column}
      ||
    ( $self->in_storage # no point tracking dirtyness on uninserted data
      ? ! $self->_eq_column_values ($column, $old_value, $new_value)
      : 1
    )
  ;

  if ($dirty) {
    # FIXME sadly the update code just checks for keys, not for their value
    $self->{_dirty_columns}{$column} = 1;

    # Clear out the relation/inflation cache related to this column
    #
    # FIXME - this is a quick *largely incorrect* hack, pending a more
    # serious rework during the merge of single and filter rels
    my $rel_names = $self->result_source->{_relationships};
    for my $rel_name (keys %$rel_names) {

      my $acc = $rel_names->{$rel_name}{attrs}{accessor} || '';

      if ( $acc eq 'single' and $rel_names->{$rel_name}{attrs}{fk_columns}{$column} ) {
        delete $self->{related_resultsets}{$rel_name};
        delete $self->{_relationship_data}{$rel_name};
        #delete $self->{_inflated_column}{$rel_name};
      }
      elsif ( $acc eq 'filter' and $rel_name eq $column) {
        delete $self->{related_resultsets}{$rel_name};
        #delete $self->{_relationship_data}{$rel_name};
        delete $self->{_inflated_column}{$rel_name};
      }
    }

    if (
      # value change from something (even if NULL)
      $had_value
        and
      # no storage - no storage-value
      $self->in_storage
        and
      # no value already stored (multiple changes before commit to storage)
      ! exists $self->{_column_data_in_storage}{$column}
        and
      $self->_track_storage_value($column)
    ) {
      $self->{_column_data_in_storage}{$column} = $old_value;
    }
  }

  return $new_value;
}

sub _eq_column_values {
  my ($self, $col, $old, $new) = @_;

  if (defined $old xor defined $new) {
    return 0;
  }
  elsif (not defined $old) {  # both undef
    return 1;
  }
  elsif (
    is_literal_value $old
      or
    is_literal_value $new
  ) {
    return 0;
  }
  elsif ($old eq $new) {
    return 1;
  }
  elsif ($self->_is_column_numeric($col)) {  # do a numeric comparison if datatype allows it
    return $old == $new;
  }
  else {
    return 0;
  }
}

# returns a boolean indicating if the passed column should have its original
# value tracked between column changes and commitment to storage
sub _track_storage_value {
  my ($self, $col) = @_;
  return defined first { $col eq $_ } ($self->result_source->primary_columns);
}

=head2 set_columns

  $result->set_columns({ $col => $val, ... });

=over

=item Arguments: \%columndata

=item Return Value: L<$result|DBIx::Class::Manual::ResultClass>

=back

Sets multiple column, raw value pairs at once.

Works as L</set_column>.

=cut

sub set_columns {
  my ($self, $values) = @_;
  $self->set_column( $_, $values->{$_} ) for keys %$values;
  return $self;
}

=head2 set_inflated_columns

  $result->set_inflated_columns({ $col => $val, $rel_name => $obj, ... });

=over

=item Arguments: \%columndata

=item Return Value: L<$result|DBIx::Class::Manual::ResultClass>

=back

Sets more than one column value at once. Any inflated values are
deflated and the raw values stored.

Any related values passed as Result objects, using the relation name as a
key, are reduced to the appropriate foreign key values and stored. If
instead of related result objects, a hashref of column, value data is
passed, will create the related object first then store.

Will even accept arrayrefs of data as a value to a
L<DBIx::Class::Relationship/has_many> key, and create the related
objects if necessary.

Be aware that the input hashref might be edited in place, so don't rely
on it being the same after a call to C<set_inflated_columns>. If you
need to preserve the hashref, it is sufficient to pass a shallow copy
to C<set_inflated_columns>, e.g. ( { %{ $href } } )

See also L<DBIx::Class::Relationship::Base/set_from_related>.

=cut

sub set_inflated_columns {
  my ( $self, $upd ) = @_;
  my $rsrc;
  foreach my $key (keys %$upd) {
    if (ref $upd->{$key}) {
      $rsrc ||= $self->result_source;
      my $info = $rsrc->relationship_info($key);
      my $acc_type = $info->{attrs}{accessor} || '';

      if ($acc_type eq 'single') {
        my $rel_obj = delete $upd->{$key};
        $self->set_from_related($key => $rel_obj);
        $self->{_relationship_data}{$key} = $rel_obj;
      }
      elsif ($acc_type eq 'multi') {
        $self->throw_exception(
          "Recursive update is not supported over relationships of type '$acc_type' ($key)"
        );
      }
      elsif (
        $rsrc->has_column($key)
          and
        exists $rsrc->column_info($key)->{_inflate_info}
      ) {
        $self->set_inflated_column($key, delete $upd->{$key});
      }
    }
  }
  $self->set_columns($upd);
}

=head2 copy

  my $copy = $orig->copy({ change => $to, ... });

=over

=item Arguments: \%replacementdata

=item Return Value: L<$result|DBIx::Class::Manual::ResultClass> copy

=back

Inserts a new row into the database, as a copy of the original
object. If a hashref of replacement data is supplied, these will take
precedence over data in the original. Also any columns which have
the L<column info attribute|DBIx::Class::ResultSource/add_columns>
C<< is_auto_increment => 1 >> are explicitly removed before the copy,
so that the database can insert its own autoincremented values into
the new object.

Relationships will be followed by the copy procedure B<only> if the
relationship specifies a true value for its
L<cascade_copy|DBIx::Class::Relationship::Base> attribute. C<cascade_copy>
is set by default on C<has_many> relationships and unset on all others.

=cut

sub copy {
  my ($self, $changes) = @_;
  $changes ||= {};
  my $col_data = { $self->get_columns };

  my $rsrc = $self->result_source;

  my $colinfo = $rsrc->columns_info;
  foreach my $col (keys %$col_data) {
    delete $col_data->{$col}
      if ( ! $colinfo->{$col} or $colinfo->{$col}{is_auto_increment} );
  }

  my $new = { _column_data => $col_data };
  bless $new, ref $self;

  $new->result_source($rsrc);
  $new->set_inflated_columns($changes);
  $new->insert;

  # Its possible we'll have 2 relations to the same Source. We need to make
  # sure we don't try to insert the same row twice else we'll violate unique
  # constraints
  my $rel_names_copied = {};

  foreach my $rel_name ($rsrc->relationships) {
    my $rel_info = $rsrc->relationship_info($rel_name);

    next unless $rel_info->{attrs}{cascade_copy};

    my $resolved = $rsrc->_resolve_condition(
      $rel_info->{cond}, $rel_name, $new, $rel_name
    );

    my $copied = $rel_names_copied->{ $rel_info->{source} } ||= {};
    foreach my $related ($self->search_related($rel_name)->all) {
      $related->copy($resolved)
        unless $copied->{$related->ID}++;
    }

  }
  return $new;
}

=head2 store_column

  $result->store_column($col => $val);

=over

=item Arguments: $columnname, $value

=item Return Value: The value sent to storage

=back

Set a raw value for a column without marking it as changed. This
method is used internally by L</set_column> which you should probably
be using.

This is the lowest level at which data is set on a result object,
extend this method to catch all data setting methods.

=cut

sub store_column {
  my ($self, $column, $value) = @_;
  $self->throw_exception( "No such column '${column}' on " . ref $self )
    unless exists $self->{_column_data}{$column} || $self->result_source->has_column($column);
  $self->throw_exception( "set_column called for ${column} without value" )
    if @_ < 3;
  return $self->{_column_data}{$column} = $value;
}

=head2 inflate_result

  Class->inflate_result($result_source, \%me, \%prefetch?)

=over

=item Arguments: L<$result_source|DBIx::Class::ResultSource>, \%columndata, \%prefetcheddata

=item Return Value: L<$result|DBIx::Class::Manual::ResultClass>

=back

All L<DBIx::Class::ResultSet> methods that retrieve data from the
database and turn it into result objects call this method.

Extend this method in your Result classes to hook into this process,
for example to rebless the result into a different class.

Reblessing can also be done more easily by setting C<result_class> in
your Result class. See L<DBIx::Class::ResultSource/result_class>.

Different types of results can also be created from a particular
L<DBIx::Class::ResultSet>, see L<DBIx::Class::ResultSet/result_class>.

=cut

sub inflate_result {
  my ($class, $rsrc, $me, $prefetch) = @_;

  my $new = bless
    { _column_data => $me, _result_source => $rsrc },
    ref $class || $class
  ;

  if ($prefetch) {
    for my $rel_name ( keys %$prefetch ) {

      my $relinfo = $rsrc->relationship_info($rel_name) or do {
        my $err = sprintf
          "Inflation into non-existent relationship '%s' of '%s' requested",
          $rel_name,
          $rsrc->source_name,
        ;
        if (my ($colname) = sort { length($a) <=> length ($b) } keys %{$prefetch->{$rel_name}[0] || {}} ) {
          $err .= sprintf ", check the inflation specification (columns/as) ending in '...%s.%s'",
          $rel_name,
          $colname,
        }

        $rsrc->throw_exception($err);
      };

      $class->throw_exception("No accessor type declared for prefetched relationship '$rel_name'")
        unless $relinfo->{attrs}{accessor};

      my $rel_rs = $new->related_resultset($rel_name);

      my @rel_objects;
      if (
        @{ $prefetch->{$rel_name} || [] }
          and
        ref($prefetch->{$rel_name}) ne $DBIx::Class::ResultSource::RowParser::Util::null_branch_class
      ) {

        if (ref $prefetch->{$rel_name}[0] eq 'ARRAY') {
          my $rel_rsrc = $rel_rs->result_source;
          my $rel_class = $rel_rs->result_class;
          my $rel_inflator = $rel_class->can('inflate_result');
          @rel_objects = map
            { $rel_class->$rel_inflator ( $rel_rsrc, @$_ ) }
            @{$prefetch->{$rel_name}}
          ;
        }
        else {
          @rel_objects = $rel_rs->result_class->inflate_result(
            $rel_rs->result_source, @{$prefetch->{$rel_name}}
          );
        }
      }

      if ($relinfo->{attrs}{accessor} eq 'single') {
        $new->{_relationship_data}{$rel_name} = $rel_objects[0];
      }
      elsif ($relinfo->{attrs}{accessor} eq 'filter') {
        $new->{_inflated_column}{$rel_name} = $rel_objects[0];
      }

      $rel_rs->set_cache(\@rel_objects);
    }
  }

  $new->in_storage (1);
  return $new;
}

=head2 update_or_insert

  $result->update_or_insert

=over

=item Arguments: none

=item Return Value: Result of update or insert operation

=back

L</update>s the object if it's already in the database, according to
L</in_storage>, else L</insert>s it.

=head2 insert_or_update

  $obj->insert_or_update

Alias for L</update_or_insert>

=cut

sub insert_or_update { shift->update_or_insert(@_) }

sub update_or_insert {
  my $self = shift;
  return ($self->in_storage ? $self->update : $self->insert);
}

=head2 is_changed

  my @changed_col_names = $result->is_changed();
  if ($result->is_changed()) { ... }

=over

=item Arguments: none

=item Return Value: 0|1 or @columnnames

=back

In list context returns a list of columns with uncommited changes, or
in scalar context returns a true value if there are uncommitted
changes.

=cut

sub is_changed {
  return keys %{shift->{_dirty_columns} || {}};
}

=head2 is_column_changed

  if ($result->is_column_changed('col')) { ... }

=over

=item Arguments: $columname

=item Return Value: 0|1

=back

Returns a true value if the column has uncommitted changes.

=cut

sub is_column_changed {
  my( $self, $col ) = @_;
  return exists $self->{_dirty_columns}->{$col};
}

=head2 result_source

  my $resultsource = $result->result_source;

=over

=item Arguments: L<$result_source?|DBIx::Class::ResultSource>

=item Return Value: L<$result_source|DBIx::Class::ResultSource>

=back

Accessor to the L<DBIx::Class::ResultSource> this object was created from.

=cut

sub result_source {
  $_[0]->throw_exception( 'result_source can be called on instances only' )
    unless ref $_[0];

  @_ > 1
    ? $_[0]->{_result_source} = $_[1]

    # note this is a || not a ||=, the difference is important
    : $_[0]->{_result_source} || do {
        my $class = ref $_[0];
        $_[0]->can('result_source_instance')
          ? $_[0]->result_source_instance
          : $_[0]->throw_exception(
            "No result source instance registered for $class, did you forget to call $class->table(...) ?"
          )
      }
  ;
}

=head2 register_column

  $column_info = { .... };
  $class->register_column($column_name, $column_info);

=over

=item Arguments: $columnname, \%columninfo

=item Return Value: not defined

=back

Registers a column on the class. If the column_info has an 'accessor'
key, creates an accessor named after the value if defined; if there is
no such key, creates an accessor with the same name as the column

The column_info attributes are described in
L<DBIx::Class::ResultSource/add_columns>

=cut

sub register_column {
  my ($class, $col, $info) = @_;
  my $acc = $col;
  if (exists $info->{accessor}) {
    return unless defined $info->{accessor};
    $acc = [ $info->{accessor}, $col ];
  }
  $class->mk_group_accessors('column' => $acc);
}

=head2 get_from_storage

  my $copy = $result->get_from_storage($attrs)

=over

=item Arguments: \%attrs

=item Return Value: A Result object

=back

Fetches a fresh copy of the Result object from the database and returns it.
Throws an exception if a proper WHERE clause identifying the database row
can not be constructed (i.e. if the original object does not contain its
entire
 L<primary key|DBIx::Class::Manual::Intro/The Significance and Importance of Primary Keys>
). If passed the \%attrs argument, will first apply these attributes to
the resultset used to find the row.

This copy can then be used to compare to an existing result object, to
determine if any changes have been made in the database since it was
created.

To just update your Result object with any latest changes from the
database, use L</discard_changes> instead.

The \%attrs argument should be compatible with
L<DBIx::Class::ResultSet/ATTRIBUTES>.

=cut

sub get_from_storage {
    my $self = shift @_;
    my $attrs = shift @_;
    my $resultset = $self->result_source->resultset;

    if(defined $attrs) {
      $resultset = $resultset->search(undef, $attrs);
    }

    return $resultset->find($self->_storage_ident_condition);
}

=head2 discard_changes

  $result->discard_changes

=over

=item Arguments: none or $attrs

=item Return Value: self (updates object in-place)

=back

Re-selects the row from the database, losing any changes that had
been made. Throws an exception if a proper C<WHERE> clause identifying
the database row can not be constructed (i.e. if the original object
does not contain its entire
L<primary key|DBIx::Class::Manual::Intro/The Significance and Importance of Primary Keys>).

This method can also be used to refresh from storage, retrieving any
changes made since the row was last read from storage.

$attrs, if supplied, is expected to be a hashref of attributes suitable for passing as the
second argument to C<< $resultset->search($cond, $attrs) >>;

Note: If you are using L<DBIx::Class::Storage::DBI::Replicated> as your
storage, a default of
L<< C<< { force_pool => 'master' } >>
|DBIx::Class::Storage::DBI::Replicated/SYNOPSIS >>  is automatically set for
you. Prior to C<< DBIx::Class 0.08109 >> (before 2010) one would have been
required to explicitly wrap the entire operation in a transaction to guarantee
that up-to-date results are read from the master database.

=cut

sub discard_changes {
  my ($self, $attrs) = @_;
  return unless $self->in_storage; # Don't reload if we aren't real!

  # add a replication default to read from the master only
  $attrs = { force_pool => 'master', %{$attrs||{}} };

  if( my $current_storage = $self->get_from_storage($attrs)) {

    # Set $self to the current.
    %$self = %$current_storage;

    # Avoid a possible infinite loop with
    # sub DESTROY { $_[0]->discard_changes }
    bless $current_storage, 'Do::Not::Exist';

    return $self;
  }
  else {
    $self->in_storage(0);
    return $self;
  }
}

=head2 throw_exception

See L<DBIx::Class::Schema/throw_exception>.

=cut

sub throw_exception {
  my $self=shift;

  if (ref $self && ref (my $rsrc = try { $self->result_source_instance } ) ) {
    $rsrc->throw_exception(@_)
  }
  else {
    DBIx::Class::Exception->throw(@_);
  }
}

=head2 id

  my @pk = $result->id;

=over

=item Arguments: none

=item Returns: A list of primary key values

=back

Returns the primary key(s) for a row. Can't be called as a class method.
Actually implemented in L<DBIx::Class::PK>

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.

=cut

1;
