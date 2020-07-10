package DBIx::Class;

use strict;
use warnings;

our $VERSION;
# Always remember to do all digits for the version even if they're 0
# i.e. first release of 0.XX *must* be 0.XX000. This avoids fBSD ports
# brain damage and presumably various other packaging systems too

# $VERSION declaration must stay up here, ahead of any other package
# declarations, as to not confuse various modules attempting to determine
# this ones version, whether that be s.c.o. or Module::Metadata, etc
$VERSION = '0.082841';

$VERSION = eval $VERSION if $VERSION =~ /_/; # numify for warning-free dev releases

use DBIx::Class::_Util;
use mro 'c3';

use DBIx::Class::Optional::Dependencies;

use base qw/DBIx::Class::Componentised DBIx::Class::AccessorGroup/;
use DBIx::Class::StartupCheck;
use DBIx::Class::Exception;

__PACKAGE__->mk_group_accessors(inherited => '_skip_namespace_frames');
__PACKAGE__->_skip_namespace_frames('^DBIx::Class|^SQL::Abstract|^Try::Tiny|^Class::Accessor::Grouped|^Context::Preserve');

# FIXME - this is not really necessary, and is in
# fact going to slow things down a bit
# However it is the right thing to do in order to get
# various install bases to highlight their brokenness
# Remove at some unknown point in the future
sub DESTROY { &DBIx::Class::_Util::detected_reinvoked_destructor }

sub mk_classdata {
  shift->mk_classaccessor(@_);
}

sub mk_classaccessor {
  my $self = shift;
  $self->mk_group_accessors('inherited', $_[0]);
  $self->set_inherited(@_) if @_ > 1;
}

sub component_base_class { 'DBIx::Class' }

sub MODIFY_CODE_ATTRIBUTES {
  my ($class,$code,@attrs) = @_;
  $class->mk_classdata('__attr_cache' => {})
    unless $class->can('__attr_cache');
  $class->__attr_cache->{$code} = [@attrs];
  return ();
}

sub _attr_cache {
  my $self = shift;
  my $cache = $self->can('__attr_cache') ? $self->__attr_cache : {};

  return {
    %$cache,
    %{ $self->maybe::next::method || {} },
  };
}

# *DO NOT* change this URL nor the identically named =head1 below
# it is linked throughout the ecosystem
sub DBIx::Class::_ENV_::HELP_URL () {
  'http://p3rl.org/DBIx::Class#GETTING_HELP/SUPPORT'
}

1;

__END__

# This is the only file where an explicit =encoding is needed,
# as the distbuild-time injected author list is utf8 encoded
# Without this pod2text output is less than ideal
#
# A bit regarding selection/compatiblity:
# Before 5.8.7 UTF-8 was == utf8, both behaving like the (lax) utf8 we know today
# Then https://www.nntp.perl.org/group/perl.unicode/2004/12/msg2705.html happened
# Encode way way before 5.8.0 supported UTF-8: https://metacpan.org/source/DANKOGAI/Encode-1.00/lib/Encode/Supported.pod#L44
# so it is safe for the oldest toolchains.
# Additionally we inject all the utf8 programattically and test its well-formedness
# so all is well
#
=encoding UTF-8

=head1 NAME

DBIx::Class - Extensible and flexible object <-> relational mapper.

=head1 WHERE TO START READING

See L<DBIx::Class::Manual::DocMap> for an overview of the exhaustive documentation.
To get the most out of DBIx::Class with the least confusion it is strongly
recommended to read (at the very least) the
L<Manuals|DBIx::Class::Manual::DocMap/Manuals> in the order presented there.

=cut

=head1 GETTING HELP/SUPPORT

Due to the sheer size of its problem domain, DBIx::Class is a relatively
complex framework. After you start using DBIx::Class questions will inevitably
arise. If you are stuck with a problem or have doubts about a particular
approach do not hesitate to contact us via any of the following options (the
list is sorted by "fastest response time"):

