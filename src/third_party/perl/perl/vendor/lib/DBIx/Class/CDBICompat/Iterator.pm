package DBIx::Class::CDBICompat::Iterator;

use strict;
use warnings;


=head1 NAME

DBIx::Class::CDBICompat::Iterator - Emulates the extra behaviors of the Class::DBI search iterator.

=head1 SYNOPSIS

See DBIx::Class::CDBICompat for usage directions.

=head1 DESCRIPTION

Emulates the extra behaviors of the Class::DBI search iterator.

=head2 Differences from DBIx::Class result set

The CDBI iterator returns true if there were any results, false otherwise.  The DBIC result set always returns true.

=cut


sub _init_result_source_instance {
  my $class = shift;

  my $table = $class->next::method(@_);
  $table->resultset_class("DBIx::Class::CDBICompat::Iterator::ResultSet");

  return $table;
}

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.

=cut

package # hide
  DBIx::Class::CDBICompat::Iterator::ResultSet;

use strict;
use warnings;

use base qw(DBIx::Class::ResultSet);

sub _bool {
    # Performance hack so internal checks whether the result set
    # exists won't do a SQL COUNT.
    return 1 if caller =~ /^DBIx::Class::/;

    return $_[0]->count;
}

sub _construct_results {
  my $self = shift;

  my $rows = $self->next::method(@_);

  if (my $f = $self->_resolved_attrs->{record_filter}) {
    $_ = $f->($_) for @$rows;
  }

  return $rows;
}

1;
