# $Id: Populate.pm 35 2011-06-17 01:34:42Z stro $

package CPAN::SQLite::Populate;
use strict;
use warnings;
no warnings qw(redefine);
use CPAN::SQLite::Util qw($table_id has_hash_data print_debug);
use CPAN::SQLite::DBI::Index;
use CPAN::SQLite::DBI qw($dbh);
use File::Find;
use File::Basename;
use File::Spec::Functions;
use File::Path;

our $dbh = $CPAN::SQLite::DBI::dbh;
my ($setup);
our $VERSION = '0.202';

my %tbl2obj;
$tbl2obj{$_} = __PACKAGE__ . '::' . $_ 
    for (qw(dists mods auths chaps));
my %obj2tbl  = reverse %tbl2obj;

sub new {
  my ($class, %args) = @_;

  $setup = $args{setup};

  my $index = $args{index};
  my @tables = qw(dists mods auths);
  foreach my $table (@tables) {
    my $obj = $index->{$table};
    die "Please supply a CPAN::SQLite::Index::$table object"
      unless ($obj and ref($obj) eq "CPAN::SQLite::Index::$table");
  }
  my $state = $args{state};
  unless ($setup) {
    die "Please supply a CPAN::SQLite::State object"
      unless ($state and ref($state) eq 'CPAN::SQLite::State');
  }
  my $cdbi = CPAN::SQLite::DBI::Index->new(%args);

  my $self = {index => $index,
              state => $state,
              obj => {},
              cdbi => $cdbi,
              db_name => $args{db_name},
             };
  return bless $self, $class;
}

sub populate {
  my $self = shift;

  if ($setup) {
    unless ($self->{cdbi}->create_tables(setup => $setup)) {
      warn "Creating tables failed";
      return;
    }
  }
  unless ($self->create_objs()) {
    warn "Cannot create objects";
    return;
  }
  unless ($self->populate_tables()) {
    warn "Populating tables failed";
    return;
  }
  return 1;
}

sub create_objs {
  my $self = shift;
  my @tables = qw(dists auths mods chaps);

  foreach my $table (@tables) {
    my $obj;
    my $pack = $tbl2obj{$table};
    my $index = $self->{index}->{$table};
    if ($index and ref($index) eq "CPAN::SQLite::Index::$table") {
      my $info = $index->{info};
      return unless has_hash_data($info);
      $obj = $pack->new(info => $info,
                        cdbi => $self->{cdbi}->{objs}->{$table});
    }
    else {
      $obj = $pack->new(cdbi => $self->{cdbi}->{objs}->{$table});
    }
    $self->{obj}->{$table} = $obj;
  }
  foreach my $table (@tables) {
    my $obj = $self->{obj}->{$table};
    foreach (@tables) {
      next if ref($obj) eq $tbl2obj{$_};
      $obj->{obj}->{$_} = $self->{obj}->{$_};
    }
  }

  unless ($setup) {
    my $state = $self->{state};
    my @tables = qw(auths dists mods);
    my @data = qw(ids insert update delete);

    foreach my $table (@tables) {
      my $state_obj = $state->{obj}->{$table};
      my $pop_obj = $self->{obj}->{$table};
      $pop_obj->{$_} = $state_obj->{$_} for (@data);
    }
  }
  return 1;
}

sub populate_tables {
  my $self = shift;
  my @methods = $setup ? qw(insert) : qw(insert update delete);
  my @tables = qw(auths dists mods chaps);
  for my $method (@methods) {
    for my $table (@tables) {
      my $obj = $self->{obj}->{$table};
      unless ($obj->$method()) {
        if (my $error = $obj->{error_msg}) {
          print_debug("Fatal error from ", ref($obj), ": ", $error, $/);
          return;
        }
        else {
          my $info = $obj->{info_msg};
          print_debug("Info from ", ref($obj), ": ", $info, $/);
        }
      }
    }
  }
  return 1;
}

package CPAN::SQLite::Populate::auths;
use base qw(CPAN::SQLite::Populate);
use CPAN::SQLite::Util qw(has_hash_data print_debug);

sub new {
  my ($class, %args) = @_;
  my $info = $args{info};
  die "No author info available" unless has_hash_data($info);
  my $cdbi = $args{cdbi};
  die "No dbi object available"
    unless ($cdbi and ref($cdbi) eq 'CPAN::SQLite::DBI::Index::auths');
  my $self = {
              info => $info,
              insert => {},
              update => {},
              delete => {},
              ids => {},
              obj => {},
              cdbi => $cdbi,
              error_msg => '',
              info_msg => '',
             };
  return bless $self, $class;
}

