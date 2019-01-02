# $Id: SQLite.pm 35 2011-06-17 01:34:42Z stro $

package CPAN::SQLite;
use strict;
use warnings;
use File::HomeDir;
require File::Spec;
use Cwd;
require CPAN::SQLite::META;

our $VERSION = '0.202';

# an array ref of distributions to ignore indexing
my $ignore = [qw(SpreadSheet-WriteExcel-WebPivot)];
our $db_name = 'cpandb.sql';

use constant WIN32 => $^O eq 'MSWin32';

sub new {
    my $class = shift;
    my %args = @_;

    my ($CPAN, $update_indices);
    my $db_dir = $args{db_dir};
    my $urllist = [];
    my $keep_source_where;
    # for testing undr Darwin, must load CPAN::MyConfig contained
    # in PERL5LIB, as File::HomeDir doesn't use this
    if ($ENV{CPAN_SQLITE_TESTING}) {
      eval {require CPAN::MyConfig;};
    }
    eval {require CPAN; CPAN::HandleConfig->load;};
    if ( not $@ and not defined $args{CPAN} ) {
      $CPAN = $CPAN::Config->{cpan_home};
      $db_dir = $CPAN;
      $keep_source_where = $CPAN::Config->{keep_source_where};
      $urllist = $CPAN::Config->{urllist};
      # Sometimes this directory dosn't exist (like on new installations)
      unless (-d $CPAN) {
          eval { File::Path::mkpath($CPAN); }; # copied from CPAN.pm
      }
      die qq{The '$CPAN' directory doesn't exist} unless -d $CPAN;
      $update_indices = 0;
    }
    else {
      $CPAN = $args{CPAN} || '';
      die qq{Please specify the CPAN location} unless defined $CPAN;
      die qq{The '$CPAN' directory doesn't exist} unless (-d $CPAN);
      $update_indices = (-f File::Spec->catfile($CPAN, 'MIRRORING.FROM')) ?
        0 : 1;
    }
    push @$urllist, q{http://www.cpan.org/};
    $db_dir ||= cwd;
    my $self = {%args, CPAN => $CPAN, update_indices => $update_indices,
                db_name => $db_name, urllist => $urllist,
                keep_source_where => $keep_source_where, db_dir => $db_dir};
    return bless $self, $class;
}

sub index {
  my ($self, %args) = @_;
  require CPAN::SQLite::Index;
  my %wanted = map {$_ => $self->{$_}} 
    qw(CPAN ignore update_indices db_name db_dir
       keep_source_where setup reindex urllist);
  my $log_dir = $self->{CPAN} || $self->{db_dir};
  die qq{Please create the directory '$log_dir' first} unless -d $log_dir;
  my $index = CPAN::SQLite::Index->new(%wanted, %args, log_dir => $log_dir);
  $index->index() or do {
    warn qq{Indexing failed!};
    return;
  };
  return 1;
}

sub query {
  my ($self, %args) = @_;
  require CPAN::SQLite::Search;
  my %wanted = map {$_ => $self->{$_}} 
    qw(max_results CPAN db_name db_dir meta_obj);
  my $query = CPAN::SQLite::Search->new(%wanted, %args);
  %wanted = map {$_ => $self->{$_}} qw(mode query id name);
  $query->query(%wanted, %args) or do {
    warn qq{Query failed!};
    return;
  };
  my $results = $query->{results};
  return unless defined $results;
  $self->{results} = $query->{results};
  return 1;
}

1;

__END__

=head1 NAME

CPAN::SQLite - maintain and search a minimal CPAN database

=head1 SYNOPSIS

  my $obj = CPAN::SQLite->new(CPAN => '/path/to/CPAN');
  $obj->index(setup => 1);

  $obj->query(mode => 'dist', name => 'CPAN');
  my $results = $obj->{results};

=head1 DESCRIPTION

This package is used for setting up, maintaining, and
searching a CPAN database consisting of the information
stored in the three main CPAN indices: 
F<$CPAN/modules/03modlist.data.gz>,
F<$CPAN/modules/02packages.details.txt.gz>, and
F<$CPAN/authors/01mailrc.txt.gz>. It should be
considered at an alpha stage of development.

One begins by creating the object as

  my $obj = CPAN::SQLite->new(%args);

which accepts the following arguments:

=over 3

=item * C<CPAN =E<gt> '/path/to/CPAN'>

This specifies the path to where the index files are
to be stored. This could be a local CPAN mirror,
defined here by the presence of a F<MIRRORED.BY> file beneath
this directory, or a local directory in which to store
these files from a remote CPAN mirror. In the latter case,
the index files are fetched from a remote CPAN mirror,
using the same list that C<CPAN.pm> uses, if this is
configured, and are updated if they are more than one
day old.

If the C<CPAN> option is not given, it will default
to C<cpan_home> of L<CPAN::>, if this is configured,
with the index files found under C<keep_source_where>.
A fatal error results if such a directory isn't found.
Updates to these index files are assumed here to be
handled by C<CPAN.pm>.

=item * C<db_dir =E<gt> '/path/to/db/dir'>

This specifies the path to where the database file is
found. If not given, it defaults to the
C<cpan_home> directory of C<CPAN.pm>, if present, or to
the directory in which the script was invoked. The name
of the database file is C<cpandb.sql>.

=back

There are two main methods available.

=head2 C<$obj-E<gt>index(%args);>

This is used to set up and maintain the database. The
following arguments are accepted:

=over 3

=item * setup =E<gt> 1

This specifies that the database is to be created and
populated from the CPAN indices; any exisiting database
will be overwritten. Not specifying this option will
assume that an existing database is to be updated.

=item * reindex =E<gt> 'dist_name'

This specifies that the CPAN distribution C<dist_name>
is to be reindexed.

=back

=head2 C<$obj-E<gt>query(%args);>

This is used for querying the database by distribution
name, module name, or CPAN author name. There are
two arguments needed to specify such queries.

=over 3

=item * C<mode =E<gt> some_value>

This specifies what type of query to perform,
with C<mode> being one of C<dist>, C<module>,
or C<author>, for searching through, respectively,
CPAN distribution names, module names, or author names and
CPAN ids.

=item * C<type =E<gt> query_term>

This specifies the query term for the search, with
C<type> being one of C<name>, to search for an
exact match, or C<search>, for searching for partial
matches. Perl regular expressions are supported in
the C<query_term> for the C<search> option.

=back

As well, an option of C<max_results =E<gt> some_number> will
limit the number of results returned; if not specified,
this defaults to 200.

=head1 CPAN.pm support

As of CPAN.pm version 1.88_65, there is experimental support
within CPAN.pm for using CPAN::SQLite to obtain
information on packages, modules, and authors. One goal
of this is to reduce the memory footprint of the CPAN.pm
shell, as this information is no longer all preloaded into
memory. This can be enabled through

   perl -MCPAN -e shell
   cpan> o conf use_sqlite 1

Use

  cpan> o conf commit

to save this setting for future sessions.

Using CPAN::SQLite, what happens is that a request for information
through CPAN.pm, such as

  cpan> a ANDK

will cause a query to the SQLite database to be made.
If successful, this will place the relevant data for this
request into the data structure CPAN.pm uses to store and
retrieve such information. Thus, at any given time, the
only information CPAN.pm stores in memory is that for
packages, modules, and authors for which previous queries
have been made. There are certain requests, such as

  cpan> r

to make a list of recommended packages for which upgrades
on CPAN are available, which will result in loading
information on all available packages into memory; if such
a query is made, the subsequent memory footprint of CPAN.pm
with and without CPAN::SQLite will be essentially the same.

The database itself, called F<cpandb.sql>, will be stored
in the location specified by C<$CPAN::Config-E<gt>{cpan_home}>.
When first started, this database will be created, and afterwards,
it will be updated if the database is older than one day since
the last update. A log file of the creation or update process, called
F<cpan_search_log.dddddddddd>, will be created in the same
directory as the database file.

=head1 SEE ALSO

L<CPAN::SQLite::Index>, for setting up and maintaining
the database, and L<CPAN::SQLite::Search> for an
interface to querying the database. Some details
of the interaction with L<CPAN::> is available from
L<CPAN::SQLite::META>. See also the L<cpandb> script for a
command-line interface to the
indexing and querying of the database.

Development takes place on the CPAN-Search-Lite project
at L<http://cpan-search.svn.sourceforge.net/viewvc/cpan-search/CPAN-SQLite/>.

=head1 SUPPORT

You can find documentation for this module with the perldoc command.

    perldoc CPAN::SQLite

You can also look for information at:

=over 4

=item * AnnoCPAN: Annotated CPAN documentation

L<http://annocpan.org/dist/CPAN-SQLite>

=item * CPAN::Forum: Discussion forum

L<http:///www.cpanforum.com/dist/CPAN-SQLite>

=item * CPAN Ratings

L<http://cpanratings.perl.org/d/CPAN-SQLite>

=item * RT: CPAN's request tracker

L<http://rt.cpan.org/NoAuth/Bugs.html?Dist=CPAN-SQLite>

=item * Search CPAN

L<http://search.cpan.org/dist/CPAN-SQLite>

=back

=head1 BUGS

At this time, CPAN::SQLite keeps information contained only
in the latest version of a CPAN distribution. This means that
modules that are provided only in older versions of a CPAN
distribution will not be present in the database; for example,
at this time, the latest version of the I<libwww-perl> distribution
on CPAN is 5.805, but there are modules such as I<URI::URL::finger>
contained in version 5.10 of libwww-perl that are not present in 5.805.
This behaviour differs from that of L<CPAN::> without CPAN::SQLite.
This may change in the future.

Please report bugs and feature requests via
L<http://rt.cpan.org/NoAuth/Bugs.html?Dist=CPAN-SQLite>.

=head1 ENVIRONMENT VARIABLES

Information messages from the indexing procedures are printed
out to STDOUT if the environment variable CPAN_SQLITE_DEBUG
is set. This is automatically set within L<CPAN::SQLite::Index>.
If CPAN_SQLITE_NO_LOG_FILES is set, no log files will be created
during the indexing procedures.

=head1 AUTHORS

Randy Kobes (passed away on September 18, 2010)

Serguei Trouchelle E<lt>stro@cpan.orgE<gt>

=head1 COPYRIGHT

Copyright 2006,2008 by Randy Kobes E<lt>r.kobes@uwinnipeg.caE<gt>. 

Copyright 2011 by Serguei Trouchelle E<lt>stro@cpan.orgE<gt>.

Use and redistribution are under the same terms as Perl itself.

=cut
