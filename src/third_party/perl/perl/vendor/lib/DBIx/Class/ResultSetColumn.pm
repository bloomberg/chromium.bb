package DBIx::Class::ResultSetColumn;

use strict;
use warnings;

use base 'DBIx::Class';
use DBIx::Class::Carp;
use DBIx::Class::_Util 'fail_on_internal_wantarray';
use namespace::clean;

# not importing first() as it will clash with our own method
use List::Util ();

=head1 NAME

  DBIx::Class::ResultSetColumn - helpful methods for messing
  with a single column of the resultset

=head1 SYNOPSIS

  $rs = $schema->resultset('CD')->search({ artist => 'Tool' });
  $rs_column = $rs->get_column('year');
  $max_year = $rs_column->max; #returns latest year

=head1 DESCRIPTION

A convenience class used to perform operations on a specific column of
a resultset.

=cut

=head1 METHODS

=head2 new

  my $obj = DBIx::Class::ResultSetColumn->new($rs, $column);

Creates a new resultset column object from the resultset and column
passed as params. Used internally by L<DBIx::Class::ResultSet/get_column>.

=cut

sub new {
  my ($class, $rs, $column) = @_;
  $class = ref $class if ref $class;

  $rs->throw_exception('column must be supplied') unless $column;

  my $orig_attrs = $rs->_resolved_attrs;
  my $alias = $rs->current_source_alias;
  my $rsrc = $rs->result_source;

  # If $column can be found in the 'as' list of the parent resultset, use the
  # corresponding element of its 'select' list (to keep any custom column
  # definition set up with 'select' or '+select' attrs), otherwise use $column
  # (to create a new column definition on-the-fly).
  my $as_list = $orig_attrs->{as} || [];
  my $select_list = $orig_attrs->{select} || [];
  my $as_index = List::Util::first { ($as_list->[$_] || "") eq $column } 0..$#$as_list;
  my $select = defined $as_index ? $select_list->[$as_index] : $column;

  my $colmap;
  for ($rsrc->columns, $column) {
    if ($_ =~ /^ \Q$alias\E \. ([^\.]+) $ /x) {
      $colmap->{$_} = $1;
    }
    elsif ($_ !~ /\./) {
      $colmap->{"$alias.$_"} = $_;
      $colmap->{$_} = $_;
    }
  }

  my $new_parent_rs;
  # analyze the order_by, and see if it is done over a function/nonexistentcolumn
  # if this is the case we will need to wrap a subquery since the result of RSC
  # *must* be a single column select
  if (
    scalar grep
      { ! exists $colmap->{$_->[0]} }
      ( $rsrc->schema->storage->_extract_order_criteria ($orig_attrs->{order_by} ) )
  ) {
    # nuke the prefetch before collapsing to sql
    my $subq_rs = $rs->search_rs;
    $subq_rs->{attrs}{join} = $subq_rs->_merge_joinpref_attr( $subq_rs->{attrs}{join}, delete $subq_rs->{attrs}{prefetch} );
    $new_parent_rs = $subq_rs->as_subselect_rs;
  }

  $new_parent_rs ||= $rs->search_rs;
  my $new_attrs = $new_parent_rs->{attrs} ||= {};

  # prefetch causes additional columns to be fetched, but we can not just make a new
  # rs via the _resolved_attrs trick - we need to retain the separation between
  # +select/+as and select/as. At the same time we want to preserve any joins that the
  # prefetch would otherwise generate.
  $new_attrs->{join} = $rs->_merge_joinpref_attr( $new_attrs->{join}, delete $new_attrs->{prefetch} );

  # {collapse} would mean a has_many join was injected, which in turn means
  # we need to group *IF WE CAN* (only if the column in question is unique)
  if (!$orig_attrs->{group_by} && $orig_attrs->{collapse}) {

    if ($colmap->{$select} and $rsrc->_identifying_column_set([$colmap->{$select}])) {
      $new_attrs->{group_by} = [ $select ];
      delete @{$new_attrs}{qw(distinct _grouped_by_distinct)}; # it is ignored when group_by is present
    }
    else {
      carp (
          "Attempting to retrieve non-unique column '$column' on a resultset containing "
        . 'one-to-many joins will return duplicate results.'
      );
    }
  }

  return bless {
    _select => $select,
    _as => $column,
    _parent_resultset => $new_parent_rs
  }, $class;
}

=head2 as_query

=over 4

=item Arguments: none

=item Return Value: \[ $sql, L<@bind_values|DBIx::Class::ResultSet/DBIC BIND VALUES> ]

=back

Returns the SQL query and bind vars associated with the invocant.

This is generally used as the RHS for a subquery.

=cut

sub as_query { return shift->_resultset->as_query(@_) }

=head2 next

=over 4

=item Arguments: none

=item Return Value: $value

=back

Returns the next value of the column in the resultset (or C<undef> if
there is none).

Much like L<DBIx::Class::ResultSet/next> but just returning the
one value.

=cut

sub next {
  my $self = shift;

  # using cursor so we don't inflate anything
  my ($row) = $self->_resultset->cursor->next;

  return $row;
}

=head2 all

=over 4

=item Arguments: none

=item Return Value: @values

=back

Returns all values of the column in the resultset (or C<undef> if
there are none).

Much like L<DBIx::Class::ResultSet/all> but returns values rather
than result objects.

=cut

sub all {
  my $self = shift;

  # using cursor so we don't inflate anything
  return map { $_->[0] } $self->_resultset->cursor->all;
}

=head2 reset

=over 4

=item Arguments: none

=item Return Value: $self

=back

Resets the underlying resultset's cursor, so you can iterate through the
elements of the column again.

Much like L<DBIx::Class::ResultSet/reset>.

=cut

