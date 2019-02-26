# $Id: META.pm 35 2011-06-17 01:34:42Z stro $

package CPAN::SQLite::META;
require CPAN::SQLite;
use strict;
use warnings;
use base qw(Exporter);
our @EXPORT_OK;
@EXPORT_OK = qw(setup update);
our $global_id;
our $VERSION = '0.202';

# This is usually already defined in real life, but tests need it to be set
$CPAN::FrontEnd ||= "CPAN::Shell";

sub new {
  my ($class, $cpan_meta) = @_;
  my $cpan_sqlite = CPAN::SQLite->new();
  return bless {cpan_meta => $cpan_meta, cpan_sqlite => $cpan_sqlite}, $class;
}

sub set {
  my ($self, $class, $id) = @_;
  my $sqlite_obj = $self->make_obj(class => $class, id => $id);
  return $sqlite_obj->set_one();
}

sub search {
  my ($self, $class, $regex) = @_;
  my $sqlite_obj = $self->make_obj(class => $class, regex => $regex);
  return $sqlite_obj->set_many();
}

sub make_obj {
  my ($self, %args)  = @_;
  my $class = $args{class};
  die qq{Must supply a CPAN::* class string}
    unless ($class and $class =~ /^CPAN::/);
  (my $type = $class) =~ s/^CPAN//;
  my $package = __PACKAGE__ . $type;
  return bless {cpan_meta => $self->{cpan_meta},
        cpan_sqlite => $self->{cpan_sqlite},
        class => $class,
        id => $args{id}, regex => $args{regex},
        }, $package;
}

package CPAN::SQLite::META::Author;
use base qw(CPAN::SQLite::META);
use CPAN::SQLite::Util qw(has_hash_data);

sub set_one {
  my $self = shift;
  my $cpan_sqlite = $self->{cpan_sqlite};
  my $id = $self->{id};
  my $class = $self->{class};
  $cpan_sqlite->{results} = {};
  $cpan_sqlite->query(mode => 'author', name => $id, meta_obj => $self);
  my $cpan_meta = $self->{cpan_meta};
  return $cpan_meta->{readonly}{$class}{$id};
}

sub set_many {
  my $self = shift;
  my $cpan_sqlite = $self->{cpan_sqlite};
  my $regex = $self->{regex};
  $cpan_sqlite->{results} = [];
  return $cpan_sqlite->query(mode => 'author', query => $regex, meta_obj => $self);
}

sub set_data {
  my ($self, $results) = @_;
  return $self->set_author($results->{cpanid}, $results);
}

package CPAN::SQLite::META::Distribution;
use base qw(CPAN::SQLite::META);
use CPAN::SQLite::Util qw(has_hash_data download);
use CPAN::DistnameInfo;
my $ext = qr{\.(tar\.gz|tar\.Z|tgz|zip)$};

sub set_one {
  my $self = shift;
  my $cpan_sqlite = $self->{cpan_sqlite};
  my $id = $self->{id};
  my ($dist_name, $dist_id);
  if ($id =~ /$ext/) {
    ($dist_name, $dist_id) = $self->extract_distinfo($id);
  }
  return unless ($dist_name and $dist_id);
  my $class = $self->{class};
  $cpan_sqlite->{results} = {};
  $cpan_sqlite->query(mode => 'dist', name => $dist_name, meta_obj => $self);
  my $cpan_meta = $self->{cpan_meta};
  return $cpan_meta->{readonly}{$class}{$dist_id};
}

sub set_many {
  my $self = shift;
  my $cpan_sqlite = $self->{cpan_sqlite};
  my $regex = $self->{regex};
  $cpan_sqlite->{results} = [];
  return $cpan_sqlite->query(mode => 'dist', query => $regex, meta_obj => $self);
}

sub set_data {
  my ($self, $results) = @_;
  $global_id = $results->{download};
  return $self->set_dist($results->{download}, $results);
}

