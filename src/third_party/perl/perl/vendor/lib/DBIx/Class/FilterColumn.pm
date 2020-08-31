package DBIx::Class::FilterColumn;
use strict;
use warnings;

use base 'DBIx::Class::Row';
use SQL::Abstract 'is_literal_value';
use namespace::clean;

sub filter_column {
  my ($self, $col, $attrs) = @_;

  my $colinfo = $self->column_info($col);

  $self->throw_exception("FilterColumn can not be used on a column with a declared InflateColumn inflator")
    if defined $colinfo->{_inflate_info} and $self->isa('DBIx::Class::InflateColumn');

  $self->throw_exception("No such column $col to filter")
    unless $self->has_column($col);

  $self->throw_exception('filter_column expects a hashref of filter specifications')
    unless ref $attrs eq 'HASH';

  $self->throw_exception('An invocation of filter_column() must specify either a filter_from_storage or filter_to_storage')
    unless $attrs->{filter_from_storage} || $attrs->{filter_to_storage};

  $colinfo->{_filter_info} = $attrs;
  my $acc = $colinfo->{accessor};
  $self->mk_group_accessors(filtered_column => [ (defined $acc ? $acc : $col), $col]);
  return 1;
}

sub _column_from_storage {
  my ($self, $col, $value) = @_;

  return $value if is_literal_value($value);

  my $info = $self->result_source->column_info($col)
    or $self->throw_exception("No column info for $col");

  return $value unless exists $info->{_filter_info};

  my $filter = $info->{_filter_info}{filter_from_storage};

  return defined $filter ? $self->$filter($value) : $value;
}

sub _column_to_storage {
  my ($self, $col, $value) = @_;

  return $value if is_literal_value($value);

  my $info = $self->result_source->column_info($col) or
    $self->throw_exception("No column info for $col");

  return $value unless exists $info->{_filter_info};

  my $unfilter = $info->{_filter_info}{filter_to_storage};

  return defined $unfilter ? $self->$unfilter($value) : $value;
}

sub get_filtered_column {
  my ($self, $col) = @_;

  $self->throw_exception("$col is not a filtered column")
    unless exists $self->result_source->column_info($col)->{_filter_info};

  return $self->{_filtered_column}{$col}
    if exists $self->{_filtered_column}{$col};

  my $val = $self->get_column($col);

  return $self->{_filtered_column}{$col} = $self->_column_from_storage(
    $col, $val
  );
}

sub get_column {
  my ($self, $col) = @_;

  ! exists $self->{_column_data}{$col}
    and
  exists $self->{_filtered_column}{$col}
    and
  $self->{_column_data}{$col} = $self->_column_to_storage (
    $col, $self->{_filtered_column}{$col}
  );

  return $self->next::method ($col);
}

# sadly a separate codepath in Row.pm ( used by insert() )
sub get_columns {
  my $self = shift;

  $self->{_column_data}{$_} = $self->_column_to_storage (
    $_, $self->{_filtered_column}{$_}
  ) for grep
    { ! exists $self->{_column_data}{$_} }
    keys %{$self->{_filtered_column}||{}}
  ;

  $self->next::method (@_);
}

# and *another* separate codepath, argh!
sub get_dirty_columns {
  my $self = shift;

  $self->{_dirty_columns}{$_}
    and
  ! exists $self->{_column_data}{$_}
    and
  $self->{_column_data}{$_} = $self->_column_to_storage (
    $_, $self->{_filtered_column}{$_}
  )
    for keys %{$self->{_filtered_column}||{}};

  $self->next::method(@_);
}

sub store_column {
  my ($self, $col) = (shift, @_);

  # blow cache
  delete $self->{_filtered_column}{$col};

  $self->next::method(@_);
}

sub has_column_loaded {
  my ($self, $col) = @_;
  return 1 if exists $self->{_filtered_column}{$col};
  return $self->next::method($col);
}

