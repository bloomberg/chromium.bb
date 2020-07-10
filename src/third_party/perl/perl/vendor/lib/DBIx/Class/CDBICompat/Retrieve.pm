package # hide from PAUSE
    DBIx::Class::CDBICompat::Retrieve;

use strict;

# even though fatalization has been proven over and over to be a universally
# bad idea, this line has been part of the code from the beginning
# leaving the compat layer as-is, something may in fact depend on that
use warnings FATAL => 'all';

sub retrieve {
  my $self = shift;
  die "No args to retrieve" unless @_ > 0;

  my @cols = $self->primary_columns;

  my $query;
  if (ref $_[0] eq 'HASH') {
    $query = { %{$_[0]} };
  }
  elsif (@_ == @cols) {
    $query = {};
    @{$query}{@cols} = @_;
  }
  else {
    $query = {@_};
  }

  $query = $self->_build_query($query);
  $self->find($query);
}

sub find_or_create {
  my $self = shift;
  my $query = ref $_[0] eq 'HASH' ? shift : {@_};

  $query = $self->_build_query($query);
  $self->next::method($query);
}

# _build_query
#
# Build a query hash. Defaults to a no-op; ColumnCase overrides.

sub _build_query {
  my ($self, $query) = @_;

  return $query;
}

sub retrieve_from_sql {
  my ($class, $cond, @rest) = @_;

  $cond =~ s/^\s*WHERE//i;

  # Need to parse the SQL clauses after WHERE in reverse
  # order of appearance.

  my %attrs;

  if( $cond =~ s/\bLIMIT\s+(\d+)\s*$//i ) {
      $attrs{rows} = $1;
  }

  if ( $cond =~ s/\bORDER\s+BY\s+(.*)\s*$//i ) {
      $attrs{order_by} = $1;
  }

  if( $cond =~ s/\bGROUP\s+BY\s+(.*)\s*$//i ) {
      $attrs{group_by} = $1;
  }

  return $class->search_literal($cond, @rest, ( %attrs ? \%attrs : () ) );
}

sub construct {
    my $class = shift;
    my $obj = $class->resultset_instance->new_result(@_);
    $obj->in_storage(1);

    return $obj;
}

sub retrieve_all      { shift->search              }
sub count_all         { shift->count               }

sub maximum_value_of  {
    my($class, $col) = @_;
    return $class->resultset_instance->get_column($col)->max;
}

sub minimum_value_of  {
    my($class, $col) = @_;
    return $class->resultset_instance->get_column($col)->min;
}

1;
