package DBIx::Class::Storage::DBI::SQLite;

use strict;
use warnings;

use base qw/DBIx::Class::Storage::DBI/;
use mro 'c3';

use SQL::Abstract 'is_plain_value';
use DBIx::Class::_Util qw(modver_gt_or_eq sigwarn_silencer);
use DBIx::Class::Carp;
use Try::Tiny;
use namespace::clean;

__PACKAGE__->sql_maker_class('DBIx::Class::SQLMaker::SQLite');
__PACKAGE__->sql_limit_dialect ('LimitOffset');
__PACKAGE__->sql_quote_char ('"');
__PACKAGE__->datetime_parser_type ('DateTime::Format::SQLite');

=head1 NAME

DBIx::Class::Storage::DBI::SQLite - Automatic primary key class for SQLite

=head1 SYNOPSIS

  # In your table classes
  use base 'DBIx::Class::Core';
  __PACKAGE__->set_primary_key('id');

=head1 DESCRIPTION

This class implements autoincrements for SQLite.

=head2 Known Issues

=over

=item RT79576

 NOTE - This section applies to you only if ALL of these are true:

  * You are or were using DBD::SQLite with a version lesser than 1.38_01

  * You are or were using DBIx::Class versions between 0.08191 and 0.08209
    (inclusive) or between 0.08240-TRIAL and 0.08242-TRIAL (also inclusive)

  * You use objects with overloaded stringification and are feeding them
    to DBIC CRUD methods directly

An unfortunate chain of events led to DBIx::Class silently hitting the problem
described in L<RT#79576|https://rt.cpan.org/Public/Bug/Display.html?id=79576>.

In order to trigger the bug condition one needs to supply B<more than one>
bind value that is an object with overloaded stringification (numification
is not relevant, only stringification is). When this is the case the internal
DBIx::Class call to C<< $sth->bind_param >> would be executed in a way that
triggers the above-mentioned DBD::SQLite bug. As a result all the logs and
tracers will contain the expected values, however SQLite will receive B<all>
these bind positions being set to the value of the B<last> supplied
stringifiable object.

Even if you upgrade DBIx::Class (which works around the bug starting from
version 0.08210) you may still have corrupted/incorrect data in your database.
DBIx::Class warned about this condition for several years, hoping to give
anyone affected sufficient notice of the potential issues. The warning was
removed in 2015/v0.082820.

=back

=head1 METHODS

=cut

sub backup {

  require File::Spec;
  require File::Copy;
  require POSIX;

  my ($self, $dir) = @_;
  $dir ||= './';

  ## Where is the db file?
  my $dsn = $self->_dbi_connect_info()->[0];

  my $dbname = $1 if($dsn =~ /dbname=([^;]+)/);
  if(!$dbname)
  {
    $dbname = $1 if($dsn =~ /^dbi:SQLite:(.+)$/i);
  }
  $self->throw_exception("Cannot determine name of SQLite db file")
    if(!$dbname || !-f $dbname);

#  print "Found database: $dbname\n";
#  my $dbfile = file($dbname);
  my ($vol, $dbdir, $file) = File::Spec->splitpath($dbname);
#  my $file = $dbfile->basename();
  $file = POSIX::strftime("%Y-%m-%d-%H_%M_%S", localtime()) . $file;
  $file = "B$file" while(-f $file);

  mkdir($dir) unless -f $dir;
  my $backupfile = File::Spec->catfile($dir, $file);

  my $res = File::Copy::copy($dbname, $backupfile);
  $self->throw_exception("Backup failed! ($!)") if(!$res);

  return $backupfile;
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

  $self->_dbh->do("ROLLBACK TO SAVEPOINT $name");

  # resync state for older DBD::SQLite (RT#67843)
  # https://github.com/DBD-SQLite/DBD-SQLite/commit/9b3cdbf
  if (
    ! modver_gt_or_eq('DBD::SQLite', '1.33')
      and
    $self->_dbh->FETCH('AutoCommit')
  ) {
    $self->_dbh->STORE('AutoCommit', 0);
    $self->_dbh->STORE('BegunWork', 1);
  }
}

