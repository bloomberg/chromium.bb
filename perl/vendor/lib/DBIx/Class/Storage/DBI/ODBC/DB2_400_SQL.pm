package DBIx::Class::Storage::DBI::ODBC::DB2_400_SQL;

use strict;
use warnings;

use base qw/
    DBIx::Class::Storage::DBI::ODBC
    DBIx::Class::Storage::DBI::DB2
/;
use mro 'c3';

1;

=head1 NAME

DBIx::Class::Storage::DBI::ODBC::DB2_400_SQL - Support specific to DB2/400
over ODBC

=head1 DESCRIPTION

This is an empty subclass of L<DBIx::Class::Storage::DBI::DB2>.

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.

