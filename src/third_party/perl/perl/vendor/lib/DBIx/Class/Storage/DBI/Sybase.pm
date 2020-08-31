package DBIx::Class::Storage::DBI::Sybase;

use strict;
use warnings;
use Try::Tiny;
use namespace::clean;

use base qw/DBIx::Class::Storage::DBI/;

=head1 NAME

DBIx::Class::Storage::DBI::Sybase - Base class for drivers using
L<DBD::Sybase>

=head1 DESCRIPTION

This is the base class/dispatcher for Storage's designed to work with
L<DBD::Sybase>

=head1 METHODS

=cut

sub _rebless {
  my $self = shift;

  my $dbtype;
  try {
    $dbtype = @{$self->_get_dbh->selectrow_arrayref(qq{sp_server_info \@attribute_id=1})}[2]
  } catch {
    $self->throw_exception("Unable to establish connection to determine database type: $_")
  };

  if ($dbtype) {
    $dbtype =~ s/\W/_/gi;

    # saner class name
    $dbtype = 'ASE' if $dbtype eq 'SQL_Server';

    my $subclass = __PACKAGE__ . "::$dbtype";
    if ($self->load_optional_class($subclass)) {
      bless $self, $subclass;
      $self->_rebless;
    }
  }
}

sub _init {
  # once the driver is determined see if we need to insert the DBD::Sybase w/ FreeTDS fixups
  # this is a dirty version of "instance role application", \o/ DO WANT Moo \o/
  my $self = shift;
  if (! $self->isa('DBIx::Class::Storage::DBI::Sybase::FreeTDS') and $self->_using_freetds) {
    require DBIx::Class::Storage::DBI::Sybase::FreeTDS;

    my @isa = @{mro::get_linear_isa(ref $self)};
    my $class = shift @isa; # this is our current ref

    my $trait_class = $class . '::FreeTDS';
    mro::set_mro ($trait_class, 'c3');
    no strict 'refs';
    @{"${trait_class}::ISA"} = ($class, 'DBIx::Class::Storage::DBI::Sybase::FreeTDS', @isa);

    bless ($self, $trait_class);

    Class::C3->reinitialize() if DBIx::Class::_ENV_::OLD_MRO;

    $self->_init(@_);
  }

  $self->next::method(@_);
}

sub _ping {
  my $self = shift;

  my $dbh = $self->_dbh or return 0;

  local $dbh->{RaiseError} = 1;
  local $dbh->{PrintError} = 0;

# FIXME if the main connection goes stale, does opening another for this statement
# really determine anything?

  if ($dbh->{syb_no_child_con}) {
    return try {
      $self->_connect->do('select 1');
      1;
    }
    catch {
      0;
    };
  }

  return try {
    $dbh->do('select 1');
    1;
  }
  catch {
    0;
  };
}

sub _set_max_connect {
  my $self = shift;
  my $val  = shift || 256;

  my $dsn = $self->_dbi_connect_info->[0];

  return if ref($dsn) eq 'CODE';

  if ($dsn !~ /maxConnect=/) {
    $self->_dbi_connect_info->[0] = "$dsn;maxConnect=$val";
    my $connected = defined $self->_dbh;
    $self->disconnect;
    $self->ensure_connected if $connected;
  }
}

# Whether or not DBD::Sybase was compiled against FreeTDS. If false, it means
# the Sybase OpenClient libraries were used.
sub _using_freetds {
  my $self = shift;
  return ($self->_get_dbh->{syb_oc_version}||'') =~ /freetds/i;
}

# Either returns the FreeTDS version against which DBD::Sybase was compiled,
# 0 if can't be determined, or undef otherwise
sub _using_freetds_version {
  my $inf = shift->_get_dbh->{syb_oc_version};
  return undef unless ($inf||'') =~ /freetds/i;
  return $inf =~ /v([0-9\.]+)/ ? $1 : 0;
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

