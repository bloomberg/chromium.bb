# $Id: State.pm 35 2011-06-17 01:34:42Z stro $

package CPAN::SQLite::State;
use strict;
use warnings;
no warnings qw(redefine);
use CPAN::SQLite::DBI qw($dbh);
use CPAN::SQLite::DBI::Index;
use CPAN::SQLite::Util qw(has_hash_data print_debug);
our $VERSION = '0.202';

my %tbl2obj;
$tbl2obj{$_} = __PACKAGE__ . '::' . $_ for (qw(dists mods auths));
my %obj2tbl = reverse %tbl2obj;

our $dbh = $CPAN::SQLite::DBI::dbh;

sub new {
  my ($class, %args) = @_;

  if ($args{setup}) {
    die "No state information available under setup";
  }

  my $index = $args{index};
  my @tables = qw(dists mods auths);
  foreach my $table (@tables) {
    my $obj = $index->{$table};
    die "Please supply a CPAN::SQLite::Index::$table object"
      unless ($obj and ref($obj) eq "CPAN::SQLite::Index::$table");
  }
  my $cdbi = CPAN::SQLite::DBI::Index->new(%args);

  my $self = {index => $index,
              obj => {},
              cdbi => $cdbi,
              reindex => $args{reindex},
             };
  return bless $self, $class;
}

sub state {
  my $self = shift;
  unless ($self->create_objs()) {
    print_debug("Cannot create objects");
    return;
  }
  unless ($self->state_info()) {
    print_debug("Getting state information failed");
    return;
  };
  return 1;
}

sub create_objs {
  my $self = shift;
  my @tables = qw(dists auths mods);

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
      $obj = $pack->new();
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
  return 1;
}

sub state_info {
  my $self = shift;
  my @methods = qw(ids state);
  my @tables = qw(dists auths mods);

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

package CPAN::SQLite::State::auths;
use base qw(CPAN::SQLite::State);
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

sub ids {
  my $self = shift;
  my $cdbi = $self->{cdbi};
  $self->{ids} = $cdbi->fetch_ids() or do {
    $self->{error_msg} = $cdbi->{error_msg};
    return;
  };
  return 1;
}

sub state {
  my $self = shift;
  my $auth_ids = $self->{ids};
  return unless my $dist_obj = $self->{obj}->{dists};
  my $dist_update = $dist_obj->{update};
  my $dist_insert = $dist_obj->{insert};
  my $dists = $dist_obj->{info};
  my ($update, $insert);
  if (has_hash_data($dist_insert)) {
    foreach my $distname (keys %{$dist_insert}) {
      my $cpanid = $dists->{$distname}->{cpanid};
      if (my $auth_id = $auth_ids->{$cpanid}) {
        $update->{$cpanid} = $auth_id;
      }
      else {
        $insert->{$cpanid}++;
      }
    }
  }
  if (has_hash_data($dist_update)) {
    foreach my $distname (keys %{$dist_update}) {
      my $cpanid = $dists->{$distname}->{cpanid};
      if (my $auth_id = $auth_ids->{$cpanid}) {
        $update->{$cpanid} = $auth_id;
      }
      else {
        $insert->{$cpanid}++;
      }
    }
  }
  $self->{update} = $update;
  $self->{insert} = $insert;
  return 1;
}

package CPAN::SQLite::State::dists;
use base qw(CPAN::SQLite::State);
use CPAN::SQLite::Util qw(vcmp has_hash_data print_debug);

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
              versions => {},
              obj => {},
              cdbi => $cdbi,
              error_msg => '',
              info_msg => '',
              reindex => undef,
  };
  return bless $self, $class;
}

sub ids {
  my $self = shift;
  my $cdbi = $self->{cdbi};
  ($self->{ids}, $self->{versions}) = $cdbi->fetch_ids() or do {
    $self->{error_msg} = $cdbi->{error_msg};
    return;
  };
  return 1;
}

