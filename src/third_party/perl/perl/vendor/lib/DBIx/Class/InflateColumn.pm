package DBIx::Class::InflateColumn;

use strict;
use warnings;

use base 'DBIx::Class::Row';
use SQL::Abstract 'is_literal_value';
use namespace::clean;

=head1 NAME

DBIx::Class::InflateColumn - Automatically create references from column data

=head1 SYNOPSIS

  # In your table classes
  __PACKAGE__->inflate_column('column_name', {
    inflate => sub {
      my ($raw_value_from_db, $result_object) = @_;
      ...
    },
    deflate => sub {
      my ($inflated_value_from_user, $result_object) = @_;
      ...
    },
  });

=head1 DESCRIPTION

This component translates column data into references, i.e. "inflating"
the column data. It also "deflates" references into an appropriate format
for the database.

It can be used, for example, to automatically convert to and from
L<DateTime> objects for your date and time fields. There's a
convenience component to actually do that though, try
L<DBIx::Class::InflateColumn::DateTime>.

It will handle all types of references except scalar references. It
will not handle scalar values, these are ignored and thus passed
through to L<SQL::Abstract>. This is to allow setting raw values to
"just work". Scalar references are passed through to the database to
deal with, to allow such settings as C< \'year + 1'> and C< \'DEFAULT' >
to work.

If you want to filter plain scalar values and replace them with
something else, see L<DBIx::Class::FilterColumn>.

=head1 METHODS

=head2 inflate_column

Instruct L<DBIx::Class> to inflate the given column.

In addition to the column name, you must provide C<inflate> and
C<deflate> methods. The C<inflate> method is called when you access
the field, while the C<deflate> method is called when the field needs
to used by the database.

For example, if you have a table C<events> with a timestamp field
named C<insert_time>, you could inflate the column in the
corresponding table class using something like:

    __PACKAGE__->inflate_column('insert_time', {
        inflate => sub {
          my ($insert_time_raw_value, $event_result_object) = @_;
          DateTime->from_epoch( epoch => $insert_time_raw_value );
        },
        deflate => sub {
          my ($insert_time_dt_object, $event_result_object) = @_;
          $insert_time_dt_object->epoch;
        },
    });

The coderefs you set for inflate and deflate are called with two parameters,
the first is the value of the column to be inflated/deflated, the second is
the result object itself.

In this example, calls to an event's C<insert_time> accessor return a
L<DateTime> object. This L<DateTime> object is later "deflated" back
to the integer epoch representation when used in the database layer.
For a much more thorough handling of the above example, please see
L<DBIx::Class::DateTime::Epoch>

=cut

sub inflate_column {
  my ($self, $col, $attrs) = @_;

  my $colinfo = $self->column_info($col);

  $self->throw_exception("InflateColumn can not be used on a column with a declared FilterColumn filter")
    if defined $colinfo->{_filter_info} and $self->isa('DBIx::Class::FilterColumn');

  $self->throw_exception("No such column $col to inflate")
    unless $self->has_column($col);
  $self->throw_exception("inflate_column needs attr hashref")
    unless ref $attrs eq 'HASH';
  $colinfo->{_inflate_info} = $attrs;
  my $acc = $colinfo->{accessor};
  $self->mk_group_accessors('inflated_column' => [ (defined $acc ? $acc : $col), $col]);
  return 1;
}

sub _inflated_column {
  my ($self, $col, $value) = @_;

  return $value if (
    ! defined $value # NULL is NULL is NULL
      or
    is_literal_value($value) #that would be a not-yet-reloaded literal update
  );

  my $info = $self->result_source->column_info($col)
    or $self->throw_exception("No column info for $col");

  return $value unless exists $info->{_inflate_info};

  return (
    $info->{_inflate_info}{inflate}
      ||
    $self->throw_exception("No inflator found for '$col'")
  )->($value, $self);
}

sub _deflated_column {
  my ($self, $col, $value) = @_;

  ## Deflate any refs except for literals, pass through plain values
  return $value if (
    ! length ref $value
      or
    is_literal_value($value)
  );

  my $info = $self->result_source->column_info($col) or
    $self->throw_exception("No column info for $col");

  return $value unless exists $info->{_inflate_info};

  return (
    $info->{_inflate_info}{deflate}
      ||
    $self->throw_exception("No deflator found for '$col'")
  )->($value, $self);
}

=head2 get_inflated_column

  my $val = $obj->get_inflated_column($col);

Fetch a column value in its inflated state.  This is directly
analogous to L<DBIx::Class::Row/get_column> in that it only fetches a
column already retrieved from the database, and then inflates it.
Throws an exception if the column requested is not an inflated column.

=cut

sub get_inflated_column {
  my ($self, $col) = @_;

  $self->throw_exception("$col is not an inflated column")
    unless exists $self->result_source->column_info($col)->{_inflate_info};

  # we take care of keeping things in sync
  return $self->{_inflated_column}{$col}
    if exists $self->{_inflated_column}{$col};

  my $val = $self->get_column($col);

  return $self->{_inflated_column}{$col} = $self->_inflated_column($col, $val);
}

=head2 set_inflated_column

  my $copy = $obj->set_inflated_column($col => $val);

Sets a column value from an inflated value.  This is directly
analogous to L<DBIx::Class::Row/set_column>.

=cut

sub set_inflated_column {
  my ($self, $col, $value) = @_;

  # pass through deflated stuff
  if (! length ref $value or is_literal_value($value)) {
    $self->set_column($col, $value);
    delete $self->{_inflated_column}{$col};
  }
  # need to call set_column with the deflate cycle so that
  # relationship caches are nuked if any
  # also does the compare-for-dirtyness and change tracking dance
  else {
    $self->set_column($col, $self->_deflated_column($col, $value));
    $self->{_inflated_column}{$col} = $value;
  }

  return $value;
}

=head2 store_inflated_column

  my $copy = $obj->store_inflated_column($col => $val);

Sets a column value from an inflated value without marking the column
as dirty. This is directly analogous to L<DBIx::Class::Row/store_column>.

=cut

sub store_inflated_column {
  my ($self, $col, $value) = @_;

  if (! length ref $value or is_literal_value($value)) {
    delete $self->{_inflated_column}{$col};
    $self->store_column($col => $value);
  }
  else {
    delete $self->{_column_data}{$col};
    $self->{_inflated_column}{$col} = $value;
  }

  return $value;
}

=head1 SEE ALSO

=over 4

=item L<DBIx::Class::Core> - This component is loaded as part of the
      C<core> L<DBIx::Class> components; generally there is no need to
      load it directly

=back

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.

=cut

1;
