package DBIx::Class::Storage::DBI::Firebird;

use strict;
use warnings;

# Because DBD::Firebird is more or less a copy of
# DBD::Interbase, inherit all the workarounds contained
# in ::Storage::DBI::InterBase as opposed to inheriting
# directly from ::Storage::DBI::Firebird::Common
use base qw/DBIx::Class::Storage::DBI::InterBase/;
use mro 'c3';

1;

=head1 NAME

DBIx::Class::Storage::DBI::Firebird - Driver for the Firebird RDBMS via
L<DBD::Firebird>

=head1 DESCRIPTION

This is an empty subclass of L<DBIx::Class::Storage::DBI::InterBase> for use
with L<DBD::Firebird>, see that driver for details.

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.
