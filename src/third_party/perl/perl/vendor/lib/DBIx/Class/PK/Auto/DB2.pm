package # hide package from pause
  DBIx::Class::PK::Auto::DB2;

use strict;
use warnings;

use base qw/DBIx::Class/;

__PACKAGE__->load_components(qw/PK::Auto/);

1;

__END__

=head1 NAME

DBIx::Class::PK::Auto::DB2 - (DEPRECATED) Automatic primary key class for DB2

=head1 SYNOPSIS

Just load PK::Auto instead; auto-inc is now handled by Storage.

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.
