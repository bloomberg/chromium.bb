package DBIx::Class::Storage::DBI::ODBC::Microsoft_SQL_Server;
use strict;
use warnings;

use base qw/
  DBIx::Class::Storage::DBI::ODBC
  DBIx::Class::Storage::DBI::MSSQL
/;
use mro 'c3';
use Scalar::Util 'reftype';
use Try::Tiny;
use DBIx::Class::Carp;
use namespace::clean;

__PACKAGE__->mk_group_accessors(simple => qw/
  _using_dynamic_cursors
/);

=head1 NAME

DBIx::Class::Storage::DBI::ODBC::Microsoft_SQL_Server - Support specific
to Microsoft SQL Server over ODBC

=head1 DESCRIPTION

This class implements support specific to Microsoft SQL Server over ODBC.  It is
loaded automatically by DBIx::Class::Storage::DBI::ODBC when it detects a
MSSQL back-end.

Most of the functionality is provided from the superclass
L<DBIx::Class::Storage::DBI::MSSQL>.

=head1 USAGE NOTES

=head2 Basic Linux Setup (Debian)

  sudo aptitude install tdsodbc libdbd-odbc-perl unixodbc

In case it is not already there put the following (adjust for non-64bit arch) in
C</etc/odbcinst.ini>:

  [FreeTDS]
  Description = FreeTDS
  Driver      = /usr/lib/x86_64-linux-gnu/odbc/libtdsodbc.so
  Setup       = /usr/lib/x86_64-linux-gnu/odbc/libtdsS.so
  UsageCount  = 1

Set your C<$dsn> in L<connect_info|DBIx::Class::Storage::DBI/connect_info> as follows:

  dbi:ODBC:server=<my.host.name>;port=1433;driver=FreeTDS;tds_version=8.0

