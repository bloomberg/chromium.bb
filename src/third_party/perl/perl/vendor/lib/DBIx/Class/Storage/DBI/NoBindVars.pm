package DBIx::Class::Storage::DBI::NoBindVars;

use strict;
use warnings;

use base 'DBIx::Class::Storage::DBI';
use mro 'c3';

use DBIx::Class::SQLMaker::LimitDialects;
use List::Util qw/first/;

use namespace::clean;

=head1 NAME

DBIx::Class::Storage::DBI::NoBindVars - Sometime DBDs have poor to no support for bind variables

=head1 DESCRIPTION

This class allows queries to work when the DBD or underlying library does not
support the usual C<?> placeholders, or at least doesn't support them very
well, as is the case with L<DBD::Sybase>

=head1 METHODS

=head2 connect_info

We can't cache very effectively without bind variables, so force the C<disable_sth_caching> setting to be turned on when the connect info is set.

=cut

sub connect_info {
    my $self = shift;
    my $retval = $self->next::method(@_);
    $self->disable_sth_caching(1);
    $retval;
}

=head2 _prep_for_execute

Manually subs in the values for the usual C<?> placeholders.

=cut

sub _prep_for_execute {
  my $self = shift;

  my ($sql, $bind) = $self->next::method(@_);

  # stringify bind args, quote via $dbh, and manually insert
  #my ($op, $ident, $args) = @_;
  my $ident = $_[1];

  my @sql_part = split /\?/, $sql;
  my $new_sql;

  for (@$bind) {
    my $data = (ref $_->[1]) ? "$_->[1]" : $_->[1]; # always stringify, array types are currently not supported

    my $datatype = $_->[0]{sqlt_datatype};

    $data = $self->_prep_interpolated_value($datatype, $data)
      if $datatype;

    $data = $self->_get_dbh->quote($data)
      unless ($datatype and $self->interpolate_unquoted($datatype, $data) );

    $new_sql .= shift(@sql_part) . $data;
  }

  $new_sql .= join '', @sql_part;

  return ($new_sql, []);
}

=head2 interpolate_unquoted

This method is called by L</_prep_for_execute> for every column in
order to determine if its value should be quoted or not. The arguments
are the current column data type and the actual bind value. The return
value is interpreted as: true - do not quote, false - do quote. You should
override this in you Storage::DBI::<database> subclass, if your RDBMS
does not like quotes around certain datatypes (e.g. Sybase and integer
columns). The default method returns false, except for integer datatypes
paired with values containing nothing but digits.

 WARNING!!!

 Always validate that the bind-value is valid for the current datatype.
 Otherwise you may very well open the door to SQL injection attacks.

=cut

sub interpolate_unquoted {
  #my ($self, $datatype, $value) = @_;

  return 1 if (
    defined $_[2]
      and
    $_[1]
      and
    $_[2] !~ /\D/
      and
    $_[1] =~ /int(?:eger)? | (?:tiny|small|medium|big)int/ix
  );

  return 0;
}

=head2 _prep_interpolated_value

Given a datatype and the value to be inserted directly into a SQL query, returns
the necessary string to represent that value (by e.g. adding a '$' sign)

=cut

sub _prep_interpolated_value {
  #my ($self, $datatype, $value) = @_;
  return $_[2];
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