=over

=item * IRC: irc.perl.org#dbix-class

=for html
<a href="https://chat.mibbit.com/#dbix-class@irc.perl.org">(click for instant chatroom login)</a>

=item * Mailing list: L<http://lists.scsys.co.uk/mailman/listinfo/dbix-class>

=item * RT Bug Tracker: L<https://rt.cpan.org/NoAuth/Bugs.html?Dist=DBIx-Class>

=item * Twitter: L<https://www.twitter.com/dbix_class>

=item * Web Site: L<http://www.dbix-class.org/>

=back

=head1 SYNOPSIS

For the very impatient: L<DBIx::Class::Manual::QuickStart>

This code in the next step can be generated automatically from an existing
database, see L<dbicdump> from the distribution C<DBIx-Class-Schema-Loader>.

=head2 Schema classes preparation

Create a schema class called F<MyApp/Schema.pm>:

  package MyApp::Schema;
  use base qw/DBIx::Class::Schema/;

  __PACKAGE__->load_namespaces();

  1;

Create a result class to represent artists, who have many CDs, in
F<MyApp/Schema/Result/Artist.pm>:

See L<DBIx::Class::ResultSource> for docs on defining result classes.

  package MyApp::Schema::Result::Artist;
  use base qw/DBIx::Class::Core/;

  __PACKAGE__->table('artist');
  __PACKAGE__->add_columns(qw/ artistid name /);
  __PACKAGE__->set_primary_key('artistid');
  __PACKAGE__->has_many(cds => 'MyApp::Schema::Result::CD', 'artistid');

  1;

A result class to represent a CD, which belongs to an artist, in
F<MyApp/Schema/Result/CD.pm>:

  package MyApp::Schema::Result::CD;
  use base qw/DBIx::Class::Core/;

  __PACKAGE__->load_components(qw/InflateColumn::DateTime/);
  __PACKAGE__->table('cd');
  __PACKAGE__->add_columns(qw/ cdid artistid title year /);
  __PACKAGE__->set_primary_key('cdid');
  __PACKAGE__->belongs_to(artist => 'MyApp::Schema::Result::Artist', 'artistid');

  1;

=head2 API usage

Then you can use these classes in your application's code:

  # Connect to your database.
  use MyApp::Schema;
  my $schema = MyApp::Schema->connect($dbi_dsn, $user, $pass, \%dbi_params);

  # Query for all artists and put them in an array,
  # or retrieve them as a result set object.
  # $schema->resultset returns a DBIx::Class::ResultSet
  my @all_artists = $schema->resultset('Artist')->all;
  my $all_artists_rs = $schema->resultset('Artist');

  # Output all artists names
  # $artist here is a DBIx::Class::Row, which has accessors
  # for all its columns. Rows are also subclasses of your Result class.
  foreach $artist (@all_artists) {
    print $artist->name, "\n";
  }

  # Create a result set to search for artists.
  # This does not query the DB.
  my $johns_rs = $schema->resultset('Artist')->search(
    # Build your WHERE using an SQL::Abstract structure:
    { name => { like => 'John%' } }
  );

  # Execute a joined query to get the cds.
  my @all_john_cds = $johns_rs->search_related('cds')->all;

  # Fetch the next available row.
  my $first_john = $johns_rs->next;

  # Specify ORDER BY on the query.
  my $first_john_cds_by_title_rs = $first_john->cds(
    undef,
    { order_by => 'title' }
  );

  # Create a result set that will fetch the artist data
  # at the same time as it fetches CDs, using only one query.
  my $millennium_cds_rs = $schema->resultset('CD')->search(
    { year => 2000 },
    { prefetch => 'artist' }
  );

  my $cd = $millennium_cds_rs->next; # SELECT ... FROM cds JOIN artists ...
  my $cd_artist_name = $cd->artist->name; # Already has the data so no 2nd query

  # new() makes a Result object but doesn't insert it into the DB.
  # create() is the same as new() then insert().
  my $new_cd = $schema->resultset('CD')->new({ title => 'Spoon' });
  $new_cd->artist($cd->artist);
  $new_cd->insert; # Auto-increment primary key filled in after INSERT
  $new_cd->title('Fork');

  $schema->txn_do(sub { $new_cd->update }); # Runs the update in a transaction

  # change the year of all the millennium CDs at once
  $millennium_cds_rs->update({ year => 2002 });

