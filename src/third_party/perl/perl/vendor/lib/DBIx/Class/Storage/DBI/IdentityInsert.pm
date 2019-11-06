package DBIx::Class::Storage::DBI::IdentityInsert;

use strict;
use warnings;
use base 'DBIx::Class::Storage::DBI';
use mro 'c3';

use namespace::clean;

=head1 NAME

DBIx::Class::Storage::DBI::IdentityInsert - Storage Component for Sybase ASE and
MSSQL for Identity Inserts / Updates

=head1 DESCRIPTION

This is a storage component for Sybase ASE
(L<DBIx::Class::Storage::DBI::Sybase::ASE>) and Microsoft SQL Server
(L<DBIx::Class::Storage::DBI::MSSQL>) to support identity inserts, that is
inserts of explicit values into C<IDENTITY> columns.

This is done by wrapping C<INSERT> operations in a pair of table identity
toggles like:

  SET IDENTITY_INSERT $table ON
  $sql
  SET IDENTITY_INSERT $table OFF

=cut

# SET IDENTITY_X only works as part of a statement scope. We can not
# $dbh->do the $sql and the wrapping set()s individually. Hence the
# sql mangling. The newlines are important.
sub _prep_for_execute {
  my $self = shift;

  return $self->next::method(@_) unless $self->_autoinc_supplied_for_op;

  my ($op, $ident) = @_;

  my $table = $self->sql_maker->_quote($ident->name);
  $op = uc $op;

  my ($sql, $bind) = $self->next::method(@_);

  return (<<EOS, $bind);
SET IDENTITY_$op $table ON
$sql
SET IDENTITY_$op $table OFF
EOS

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
