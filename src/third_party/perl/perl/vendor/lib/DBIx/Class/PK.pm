package DBIx::Class::PK;

use strict;
use warnings;

use base qw/DBIx::Class::Row/;

=head1 NAME

DBIx::Class::PK - Primary Key class

=head1 SYNOPSIS

=head1 DESCRIPTION

This class contains methods for handling primary keys and methods
depending on them.

=head1 METHODS

=cut

=head2 id

Returns the primary key(s) for a row. Can't be called as
a class method.

=cut

sub id {
  my ($self) = @_;
  $self->throw_exception( "Can't call id() as a class method" )
    unless ref $self;
  my @id_vals = $self->_ident_values;
  return (wantarray ? @id_vals : $id_vals[0]);
}

sub _ident_values {
  my ($self, $use_storage_state) = @_;

  my (@ids, @missing);

  for ($self->result_source->_pri_cols_or_die) {
    push @ids, ($use_storage_state and exists $self->{_column_data_in_storage}{$_})
      ? $self->{_column_data_in_storage}{$_}
      : $self->get_column($_)
    ;
    push @missing, $_ if (! defined $ids[-1] and ! $self->has_column_loaded ($_) );
  }

  if (@missing && $self->in_storage) {
    $self->throw_exception (
      'Unable to uniquely identify result object with missing PK columns: '
      . join (', ', @missing )
    );
  }

  return @ids;
}

=head2 ID

Returns a unique id string identifying a result object by primary key.
Used by L<DBIx::Class::CDBICompat::LiveObjectIndex> and
L<DBIx::Class::ObjectCache>.

=over

=item WARNING

The default C<_create_ID> method used by this function orders the returned
values by the alphabetical order of the primary column names, B<unlike>
the L</id> method, which follows the same order in which columns were fed
to L<DBIx::Class::ResultSource/set_primary_key>.

=back

=cut

sub ID {
  my ($self) = @_;
  $self->throw_exception( "Can't call ID() as a class method" )
    unless ref $self;
  return undef unless $self->in_storage;
  return $self->_create_ID(%{$self->ident_condition});
}

sub _create_ID {
  my ($self, %vals) = @_;
  return undef if grep { !defined } values %vals;
  return join '|', ref $self || $self, $self->result_source->name,
    map { $_ . '=' . $vals{$_} } sort keys %vals;
}

=head2 ident_condition

  my $cond = $result_source->ident_condition();

  my $cond = $result_source->ident_condition('alias');

Produces a condition hash to locate a row based on the primary key(s).

=cut

sub ident_condition {
  shift->_mk_ident_cond(@_);
}

sub _storage_ident_condition {
  shift->_mk_ident_cond(shift, 1);
}

sub _mk_ident_cond {
  my ($self, $alias, $use_storage_state) = @_;

  my @pks = $self->result_source->_pri_cols_or_die;
  my @vals = $self->_ident_values($use_storage_state);

  my (%cond, @undef);
  my $prefix = defined $alias ? $alias.'.' : '';
  for my $col (@pks) {
    if (! defined ($cond{$prefix.$col} = shift @vals) ) {
      push @undef, $col;
    }
  }

  if (@undef && $self->in_storage) {
    $self->throw_exception (
      'Unable to construct result object identity condition due to NULL PK columns: '
      . join (', ', @undef)
    );
  }

  return \%cond;
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
