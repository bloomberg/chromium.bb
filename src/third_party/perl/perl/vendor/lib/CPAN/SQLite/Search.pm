# $Id: Search.pm 35 2011-06-17 01:34:42Z stro $

package CPAN::SQLite::Search;
use strict;
use warnings;
no warnings qw(redefine);
use utf8;
use CPAN::SQLite::Util qw($mode_info);
use CPAN::SQLite::DBI::Search;

our $max_results = 0;
our $VERSION = '0.202';
my $cdbi_query;

my %mode2obj;
$mode2obj{$_} = __PACKAGE__ . '::' . $_
  for (qw(dist author module));

sub new {
  my ($class, %args) = @_;
  $cdbi_query = CPAN::SQLite::DBI::Search->new(%args);
  $max_results = $args{max_results} if $args{max_results};
  my $self = {results => undef, error => '', %args};
  return bless $self, $class;
}

sub query {
  my ($self, %args) = @_;
  my $mode = $args{mode} || 'module';
  unless ($mode) {
    $self->{error} = q{Please specify a 'mode' argument};
    return;
  }
  my $info = $mode_info->{$mode};
  my $table = $info->{table};
  unless ($table) {
    $self->{error} = qq{No table exists for '$mode'};
    return;
  }
  my $cdbi = $cdbi_query->{objs}->{$table};
  my $class = 'CPAN::SQLite::DBI::Search::' . $table;
  unless ($cdbi and ref($cdbi) eq $class) {
    $self->{error} = qq{No cdbi object exists for '$table'};
    return;
  }
  my $obj;
  eval {$obj = $mode2obj{$mode}->make(table => $table, cdbi => $cdbi);};
  if ($@) {
    $self->{error} = qq{Mode '$mode' is not known};
    return;
  }
  my $value;
  my $search = {name => $info->{name},
               text => $info->{text},
               id => $info->{id},
               };
 TYPE: {
    ($value = $args{query}) and do {
      $search->{value} = $value;
      $search->{type} = 'query';
      $search->{wantarray} = 1;
      last TYPE;
    };
    ($value = $args{id}) and do {
      $search->{value} = $value;
      $search->{type} = 'id';
      $search->{distinct} = 1;
      last TYPE;
    };
    ($value = $args{name}) and do {
      $search->{value} = $value;
      $search->{type} = 'name';
      $search->{distinct} = 1;
      last TYPE;
    };
    $self->{error} = q{Cannot determine the type of search};
    return;
  }

  $obj->search(search => $search, meta_obj => $self->{meta_obj});
  $self->{results} = $obj->{results};
  if (my $error = $obj->{error}) {
    $self->{error} = $error;
    return;
  }
  return 1;
}

sub make {
  my ($class, %args) = @_;
  for (qw(table cdbi)) {
    die qq{Must supply an '$_' arg} unless defined $args{$_};
  }
  my $self = {results => undef, error => '',
              table => $args{table}, cdbi => $args{cdbi}};
  return bless $self, $class;
}

package CPAN::SQLite::Search::author;
use base qw(CPAN::SQLite::Search);

sub search {
  my ($self, %args) = @_;
  return unless $args{search};
  my $cdbi = $self->{cdbi};
  my $meta_obj = $args{meta_obj};
  $args{fields} = [ qw(auth_id cpanid fullname email) ];
  $args{table} = 'auths';
  if ($max_results) {
    $args{limit} = $max_results;
  }
  $args{order_by} = 'cpanid';
  my $results;
  return unless $results = ($meta_obj ?
                            $cdbi->fetch_and_set(%args) :
                            $cdbi->fetch(%args));
  unless ($meta_obj) {
    $self->{results} =
      (ref($results) eq 'ARRAY' and scalar @$results == 1) ?
        $results->[0] : $results;
  }
  return 1 if $meta_obj;

# The following will get all the dists associated with the cpanid
  $args{join} = undef;
  $args{table} = 'dists';
  $args{fields} = [ qw(dist_file dist_abs) ];
  $args{order_by} = 'dist_file';
  my @items = (ref($results) eq 'ARRAY') ? @$results : ($results);
  foreach my $item (@items) {
    my $search = {id => 'auth_id',
                  value => $item->{auth_id},
                  type => 'id',
                  wantarray => 1,
                 };
    my $dists;
    next unless ($dists = $cdbi->fetch(%args, search => $search));
    $item->{dists} = (ref($dists) eq 'ARRAY') ? $dists : [$dists];
  }
  $self->{results} = 
    (ref($results) eq 'ARRAY' and scalar @$results == 1) ?
      $results->[0] : $results;
  return 1;
}