sub reset {
  my $self = shift;
  $self->_resultset->cursor->reset;
  return $self;
}

=head2 first

=over 4

=item Arguments: none

=item Return Value: $value

=back

Resets the underlying resultset and returns the next value of the column in the
resultset (or C<undef> if there is none).

Much like L<DBIx::Class::ResultSet/first> but just returning the one value.

=cut

sub first {
  my $self = shift;

  # using cursor so we don't inflate anything
  $self->_resultset->cursor->reset;
  my ($row) = $self->_resultset->cursor->next;

  return $row;
}

=head2 single

=over 4

=item Arguments: none

=item Return Value: $value

=back

Much like L<DBIx::Class::ResultSet/single> fetches one and only one column
value using the cursor directly. If additional rows are present a warning
is issued before discarding the cursor.

=cut

sub single {
  my $self = shift;

  my $attrs = $self->_resultset->_resolved_attrs;
  my ($row) = $self->_resultset->result_source->storage->select_single(
    $attrs->{from}, $attrs->{select}, $attrs->{where}, $attrs
  );

  return $row;
}

=head2 min

=over 4

=item Arguments: none

=item Return Value: $lowest_value

=back

  my $first_year = $year_col->min();

Wrapper for ->func. Returns the lowest value of the column in the
resultset (or C<undef> if there are none).

=cut

sub min {
  return shift->func('MIN');
}

=head2 min_rs

=over 4

=item Arguments: none

=item Return Value: L<$resultset|DBIx::Class::ResultSet>

=back

  my $rs = $year_col->min_rs();

Wrapper for ->func_rs for function MIN().

=cut

sub min_rs { return shift->func_rs('MIN') }

=head2 max

=over 4

=item Arguments: none

=item Return Value: $highest_value

=back

  my $last_year = $year_col->max();

Wrapper for ->func. Returns the highest value of the column in the
resultset (or C<undef> if there are none).

=cut

sub max {
  return shift->func('MAX');
}

=head2 max_rs

=over 4

=item Arguments: none

=item Return Value: L<$resultset|DBIx::Class::ResultSet>

=back

  my $rs = $year_col->max_rs();

Wrapper for ->func_rs for function MAX().

=cut

sub max_rs { return shift->func_rs('MAX') }

=head2 sum

=over 4

=item Arguments: none

=item Return Value: $sum_of_values

=back

  my $total = $prices_col->sum();

Wrapper for ->func. Returns the sum of all the values in the column of
the resultset. Use on varchar-like columns at your own risk.

=cut

sub sum {
  return shift->func('SUM');
}

=head2 sum_rs

=over 4

=item Arguments: none

=item Return Value: L<$resultset|DBIx::Class::ResultSet>

=back

  my $rs = $year_col->sum_rs();

Wrapper for ->func_rs for function SUM().

=cut

sub sum_rs { return shift->func_rs('SUM') }

=head2 func

=over 4

=item Arguments: $function

=item Return Value: $function_return_value

=back

  $rs = $schema->resultset("CD")->search({});
  $length = $rs->get_column('title')->func('LENGTH');

Runs a query using the function on the column and returns the
value. Produces the following SQL:

  SELECT LENGTH( title ) FROM cd me

=cut

sub func {
  my ($self,$function) = @_;
  my $cursor = $self->func_rs($function)->cursor;

  if( wantarray ) {
    DBIx::Class::_ENV_::ASSERT_NO_INTERNAL_WANTARRAY and my $sog = fail_on_internal_wantarray;
    return map { $_->[ 0 ] } $cursor->all;
  }

  return ( $cursor->next )[ 0 ];
}

=head2 func_rs

=over 4

=item Arguments: $function

=item Return Value: L<$resultset|DBIx::Class::ResultSet>

=back

Creates the resultset that C<func()> uses to run its query.

=cut

sub func_rs {
  my ($self,$function) = @_;

  my $rs = $self->{_parent_resultset};
  my $select = $self->{_select};

  # wrap a grouped rs
  if ($rs->_resolved_attrs->{group_by}) {
    $select = $self->{_as};
    $rs = $rs->as_subselect_rs;
  }

  $rs->search( undef, {
    columns => { $self->{_as} => { $function => $select } }
  } );
}

=head2 throw_exception

See L<DBIx::Class::Schema/throw_exception> for details.

=cut

sub throw_exception {
  my $self = shift;

  if (ref $self && $self->{_parent_resultset}) {
    $self->{_parent_resultset}->throw_exception(@_);
  }
  else {
    DBIx::Class::Exception->throw(@_);
  }
}

# _resultset
#
# Arguments: none
#
# Return Value: $resultset
#
#  $year_col->_resultset->next
#
# Returns the underlying resultset. Creates it from the parent resultset if
# necessary.
#
sub _resultset {
  my $self = shift;

  return $self->{_resultset} ||= do {

    my $select = $self->{_select};

    if ($self->{_parent_resultset}{attrs}{distinct}) {
      my $alias = $self->{_parent_resultset}->current_source_alias;
      my $rsrc = $self->{_parent_resultset}->result_source;
      my %cols = map { $_ => 1, "$alias.$_" => 1 } $rsrc->columns;

      unless( $cols{$select} ) {
        carp_unique(
          'Use of distinct => 1 while selecting anything other than a column '
        . 'declared on the primary ResultSource is deprecated (you selected '
        . "'$self->{_as}') - please supply an explicit group_by instead"
        );

        # collapse the selector to a literal so that it survives the distinct parse
        # if it turns out to be an aggregate - at least the user will get a proper exception
        # instead of silent drop of the group_by altogether
        $select = \[ $rsrc->storage->sql_maker->_recurse_fields($select) ];
      }
    }

    $self->{_parent_resultset}->search(undef, {
      columns => { $self->{_as} => $select }
    });
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

