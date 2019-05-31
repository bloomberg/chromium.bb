package DBIx::Class::Storage::DBI::mysql;

use strict;
use warnings;

use base qw/DBIx::Class::Storage::DBI/;

use namespace::clean;

__PACKAGE__->sql_maker_class('DBIx::Class::SQLMaker::MySQL');
__PACKAGE__->sql_limit_dialect ('LimitXY');
__PACKAGE__->sql_quote_char ('`');

__PACKAGE__->_use_multicolumn_in (1);

sub with_deferred_fk_checks {
  my ($self, $sub) = @_;

  $self->_do_query('SET FOREIGN_KEY_CHECKS = 0');
  $sub->();
  $self->_do_query('SET FOREIGN_KEY_CHECKS = 1');
}

sub connect_call_set_strict_mode {
  my $self = shift;

  # the @@sql_mode puts back what was previously set on the session handle
  $self->_do_query(q|SET SQL_MODE = CONCAT('ANSI,TRADITIONAL,ONLY_FULL_GROUP_BY,', @@sql_mode)|);
  $self->_do_query(q|SET SQL_AUTO_IS_NULL = 0|);
}

sub _dbh_last_insert_id {
  my ($self, $dbh, $source, $col) = @_;
  $dbh->{mysql_insertid};
}

sub _prep_for_execute {
  my $self = shift;
  #(my $op, $ident, $args) = @_;

  # Only update and delete need special double-subquery treatment
  # Insert referencing the same table (i.e. SELECT MAX(id) + 1) seems
  # to work just fine on MySQL
  return $self->next::method(@_) if ( $_[0] eq 'select' or $_[0] eq 'insert' );


  # FIXME FIXME FIXME - this is a terrible, gross, incomplete hack
  # it should be trivial for mst to port this to DQ (and a good
  # exercise as well, since we do not yet have such wide tree walking
  # in place). For the time being this will work in limited cases,
  # mainly complex update/delete, which is really all we want it for
  # currently (allows us to fix some bugs without breaking MySQL in
  # the process, and is also crucial for Shadow to be usable)

  # extract the source name, construct modification indicator re
  my $sm = $self->sql_maker;

  my $target_name = $_[1]->from;

  if (ref $target_name) {
    if (
      ref $target_name eq 'SCALAR'
        and
      $$target_name =~ /^ (?:
          \` ( [^`]+ ) \` #`
        | ( [\w\-]+ )
      ) $/x
    ) {
      # this is just a plain-ish name, which has been literal-ed for
      # whatever reason
      $target_name = (defined $1) ? $1 : $2;
    }
    else {
      # this is something very complex, perhaps a custom result source or whatnot
      # can't deal with it
      undef $target_name;
    }
  }

  local $sm->{_modification_target_referenced_re} =
      qr/ (?<!DELETE) [\s\)] (?: FROM | JOIN ) \s (?: \` \Q$target_name\E \` | \Q$target_name\E ) [\s\(] /xi
    if $target_name;

  $self->next::method(@_);
}

# here may seem like an odd place to override, but this is the first
# method called after we are connected *and* the driver is determined
# ($self is reblessed). See code flow in ::Storage::DBI::_populate_dbh
sub _run_connection_actions {
  my $self = shift;

  # default mysql_auto_reconnect to off unless explicitly set
  if (
    $self->_dbh->{mysql_auto_reconnect}
      and
    ! exists $self->_dbic_connect_attributes->{mysql_auto_reconnect}
  ) {
    $self->_dbh->{mysql_auto_reconnect} = 0;
  }

  $self->next::method(@_);
}

# we need to figure out what mysql version we're running
sub sql_maker {
  my $self = shift;

  # it is critical to get the version *before* calling next::method
  # otherwise the potential connect will obliterate the sql_maker
  # next::method will populate in the _sql_maker accessor
  my $mysql_ver = $self->_server_info->{normalized_dbms_version};

  my $sm = $self->next::method(@_);

  # mysql 3 does not understand a bare JOIN
  $sm->{_default_jointype} = 'INNER' if $mysql_ver < 4;

  $sm;
}

sub sqlt_type {
  return 'MySQL';
}

sub deployment_statements {
  my $self = shift;
  my ($schema, $type, $version, $dir, $sqltargs, @rest) = @_;

  $sqltargs ||= {};

  if (
    ! exists $sqltargs->{producer_args}{mysql_version}
      and
    my $dver = $self->_server_info->{normalized_dbms_version}
  ) {
    $sqltargs->{producer_args}{mysql_version} = $dver;
  }

  $self->next::method($schema, $type, $version, $dir, $sqltargs, @rest);
}

sub _exec_svp_begin {
    my ($self, $name) = @_;

    $self->_dbh->do("SAVEPOINT $name");
}

sub _exec_svp_release {
    my ($self, $name) = @_;

    $self->_dbh->do("RELEASE SAVEPOINT $name");
}

sub _exec_svp_rollback {
    my ($self, $name) = @_;

    $self->_dbh->do("ROLLBACK TO SAVEPOINT $name")
}

sub is_replicating {
    my $status = shift->_get_dbh->selectrow_hashref('show slave status');
    return ($status->{Slave_IO_Running} eq 'Yes') && ($status->{Slave_SQL_Running} eq 'Yes');
}

sub lag_behind_master {
    return shift->_get_dbh->selectrow_hashref('show slave status')->{Seconds_Behind_Master};
}

1;

=head1 NAME

DBIx::Class::Storage::DBI::mysql - Storage::DBI class implementing MySQL specifics

=head1 SYNOPSIS

Storage::DBI autodetects the underlying MySQL database, and re-blesses the
C<$storage> object into this class.

  my $schema = MyApp::Schema->connect( $dsn, $user, $pass, { on_connect_call => 'set_strict_mode' } );

=head1 DESCRIPTION

This class implements MySQL specific bits of L<DBIx::Class::Storage::DBI>,
like AutoIncrement column support and savepoints. Also it augments the
SQL maker to support the MySQL-specific C<STRAIGHT_JOIN> join type, which
you can use by specifying C<< join_type => 'straight' >> in the
L<relationship attributes|DBIx::Class::Relationship::Base/join_type>


It also provides a one-stop on-connect macro C<set_strict_mode> which sets
session variables such that MySQL behaves more predictably as far as the
SQL standard is concerned.

=head1 STORAGE OPTIONS

=head2 set_strict_mode

Enables session-wide strict options upon connecting. Equivalent to:

  ->connect ( ... , {
    on_connect_do => [
      q|SET SQL_MODE = CONCAT('ANSI,TRADITIONAL,ONLY_FULL_GROUP_BY,', @@sql_mode)|,
      q|SET SQL_AUTO_IS_NULL = 0|,
    ]
  });

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.