package CPAN::SQLite::Search::module;
use base qw(CPAN::SQLite::Search);

sub search {
  my ($self, %args) = @_;
  return unless $args{search};
  my $cdbi = $self->{cdbi};
  my $meta_obj = $args{meta_obj};

  $args{fields} = [ qw(mod_id mod_name mod_abs mod_vers chapterid
                       dslip dist_id dist_name dist_file dist_vers dist_abs
                       auth_id cpanid fullname email) ];
  $args{table} = 'dists';
  $args{join} = { mods => 'dist_id',
                  auths => 'auth_id',
                };
  $args{order_by} = 'mod_name';
  if ($max_results) {
    $args{limit} = $max_results;
  }
  my $results;
  return unless $results = ($meta_obj ? 
                            $cdbi->fetch_and_set(%args, want_ids => 1) :
                            $cdbi->fetch(%args));
# if running under CPAN.pm, need to build a list of modules
# contained in the distribution
  if ($meta_obj) {
    my %seen;
    $args{join} = undef;
    $args{table} = 'mods';
    my @items = (ref($results) eq 'ARRAY') ? @$results : ($results);
    foreach my $item(@items) {
      my $dist_id = $item->{dist_id};
      next if $seen{$dist_id};
      $args{fields} = [ qw(mod_name mod_abs) ];
      $args{order_by} = 'mod_name';
      $args{join} = undef;
      my $search = {id => 'dist_id',
                    value => $item->{dist_id},
                    type => 'id',
                    wantarray => 1,
                   };
      $seen{$dist_id}++;
      my $mods;
      next unless $mods = $cdbi->fetch_and_set(%args, 
                                               search => $search,
                                               set_list => 1,
                                               download => $item->{download});
    }
  }
  unless ($meta_obj) {
    $self->{results} =
      (ref($results) eq 'ARRAY' and scalar @$results == 1) ?
        $results->[0] : $results;
  }
  return 1;
}

package CPAN::SQLite::Search::dist;
use base qw(CPAN::SQLite::Search);

sub search {
  my ($self, %args) = @_;
  return unless $args{search};
  my $cdbi = $self->{cdbi};
  my $meta_obj = $args{meta_obj};

  $args{fields} = [ qw(dist_id dist_name dist_abs dist_vers dist_dslip
                       dist_file auth_id cpanid fullname email) ];
  $args{table} = 'dists';
  $args{join} = {auths => 'auth_id'};
  $args{order_by} = 'dist_name';
  if ($max_results) {
    $args{limit} = $max_results;
  }
  my $results;
  return unless $results = ($meta_obj ?
                            $cdbi->fetch_and_set(%args, want_ids => 1) :
                            $cdbi->fetch(%args));

  $args{join} = undef;
  $args{table} = 'mods';
  $args{fields} = [ qw(mod_name mod_abs) ];
  $args{order_by} = 'mod_name';
  my @items = (ref($results) eq 'ARRAY') ? @$results : ($results);
  foreach my $item(@items) {
    my $search = {id => 'dist_id',
                  value => $item->{dist_id},
                  type => 'id',
                  wantarray => 1,
                 };
    my $mods;
    next unless $mods = ($meta_obj ?
                         $cdbi->fetch_and_set(%args, 
                                              search => $search,
                                              set_list => 1,
                                              download => $item->{download}) :
                         $cdbi->fetch(%args,
                                      search => $search) );
    next if $meta_obj;
    $item->{mods} = (ref($mods) eq 'ARRAY') ? $mods : [$mods];
  }
  unless ($meta_obj) {
    $self->{results} =
      (ref($results) eq 'ARRAY' and scalar @$results == 1) ?
        $results->[0] : $results;
  }
  return 1;
}