sub insert {
  my $self = shift;
  unless ($dbh) {
    $self->{error_msg} = q{No db handle available};
    return;
  }
  my $info = $self->{info};
  my $cdbi = $self->{cdbi};
  my $data = $setup ? $info : $self->{insert};
  unless (has_hash_data($data)) {
    $self->{info_msg} = q{No author data to insert};
    return;
  }
  my $auth_ids = $self->{ids};
  my @fields = qw(cpanid email fullname);
  my $sth = $cdbi->sth_insert(\@fields) or do {
    $self->{error_msg} = $cdbi->{error_msg};
    return;
  };
  foreach my $cpanid (keys %$data) {
    my $values = $info->{$cpanid};
    next unless ($values and $cpanid);
    print_debug("Inserting author $cpanid\n");
    $sth->execute($cpanid, $values->{email}, $values->{fullname})
      or do {
        $cdbi->db_error($sth);
        $self->{error_msg} = $cdbi->{error_msg};
        return;
      };
    $auth_ids->{$cpanid} = 
      $dbh->func('last_insert_rowid') or do {
        $cdbi->db_error($sth);
        $self->{error_msg} = $cdbi->{error_msg};
        return;
      };
  }
  $sth->finish();
  undef $sth;
  $dbh->commit() or do {
    $cdbi->db_error();
    $self->{error_msg} = $cdbi->{error_msg};
    return;
  };
  return 1;
}

sub update {
  my $self = shift;
  unless ($dbh) {
    $self->{error_msg} = q{No db handle available};
    return;
  }
  my $data = $self->{update};
  my $cdbi = $self->{cdbi};
  unless (has_hash_data($data)) {
    $self->{info_msg} = q{No author data to update};
    return;
  }

  my $info = $self->{info};
  my @fields = qw(cpanid email fullname);
  foreach my $cpanid (keys %$data) {
    print_debug("Updating author $cpanid\n");
    next unless $data->{$cpanid};
    my $sth = $cdbi->sth_update(\@fields, $data->{$cpanid});
    my $values = $info->{$cpanid};
    next unless ($cpanid and $values);
    $sth->execute($cpanid, $values->{email}, $values->{fullname})
      or do {
        $cdbi->db_error($sth);
        $self->{error_msg} = $cdbi->{error_msg};
        return;
      };
    $sth->finish();
    undef $sth;
  }
  $dbh->commit() or do {
    $cdbi->db_error();
    $self->{error_msg} = $cdbi->{error_msg};
    return;
  };
  return 1;
}

sub delete {
  my $self = shift;
  $self->{info_msg} = q{No author data to delete};
  return;
}

package CPAN::SQLite::Populate::dists;
use base qw(CPAN::SQLite::Populate);
use CPAN::SQLite::Util qw(has_hash_data print_debug);

sub new {
  my ($class, %args) = @_;
  my $info = $args{info};
  die "No dist info available" unless has_hash_data($info);
  my $cdbi = $args{cdbi};
  die "No dbi object available"
    unless ($cdbi and ref($cdbi) eq 'CPAN::SQLite::DBI::Index::dists');
  my $self = {
              info => $info,
              insert => {},
              update => {},
              delete => {},
              ids => {},
              obj => {},
              cdbi => $cdbi,
              error_msg => '',
              info_msg => '',
  };
  return bless $self, $class;
}

sub insert {
  my $self = shift;
  unless ($dbh) {
    $self->{error_msg} = q{No db handle available};
    return;
  }
  return unless my $auth_obj = $self->{obj}->{auths};
  my $cdbi = $self->{cdbi};
  my $auth_ids = $auth_obj->{ids};
  my $dists = $self->{info};
  my $data = $setup ? $dists : $self->{insert};
  unless (has_hash_data($data)) {
    $self->{info_msg} = q{No dist data to insert};
    return;
  }
  unless ($dists and $auth_ids) {
    $self->{error_msg}->{index} = q{No dist index data available};
    return;
  }

  my $dist_ids = $self->{ids};
  my @fields = qw(auth_id dist_name dist_file dist_vers dist_abs dist_dslip);
  my $sth = $cdbi->sth_insert(\@fields) or do {
    $self->{error_msg} = $cdbi->{error_msg};
    return;
  };
  foreach my $distname (keys %$data) {
    my $values = $dists->{$distname};
    my $cpanid = $values->{cpanid};
    next unless ($values and $cpanid and $auth_ids->{$cpanid});
    print_debug("Inserting $distname of $cpanid\n");
    $sth->execute($auth_ids->{$values->{cpanid}}, $distname,
                    $values->{dist_file}, $values->{dist_vers},
                    $values->{dist_abs}, $values->{dslip}) or do {
                      $cdbi->db_error($sth);
                      $self->{error_msg} = $cdbi->{error_msg};
                      return;
                    };
    $dist_ids->{$distname} = $dbh->func('last_insert_rowid') or do {
      $cdbi->db_error($sth);
      $self->{error_msg} = $cdbi->{error_msg};
      return;
    };
  }
  $sth->finish();
  undef $sth;
  $dbh->commit() or do {
    $cdbi->db_error();
    $self->{error_msg} = $cdbi->{error_msg};
    return;
  };
  return 1;
}