sub _ping {
  my $self = shift;

  # Be extremely careful what we do here. SQLite is notoriously bad at
  # synchronizing its internal transaction state with {AutoCommit}
  # https://metacpan.org/source/ADAMK/DBD-SQLite-1.37/lib/DBD/SQLite.pm#L921
  # There is a function http://www.sqlite.org/c3ref/get_autocommit.html
  # but DBD::SQLite does not expose it (nor does it seem to properly use it)

  # Therefore only execute a "ping" when we have no other choice *AND*
  # scrutinize the thrown exceptions to make sure we are where we think we are
  my $dbh = $self->_dbh or return undef;
  return undef unless $dbh->FETCH('Active');
  return undef unless $dbh->ping;

  my $ping_fail;

  # older DBD::SQLite does not properly synchronize commit state between
  # the libsqlite and the $dbh
  unless (defined $DBD::SQLite::__DBIC_TXN_SYNC_SANE__) {
    $DBD::SQLite::__DBIC_TXN_SYNC_SANE__ = modver_gt_or_eq('DBD::SQLite', '1.38_02');
  }

  # fallback to travesty
  unless ($DBD::SQLite::__DBIC_TXN_SYNC_SANE__) {
    # since we do not have access to sqlite3_get_autocommit(), do a trick
    # to attempt to *safely* determine what state are we *actually* in.
    # FIXME
    # also using T::T here leads to bizarre leaks - will figure it out later
    my $really_not_in_txn = do {
      local $@;

      # older versions of DBD::SQLite do not properly detect multiline BEGIN/COMMIT
      # statements to adjust their {AutoCommit} state. Hence use such a statement
      # pair here as well, in order to escape from poking {AutoCommit} needlessly
      # https://rt.cpan.org/Public/Bug/Display.html?id=80087
      eval {
        # will fail instantly if already in a txn
        $dbh->do("-- multiline\nBEGIN");
        $dbh->do("-- multiline\nCOMMIT");
        1;
      } or do {
        ($@ =~ /transaction within a transaction/)
          ? 0
          : undef
        ;
      };
    };

    # if we were unable to determine this - we may very well be dead
    if (not defined $really_not_in_txn) {
      $ping_fail = 1;
    }
    # check the AC sync-state
    elsif ($really_not_in_txn xor $dbh->{AutoCommit}) {
      carp_unique (sprintf
        'Internal transaction state of handle %s (apparently %s a transaction) does not seem to '
      . 'match its AutoCommit attribute setting of %s - this is an indication of a '
      . 'potentially serious bug in your transaction handling logic',
        $dbh,
        $really_not_in_txn ? 'NOT in' : 'in',
        $dbh->{AutoCommit} ? 'TRUE' : 'FALSE',
      );

      # it is too dangerous to execute anything else in this state
      # assume everything works (safer - worst case scenario next statement throws)
      return 1;
    }
  }

  # do the actual test and return on no failure
  ( $ping_fail ||= ! try { $dbh->do('SELECT * FROM sqlite_master LIMIT 1'); 1 } )
    or return 1; # the actual RV of _ping()

  # ping failed (or so it seems) - need to do some cleanup
  # it is possible to have a proper "connection", and have "ping" return
  # false anyway (e.g. corrupted file). In such cases DBD::SQLite still
  # keeps the actual file handle open. We don't really want this to happen,
  # so force-close the handle via DBI itself
  #
  local $@; # so that we do not clobber the real error as set above
  eval { $dbh->disconnect }; # if it fails - it fails
  undef; # the actual RV of _ping()
}

sub deployment_statements {
  my $self = shift;
  my ($schema, $type, $version, $dir, $sqltargs, @rest) = @_;

  $sqltargs ||= {};

  if (
    ! exists $sqltargs->{producer_args}{sqlite_version}
      and
    my $dver = $self->_server_info->{normalized_dbms_version}
  ) {
    $sqltargs->{producer_args}{sqlite_version} = $dver;
  }

  $self->next::method($schema, $type, $version, $dir, $sqltargs, @rest);
}

sub bind_attribute_by_data_type {

  # According to http://www.sqlite.org/datatype3.html#storageclasses
  # all numeric types are dynamically allocated up to 8 bytes per
  # individual value
  # Thus it should be safe and non-wasteful to bind everything as
  # SQL_BIGINT and have SQLite deal with storage/comparisons however
  # it deems correct
  $_[1] =~ /^ (?: int(?:[1248]|eger)? | (?:tiny|small|medium|big)int ) $/ix
    ? DBI::SQL_BIGINT()
    : undef
  ;
}