sub state {
  my $self = shift;
  my $dist_versions = $self->{versions};
  my $dists = $self->{info};
  my $dist_ids = $self->{ids};
  my ($insert, $update, $delete);

  my $reindex = $self->{reindex};
  if (defined $reindex) {
    my @dists = ref($reindex) eq 'ARRAY' ? @$reindex : ($reindex);
    foreach my $distname(@dists) {
      my $id = $dist_ids->{$distname};
      if (not defined $id) {
        print_debug(qq{"$distname" does not have an id: reindexing ignored\n});
        next;
      }
      $update->{$distname} = $id;
    }
    $self->{update} = $update;
    return 1;
  }

  foreach my $distname (keys %$dists) {
    if (not defined $dist_versions->{$distname}) {
      $insert->{$distname}++;
    }
    elsif (vcmp($dists->{$distname}->{dist_vers}, 
                       $dist_versions->{$distname}) > 0) {
      $update->{$distname} = $dist_ids->{$distname};
    }
  }
  $self->{update} = $update;
  $self->{insert} = $insert;
  foreach my $distname(keys %$dist_versions) {
    next if $dists->{$distname};
    $delete->{$distname} = $dist_ids->{$distname};
    print_debug("Will delete $distname\n");
  }
  $self->{delete} = $delete;
  return 1;
}

package CPAN::SQLite::State::mods;
use base qw(CPAN::SQLite::State);
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

sub ids {
  my $self = shift;
  my $cdbi = $self->{cdbi};
  $self->{ids} = $cdbi->fetch_ids() or do {
    $self->{error_msg} = $cdbi->{error_msg};
    return;
  };
  return 1;
}

sub state {
  my $self = shift;
  my $mod_ids = $self->{ids};
  return unless my $dist_obj = $self->{obj}->{dists};
  my $dists = $dist_obj->{info};
  my $dist_update = $dist_obj->{update};
  my $dist_insert = $dist_obj->{insert};
  my ($update, $insert, $delete);
  my $cdbi = $self->{cdbi};
  if (has_hash_data($dist_insert)) {
    foreach my $distname (keys %{$dist_insert}) {
      foreach my $module(keys %{$dists->{$distname}->{modules}}) {
        $insert->{$module}++;
      }
    }
  }
  if (has_hash_data($dist_update)) {
    foreach my $distname (keys %{$dist_update}) {
      foreach my $module(keys %{$dists->{$distname}->{modules}}) {
        my $mod_id = $mod_ids->{$module};
        if ($mod_id) {
          $update->{$module} = $mod_id;
        }
        else {
          $insert->{$module}++;
        }
      }   
    }
  }

  if (has_hash_data($dist_update)) {
    my $sql = q{SELECT mod_id,mod_name from mods,dists WHERE dists.dist_id = mods.dist_id and dists.dist_id = ?};
    my $sth = $dbh->prepare($sql) or do {
      $cdbi->db_error();
      $self->{error_msg} = $cdbi->{error_msg};
      return;
    };
    my $dist_ids = $dist_obj->{ids};
    foreach my $distname (keys %{$dist_update}) {
      my %mods = ();
      %mods = map {$_ => 1} keys %{$dists->{$distname}->{modules}};
      $sth->execute($dist_ids->{$distname}) or do {
        $cdbi->db_error($sth);
        $self->{error_msg} = $cdbi->{error_msg};
        return;
      };
      while (my($mod_id, $mod_name) = $sth->fetchrow_array) {
        next if $mods{$mod_name};
        $delete->{$mod_name} = $mod_id;
      }
    }
    $sth->finish;
    undef $sth;
  }

  $self->{update} = $update;
  $self->{insert} = $insert;
  $self->{delete} = $delete;
  return 1;
}


package CPAN::SQLite::State;

1;

__END__

=head1 NAME

CPAN::SQLite::State - get state information on the database

=head1 DESCRIPTION

This module gets information on the current state of the
database and compares it to that obtained from the CPAN
index files from I<CPAN::SQLite::Info> and from the
repositories from I<CPAN::SQLite::PPM>. For each of the
four tables I<dists>, I<mods>, I<auths>, and I<ppms>,
two methods are used to get this information:

=over 3

=item * C<ids>

This method gets the ids of the relevant names, and
versions, if applicable, in the table.

=item * C<state>

This method compares the information in the tables
obtained from the C<ids> method to that from the
CPAN indices and ppm repositories. One of three actions
is then decided, which is subsequently acted upon in 
I<CPAN::SQLite::Populate>.

=over 3

=item * C<insert>

If the information in the indices is not in the
database, this information is marked for insertion.

=item * C<update>

If the information in the database is older than that
form the indices (generally, this means an older version),
the information is marked for updating.

=item * C<delete>

If the information in the database is no longer present
in the indices, the information is marked for deletion.

=back

=back

=cut

