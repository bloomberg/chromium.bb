package DBIx::Class::Storage::DBI::Sybase::Microsoft_SQL_Server::NoBindVars;

use strict;
use warnings;

use base qw/
  DBIx::Class::Storage::DBI::NoBindVars
  DBIx::Class::Storage::DBI::Sybase::Microsoft_SQL_Server
/;
use mro 'c3';

sub _init {
  my $self = shift;
  $self->disable_sth_caching(1);

  $self->next::method(@_);
}

1;

=head1 NAME

DBIx::Class::Storage::DBI::Sybase::Microsoft_SQL_Server::NoBindVars - Support for Microsoft
SQL Server via DBD::Sybase without placeholders

=head1 SYNOPSIS

This subclass supports MSSQL server connections via DBD::Sybase when ? style
placeholders are not available.

=head1 DESCRIPTION

If you are using this driver then your combination of L<DBD::Sybase> and
libraries (most likely FreeTDS) does not support ? style placeholders.

This storage driver uses L<DBIx::Class::Storage::DBI::NoBindVars> as a base.
This means that bind variables will be interpolated (properly quoted of course)
into the SQL query itself, without using bind placeholders.

More importantly this means that caching of prepared statements is explicitly
disabled, as the interpolation renders it useless.

In all other respects, it is a subclass of
L<DBIx::Class::Storage::DBI::Sybase::Microsoft_SQL_Server>.

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.