package CPAN::SQLite::Search;

sub mod_subchapter {
  my ($self, $mod_name) = @_;
  (my $sc = $mod_name) =~ s{^([^:]+).*}{$1};
  return $sc;
}

sub dist_subchapter {
  my ($self, $dist_name) = @_;
  (my $sc = $dist_name) =~ s{^([^-]+).*}{$1};
  return $sc;
}


1;

__END__

=head1 NAME

CPAN::SQLite::Search - perform queries on the database

=head1 SYNOPSIS

  my $max_results = 200;
  my $query = CPAN::SQLite::Search->new(db_dir => $db_dir,
                                        db_name => $db_name,
                                        max_results => $max_results);
  $query->query(mode => 'module', name => 'Net::FTP');
  my $results = $query->{results};

=head1 CONSTRUCTING THE QUERY

This module queries the database via various types of queries
and returns the results for subsequent display. The 
C<CPAN::SQLite::Search> object is created via the C<new> method as

  my $query = CPAN::SQLite::Search->new(db_dir => $db_dir,
                                        db_name => $db_name,
                                        max_results => $max_results);

which takes as arguments

=over 3

=item * db_dir =E<gt> $db_dir

This is the directory where the database file is stored. This is
optional if the C<CPAN> option is given.

=item * CPAN =E<gt> $CPAN

This option specifies the C<cpan_home> directory of an
already configured CPAN.pm, which is where the database
file will be stored if C<db_dir> is not given.

=item * max_results =E<gt> $max_results

This is the maximum value used to limit the number of results
returned under a user query. If not specified, a value contained
within C<CPAN::SQLite::Search> will be used.

=back

A basic query then is constructed as

   $query->query(mode => $mode, $type => $value);

with the results available as

   my $results = $query->{results}

There are three basic modes:

=over 3

=item * module

This is for information on modules.

=item * dist

This is for information on distributions.

=item * author

This is for information on CPAN authors or cpanids.

=back

=head2 C<module>, C<dist>, and C<author> modes

For a mode of C<module>, C<dist>, and C<author>, there are
four basic options to be used for the C<$type =E<gt> $value> option:

=over 3

=item * query =E<gt> $query_term

This will search through module names, 
distribution names, or CPAN author names and ids
(for C<module>, C<dist>, and C<author> modes
respectively). The results are case insensitive,
and Perl regular expressions for the C<$query_term>
are recognized.

=item * name =E<gt> $name

This will report exact matches (in a case sensitive manner)
for the module name, distribution name, or CPAN author id,
for C<module>, C<dist>, and C<author> modes
respectively.

=item * id =E<gt> $id

This will look up information on the primary key according
to the mode specified. This is more for internal use,
to help speed up queries; using this "publically" is
probably not a good idea, as the ids may change over the
course of time.

=back

=head1 RESULTS

After making the query, the results can be accessed through

  my $results = $query->{results};

No results either can mean no matches were found, or
else an error in making the query resulted (in which case,
a brief error message is contained in C<$query-E<gt>{error}>).
Assuming there are results, what is returned depends on
the mode and on the type of query. See L<CPAN::SQLite::Populate>
for a description of the fields in the various tables
listed below - these fields are used as the keys of the
hash references that arise.

=head2 C<author> mode

=over 3

=item * C<name> or C<id> query