sub update {
  my $self = shift;
  unless ($dbh) {
    $self->{error_msg} = q{No db handle available};
    return;
  }
  my $cdbi = $self->{cdbi};
  my $data = $self->{update};
  unless (has_hash_data($data)) {
    $self->{info_msg} = q{No dist data to update};
    return;
  }
  return unless my $auth_obj = $self->{obj}->{auths};
  my $auth_ids = $auth_obj->{ids};
  my $dists = $self->{info};
  unless ($dists and $auth_ids) {
    $self->{error_msg} = q{No dist index data available};
    return;
  }

  my @fields = qw(auth_id dist_name dist_file dist_vers dist_abs dist_dslip);
  foreach my $distname (keys %$data) {
    next unless $data->{$distname};
    my $sth = $cdbi->sth_update(\@fields, $data->{$distname});
    my $values = $dists->{$distname};
    my $cpanid = $values->{cpanid};
    next unless ($values and $cpanid and $auth_ids->{$cpanid});
    print_debug("Updating $distname of $cpanid\n");
    $sth->execute($auth_ids->{$values->{cpanid}}, $distname, 
                  $values->{dist_file}, $values->{dist_vers}, 
                  $values->{dist_abs}, $values->{dslip}) or do {
                    $cdbi->db_error($sth);
                    $self->{error_msg} = $cdbi->{error_msg};
                    return;
                  };
    $sth->finish();
    undef $sth;
  }
  $dbh->commit() or do {
    $cdbi->db_error();
    $self->{error_msg} = $cdbi->{error_msg};
    return;
  };
  return 1;
}

sub delete {
  my $self = shift;
  unless ($dbh) {
    $self->{error_msg} = q{No db handle available};
    return;
  }
  my $cdbi = $self->{cdbi};
  my $data = $self->{delete};
  unless (has_hash_data($data)) {
    $self->{info_msg} = q{No dist data to delete};
    return;
  }

  my $sth = $cdbi->sth_delete('dist_id');
  foreach my $distname(keys %$data) {
    print_debug("Deleting $distname\n");
    $sth->execute($data->{$distname}) or do {
      $cdbi->db_error($sth);
      $self->{error_msg} = $cdbi->{error_msg};
      return;
    };
  }
  $sth->finish();
  undef $sth;
  $dbh->commit() or do {
    $cdbi->db_error();
    $self->{error_msg} = $cdbi->{error_msg};
    return;
  };
  return 1;
}

package CPAN::SQLite::Populate::mods;
use base qw(CPAN::SQLite::Populate);
use CPAN::SQLite::Util qw(has_hash_data print_debug);

sub new {
  my ($class, %args) = @_;
  my $info = $args{info};
  die "No module info available" unless has_hash_data($info);
  my $cdbi = $args{cdbi};
  die "No dbi object available"
    unless ($cdbi and ref($cdbi) eq 'CPAN::SQLite::DBI::Index::mods');
  my $self = {
              info => $info,
              insert => {},
              update => {},
              delete => {},
              ids => {},
              obj => {},
              cdbi => $cdbi,
              error_msg => '',
              info_msg => '',
             };
  return bless $self, $class;
}