=head1 DESCRIPTION

This is an SQL to OO mapper with an object API inspired by L<Class::DBI>
(with a compatibility layer as a springboard for porting) and a resultset API
that allows abstract encapsulation of database operations. It aims to make
representing queries in your code as perl-ish as possible while still
providing access to as many of the capabilities of the database as possible,
including retrieving related records from multiple tables in a single query,
C<JOIN>, C<LEFT JOIN>, C<COUNT>, C<DISTINCT>, C<GROUP BY>, C<ORDER BY> and
C<HAVING> support.

DBIx::Class can handle multi-column primary and foreign keys, complex
queries and database-level paging, and does its best to only query the
database in order to return something you've directly asked for. If a
resultset is used as an iterator it only fetches rows off the statement
handle as requested in order to minimise memory usage. It has auto-increment
support for SQLite, MySQL, PostgreSQL, Oracle, SQL Server and DB2 and is
known to be used in production on at least the first four, and is fork-
and thread-safe out of the box (although
L<your DBD may not be|DBI/Threads and Thread Safety>).

This project is still under rapid development, so large new features may be
marked B<experimental> - such APIs are still usable but may have edge bugs.
Failing test cases are I<always> welcome and point releases are put out rapidly
as bugs are found and fixed.

We do our best to maintain full backwards compatibility for published
APIs, since DBIx::Class is used in production in many organisations,
and even backwards incompatible changes to non-published APIs will be fixed
if they're reported and doing so doesn't cost the codebase anything.

The test suite is quite substantial, and several developer releases
are generally made to CPAN before the branch for the next release is
merged back to trunk for a major release.

=head1 HOW TO CONTRIBUTE

Contributions are always welcome, in all usable forms (we especially
welcome documentation improvements). The delivery methods include git-
or unified-diff formatted patches, GitHub pull requests, or plain bug
reports either via RT or the Mailing list. Do not hesitate to
L<get in touch|/GETTING HELP/SUPPORT> with any further questions you may
have.

=for comment
FIXME: Getty, frew and jnap need to get off their asses and finish the contrib section so we can link it here ;)

This project is maintained in a git repository. The code and related tools are
accessible at the following locations:

=over

=item * Current git repository: L<https://github.com/Perl5/DBIx-Class>

=item * Travis-CI log: L<https://travis-ci.org/Perl5/DBIx-Class/branches>

=back

=head1 AUTHORS

Even though a large portion of the source I<appears> to be written by just a
handful of people, this library continues to remain a collaborative effort -
perhaps one of the most successful such projects on L<CPAN|http://cpan.org>.
It is important to remember that ideas do not always result in a direct code
contribution, but deserve acknowledgement just the same. Time and time again
the seemingly most insignificant questions and suggestions have been shown
to catalyze monumental improvements in consistency, accuracy and performance.

=for comment this line is replaced with the author list at dist-building time

The canonical source of authors and their details is the F<AUTHORS> file at
the root of this distribution (or repository). The canonical source of
per-line authorship is the L<git repository|/HOW TO CONTRIBUTE> history
itself.

=head1 COPYRIGHT AND LICENSE

Copyright (c) 2005 by mst, castaway, ribasushi, and other DBIx::Class
L</AUTHORS> as listed above and in F<AUTHORS>.

This library is free software and may be distributed under the same terms
as perl5 itself. See F<LICENSE> for the complete licensing terms.