This returns the C<auth_id>, C<cpanid>, C<email>, and C<fullname>
of the C<auths> table. As well, an array reference
C<$results-E<gt>{dists}> is returned representing
all distributions associated with that C<cpanid> - each
member of the array reference is a hash reference
describing the C<dist_id>, C<dist_name>, 
C<dist_abs>, C<dist_vers>, and C<dist_file> fields in the
C<dists> table. An additional entry, C<download>, is
supplied, which can be used as C<$CPAN/authors/id/$download>
to specify the url of the distribution.

=item * C<query> query

If this results in more than one match, an array reference
is returned, each member of which is a hash reference containg
the C<auth_id>, C<cpanid>, and C<fullname> fields. If there
is only one result found, a C<name> query based on the
matched C<cpanid> is performed.

=back

=head2 C<module> mode

=over 3

=item * C<name> or C<id> query

This returns the C<mod_id>, C<mod_name>, C<mod_abs>, C<mod_vers>,
C<dslip>, C<chapterid>, C<dist_id>, C<dist_name>, C<dist_file>,
C<auth_id>, C<cpanid>, C<fullname>, and C<email> 
of the C<auths>, C<mods>, and C<dists> tables.
As well, the following entries may be present.

=over 3

=item * C<download>

This can be used as C<$CPAN/authors/id/$download>
to specify the url of the distribution.

=item * C<dslip_info>

If C<dslip> is available, an array reference C<dslip_info> is supplied,
each entry being a hash reference. The hash reference contains
two keys - C<desc>, whose value is a general description of the
what the dslip entry represents, and C<what>, whose value is
a description of the entry itself.

=back

=item * C<query> query

If this results in more than one match, an array reference
is returned, each member of which is a hash reference containing
the C<mod_id>, C<mod_name>, C<mod_abs>, C<mod_abs>, C<dist_vers>, C<dist_abs>,
C<auth_id>, C<cpanid>, C<dist_id>, C<dist_name>, and C<dist_file>.
As well, a C<download> field which
can be used as C<$CPAN/authors/id/$download>
to specify the url of the distribution is provided. If there
is only one result found, a C<name> query based on the
matched C<mod_name> is performed.

=back

=head2 C<dist> mode

=over 3

=item * C<name> or C<id> query

This returns the C<dist_id>, C<dist_name>, C<dist_abs>, C<dist_vers>,
C<dist_file>, C<size>, C<birth>, C<auth_id>, C<cpanid>, and C<fullname>
of the C<auths>, C<mods>, and C<dists> tables.
As well, the following entries may be present.

=over 3

=item * C<download>

This can be used as C<$CPAN/authors/id/$download>
to specify the url of the distribution.

=item * C<mods>

This is an array reference containing information on the
modules present. Each entry is a hash reference containing the
C<mod_id>, C<mod_name>, C<mod_abs>, C<mod_vers>, and C<dslip>
fields for the module.

=item * C<dslip> and C<dslip_info>

If the module name and distribution name are related by
C<s/::/->, the C<dslip> and C<dslip_info> entries for
that module are returned.

=back

=item * C<query> query

If this results in more than one match, an array reference
is returned, each member of which is a hash reference containing
the C<dist_id>, C<dist_name>, C<dist_abs>, C<dist_file>,
and C<cpanid> fields. As well, a C<download> field which
can be used as C<$CPAN/authors/id/$download>
to specify the url of the distribution is provided. If there
is only one result found, a C<name> query based on the
matched C<dist_name> is performed.

=back

=head1 SEE ALSO

L<CPAN::SQLite::Populate>.

=head1 AUTHORS

Randy Kobes (passed away on September 18, 2010)

Serguei Trouchelle E<lt>stro@cpan.orgE<gt>

=head1 COPYRIGHT

Copyright 2006,2008 by Randy Kobes E<lt>r.kobes@uwinnipeg.caE<gt>. 

Copyright 2011 by Serguei Trouchelle E<lt>stro@cpan.orgE<gt>.

Use and redistribution are under the same terms as Perl itself.

=cut