sub insert {
  my $self = shift;
  unless ($dbh) {
    $self->{error_msg} = q{No db handle available};
    return;
  }
  return unless my $dist_obj = $self->{obj}->{dists};
  my $cdbi = $self->{cdbi};
  my $dist_ids = $dist_obj->{ids};
  my $mods = $self->{info};
  my $data = $setup ? $mods : $self->{insert};
  unless (has_hash_data($data)) {
    $self->{info_msg} = q{No module data to insert};
    return;
  }
  unless ($mods and $dist_ids) {
    $self->{error_msg} = q{No module index data available};
    return;
  }

  my $mod_ids = $self->{ids};
  my @fields = qw(dist_id mod_name mod_abs
                  mod_vers dslip chapterid);

  my $sth = $cdbi->sth_insert(\@fields) or do {
    $self->{error_msg} = $cdbi->{error_msg};
    return;
  };
  foreach my $modname(keys %$data) {
    my $values = $mods->{$modname};
    next unless ($values and $dist_ids->{$values->{dist_name}});
    $sth->execute($dist_ids->{$values->{dist_name}}, $modname,
                  $values->{mod_abs}, $values->{mod_vers},
                  $values->{dslip}, $values->{chapterid})
      or do {
          $cdbi->db_error($sth);
          $self->{error_msg} = $cdbi->{error_msg};
          return;
        };
    $mod_ids->{$modname} = $dbh->func('last_insert_rowid') or do {
      $cdbi->db_error($sth);
      $self->{error_msg} = $cdbi->{error_msg};
      return;
    };
  }
  $sth->finish();
  undef $sth;
  $dbh->commit() or do {
    $cdbi->db_error();
    $self->{error_msg} = $cdbi->{error_msg};
    return;
  };
  return 1;
}

sub update {
  my $self = shift;
  unless ($dbh) {
    $self->{error_msg} = q{No db handle available};
    return;
  }
  my $cdbi = $self->{cdbi};
  my $data = $self->{update};
  unless (has_hash_data($data)) {
    $self->{info_msg} = q{No module data to update};
    return;
  }
  return unless my $dist_obj = $self->{obj}->{dists};
  my $dist_ids = $dist_obj->{ids};
  my $mods = $self->{info};
  unless ($dist_ids and $mods) {
    $self->{error_msg} = q{No module index data available};
    return;
  }

  my @fields = qw(dist_id mod_name mod_abs
                  mod_vers dslip chapterid);

  foreach my $modname (keys %$data) {
    next unless $data->{$modname};
    print_debug("Updating $modname\n");
    my $sth = $cdbi->sth_update(\@fields, $data->{$modname});
    my $values = $mods->{$modname};
    next unless ($values and $dist_ids->{$values->{dist_name}});
    $sth->execute($dist_ids->{$values->{dist_name}}, $modname,
                  $values->{mod_abs}, $values->{mod_vers},
                  $values->{dslip}, $values->{chapterid})
      or do {
        $cdbi->db_error($sth);
        $self->{error_msg} = $cdbi->{error_msg};
        return;
      };
    $sth->finish();
    undef $sth;
  }
  $dbh->commit() or do {
    $cdbi->db_error();
    $self->{error_msg} = $cdbi->{error_msg};
    return;
  };
  return 1;
}

sub delete {
  my $self = shift;
  unless ($dbh) {
    $self->{error_msg} = q{No db handle available};
        return;
  }
  return unless my $dist_obj = $self->{obj}->{dists};
  my $cdbi = $self->{cdbi};
  my $data = $dist_obj->{delete};
  if (has_hash_data($data)) {
    my $sth = $cdbi->sth_delete('dist_id');
    foreach my $distname(keys %$data) {
      $sth->execute($data->{$distname}) or do {
        $cdbi->db_error($sth);
        $self->{error_msg} = $cdbi->{error_msg};
        return;
      };
    }
    $sth->finish();
    undef $sth;
  }

  $data = $self->{delete};
  if (has_hash_data($data)) {
    my $sth = $cdbi->sth_delete('mod_id');
    foreach my $modname(keys %$data) {
      $sth->execute($data->{$modname}) or do {
        $cdbi->db_error($sth);
        $self->{error_msg} = $cdbi->{error_msg};
        return;
      };
      print_debug("Deleting $modname\n");
    }
    $sth->finish;
    undef $sth;
  }
  $dbh->commit() or do {
    $cdbi->db_error();
    $self->{error_msg} = $cdbi->{error_msg};
    return;
  };
  return 1;
}

package CPAN::SQLite::Populate::chaps;
use base qw(CPAN::SQLite::Populate);
use CPAN::SQLite::Util qw(has_hash_data print_debug);

sub new {
  my ($class, %args) = @_;
  my $cdbi = $args{cdbi};
  die "No dbi object available"
    unless ($cdbi and ref($cdbi) eq 'CPAN::SQLite::DBI::Index::chaps');
  my $self = {
              obj => {},
              cdbi => $cdbi,
              error_msg => '',
              info_msg => '',
             };
  return bless $self, $class;
}