# FIXME - what the flying fuck... work around RT#76395
# DBD::SQLite warns on binding >32 bit values with 32 bit IVs
sub _dbh_execute {
  if (
    (
      DBIx::Class::_ENV_::IV_SIZE < 8
        or
      DBIx::Class::_ENV_::OS_NAME eq 'MSWin32'
    )
      and
    ! defined $DBD::SQLite::__DBIC_CHECK_dbd_mishandles_bound_BIGINT
  ) {
    $DBD::SQLite::__DBIC_CHECK_dbd_mishandles_bound_BIGINT = (
      modver_gt_or_eq('DBD::SQLite', '1.37')
    ) ? 1 : 0;
  }

  local $SIG{__WARN__} = sigwarn_silencer( qr/
    \Qdatatype mismatch: bind\E \s (?:
      param \s+ \( \d+ \) \s+ [-+]? \d+ (?: \. 0*)? \Q as integer\E
        |
      \d+ \s type \s @{[ DBI::SQL_BIGINT() ]} \s as \s [-+]? \d+ (?: \. 0*)?
    )
  /x ) if (
    (
      DBIx::Class::_ENV_::IV_SIZE < 8
        or
      DBIx::Class::_ENV_::OS_NAME eq 'MSWin32'
    )
      and
    $DBD::SQLite::__DBIC_CHECK_dbd_mishandles_bound_BIGINT
  );

  shift->next::method(@_);
}

# DBD::SQLite (at least up to version 1.31 has a bug where it will
# non-fatally numify a string value bound as an integer, resulting
# in insertions of '0' into supposed-to-be-numeric fields
# Since this can result in severe data inconsistency, remove the
# bind attr if such a situation is detected
#
# FIXME - when a DBD::SQLite version is released that eventually fixes
# this situation (somehow) - no-op this override once a proper DBD
# version is detected
sub _dbi_attrs_for_bind {
  my ($self, $ident, $bind) = @_;

  my $bindattrs = $self->next::method($ident, $bind);

  if (! defined $DBD::SQLite::__DBIC_CHECK_dbd_can_bind_bigint_values) {
    $DBD::SQLite::__DBIC_CHECK_dbd_can_bind_bigint_values
      = modver_gt_or_eq('DBD::SQLite', '1.37') ? 1 : 0;
  }

  for my $i (0.. $#$bindattrs) {
    if (
      defined $bindattrs->[$i]
        and
      defined $bind->[$i][1]
        and
      grep { $bindattrs->[$i] eq $_ } (
        DBI::SQL_INTEGER(), DBI::SQL_TINYINT(), DBI::SQL_SMALLINT(), DBI::SQL_BIGINT()
      )
    ) {
      if ( $bind->[$i][1] !~ /^ [\+\-]? [0-9]+ (?: \. 0* )? $/x ) {
        carp_unique( sprintf (
          "Non-integer value supplied for column '%s' despite the integer datatype",
          $bind->[$i][0]{dbic_colname} || "# $i"
        ) );
        undef $bindattrs->[$i];
      }
      elsif (
        ! $DBD::SQLite::__DBIC_CHECK_dbd_can_bind_bigint_values
      ) {
        # unsigned 32 bit ints have a range of −2,147,483,648 to 2,147,483,647
        # alternatively expressed as the hexadecimal numbers below
        # the comparison math will come out right regardless of ivsize, since
        # we are operating within 31 bits
        # P.S. 31 because one bit is lost for the sign
        if ($bind->[$i][1] > 0x7fff_ffff or $bind->[$i][1] < -0x8000_0000) {
          carp_unique( sprintf (
            "An integer value occupying more than 32 bits was supplied for column '%s' "
          . 'which your version of DBD::SQLite (%s) can not bind properly so DBIC '
          . 'will treat it as a string instead, consider upgrading to at least '
          . 'DBD::SQLite version 1.37',
            $bind->[$i][0]{dbic_colname} || "# $i",
            DBD::SQLite->VERSION,
          ) );
          undef $bindattrs->[$i];
        }
        else {
          $bindattrs->[$i] = DBI::SQL_INTEGER()
        }
      }
    }
  }

  return $bindattrs;
}

=head2 connect_call_use_foreign_keys

Used as:

    on_connect_call => 'use_foreign_keys'

In L<connect_info|DBIx::Class::Storage::DBI/connect_info> to turn on foreign key
(including cascading) support for recent versions of SQLite and L<DBD::SQLite>.

Executes:

  PRAGMA foreign_keys = ON

See L<http://www.sqlite.org/foreignkeys.html> for more information.

=cut

sub connect_call_use_foreign_keys {
  my $self = shift;

  $self->_do_query(
    'PRAGMA foreign_keys = ON'
  );
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
