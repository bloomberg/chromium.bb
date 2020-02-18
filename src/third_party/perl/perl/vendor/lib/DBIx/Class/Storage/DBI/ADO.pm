package DBIx::Class::Storage::DBI::ADO;

use warnings;
use strict;

use base 'DBIx::Class::Storage::DBI';
use mro 'c3';

use Sub::Name;
use Try::Tiny;
use DBIx::Class::_Util 'sigwarn_silencer';
use namespace::clean;

=head1 NAME

DBIx::Class::Storage::DBI::ADO - Support for L<DBD::ADO>

=head1 DESCRIPTION

This class provides a mechanism for discovering and loading a sub-class
for a specific ADO backend, as well as some workarounds for L<DBD::ADO>. It
should be transparent to the user.

=cut

sub _rebless { shift->_determine_connector_driver('ADO') }

# cleanup some warnings from DBD::ADO
# RT#65563, not fixed as of DBD::ADO v2.98
sub _dbh_get_info {
  my $self = shift;

  local $SIG{__WARN__} = sigwarn_silencer(
    qr{^Missing argument in sprintf at \S+/ADO/GetInfo\.pm}
  );

  $self->next::method(@_);
}

# Monkeypatch out the horrible warnings during global destruction.
# A patch to DBD::ADO has been submitted as well, and it was fixed
# as of 2.99
# https://rt.cpan.org/Ticket/Display.html?id=65563
sub _init {
  unless ($DBD::ADO::__DBIC_MONKEYPATCH_CHECKED__) {
    require DBD::ADO;

    unless (try { DBD::ADO->VERSION('2.99'); 1 }) {
      no warnings 'redefine';
      my $disconnect = *DBD::ADO::db::disconnect{CODE};

      *DBD::ADO::db::disconnect = subname 'DBD::ADO::db::disconnect' => sub {
        local $SIG{__WARN__} = sigwarn_silencer(
          qr/Not a Win32::OLE object|uninitialized value/
        );
        $disconnect->(@_);
      };
    }

    $DBD::ADO::__DBIC_MONKEYPATCH_CHECKED__ = 1;
  }
}

# Here I was just experimenting with ADO cursor types, left in as a comment in
# case you want to as well. See the DBD::ADO docs.
#sub _prepare_sth {
#  my ($self, $dbh, $sql) = @_;
#
#  my $sth = $self->disable_sth_caching
#    ? $dbh->prepare($sql, { CursorType => 'adOpenStatic' })
#    : $dbh->prepare_cached($sql, { CursorType => 'adOpenStatic' }, 3);
#
#  $self->throw_exception($dbh->errstr) if !$sth;
#
#  $sth;
#}

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.

=cut

1;

# vim:sts=2 sw=2:
