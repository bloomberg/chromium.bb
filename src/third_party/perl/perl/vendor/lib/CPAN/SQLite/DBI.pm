# $Id: DBI.pm 79 2019-01-30 02:35:31Z stro $

package CPAN::SQLite::DBI;
use strict;
use warnings;

our $VERSION = '0.217';

use English qw/-no_match_vars/;

require File::Spec;
use DBI;

use parent 'Exporter';
our ($dbh, $tables, @EXPORT_OK);
@EXPORT_OK = qw($dbh $tables);

$tables = {
  'info' => {
    'primary' => {
      'status' => q!INTEGER NOT NULL PRIMARY KEY!,
    },
    'other' => {},
    'key'   => [],
    'name'  => 'status',
    'id'    => 'status',
  },
  mods => {
    primary => { mod_id => q{INTEGER NOT NULL PRIMARY KEY} },
    other   => {
      mod_name => q{VARCHAR(100) NOT NULL},
      dist_id  => q{INTEGER NOT NULL},
      mod_abs  => q{TEXT},
      mod_vers => q{VARCHAR(10)},
    },
    key   => [qw/dist_id mod_name/],
    name  => 'mod_name',
    id    => 'mod_id',
    has_a => { dists => 'dist_id' },
  },
  dists => {
    primary => { dist_id => q{INTEGER NOT NULL PRIMARY KEY} },
    other   => {
      dist_name => q{VARCHAR(90) NOT NULL},
      auth_id   => q{INTEGER NOT NULL},
      dist_file => q{VARCHAR(110) NOT NULL},
      dist_vers => q{VARCHAR(20)},
      dist_abs  => q{TEXT},
    },
    key      => [qw/auth_id dist_name/],
    name     => 'dist_name',
    id       => 'dist_id',
    has_a    => { auths => 'auth_id' },
    has_many => { mods => 'dist_id', },
  },
  auths => {
    primary => { auth_id => q{INTEGER NOT NULL PRIMARY KEY} },
    other   => {
      cpanid   => q{VARCHAR(20) NOT NULL},
      fullname => q{VARCHAR(40) NOT NULL},
      email    => q{TEXT},
    },
    key      => [qw/cpanid/],
    has_many => { dists => 'dist_id' },
    name     => 'cpanid',
    id       => 'auth_id',
  },
};

sub new {
  my ($class, %args) = @_;
  my $db_dir = $args{db_dir} || $args{CPAN};
  my $db = File::Spec->catfile($db_dir, $args{db_name});
  $dbh ||= DBI->connect(
    "DBI:SQLite:$db",
    '', '',
    {
      RaiseError                       => 1,
      AutoCommit                       => 0,
      sqlite_use_immediate_transaction => 0,
    });
  die "Cannot connect to $db" unless $dbh;
  $dbh->{AutoCommit} = 0;

  my $objs;
  foreach my $table (keys %$tables) {
    my $cl = $class . '::' . $table;
    $objs->{$table} = $cl->make(table => $table);
  }

  for my $table (keys %$tables) {
    foreach my $type (qw(primary other)) {
      foreach my $column (keys %{ $tables->{$table}->{$type} }) {
        push @{ $tables->{$table}->{columns} }, $column;
      }
    }
  }

  return bless { objs => $objs }, $class;
}

sub make {
  my ($class, %args) = @_;
  my $table = $args{table};
  die qq{No table exists corresponding to '$class'} unless $table;
  my $info = $tables->{$table};
  die qq{No information available for table '$table'} unless $info;
  my $self = {
    table   => $table,
    columns => $info->{columns},
    id      => $info->{id},
    name    => $info->{name},
  };
  foreach (qw(name has_a has_many)) {
    next unless defined $info->{$_};
    $self->{$_} = $info->{$_};
  }
  return bless $self, $class;
}

sub db_error {
  my ($obj, $sth) = @_;
  return unless $dbh;
  if ($sth) {
    $sth->finish;
    undef $sth;
  }
  return $obj->{error_msg} = q{Database error: } . $dbh->errstr;
}

1;

=head1 NAME

CPAN::SQLite::DBI - DBI information for the CPAN::SQLite database

=head1 VERSION

version 0.217

=head1 DESCRIPTION

This module is used by L<CPAN::SQLite::Index> and
L<CPAN::SQLite::Search> to set up some basic database
information. It exports two variables:

=over 3

=item C<$tables>

This is a hash reference whose keys are the table names, with
corresponding values being hash references whose keys are the
columns of the table and values being the associated data types.

=item C<$dbh>

This is a L<DBI> database handle used to connect to the
database.

=back

The main method of this module is C<make>, which is used
to make the tables of the database.

=head1 SEE ALSO

L<CPAN::SQLite::Index> and L<CPAN::SQLite::Search>

=cut