sub set_list_data {
  my ($self, $results, $download) = @_;
  $global_id = $download;
  $self->set_containsmods($results);
  $global_id = undef;
  return;
}

package CPAN::SQLite::META::Module;
use base qw(CPAN::SQLite::META);
use CPAN::SQLite::Util qw(has_hash_data);

sub set_one {
  my $self = shift;
  my $cpan_sqlite = $self->{cpan_sqlite};
  my $id = $self->{id};
  return if ($id =~ /^Bundle::/);
  my $class = $self->{class};
  $cpan_sqlite->{results} = {};
  $cpan_sqlite->query(mode => 'module', name => $id, meta_obj => $self);
  my $cpan_meta = $self->{cpan_meta};
  return $cpan_meta->{readonly}{$class}{$id};
}

sub set_many {
  my $self = shift;
  my $cpan_sqlite = $self->{cpan_sqlite};
  my $regex = $self->{regex};
  $cpan_sqlite->{results} = [];
  return $cpan_sqlite->query(mode => 'module', query => $regex, meta_obj => $self);
}

sub set_data {
  my ($self, $results) = @_;
  $self->set_module($results->{mod_name}, $results);
  $global_id = $results->{download};
  return $self->set_dist($results->{download}, $results);
}

sub set_list_data {
  my ($self, $results, $download) = @_;
  $global_id = $download;
  $self->set_containsmods($results);
  $global_id = undef;
  return;
}

package CPAN::SQLite::META::Bundle;
use base qw(CPAN::SQLite::META);
use CPAN::SQLite::Util qw(has_hash_data);

sub set_one {
  my $self = shift;
  my $cpan_sqlite = $self->{cpan_sqlite};
  my $id = $self->{id};
  unless ($id =~ /^Bundle::/) {
    $id = 'Bundle::' . $id;
  }
  my $class = $self->{class};
  $cpan_sqlite->{results} = {};
  $cpan_sqlite->query(mode => 'module', name => $id, meta_obj => $self);
  my $cpan_meta = $self->{cpan_meta};
  return $cpan_meta->{readonly}{$class}{$id};
}

sub set_many {
  my $self = shift;
  my $cpan_sqlite = $self->{cpan_sqlite};
  my $regex = $self->{regex};
  unless ($regex =~ /(^Bundle::|[\^\$\*\+\?\|])/i) {
    $regex = '^Bundle::' . $regex;
  }
  $regex = '^Bundle::' if $regex eq '^';
  $cpan_sqlite->{results} = [];
  return $cpan_sqlite->query(mode => 'module', query => $regex, meta_obj => $self);
}

sub set_data {
  my ($self, $results) = @_;
  $self->set_bundle($results->{mod_name}, $results);
  $global_id = $results->{download};
  return $self->set_dist($results->{download}, $results);
}

sub set_list_data {
  my ($self, $results, $download) = @_;
  $global_id = $download;
  $self->set_containsmods($results);
  $global_id = undef;
  return;
}

package CPAN::SQLite::META;
use CPAN::SQLite::Util qw(download);

my @months = qw(Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec);
my @days = qw(Sun Mon Tue Wed Thu Fri Sat);

sub set_author {
  my ($self, $id, $results) = @_;
  my $class = 'CPAN::Author';
  my $cpan_meta = $self->{cpan_meta};
  return $cpan_meta->instance(
                       $class => $id
                      )->set(
                             'FULLNAME' => $results->{fullname},
                             'EMAIL' => $results->{email},
                            );
}

