package # hide from PAUSE
    DBIx::Class::CDBICompat::LazyLoading;

use strict;
use warnings;

sub resultset_instance {
  my $self = shift;
  my $rs = $self->next::method(@_);
  $rs = $rs->search(undef, { columns => [ $self->columns('Essential') ] });
  return $rs;
}


# Emulate that CDBI throws out all changed columns and reloads them on
# request in case the database modifies the new value (say, via a trigger)
sub update {
    my $self = shift;

    my @dirty_columns = keys %{$self->{_dirty_columns}};

    my $ret = $self->next::method(@_);
    $self->_clear_column_data(@dirty_columns);

    return $ret;
}


# And again for create
sub create {
    my $class = shift;
    my($data) = @_;

    my @columns = keys %$data;

    my $obj = $class->next::method(@_);
    return $obj unless defined $obj;

    my %primary_cols = map { $_ => 1 } $class->primary_columns;
    my @data_cols = grep !$primary_cols{$_}, @columns;
    $obj->_clear_column_data(@data_cols);

    return $obj;
}


sub _clear_column_data {
    my $self = shift;

    delete $self->{_column_data}{$_}     for @_;
    delete $self->{_inflated_column}{$_} for @_;
}


sub get_column {
  my ($self, $col) = @_;
  if ((ref $self) && (!exists $self->{'_column_data'}{$col})
    && $self->{'_in_storage'}) {
    $self->_flesh(grep { exists $self->_column_groups->{$_}{$col}
                           && $_ ne 'All' }
                   keys %{ $self->_column_groups || {} });
  }
  $self->next::method(@_[1..$#_]);
}

# CDBI does not explicitly declare auto increment columns, so
# we just clear out our primary columns before copying.
sub copy {
  my($self, $changes) = @_;

  for my $col ($self->primary_columns) {
    $changes->{$col} = undef unless exists $changes->{$col};
  }

  return $self->next::method($changes);
}

sub discard_changes {
  my($self) = shift;

  delete $self->{_column_data}{$_} for $self->is_changed;
  delete $self->{_dirty_columns};
  delete $self->{_relationship_data};

  return $self;
}

sub _ident_cond {
  my ($class) = @_;
  return join(" AND ", map { "$_ = ?" } $class->primary_columns);
}

sub _flesh {
  my ($self, @groups) = @_;
  @groups = ('All') unless @groups;
  my %want;
  $want{$_} = 1 for map { keys %{$self->_column_groups->{$_}} } @groups;
  if (my @want = grep { !exists $self->{'_column_data'}{$_} } keys %want) {
    my $cursor = $self->result_source->storage->select(
                $self->result_source->name, \@want,
                \$self->_ident_cond, { bind => [ $self->_ident_values ] });
    #my $sth = $self->storage->select($self->_table_name, \@want,
    #                                   $self->ident_condition);
    # Not sure why the first one works and this doesn't :(
    my @val = $cursor->next;

    return unless @val; # object must have been deleted from the database

    foreach my $w (@want) {
      $self->{'_column_data'}{$w} = shift @val;
    }
  }
}

1;