If you use the EasySoft driver (L<http://www.easysoft.com>):

  dbi:ODBC:server=<my.host.name>;port=1433;driver=Easysoft ODBC-SQL Server

=head2 Basic Windows Setup

Use the following C<$dsn> for the Microsoft ODBC driver:

  dbi:ODBC:driver={SQL Server};server=SERVER\SQL_SERVER_INSTANCE_NAME

And for the Native Client:

  dbi:ODBC:driver={SQL Server Native Client 10.0};server=SERVER\SQL_SERVER_INSTANCE_NAME

Go into Control Panel -> System and Security -> Administrative Tools -> Data
Sources (ODBC) to check driver names and to set up data sources.

Use System DSNs, not User DSNs if you want to use DSNs.

If you set up a DSN, use the following C<$dsn> for
L<connect_info|DBIx::Class::Storage::DBI/connect_info>:

  dbi:ODBC:dsn=MY_DSN

=head1 MULTIPLE ACTIVE STATEMENTS

The following options are alternative ways to enable concurrent executing
statement support. Each has its own advantages and drawbacks and works on
different platforms. Read each section carefully.

For more details about using MAS in MSSQL over DBD::ODBC see this excellent
document provided by EasySoft:
L<http://www.easysoft.com/developer/languages/perl/multiple-active-statements.html>.

In order of preference, they are:

=over 8

=item * L<mars|/connect_call_use_mars>

=item * L<dynamic_cursors|/connect_call_use_dynamic_cursors>

=item * L<server_cursors|/connect_call_use_server_cursors>

=back

=head1 METHODS

=head2 connect_call_use_mars

Use as:

  on_connect_call => 'use_mars'

in your connection info, or alternatively specify it directly:

  Your::Schema->connect (
    $original_dsn . '; MARS_Connection=Yes',
    $user,
    $pass,
    \%attrs,
  )

Use to enable a feature of SQL Server 2005 and later, "Multiple Active Result
Sets". See L<DBD::ODBC::FAQ/Does DBD::ODBC support Multiple Active Statements?>
for more information.

This does not work on FreeTDS drivers at the time of this writing, and only
works with the Native Client, later versions of the Windows MS ODBC driver, and
the Easysoft driver.

=cut

sub connect_call_use_mars {
  my $self = shift;

  my $dsn = $self->_dbi_connect_info->[0];

  if (ref($dsn) eq 'CODE') {
    $self->throw_exception('cannot change the DBI DSN on a CODE ref connect_info');
  }

  if ($dsn !~ /MARS_Connection=/) {
    if ($self->_using_freetds) {
      $self->throw_exception('FreeTDS does not support MARS at the time of '
                            .'writing.');
    }

    if (exists $self->_server_info->{normalized_dbms_version} &&
               $self->_server_info->{normalized_dbms_version} < 9) {
      $self->throw_exception('SQL Server 2005 or later required to use MARS.');
    }

    if (my ($data_source) = $dsn =~ /^dbi:ODBC:([\w-]+)\z/i) { # prefix with DSN
      carp_unique "Bare DSN in ODBC connect string, rewriting as 'dsn=$data_source'"
          ." for MARS\n";
      $dsn = "dbi:ODBC:dsn=$data_source";
    }

    $self->_dbi_connect_info->[0] = "$dsn;MARS_Connection=Yes";
    $self->disconnect;
    $self->ensure_connected;
  }
}

sub connect_call_use_MARS {
  carp "'connect_call_use_MARS' has been deprecated, use "
      ."'connect_call_use_mars' instead.";
  shift->connect_call_use_mars(@_)
}

=head2 connect_call_use_dynamic_cursors

Use as:

  on_connect_call => 'use_dynamic_cursors'

Which will add C<< odbc_cursortype => 2 >> to your DBI connection
attributes, or alternatively specify the necessary flag directly:

  Your::Schema->connect (@dsn, { ... odbc_cursortype => 2 })

See L<DBD::ODBC/odbc_cursortype> for more information.

If you're using FreeTDS, C<tds_version> must be set to at least C<8.0>.

This will not work with CODE ref connect_info's.

B<WARNING:> on FreeTDS (and maybe some other drivers) this will break
C<SCOPE_IDENTITY()>, and C<SELECT @@IDENTITY> will be used instead, which on SQL
Server 2005 and later will return erroneous results on tables which have an on
insert trigger that inserts into another table with an C<IDENTITY> column.

B<WARNING:> on FreeTDS, changes made in one statement (e.g. an insert) may not
be visible from a following statement (e.g. a select.)

B<WARNING:> FreeTDS versions > 0.82 seem to have completely broken the ODBC
protocol. DBIC will not allow dynamic cursor support with such versions to
protect your data. Please hassle the authors of FreeTDS to act on the bugs that
make their driver not overly usable with DBD::ODBC.

=cut

sub connect_call_use_dynamic_cursors {
  my $self = shift;

  if (($self->_dbic_connect_attributes->{odbc_cursortype} || 0) < 2) {

    my $dbi_inf = $self->_dbi_connect_info;

    $self->throw_exception ('Cannot set DBI attributes on a CODE ref connect_info')
      if ref($dbi_inf->[0]) eq 'CODE';

    # reenter connection information with the attribute re-set
    $dbi_inf->[3] = {} if @$dbi_inf <= 3;
    $dbi_inf->[3]{odbc_cursortype} = 2;

    $self->_dbi_connect_info($dbi_inf);

    $self->disconnect; # resetting dbi attrs, so have to reconnect
    $self->ensure_connected;
  }
}

sub _run_connection_actions {
  my $self = shift;

  $self->next::method (@_);

  # keep the dynamic_cursors_support and driver-state in sync
  # on every reconnect
  my $use_dyncursors = ($self->_dbic_connect_attributes->{odbc_cursortype} || 0) > 1;
  if (
    $use_dyncursors
      xor
    !!$self->_using_dynamic_cursors
  ) {
    if ($use_dyncursors) {
      try {
        my $dbh = $self->_dbh;
        local $dbh->{RaiseError} = 1;
        local $dbh->{PrintError} = 0;
        $dbh->do('SELECT @@IDENTITY');
      } catch {
        $self->throw_exception (
          'Your drivers do not seem to support dynamic cursors (odbc_cursortype => 2).'
         . (
          $self->_using_freetds
            ? ' If you are using FreeTDS, make sure to set tds_version to 8.0 or greater.'
            : ''
          )
        );
      };

      $self->_using_dynamic_cursors(1);
      $self->_identity_method('@@identity');
    }
    else {
      $self->_using_dynamic_cursors(0);
      $self->_identity_method(undef);
    }
  }

  $self->_no_scope_identity_query($self->_using_dynamic_cursors
    ? $self->_using_freetds
    : undef
  );

  # freetds is too damn broken, some fixups
  if ($self->_using_freetds) {

    # no dynamic cursors starting from 0.83
    if ($self->_using_dynamic_cursors) {
      my $fv = $self->_using_freetds_version || 999;  # assume large if can't be determined
      $self->throw_exception(
        'Dynamic cursors (odbc_cursortype => 2) are not supported with FreeTDS > 0.82 '
      . "(you have $fv). Please hassle FreeTDS authors to fix the outstanding bugs in "
      . 'their driver.'
      ) if $fv > 0.82
    }

    # FreeTDS is too broken wrt execute_for_fetch batching
    # just disable it outright until things quiet down
    $self->_disable_odbc_array_ops;
  }
}

=head2 connect_call_use_server_cursors

Use as:

  on_connect_call => 'use_server_cursors'

May allow multiple active select statements. See
L<DBD::ODBC/odbc_SQL_ROWSET_SIZE> for more information.

Takes an optional parameter for the value to set the attribute to, default is
C<2>.

B<WARNING>: this does not work on all versions of SQL Server, and may lock up
your database!

At the time of writing, this option only works on Microsoft's Windows drivers,
later versions of the ODBC driver and the Native Client driver.

=cut

sub connect_call_use_server_cursors {
  my $self            = shift;
  my $sql_rowset_size = shift || 2;

  if ($^O !~ /win32|cygwin/i) {
    $self->throw_exception('Server cursors only work on Windows platforms at '
                          .'the time of writing.');
  }

  $self->_get_dbh->{odbc_SQL_ROWSET_SIZE} = $sql_rowset_size;
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

# vim:sw=2 sts=2 et