sub insert {
  my $self = shift;
  unless ($dbh) {
    $self->{error_msg} = q{No db handle available};
    return;
  }
  return unless my $dist_obj = $self->{obj}->{dists};
  my $cdbi = $self->{cdbi};
  my $dist_insert = $dist_obj->{insert};
  my $dists = $dist_obj->{info};
  my $dist_ids = $dist_obj->{ids};
  my $data = $setup ? $dists : $dist_insert;
  unless (has_hash_data($data)) {
    $self->{info_msg} = q{No chap data to insert};
    return;
  }
  unless ($dists and $dist_ids) {
    $self->{error_msg} = q{No chap index data available};
    return;
  }

  my @fields = qw(chapterid dist_id subchapter);
  my $sth = $cdbi->sth_insert(\@fields) or do {
    $self->{error_msg} = $cdbi->{error_msg};
    return;
  };
  foreach my $dist (keys %$data) {
    my $values = $dists->{$dist};
    next unless defined $values->{chapterid};
    foreach my $chap_id(keys %{$values->{chapterid}}) {
      foreach my $sub_chap(keys %{$values->{chapterid}->{$chap_id}}) {
        next unless $dist_ids->{$dist};
        $sth->execute($chap_id, $dist_ids->{$dist}, $sub_chap)
          or do {
            $cdbi->db_error($sth);
            $self->{error_msg} = $cdbi->{error_msg};
            return;
          };
      }
    }
  }
  $sth->finish();
  undef $sth;
  $dbh->commit() or do {
    $cdbi->db_error();
    $self->{error_msg} = $cdbi->{error_msg};
    return;
  };
  return 1;
}

sub update {
  my $self = shift;
  unless ($dbh) {
    $self->{error_msg} = q{No db handle available};
    return;
  }
  my $cdbi = $self->{cdbi};
  return unless my $dist_obj = $self->{obj}->{dists};
  my $dists = $dist_obj->{info};
  my $dist_ids = $dist_obj->{ids};
  my $data = $dist_obj->{update};
  unless (has_hash_data($data)) {
    $self->{info_msg} = q{No chap data to update};
    return;
  }
  unless ($dist_ids and $dists) {
    $self->{error_msg} = q{No chap index data available};
    return;
  }

  my $sth = $cdbi->sth_delete('dist_id');
  foreach my $distname(keys %$data) {
      next unless $data->{$distname};
      $sth->execute($data->{$distname}) or do {
          $cdbi->db_error($sth);
          $self->{error_msg} = $cdbi->{error_msg};
          return;
      };
  }
  $sth->finish();
  undef $sth;

  my @fields = qw(chapterid dist_id subchapter);
  $sth = $cdbi->sth_insert(\@fields);
  foreach my $dist (keys %$data) {
    my $values = $dists->{$dist};
    next unless defined $values->{chapterid};
    foreach my $chap_id(keys %{$values->{chapterid}}) {
      foreach my $sub_chap(keys %{$values->{chapterid}->{$chap_id}}) {
        next unless $dist_ids->{$dist};
        $sth->execute($chap_id, $dist_ids->{$dist}, $sub_chap)
          or do {
            $cdbi->db_error($sth);
            $self->{error_msg} = $cdbi->{error_msg};
            return;
          };
      }
    }
  }
  $sth->finish();
  undef $sth;
  $dbh->commit() or do {
    $cdbi->db_error();
    $self->{error_msg} = $cdbi->{error_msg};
    return;
  };
  return 1;
}

sub delete {
  my $self = shift;
  unless ($dbh) {
    $self->{error_msg} = q{No db handle available};
        return;
  }
  return unless my $dist_obj = $self->{obj}->{dists};
  my $cdbi = $self->{cdbi};
  my $data = $dist_obj->{delete};
  unless (has_hash_data($data)) {
    $self->{info_msg} = q{No chap data to delete};
    return;
  }
  
  my $sth = $cdbi->sth_delete('dist_id');
  foreach my $distname(keys %$data) {
    $sth->execute($data->{$distname}) or do {
      $cdbi->db_error($sth);
      $self->{error_msg} = $cdbi->{error_msg};
      return;
    };
  }
  $sth->finish();
  undef $sth;
  $dbh->commit() or do {
    $cdbi->db_error();
    $self->{error_msg} = $cdbi->{error_msg};
    return;
  };
  return 1;
}

package CPAN::SQLite::Populate;

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

__END__