sub set_filtered_column {
  my ($self, $col, $filtered) = @_;

  # unlike IC, FC does not need to deal with the 'filter' abomination
  # thus we can short-curcuit filtering entirely and never call set_column
  # in case this is already a dirty change OR the row never touched storage
  if (
    ! $self->in_storage
      or
    $self->is_column_changed($col)
  ) {
    $self->make_column_dirty($col);
    delete $self->{_column_data}{$col};
  }
  else {
    $self->set_column($col, $self->_column_to_storage($col, $filtered));
  };

  return $self->{_filtered_column}{$col} = $filtered;
}

sub update {
  my ($self, $data, @rest) = @_;

  my $colinfos = $self->result_source->columns_info;

  foreach my $col (keys %{$data||{}}) {
    if ( exists $colinfos->{$col}{_filter_info} ) {
      $self->set_filtered_column($col, delete $data->{$col});

      # FIXME update() reaches directly into the object-hash
      # and we may *not* have a filtered value there - thus
      # the void-ctx filter-trigger
      $self->get_column($col) unless exists $self->{_column_data}{$col};
    }
  }

  return $self->next::method($data, @rest);
}

sub new {
  my ($class, $data, @rest) = @_;

  my $rsrc = $data->{-result_source}
    or $class->throw_exception('Sourceless rows are not supported with DBIx::Class::FilterColumn');

  my $obj = $class->next::method($data, @rest);

  my $colinfos = $rsrc->columns_info;

  foreach my $col (keys %{$data||{}}) {
    if (exists $colinfos->{$col}{_filter_info} ) {
      $obj->set_filtered_column($col, $data->{$col});
    }
  }

  return $obj;
}

1;

__END__

=head1 NAME

DBIx::Class::FilterColumn - Automatically convert column data

=head1 SYNOPSIS

In your Schema or DB class add "FilterColumn" to the top of the component list.

  __PACKAGE__->load_components(qw( FilterColumn ... ));

Set up filters for the columns you want to convert.

 __PACKAGE__->filter_column( money => {
     filter_to_storage => 'to_pennies',
     filter_from_storage => 'from_pennies',
 });

 sub to_pennies   { $_[1] * 100 }

 sub from_pennies { $_[1] / 100 }

 1;


=head1 DESCRIPTION

This component is meant to be a more powerful, but less DWIM-y,
L<DBIx::Class::InflateColumn>.  One of the major issues with said component is
that it B<only> works with references.  Generally speaking anything that can
be done with L<DBIx::Class::InflateColumn> can be done with this component.

=head1 METHODS

=head2 filter_column

 __PACKAGE__->filter_column( colname => {
     filter_from_storage => 'method'|\&coderef,
     filter_to_storage   => 'method'|\&coderef,
 })

This is the method that you need to call to set up a filtered column. It takes
exactly two arguments; the first being the column name the second being a hash
reference with C<filter_from_storage> and C<filter_to_storage> set to either
a method name or a code reference. In either case the filter is invoked as:

  $result->$filter_specification ($value_to_filter)

with C<$filter_specification> being chosen depending on whether the
C<$value_to_filter> is being retrieved from or written to permanent
storage.

If a specific directional filter is not specified, the original value will be
passed to/from storage unfiltered.

=head2 get_filtered_column

 $obj->get_filtered_column('colname')

Returns the filtered value of the column

=head2 set_filtered_column

 $obj->set_filtered_column(colname => 'new_value')

Sets the filtered value of the column

=head1 EXAMPLE OF USE

Some databases have restrictions on values that can be passed to
boolean columns, and problems can be caused by passing value that
perl considers to be false (such as C<undef>).

One solution to this is to ensure that the boolean values are set
to something that the database can handle - such as numeric zero
and one, using code like this:-

    __PACKAGE__->filter_column(
        my_boolean_column => {
            filter_to_storage   => sub { $_[1] ? 1 : 0 },
        }
    );

In this case the C<filter_from_storage> is not required, as just
passing the database value through to perl does the right thing.

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.