sub set_module {
  my ($self, $id, $results) = @_;
  my $class = 'CPAN::Module';
  my $cpan_meta = $self->{cpan_meta};
  my %dslip;
  if (my $dslip = $results->{dslip}) {
    my @values = split '', $dslip;
    for (qw(d s l i p)) {
      $dslip{'stat' . $_} = shift @values;
    }
  }
  my $d = $cpan_meta->instance(
                               $class => $id
                              );
  return $d->set(
          'description' => $results->{mod_abs},
          'userid' => $results->{cpanid},
          'CPAN_VERSION' => $results->{mod_vers},
          'CPAN_FILE' => $results->{download},
          'CPAN_USERID' => $results->{cpanid},
          'chapterid' => $results->{chapterid},
          %dslip,
         );
}

sub set_bundle {
  my ($self, $id, $results) = @_;
  my $class = 'CPAN::Bundle';
  my $cpan_meta = $self->{cpan_meta};
  my %dslip;
  if (my $dslip = $results->{dslip}) {
    my @values = split '', $dslip;
    for (qw(d s l i p)) {
      $dslip{'stat' . $_} = shift @values;
    }
  }
  my $d = $cpan_meta->instance(
                               $class => $id
                              );
  return $d->set(
          'description' => $results->{mod_abs},
          'userid' => $results->{cpanid},
          'CPAN_VERSION' => $results->{mod_vers},
          'CPAN_FILE' => $results->{download},
          'CPAN_USERID' => $results->{cpanid},
          'chapterid' => $results->{chapterid},
          %dslip,
         );
}

sub set_dist {
  my ($self, $id, $results) = @_;
  my $class = 'CPAN::Distribution';
  my $cpan_meta = $self->{cpan_meta};
  my $d = $cpan_meta->instance(
                               $class => $id
                              );
  return $d->set(
          'DESCRIPTION' => $results->{dist_abs},
          'CPAN_USERID' => $results->{cpanid},
          'CPAN_VERSION' => $results->{dist_vers},
         );
}

sub set_containsmods {
  my ($self, $mods) = @_;
  my $class = 'CPAN::Distribution';
  my $cpan_meta = $self->{cpan_meta};
  my %containsmods;
  if ($mods and (ref($mods) eq 'ARRAY')) {
    %containsmods = map {$_->{mod_name} => 1} @$mods;
  }
  my $d = $cpan_meta->instance(
                               $class => $global_id
                              );
  return $d->{CONTAINSMODS} =  \%containsmods;
}

sub reload {
  my($self, %args) = @_;

  my $time = $args{'time'} || time;
  my $force = $args{force};
  my $db_name = $CPAN::SQLite::db_name;
  my $db = File::Spec->catfile($CPAN::Config->{cpan_home}, $db_name);
  my $journal_file = $db . '-journal';
  if (-e $journal_file) {
    $CPAN::FrontEnd->mywarn('Database locked - cannot update.');
    return;
  }
  my @args = ($^X, '-MCPAN::SQLite::META=setup,update', '-e');
  if (-e $db && -s _) {
    my $mtime_db = (stat(_))[9];
    my $time_string = gmtime_string($mtime_db);
    $CPAN::FrontEnd->myprint("Database was generated on $time_string\n");
    unless ($force) {
      return if (($time - $mtime_db) < $CPAN::Config->{index_expire}*86400);
    }
    $CPAN::FrontEnd->myprint('Updating database file ... ');
    push @args, q{update};
  }
  else {
    unlink($db) if -e _;
    $CPAN::FrontEnd->myprint('Creating database file ... ');
    push @args, q{setup};
  }
  if ($CPAN::SQLite::DBI::dbh) {
    $CPAN::SQLite::DBI::dbh->disconnect();
    $CPAN::SQLite::DBI::dbh = undef;
  }
  system(@args) == 0 or die qq{system @args failed: $?};
  $CPAN::FrontEnd->myprint('Done!');
  return 1;
}

sub setup {
  my $obj = CPAN::SQLite->new(setup => 1);
  $obj->index() or die qq{CPAN::SQLite setup failed};
  return;
}

sub update {
  my $obj = CPAN::SQLite->new();
  $obj->index() or die qq{CPAN::SQLite update failed};
  return;
}