=head1 NAME

CPAN::SQLite::Populate - create and populate database tables

=head1 DESCRIPTION

This module is responsible for creating the tables
(if C<setup> is passed as an option) and then for 
inserting, updating, or deleting (as appropriate) the
relevant information from the indices of
I<CPAN::SQLite::Info> and the
state information from I<CPAN::SQLite::State>. It does
this through the C<insert>, C<update>, and C<delete>
methods associated with each table.

Note that the tables are created with the C<setup> argument
passed into the C<new> method when creating the
C<CPAN::SQLite::Index> object; existing tables will be
dropped.

=head1 TABLES

The tables used are described below - the data types correspond
to mysql tables, with the corresponding adjustments made if
the SQLite database is used.

=head2 mods

This table contains module information, and is created as

  mod_id INTEGER NOT NULL PRIMARY KEY
  mod_name VARCHAR(100) NOT NULL
  dist_id INTEGER NOT NULL
  mod_abs TEXT
  mod_vers VARCHAR(10)
  dslip VARCHAR(5)
  chapterid INTEGER

=over 3

=item * mod_id

This is the primary (unique) key of the table.

=item * dist_id

This key corresponds to the id of the associated distribution
in the C<dists> table.

=item * mod_name

This is the module's name.

=item * mod_abs

This is a description, if available, of the module.

=item * mod_vers

This value, if present, gives the version of the module.

=item * dslip

This is a 5 character string expressing the dslip
(development, support, language, interface, public
license) information.

=item * chapterid

This number corresponds to the chapter id of the module,
if present.

=back

=head2 dists

This table contains distribution information, and is created as

  dist_id INTEGER NOT NULL PRIMARY KEY
  dist_name VARCHAR(90) NOT NULL
  auth_id INTEGER NOT NULL
  dist_file VARCHAR(110) NOT NULL
  dist_vers VARCHAR(20)
  dist_abs TEXT
  dist_dslip VARCHAR(5)

=over 3

=item * dist_id

This is the primary (unique) key of the table.

=item * auth_id

This corresponds to the CPAN author id of the distribution
in the C<auths> table.

=item * dist_name

This corresponds to the distribution name (eg, for
F<My-Distname-0.22.tar.gz>, C<dist_name> will be C<My-Distname>).

=item * dist_file

This corresponds to the CPAN file name.

=item * dist_vers

This is the version of the CPAN file (eg, for
F<My-Distname-0.22.tar.gz>, C<dist_vers> will be C<0.22>).

=item * dist_abs

This is a description of the distribtion. If not directly
supplied, the description for, eg, C<Foo::Bar>, if present, will 
be used for the C<Foo-Bar> distribution.

=item * dist_dslip

This is a 5 character string expressing the dslip
(development, support, language, interface, public
license) information. Normally this comes from the
module name; this value for the distribution name
comes in simple cases where the module name
matches the distribution name by a substitution of
C<::> by C<->.

=back

=head2 auths

This table contains CPAN author information, and is created as

  auth_id INTEGER NOT NULL PRIMARY KEY
  cpanid VARCHAR(20) NOT NULL
  fullname VARCHAR(40) NOT NULL
  email TEXT

=over 3

=item * auth_id

This is the primary (unique) key of the table.

=item * cpanid

This gives the CPAN author id.

=item * fullname

This is the full name of the author.

=item * email

This is the supplied email address of the author.

=back

=head2 chaps

This table contains chapter information associated with
distributions. PAUSE allows one, when registering modules,
to associate a chapter id with each module (see the C<mods>
table). This information is used here to associate chapters
(and subchapters) with distributions in the following manner.
Suppose a distribution C<Quantum-Theory> contains a module
C<Beta::Decay> with chapter id C<55>, and
another module C<Laser> with chapter id C<87>. The
C<Quantum-Theory> distribution will then have two
entries in this table - C<chapterid> of I<55> and
C<subchapter> of I<Beta>, and C<chapterid> of I<87> and
C<subchapter> of I<Laser>.

The table is created as follows.

  chap_id INTEGER NOT NULL PRIMARY KEY
  chapterid INTEGER
  dist_id INTEGER NOT NULL
  subchapter TEXT

=over 3

=item * chap_id

This is the primary (unique) key of the table.

=item * chapterid

This number corresponds to the chapter id.

=item * dist_id

This is the id corresponding to the distribution in the
C<dists> table.

=item * subchapter

This is the subchapter.

=back

=head1 SEE ALSO

L<CPAN::SQLite::Index>

=cut
