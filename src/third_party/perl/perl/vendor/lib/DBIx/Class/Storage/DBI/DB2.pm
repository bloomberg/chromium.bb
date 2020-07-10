package DBIx::Class::Storage::DBI::DB2;

use strict;
use warnings;

use base qw/DBIx::Class::Storage::DBI/;
use mro 'c3';
use Try::Tiny;
use namespace::clean;

__PACKAGE__->datetime_parser_type('DateTime::Format::DB2');
__PACKAGE__->sql_quote_char ('"');

# lazy-default kind of thing
sub sql_name_sep {
  my $self = shift;

  my $v = $self->next::method(@_);

  if (! defined $v and ! @_) {
    $v = $self->next::method($self->_dbh_get_info('SQL_QUALIFIER_NAME_SEPARATOR') || '.');
  }

  return $v;
}

sub sql_limit_dialect {
  my $self = shift;

  my $v = $self->next::method(@_);

  if (! defined $v and ! @_) {
    $v = $self->next::method(
      ($self->_server_info->{normalized_dbms_version}||0) >= 5.004
        ? 'RowNumberOver'
        : 'FetchFirst'
    );
  }

  return $v;
}

sub _dbh_last_insert_id {
  my ($self, $dbh, $source, $col) = @_;

  my $name_sep = $self->sql_name_sep;

  my $sth = $dbh->prepare_cached(
    # An older equivalent of 'VALUES(IDENTITY_VAL_LOCAL())', for compat
    # with ancient DB2 versions. Should work on modern DB2's as well:
    # http://publib.boulder.ibm.com/infocenter/db2luw/v8/topic/com.ibm.db2.udb.doc/admin/r0002369.htm?resultof=%22%73%79%73%64%75%6d%6d%79%31%22%20
    "SELECT IDENTITY_VAL_LOCAL() FROM sysibm${name_sep}sysdummy1",
    {},
    3
  );
  $sth->execute();

  my @res = $sth->fetchrow_array();

  return @res ? $res[0] : undef;
}

=head1 NAME

DBIx::Class::Storage::DBI::DB2 - IBM DB2 support for DBIx::Class

=head1 DESCRIPTION

This class implements autoincrements for DB2, sets the limit dialect to
RowNumberOver over FetchFirst depending on the availability of support for
RowNumberOver, queries the server name_sep from L<DBI> and sets the L<DateTime>
parser to L<DateTime::Format::DB2>.

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