sub gmtime_string {
  my $time = shift;
  return unless $time;
  my @a = gmtime($time);
  my $string = sprintf("%s, %02d %s %d %02d:%02d:%02d GMT",
                      $days[$a[6]], $a[3], $months[$a[4]],
                      $a[5] + 1900, $a[2], $a[1], $a[0]);
  return $string;
}

sub extract_distinfo {
  my ($self, $pathname) = @_;
  unless ($pathname =~ m{^\w/\w\w/}) {
    $pathname =~ s{^(\w)(\w)(.*)}{$1/$1$2/$1$2$3};
  }
  my $d = CPAN::DistnameInfo->new($pathname);
  my $dist = $d->dist;
  my $download = download($d->cpanid, $d->filename);
  return ($dist and $download) ? ($dist, $download) : undef;
}

1;

__END__

=head1 NAME

CPAN::SQLite::META - helper module for CPAN.pm integration

=head1 DESCRIPTION

This module has no direct public interface, but is intended
as a helper module for use of CPAN::SQLite within the CPAN.pm
module. A new object is created as

  my $obj = CPAN::SQLite::META->new($CPAN::META);

where C<$CPAN::META> comes from CPAN.pm. There are then
two main methods available.

=over 4

=item C<set>

This is used as

   $obj->set($class, $id);

where C<$class> is one of C<CPAN::Author>, C<CPAN::Module>, or
C<CPAN::Distribution>, and C<$id> is the id CPAN.pm uses to
identify the class. The method searches the C<CPAN::SQLite>
database by name using the appropriate C<author>, C<dist>,
or C<module> mode, and if a result is found, calls

    $CPAN::META->instance(
                         $class => $id
                         )->set(
                          %attributes
                         );

to register an instance of this class within C<CPAN.pm>.

=item C<ssearch>

This is used as

   $obj->search($class, $id);

where C<$class> is one of C<CPAN::Author>, C<CPAN::Module>, or
C<CPAN::Distribution>, and C<$id> is the id CPAN.pm uses to
identify the class. The method searches the C<CPAN::SQLite>
database by C<query> using the appropriate C<author>, C<dist>,
or C<module> mode, and if results are found, calls

    $CPAN::META->instance(
                         $class => $id
                         )->set(
                          %attributes
                         );

for each match to register an instance of this class 
within C<CPAN.pm>.

=back

The attributes set within C<$CPAN::META->instance> depend
on the particular class.

=over

=item author

The attributes are

       'FULLNAME' => $results->{fullname},
       'EMAIL' => $results->{email},

where C<$results> are the results returned from C<CPAN::SQLite>.

=item module

The attributes are

        'description' => $results->{mod_abs},
        'userid' => $results->{cpanid},
        'CPAN_VERSION' => $results->{mod_vers},
        'CPAN_FILE' => $results->{download},
        'CPAN_USERID' => $results->{cpanid},
        'chapterid' => $results->{chapterid},
        %dslip,

where C<$results> are the results returned from C<CPAN::SQLite>.
Here, C<%dslip> is a hash containing keys C<statd>, C<stats>,
C<statl>, C<stati>, and C<statp>, with corresponding values
being the registered dslip entries for the module, if present.

=item dist

The attributes are

       'DESCRIPTION' => $results->{dist_abs},
       'CPAN_USERID' => $results->{cpanid},
       'CPAN_VERSION' => $results->{dist_vers},

As well, a C<CONTAINSMODS> key to C<$CPAN::META> is added, this
being a hash reference whose keys are the modules contained
within the distribution.

=back

There is also a method available C<reload>, which rebuilds
the database. It can be used as

   $obj->reload(force => 1, time => $time);

The C<time> option (which, if not passed in, will default to the
current time) will be used to compare the current time to
the mtime of the database file; if they differ by more than
one day, the database will be rebuilt. The <force> option, if
given, will force a rebuilding of the database regardless
of the time difference.

=cut

