package DBIx::Class::Storage::DBI::Oracle::WhereJoins;

use strict;
use warnings;

use base qw( DBIx::Class::Storage::DBI::Oracle::Generic );
use mro 'c3';

__PACKAGE__->sql_maker_class('DBIx::Class::SQLMaker::OracleJoins');

1;

__END__

=pod

=head1 NAME

DBIx::Class::Storage::DBI::Oracle::WhereJoins - Oracle joins in WHERE syntax
support (instead of ANSI).

=head1 PURPOSE

This module is used with Oracle < 9.0 due to lack of support for standard
ANSI join syntax.

=head1 SYNOPSIS

DBIx::Class should automagically detect Oracle and use this module with no
work from you.

=head1 DESCRIPTION

This class implements Oracle's WhereJoin support.  Instead of:

    SELECT x FROM y JOIN z ON y.id = z.id

It will write:

    SELECT x FROM y, z WHERE y.id = z.id

It should properly support left joins, and right joins.  Full outer joins are
not possible due to the fact that Oracle requires the entire query be written
to union the results of a left and right join, and by the time this module is
called to create the where query and table definition part of the SQL query,
it's already too late.

=head1 METHODS

See L<DBIx::Class::SQLMaker::OracleJoins> for implementation details.

=head1 BUGS

Does not support full outer joins.
Probably lots more.

=head1 SEE ALSO

=over

=item L<DBIx::Class::SQLMaker>

=item L<DBIx::Class::SQLMaker::OracleJoins>

=item L<DBIx::Class::Storage::DBI::Oracle::Generic>

=item L<DBIx::Class>

=back

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.
