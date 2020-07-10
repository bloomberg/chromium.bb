package DBIx::Class::ResultSet;

use strict;
use warnings;
use base qw/DBIx::Class/;
use DBIx::Class::Carp;
use DBIx::Class::ResultSetColumn;
use Scalar::Util qw/blessed weaken reftype/;
use DBIx::Class::_Util qw(
  fail_on_internal_wantarray fail_on_internal_call UNRESOLVABLE_CONDITION
);
use Try::Tiny;

# not importing first() as it will clash with our own method
use List::Util ();

BEGIN {
  # De-duplication in _merge_attr() is disabled, but left in for reference
  # (the merger is used for other things that ought not to be de-duped)
  *__HM_DEDUP = sub () { 0 };
}

# FIXME - get rid of this
use Hash::Merge ();

use namespace::clean;

use overload
        '0+'     => "count",
        'bool'   => "_bool",
        fallback => 1;

# this is real - CDBICompat overrides it with insanity
# yes, prototype won't matter, but that's for now ;)
sub _bool () { 1 }

__PACKAGE__->mk_group_accessors('simple' => qw/_result_class result_source/);

=head1 NAME

DBIx::Class::ResultSet - Represents a query used for fetching a set of results.

=head1 SYNOPSIS

  my $users_rs = $schema->resultset('User');
  while( $user = $users_rs->next) {
    print $user->username;
  }

  my $registered_users_rs = $schema->resultset('User')->search({ registered => 1 });
  my @cds_in_2005 = $schema->resultset('CD')->search({ year => 2005 })->all();

=head1 DESCRIPTION

A ResultSet is an object which stores a set of conditions representing
a query. It is the backbone of DBIx::Class (i.e. the really
important/useful bit).

No SQL is executed on the database when a ResultSet is created, it
just stores all the conditions needed to create the query.

A basic ResultSet representing the data of an entire table is returned
by calling C<resultset> on a L<DBIx::Class::Schema> and passing in a
L<Source|DBIx::Class::Manual::Glossary/ResultSource> name.

  my $users_rs = $schema->resultset('User');

A new ResultSet is returned from calling L</search> on an existing
ResultSet. The new one will contain all the conditions of the
original, plus any new conditions added in the C<search> call.

A ResultSet also incorporates an implicit iterator. L</next> and L</reset>
can be used to walk through all the L<DBIx::Class::Row>s the ResultSet
represents.

The query that the ResultSet represents is B<only> executed against
the database when these methods are called:
L</find>, L</next>, L</all>, L</first>, L</single>, L</count>.

If a resultset is used in a numeric context it returns the L</count>.
However, if it is used in a boolean context it is B<always> true.  So if
you want to check if a resultset has any results, you must use C<if $rs
!= 0>.

=head1 EXAMPLES

=head2 Chaining resultsets

Let's say you've got a query that needs to be run to return some data
to the user. But, you have an authorization system in place that
prevents certain users from seeing certain information. So, you want
to construct the basic query in one method, but add constraints to it in
another.

  sub get_data {
    my $self = shift;
    my $request = $self->get_request; # Get a request object somehow.
    my $schema = $self->result_source->schema;

    my $cd_rs = $schema->resultset('CD')->search({
      title => $request->param('title'),
      year => $request->param('year'),
    });

    $cd_rs = $self->apply_security_policy( $cd_rs );

    return $cd_rs->all();
  }

  sub apply_security_policy {
    my $self = shift;
    my ($rs) = @_;

    return $rs->search({
      subversive => 0,
    });
  }

=head3 Resolving conditions and attributes

When a resultset is chained from another resultset (e.g.:
C<< my $new_rs = $old_rs->search(\%extra_cond, \%attrs) >>), conditions
and attributes with the same keys need resolving.

If any of L</columns>, L</select>, L</as> are present, they reset the
original selection, and start the selection "clean".

The L</join>, L</prefetch>, L</+columns>, L</+select>, L</+as> attributes
are merged into the existing ones from the original resultset.

The L</where> and L</having> attributes, and any search conditions, are
merged with an SQL C<AND> to the existing condition from the original
resultset.

All other attributes are overridden by any new ones supplied in the
search attributes.

=head2 Multiple queries

Since a resultset just defines a query, you can do all sorts of
things with it with the same object.

  # Don't hit the DB yet.
  my $cd_rs = $schema->resultset('CD')->search({
    title => 'something',
    year => 2009,
  });

  # Each of these hits the DB individually.
  my $count = $cd_rs->count;
  my $most_recent = $cd_rs->get_column('date_released')->max();
  my @records = $cd_rs->all;

And it's not just limited to SELECT statements.

  $cd_rs->delete();

This is even cooler:

  $cd_rs->create({ artist => 'Fred' });

Which is the same as:

  $schema->resultset('CD')->create({
    title => 'something',
    year => 2009,
    artist => 'Fred'
  });

See: L</search>, L</count>, L</get_column>, L</all>, L</create>.

=head2 Custom ResultSet classes

To add methods to your resultsets, you can subclass L<DBIx::Class::ResultSet>, similar to:

  package MyApp::Schema::ResultSet::User;

  use strict;
  use warnings;

  use base 'DBIx::Class::ResultSet';

  sub active {
    my $self = shift;
    $self->search({ $self->current_source_alias . '.active' => 1 });
  }

  sub unverified {
    my $self = shift;
    $self->search({ $self->current_source_alias . '.verified' => 0 });
  }

  sub created_n_days_ago {
    my ($self, $days_ago) = @_;
    $self->search({
      $self->current_source_alias . '.create_date' => {
        '<=',
      $self->result_source->schema->storage->datetime_parser->format_datetime(
        DateTime->now( time_zone => 'UTC' )->subtract( days => $days_ago )
      )}
    });
  }

  sub users_to_warn { shift->active->unverified->created_n_days_ago(7) }

  1;

See L<DBIx::Class::Schema/load_namespaces> on how DBIC can discover and
automatically attach L<Result|DBIx::Class::Manual::ResultClass>-specific
L<ResulSet|DBIx::Class::ResultSet> classes.

=head3 ResultSet subclassing with Moose and similar constructor-providers

Using L<Moose> or L<Moo> in your ResultSet classes is usually overkill, but
you may find it useful if your ResultSets contain a lot of business logic
(e.g. C<has xml_parser>, C<has json>, etc) or if you just prefer to organize
your code via roles.

In order to write custom ResultSet classes with L<Moo> you need to use the
following template. The L<BUILDARGS|Moo/BUILDARGS> is necessary due to the
unusual signature of the L<constructor provided by DBIC
|DBIx::Class::ResultSet/new> C<< ->new($source, \%args) >>.

  use Moo;
  extends 'DBIx::Class::ResultSet';
  sub BUILDARGS { $_[2] } # ::RS::new() expects my ($class, $rsrc, $args) = @_

  ...your code...

  1;

If you want to build your custom ResultSet classes with L<Moose>, you need
a similar, though a little more elaborate template in order to interface the
inlining of the L<Moose>-provided
L<object constructor|Moose::Manual::Construction/WHERE'S THE CONSTRUCTOR?>,
with the DBIC one.

  package MyApp::Schema::ResultSet::User;

  use Moose;
  use MooseX::NonMoose;
  extends 'DBIx::Class::ResultSet';

  sub BUILDARGS { $_[2] } # ::RS::new() expects my ($class, $rsrc, $args) = @_

  ...your code...

  __PACKAGE__->meta->make_immutable;

  1;

The L<MooseX::NonMoose> is necessary so that the L<Moose> constructor does not
entirely overwrite the DBIC one (in contrast L<Moo> does this automatically).
Alternatively, you can skip L<MooseX::NonMoose> and get by with just L<Moose>
instead by doing:

  __PACKAGE__->meta->make_immutable(inline_constructor => 0);

=head1 METHODS

=head2 new

=over 4

=item Arguments: L<$source|DBIx::Class::ResultSource>, L<\%attrs?|/ATTRIBUTES>

=item Return Value: L<$resultset|/search>

=back

The resultset constructor. Takes a source object (usually a
L<DBIx::Class::ResultSourceProxy::Table>) and an attribute hash (see
L</ATTRIBUTES> below).  Does not perform any queries -- these are
executed as needed by the other methods.

Generally you never construct a resultset manually. Instead you get one
from e.g. a
C<< $schema->L<resultset|DBIx::Class::Schema/resultset>('$source_name') >>
or C<< $another_resultset->L<search|/search>(...) >> (the later called in
scalar context):

  my $rs = $schema->resultset('CD')->search({ title => '100th Window' });

=over

=item WARNING

If called on an object, proxies to L</new_result> instead, so

  my $cd = $schema->resultset('CD')->new({ title => 'Spoon' });

will return a CD object, not a ResultSet, and is equivalent to:

  my $cd = $schema->resultset('CD')->new_result({ title => 'Spoon' });

Please also keep in mind that many internals call L</new_result> directly,
so overloading this method with the idea of intercepting new result object
creation B<will not work>. See also warning pertaining to L</create>.

=back

=cut

sub new {
  my $class = shift;

  if (ref $class) {
    DBIx::Class::_ENV_::ASSERT_NO_INTERNAL_INDIRECT_CALLS and fail_on_internal_call;
    return $class->new_result(@_);
  }

  my ($source, $attrs) = @_;
  $source = $source->resolve
    if $source->isa('DBIx::Class::ResultSourceHandle');

  $attrs = { %{$attrs||{}} };
  delete @{$attrs}{qw(_last_sqlmaker_alias_map _simple_passthrough_construction)};

  if ($attrs->{page}) {
    $attrs->{rows} ||= 10;
  }

  $attrs->{alias} ||= 'me';

  my $self = bless {
    result_source => $source,
    cond => $attrs->{where},
    pager => undef,
    attrs => $attrs,
  }, $class;

  # if there is a dark selector, this means we are already in a
  # chain and the cleanup/sanification was taken care of by
  # _search_rs already
  $self->_normalize_selection($attrs)
    unless $attrs->{_dark_selector};

  $self->result_class(
    $attrs->{result_class} || $source->result_class
  );

  $self;
}

=head2 search

=over 4

=item Arguments: L<$cond|DBIx::Class::SQLMaker> | undef, L<\%attrs?|/ATTRIBUTES>

=item Return Value: $resultset (scalar context) | L<@result_objs|DBIx::Class::Manual::ResultClass> (list context)

=back

  my @cds    = $cd_rs->search({ year => 2001 }); # "... WHERE year = 2001"
  my $new_rs = $cd_rs->search({ year => 2005 });

  my $new_rs = $cd_rs->search([ { year => 2005 }, { year => 2004 } ]);
                 # year = 2005 OR year = 2004

In list context, C<< ->all() >> is called implicitly on the resultset, thus
returning a list of L<result|DBIx::Class::Manual::ResultClass> objects instead.
To avoid that, use L</search_rs>.

If you need to pass in additional attributes but no additional condition,
call it as C<search(undef, \%attrs)>.

  # "SELECT name, artistid FROM $artist_table"
  my @all_artists = $schema->resultset('Artist')->search(undef, {
    columns => [qw/name artistid/],
  });

For a list of attributes that can be passed to C<search>, see
L</ATTRIBUTES>. For more examples of using this function, see
L<Searching|DBIx::Class::Manual::Cookbook/SEARCHING>. For a complete
documentation for the first argument, see L<SQL::Abstract/"WHERE CLAUSES">
and its extension L<DBIx::Class::SQLMaker>.

For more help on using joins with search, see L<DBIx::Class::Manual::Joining>.

=head3 CAVEAT

Note that L</search> does not process/deflate any of the values passed in the
L<SQL::Abstract>-compatible search condition structure. This is unlike other
condition-bound methods L</new_result>, L</create> and L</find>. The user must ensure
manually that any value passed to this method will stringify to something the
RDBMS knows how to deal with. A notable example is the handling of L<DateTime>
objects, for more info see:
L<DBIx::Class::Manual::Cookbook/Formatting DateTime objects in queries>.

=cut

sub search {
  my $self = shift;
  my $rs = $self->search_rs( @_ );

  if (wantarray) {
    DBIx::Class::_ENV_::ASSERT_NO_INTERNAL_WANTARRAY and my $sog = fail_on_internal_wantarray;
    return $rs->all;
  }
  elsif (defined wantarray) {
    return $rs;
  }
  else {
    # we can be called by a relationship helper, which in
    # turn may be called in void context due to some braindead
    # overload or whatever else the user decided to be clever
    # at this particular day. Thus limit the exception to
    # external code calls only
    $self->throw_exception ('->search is *not* a mutator, calling it in void context makes no sense')
      if (caller)[0] !~ /^\QDBIx::Class::/;

    return ();
  }
}

=head2 search_rs

=over 4

=item Arguments: L<$cond|DBIx::Class::SQLMaker>, L<\%attrs?|/ATTRIBUTES>

=item Return Value: L<$resultset|/search>

=back

This method does the same exact thing as search() except it will
always return a resultset, even in list context.

=cut

sub search_rs {
  my $self = shift;

  my $rsrc = $self->result_source;
  my ($call_cond, $call_attrs);

  # Special-case handling for (undef, undef) or (undef)
  # Note that (foo => undef) is valid deprecated syntax
  @_ = () if not scalar grep { defined $_ } @_;

  # just a cond
  if (@_ == 1) {
    $call_cond = shift;
  }
  # fish out attrs in the ($condref, $attr) case
  elsif (@_ == 2 and ( ! defined $_[0] or (ref $_[0]) ne '') ) {
    ($call_cond, $call_attrs) = @_;
  }
  elsif (@_ % 2) {
    $self->throw_exception('Odd number of arguments to search')
  }
  # legacy search
  elsif (@_) {
    carp_unique 'search( %condition ) is deprecated, use search( \%condition ) instead'
      unless $rsrc->result_class->isa('DBIx::Class::CDBICompat');

    for my $i (0 .. $#_) {
      next if $i % 2;
      $self->throw_exception ('All keys in condition key/value pairs must be plain scalars')
        if (! defined $_[$i] or ref $_[$i] ne '');
    }

    $call_cond = { @_ };
  }

  # see if we can keep the cache (no $rs changes)
  my $cache;
  my %safe = (alias => 1, cache => 1);
  if ( ! List::Util::first { !$safe{$_} } keys %$call_attrs and (
    ! defined $call_cond
      or
    ref $call_cond eq 'HASH' && ! keys %$call_cond
      or
    ref $call_cond eq 'ARRAY' && ! @$call_cond
  )) {
    $cache = $self->get_cache;
  }

  my $old_attrs = { %{$self->{attrs}} };
  my ($old_having, $old_where) = delete @{$old_attrs}{qw(having where)};

  my $new_attrs = { %$old_attrs };

  # take care of call attrs (only if anything is changing)
  if ($call_attrs and keys %$call_attrs) {

    # copy for _normalize_selection
    $call_attrs = { %$call_attrs };

    my @selector_attrs = qw/select as columns cols +select +as +columns include_columns/;

    # reset the current selector list if new selectors are supplied
    if (List::Util::first { exists $call_attrs->{$_} } qw/columns cols select as/) {
      delete @{$old_attrs}{(@selector_attrs, '_dark_selector')};
    }

    # Normalize the new selector list (operates on the passed-in attr structure)
    # Need to do it on every chain instead of only once on _resolved_attrs, in
    # order to allow detection of empty vs partial 'as'
    $call_attrs->{_dark_selector} = $old_attrs->{_dark_selector}
      if $old_attrs->{_dark_selector};
    $self->_normalize_selection ($call_attrs);

    # start with blind overwriting merge, exclude selector attrs
    $new_attrs = { %{$old_attrs}, %{$call_attrs} };
    delete @{$new_attrs}{@selector_attrs};

    for (@selector_attrs) {
      $new_attrs->{$_} = $self->_merge_attr($old_attrs->{$_}, $call_attrs->{$_})
        if ( exists $old_attrs->{$_} or exists $call_attrs->{$_} );
    }

    # older deprecated name, use only if {columns} is not there
    if (my $c = delete $new_attrs->{cols}) {
      carp_unique( "Resultset attribute 'cols' is deprecated, use 'columns' instead" );
      if ($new_attrs->{columns}) {
        carp "Resultset specifies both the 'columns' and the legacy 'cols' attributes - ignoring 'cols'";
      }
      else {
        $new_attrs->{columns} = $c;
      }
    }


    # join/prefetch use their own crazy merging heuristics
    foreach my $key (qw/join prefetch/) {
      $new_attrs->{$key} = $self->_merge_joinpref_attr($old_attrs->{$key}, $call_attrs->{$key})
        if exists $call_attrs->{$key};
    }

    # stack binds together
    $new_attrs->{bind} = [ @{ $old_attrs->{bind} || [] }, @{ $call_attrs->{bind} || [] } ];
  }


  for ($old_where, $call_cond) {
    if (defined $_) {
      $new_attrs->{where} = $self->_stack_cond (
        $_, $new_attrs->{where}
      );
    }
  }

  if (defined $old_having) {
    $new_attrs->{having} = $self->_stack_cond (
      $old_having, $new_attrs->{having}
    )
  }

  my $rs = (ref $self)->new($rsrc, $new_attrs);

  $rs->set_cache($cache) if ($cache);

  return $rs;
}

my $dark_sel_dumper;
sub _normalize_selection {
  my ($self, $attrs) = @_;

  # legacy syntax
  if ( exists $attrs->{include_columns} ) {
    carp_unique( "Resultset attribute 'include_columns' is deprecated, use '+columns' instead" );
    $attrs->{'+columns'} = $self->_merge_attr(
      $attrs->{'+columns'}, delete $attrs->{include_columns}
    );
  }

  # columns are always placed first, however

  # Keep the X vs +X separation until _resolved_attrs time - this allows to
  # delay the decision on whether to use a default select list ($rsrc->columns)
  # allowing stuff like the remove_columns helper to work
  #
  # select/as +select/+as pairs need special handling - the amount of select/as
  # elements in each pair does *not* have to be equal (think multicolumn
  # selectors like distinct(foo, bar) ). If the selector is bare (no 'as'
  # supplied at all) - try to infer the alias, either from the -as parameter
  # of the selector spec, or use the parameter whole if it looks like a column
  # name (ugly legacy heuristic). If all fails - leave the selector bare (which
  # is ok as well), but make sure no more additions to the 'as' chain take place
  for my $pref ('', '+') {

    my ($sel, $as) = map {
      my $key = "${pref}${_}";

      my $val = [ ref $attrs->{$key} eq 'ARRAY'
        ? @{$attrs->{$key}}
        : $attrs->{$key} || ()
      ];
      delete $attrs->{$key};
      $val;
    } qw/select as/;

    if (! @$as and ! @$sel ) {
      next;
    }
    elsif (@$as and ! @$sel) {
      $self->throw_exception(
        "Unable to handle ${pref}as specification (@$as) without a corresponding ${pref}select"
      );
    }
    elsif( ! @$as ) {
      # no as part supplied at all - try to deduce (unless explicit end of named selection is declared)
      # if any @$as has been supplied we assume the user knows what (s)he is doing
      # and blindly keep stacking up pieces
      unless ($attrs->{_dark_selector}) {
        SELECTOR:
        for (@$sel) {
          if ( ref $_ eq 'HASH' and exists $_->{-as} ) {
            push @$as, $_->{-as};
          }
          # assume any plain no-space, no-parenthesis string to be a column spec
          # FIXME - this is retarded but is necessary to support shit like 'count(foo)'
          elsif ( ! ref $_ and $_ =~ /^ [^\s\(\)]+ $/x) {
            push @$as, $_;
          }
          # if all else fails - raise a flag that no more aliasing will be allowed
          else {
            $attrs->{_dark_selector} = {
              plus_stage => $pref,
              string => ($dark_sel_dumper ||= do {
                  require Data::Dumper::Concise;
                  Data::Dumper::Concise::DumperObject()->Indent(0);
                })->Values([$_])->Dump
              ,
            };
            last SELECTOR;
          }
        }
      }
    }
    elsif (@$as < @$sel) {
      $self->throw_exception(
        "Unable to handle an ${pref}as specification (@$as) with less elements than the corresponding ${pref}select"
      );
    }
    elsif ($pref and $attrs->{_dark_selector}) {
      $self->throw_exception(
        "Unable to process named '+select', resultset contains an unnamed selector $attrs->{_dark_selector}{string}"
      );
    }


    # merge result
    $attrs->{"${pref}select"} = $self->_merge_attr($attrs->{"${pref}select"}, $sel);
    $attrs->{"${pref}as"} = $self->_merge_attr($attrs->{"${pref}as"}, $as);
  }
}

sub _stack_cond {
  my ($self, $left, $right) = @_;

  (
    (ref $_ eq 'ARRAY' and !@$_)
      or
    (ref $_ eq 'HASH' and ! keys %$_)
  ) and $_ = undef for ($left, $right);

  # either one of the two undef
  if ( (defined $left) xor (defined $right) ) {
    return defined $left ? $left : $right;
  }
  # both undef
  elsif ( ! defined $left ) {
    return undef
  }
  else {
    return $self->result_source->schema->storage->_collapse_cond({ -and => [$left, $right] });
  }
}

=head2 search_literal

B<CAVEAT>: C<search_literal> is provided for Class::DBI compatibility and
should only be used in that context. C<search_literal> is a convenience
method. It is equivalent to calling C<< $schema->search(\[]) >>, but if you
want to ensure columns are bound correctly, use L</search>.

See L<DBIx::Class::Manual::Cookbook/SEARCHING> and
L<DBIx::Class::Manual::FAQ/Searching> for searching techniques that do not
require C<search_literal>.

=over 4

=item Arguments: $sql_fragment, @standalone_bind_values

=item Return Value: L<$resultset|/search> (scalar context) | L<@result_objs|DBIx::Class::Manual::ResultClass> (list context)

=back

  my @cds   = $cd_rs->search_literal('year = ? AND title = ?', qw/2001 Reload/);
  my $newrs = $artist_rs->search_literal('name = ?', 'Metallica');

Pass a literal chunk of SQL to be added to the conditional part of the
resultset query.

Example of how to use C<search> instead of C<search_literal>

  my @cds = $cd_rs->search_literal('cdid = ? AND (artist = ? OR artist = ?)', (2, 1, 2));
  my @cds = $cd_rs->search(\[ 'cdid = ? AND (artist = ? OR artist = ?)', [ 'cdid', 2 ], [ 'artist', 1 ], [ 'artist', 2 ] ]);

=cut

sub search_literal {
  my ($self, $sql, @bind) = @_;
  my $attr;
  if ( @bind && ref($bind[-1]) eq 'HASH' ) {
    $attr = pop @bind;
  }
  return $self->search(\[ $sql, map [ {} => $_ ], @bind ], ($attr || () ));
}

=head2 find

=over 4

=item Arguments: \%columns_values | @pk_values, { key => $unique_constraint, L<%attrs|/ATTRIBUTES> }?

=item Return Value: L<$result|DBIx::Class::Manual::ResultClass> | undef

=back

Finds and returns a single row based on supplied criteria. Takes either a
hashref with the same format as L</create> (including inference of foreign
keys from related objects), or a list of primary key values in the same
order as the L<primary columns|DBIx::Class::ResultSource/primary_columns>
declaration on the L</result_source>.

In either case an attempt is made to combine conditions already existing on
the resultset with the condition passed to this method.

To aid with preparing the correct query for the storage you may supply the
C<key> attribute, which is the name of a
L<unique constraint|DBIx::Class::ResultSource/add_unique_constraint> (the
unique constraint corresponding to the
L<primary columns|DBIx::Class::ResultSource/primary_columns> is always named
C<primary>). If the C<key> attribute has been supplied, and DBIC is unable
to construct a query that satisfies the named unique constraint fully (
non-NULL values for each column member of the constraint) an exception is
thrown.

If no C<key> is specified, the search is carried over all unique constraints
which are fully defined by the available condition.

If no such constraint is found, C<find> currently defaults to a simple
C<< search->(\%column_values) >> which may or may not do what you expect.
Note that this fallback behavior may be deprecated in further versions. If
you need to search with arbitrary conditions - use L</search>. If the query
resulting from this fallback produces more than one row, a warning to the
effect is issued, though only the first row is constructed and returned as
C<$result_object>.

In addition to C<key>, L</find> recognizes and applies standard
L<resultset attributes|/ATTRIBUTES> in the same way as L</search> does.

Note that if you have extra concerns about the correctness of the resulting
query you need to specify the C<key> attribute and supply the entire condition
as an argument to find (since it is not always possible to perform the
combination of the resultset condition with the supplied one, especially if
the resultset condition contains literal sql).

For example, to find a row by its primary key:

  my $cd = $schema->resultset('CD')->find(5);

You can also find a row by a specific unique constraint:

  my $cd = $schema->resultset('CD')->find(
    {
      artist => 'Massive Attack',
      title  => 'Mezzanine',
    },
    { key => 'cd_artist_title' }
  );

See also L</find_or_create> and L</update_or_create>.

=cut

sub find {
  my $self = shift;
  my $attrs = (@_ > 1 && ref $_[$#_] eq 'HASH' ? pop(@_) : {});

  my $rsrc = $self->result_source;

  my $constraint_name;
  if (exists $attrs->{key}) {
    $constraint_name = defined $attrs->{key}
      ? $attrs->{key}
      : $self->throw_exception("An undefined 'key' resultset attribute makes no sense")
    ;
  }

  # Parse out the condition from input
  my $call_cond;

  if (ref $_[0] eq 'HASH') {
    $call_cond = { %{$_[0]} };
  }
  else {
    # if only values are supplied we need to default to 'primary'
    $constraint_name = 'primary' unless defined $constraint_name;

    my @c_cols = $rsrc->unique_constraint_columns($constraint_name);

    $self->throw_exception(
      "No constraint columns, maybe a malformed '$constraint_name' constraint?"
    ) unless @c_cols;

    $self->throw_exception (
      'find() expects either a column/value hashref, or a list of values '
    . "corresponding to the columns of the specified unique constraint '$constraint_name'"
    ) unless @c_cols == @_;

    @{$call_cond}{@c_cols} = @_;
  }

  # process relationship data if any
  for my $key (keys %$call_cond) {
    if (
      length ref($call_cond->{$key})
        and
      my $relinfo = $rsrc->relationship_info($key)
        and
      # implicitly skip has_many's (likely MC)
      (ref (my $val = delete $call_cond->{$key}) ne 'ARRAY' )
    ) {
      my ($rel_cond, $crosstable) = $rsrc->_resolve_condition(
        $relinfo->{cond}, $val, $key, $key
      );

      $self->throw_exception("Complex condition via relationship '$key' is unsupported in find()")
         if $crosstable or ref($rel_cond) ne 'HASH';

      # supplement condition
      # relationship conditions take precedence (?)
      @{$call_cond}{keys %$rel_cond} = values %$rel_cond;
    }
  }

  my $alias = exists $attrs->{alias} ? $attrs->{alias} : $self->{attrs}{alias};
  my $final_cond;
  if (defined $constraint_name) {
    $final_cond = $self->_qualify_cond_columns (

      $self->result_source->_minimal_valueset_satisfying_constraint(
        constraint_name => $constraint_name,
        values => ($self->_merge_with_rscond($call_cond))[0],
        carp_on_nulls => 1,
      ),

      $alias,
    );
  }
  elsif ($self->{attrs}{accessor} and $self->{attrs}{accessor} eq 'single') {
    # This means that we got here after a merger of relationship conditions
    # in ::Relationship::Base::search_related (the row method), and furthermore
    # the relationship is of the 'single' type. This means that the condition
    # provided by the relationship (already attached to $self) is sufficient,
    # as there can be only one row in the database that would satisfy the
    # relationship
  }
  else {
    my (@unique_queries, %seen_column_combinations, $ci, @fc_exceptions);

    # no key was specified - fall down to heuristics mode:
    # run through all unique queries registered on the resultset, and
    # 'OR' all qualifying queries together
    #
    # always start from 'primary' if it exists at all
    for my $c_name ( sort {
        $a eq 'primary' ? -1
      : $b eq 'primary' ? 1
      : $a cmp $b
    } $rsrc->unique_constraint_names) {

      next if $seen_column_combinations{
        join "\x00", sort $rsrc->unique_constraint_columns($c_name)
      }++;

      try {
        push @unique_queries, $self->_qualify_cond_columns(
          $self->result_source->_minimal_valueset_satisfying_constraint(
            constraint_name => $c_name,
            values => ($self->_merge_with_rscond($call_cond))[0],
            columns_info => ($ci ||= $self->result_source->columns_info),
          ),
          $alias
        );
      }
      catch {
        push @fc_exceptions, $_ if $_ =~ /\bFilterColumn\b/;
      };
    }

    $final_cond =
        @unique_queries   ? \@unique_queries
      : @fc_exceptions    ? $self->throw_exception(join "; ", map { $_ =~ /(.*) at .+ line \d+$/s } @fc_exceptions )
      :                     $self->_non_unique_find_fallback ($call_cond, $attrs)
    ;
  }

  # Run the query, passing the result_class since it should propagate for find
  my $rs = $self->search ($final_cond, {result_class => $self->result_class, %$attrs});
  if ($rs->_resolved_attrs->{collapse}) {
    my $row = $rs->next;
    carp "Query returned more than one row" if $rs->next;
    return $row;
  }
  else {
    return $rs->single;
  }
}

# This is a stop-gap method as agreed during the discussion on find() cleanup:
# http://lists.scsys.co.uk/pipermail/dbix-class/2010-October/009535.html
#
# It is invoked when find() is called in legacy-mode with insufficiently-unique
# condition. It is provided for overrides until a saner way forward is devised
#
# *NOTE* This is not a public method, and it's *GUARANTEED* to disappear down
# the road. Please adjust your tests accordingly to catch this situation early
# DBIx::Class::ResultSet->can('_non_unique_find_fallback') is reasonable
#
# The method will not be removed without an adequately complete replacement
# for strict-mode enforcement
sub _non_unique_find_fallback {
  my ($self, $cond, $attrs) = @_;

  return $self->_qualify_cond_columns(
    $cond,
    exists $attrs->{alias}
      ? $attrs->{alias}
      : $self->{attrs}{alias}
  );
}


sub _qualify_cond_columns {
  my ($self, $cond, $alias) = @_;

  my %aliased = %$cond;
  for (keys %aliased) {
    $aliased{"$alias.$_"} = delete $aliased{$_}
      if $_ !~ /\./;
  }

  return \%aliased;
}

sub _build_unique_cond {
  carp_unique sprintf
    '_build_unique_cond is a private method, and moreover is about to go '
  . 'away. Please contact the development team at %s if you believe you '
  . 'have a genuine use for this method, in order to discuss alternatives.',
    DBIx::Class::_ENV_::HELP_URL,
  ;

  my ($self, $constraint_name, $cond, $croak_on_null) = @_;

  $self->result_source->_minimal_valueset_satisfying_constraint(
    constraint_name => $constraint_name,
    values => $cond,
    carp_on_nulls => !$croak_on_null
  );
}

=head2 search_related

=over 4

=item Arguments: $rel_name, $cond?, L<\%attrs?|/ATTRIBUTES>

=item Return Value: L<$resultset|/search> (scalar context) | L<@result_objs|DBIx::Class::Manual::ResultClass> (list context)

=back

  $new_rs = $cd_rs->search_related('artist', {
    name => 'Emo-R-Us',
  });

Searches the specified relationship, optionally specifying a condition and
attributes for matching records. See L</ATTRIBUTES> for more information.

In list context, C<< ->all() >> is called implicitly on the resultset, thus
returning a list of result objects instead. To avoid that, use L</search_related_rs>.

See also L</search_related_rs>.

=cut

sub search_related {
  return shift->related_resultset(shift)->search(@_);
}

=head2 search_related_rs

This method works exactly the same as search_related, except that
it guarantees a resultset, even in list context.

=cut

sub search_related_rs {
  return shift->related_resultset(shift)->search_rs(@_);
}

=head2 cursor

=over 4

=item Arguments: none

=item Return Value: L<$cursor|DBIx::Class::Cursor>

=back

Returns a storage-driven cursor to the given resultset. See
L<DBIx::Class::Cursor> for more information.

=cut

sub cursor {
  my $self = shift;

  return $self->{cursor} ||= do {
    my $attrs = $self->_resolved_attrs;
    $self->result_source->storage->select(
      $attrs->{from}, $attrs->{select}, $attrs->{where}, $attrs
    );
  };
}

=head2 single

=over 4

=item Arguments: L<$cond?|DBIx::Class::SQLMaker>

=item Return Value: L<$result|DBIx::Class::Manual::ResultClass> | undef

=back

  my $cd = $schema->resultset('CD')->single({ year => 2001 });

Inflates the first result without creating a cursor if the resultset has
any records in it; if not returns C<undef>. Used by L</find> as a lean version
of L</search>.

While this method can take an optional search condition (just like L</search>)
being a fast-code-path it does not recognize search attributes. If you need to
add extra joins or similar, call L</search> and then chain-call L</single> on the
L<DBIx::Class::ResultSet> returned.

=over

=item B<Note>

As of 0.08100, this method enforces the assumption that the preceding
query returns only one row. If more than one row is returned, you will receive
a warning:

  Query returned more than one row

In this case, you should be using L</next> or L</find> instead, or if you really
know what you are doing, use the L</rows> attribute to explicitly limit the size
of the resultset.

This method will also throw an exception if it is called on a resultset prefetching
has_many, as such a prefetch implies fetching multiple rows from the database in
order to assemble the resulting object.

=back

=cut

sub single {
  my ($self, $where) = @_;
  if(@_ > 2) {
      $self->throw_exception('single() only takes search conditions, no attributes. You want ->search( $cond, $attrs )->single()');
  }

  my $attrs = { %{$self->_resolved_attrs} };

  $self->throw_exception(
    'single() can not be used on resultsets collapsing a has_many. Use find( \%cond ) or next() instead'
  ) if $attrs->{collapse};

  if ($where) {
    if (defined $attrs->{where}) {
      $attrs->{where} = {
        '-and' =>
            [ map { ref $_ eq 'ARRAY' ? [ -or => $_ ] : $_ }
               $where, delete $attrs->{where} ]
      };
    } else {
      $attrs->{where} = $where;
    }
  }

  my $data = [ $self->result_source->storage->select_single(
    $attrs->{from}, $attrs->{select},
    $attrs->{where}, $attrs
  )];

  return undef unless @$data;
  $self->{_stashed_rows} = [ $data ];
  $self->_construct_results->[0];
}

=head2 get_column

=over 4

=item Arguments: L<$cond?|DBIx::Class::SQLMaker>

=item Return Value: L<$resultsetcolumn|DBIx::Class::ResultSetColumn>

=back

  my $max_length = $rs->get_column('length')->max;

Returns a L<DBIx::Class::ResultSetColumn> instance for a column of the ResultSet.

=cut

sub get_column {
  my ($self, $column) = @_;
  my $new = DBIx::Class::ResultSetColumn->new($self, $column);
  return $new;
}

=head2 search_like

=over 4

=item Arguments: L<$cond|DBIx::Class::SQLMaker>, L<\%attrs?|/ATTRIBUTES>

=item Return Value: L<$resultset|/search> (scalar context) | L<@result_objs|DBIx::Class::Manual::ResultClass> (list context)

=back

  # WHERE title LIKE '%blue%'
  $cd_rs = $rs->search_like({ title => '%blue%'});

Performs a search, but uses C<LIKE> instead of C<=> as the condition. Note
that this is simply a convenience method retained for ex Class::DBI users.
You most likely want to use L</search> with specific operators.

For more information, see L<DBIx::Class::Manual::Cookbook>.

This method is deprecated and will be removed in 0.09. Use L<search()|/search>
instead. An example conversion is:

  ->search_like({ foo => 'bar' });

  # Becomes

  ->search({ foo => { like => 'bar' } });

=cut

sub search_like {
  my $class = shift;
  carp_unique (
    'search_like() is deprecated and will be removed in DBIC version 0.09.'
   .' Instead use ->search({ x => { -like => "y%" } })'
   .' (note the outer pair of {}s - they are important!)'
  );
  my $attrs = (@_ > 1 && ref $_[$#_] eq 'HASH' ? pop(@_) : {});
  my $query = ref $_[0] eq 'HASH' ? { %{shift()} }: {@_};
  $query->{$_} = { 'like' => $query->{$_} } for keys %$query;
  return $class->search($query, { %$attrs });
}

=head2 slice

=over 4

=item Arguments: $first, $last

=item Return Value: L<$resultset|/search> (scalar context) | L<@result_objs|DBIx::Class::Manual::ResultClass> (list context)

=back

Returns a resultset or object list representing a subset of elements from the
resultset slice is called on. Indexes are from 0, i.e., to get the first
three records, call:

  my ($one, $two, $three) = $rs->slice(0, 2);

=cut

sub slice {
  my ($self, $min, $max) = @_;
  my $attrs = {}; # = { %{ $self->{attrs} || {} } };
  $attrs->{offset} = $self->{attrs}{offset} || 0;
  $attrs->{offset} += $min;
  $attrs->{rows} = ($max ? ($max - $min + 1) : 1);
  return $self->search(undef, $attrs);
}

=head2 next

=over 4

=item Arguments: none

=item Return Value: L<$result|DBIx::Class::Manual::ResultClass> | undef

=back

Returns the next element in the resultset (C<undef> is there is none).

Can be used to efficiently iterate over records in the resultset:

  my $rs = $schema->resultset('CD')->search;
  while (my $cd = $rs->next) {
    print $cd->title;
  }

Note that you need to store the resultset object, and call C<next> on it.
Calling C<< resultset('Table')->next >> repeatedly will always return the
first record from the resultset.

=cut

sub next {
  my ($self) = @_;

  if (my $cache = $self->get_cache) {
    $self->{all_cache_position} ||= 0;
    return $cache->[$self->{all_cache_position}++];
  }

  if ($self->{attrs}{cache}) {
    delete $self->{pager};
    $self->{all_cache_position} = 1;
    return ($self->all)[0];
  }

  return shift(@{$self->{_stashed_results}}) if @{ $self->{_stashed_results}||[] };

  $self->{_stashed_results} = $self->_construct_results
    or return undef;

  return shift @{$self->{_stashed_results}};
}

# Constructs as many results as it can in one pass while respecting
# cursor laziness. Several modes of operation:
#
# * Always builds everything present in @{$self->{_stashed_rows}}
# * If called with $fetch_all true - pulls everything off the cursor and
#   builds all result structures (or objects) in one pass
# * If $self->_resolved_attrs->{collapse} is true, checks the order_by
#   and if the resultset is ordered properly by the left side:
#   * Fetches stuff off the cursor until the "master object" changes,
#     and saves the last extra row (if any) in @{$self->{_stashed_rows}}
#   OR
#   * Just fetches, and collapses/constructs everything as if $fetch_all
#     was requested (there is no other way to collapse except for an
#     eager cursor)
# * If no collapse is requested - just get the next row, construct and
#   return
sub _construct_results {
  my ($self, $fetch_all) = @_;

  my $rsrc = $self->result_source;
  my $attrs = $self->_resolved_attrs;

  if (
    ! $fetch_all
      and
    ! $attrs->{order_by}
      and
    $attrs->{collapse}
      and
    my @pcols = $rsrc->primary_columns
  ) {
    # default order for collapsing unless the user asked for something
    $attrs->{order_by} = [ map { join '.', $attrs->{alias}, $_} @pcols ];
    $attrs->{_ordered_for_collapse} = 1;
    $attrs->{_order_is_artificial} = 1;
  }

  # this will be used as both initial raw-row collector AND as a RV of
  # _construct_results. Not regrowing the array twice matters a lot...
  # a surprising amount actually
  my $rows = delete $self->{_stashed_rows};

  my $cursor; # we may not need one at all

  my $did_fetch_all = $fetch_all;

  if ($fetch_all) {
    # FIXME SUBOPTIMAL - we can do better, cursor->next/all (well diff. methods) should return a ref
    $rows = [ ($rows ? @$rows : ()), $self->cursor->all ];
  }
  elsif( $attrs->{collapse} ) {

    # a cursor will need to be closed over in case of collapse
    $cursor = $self->cursor;

    $attrs->{_ordered_for_collapse} = (
      (
        $attrs->{order_by}
          and
        $rsrc->schema
              ->storage
               ->_extract_colinfo_of_stable_main_source_order_by_portion($attrs)
      ) ? 1 : 0
    ) unless defined $attrs->{_ordered_for_collapse};

    if (! $attrs->{_ordered_for_collapse}) {
      $did_fetch_all = 1;

      # instead of looping over ->next, use ->all in stealth mode
      # *without* calling a ->reset afterwards
      # FIXME ENCAPSULATION - encapsulation breach, cursor method additions pending
      if (! $cursor->{_done}) {
        $rows = [ ($rows ? @$rows : ()), $cursor->all ];
        $cursor->{_done} = 1;
      }
    }
  }

  if (! $did_fetch_all and ! @{$rows||[]} ) {
    # FIXME SUBOPTIMAL - we can do better, cursor->next/all (well diff. methods) should return a ref
    $cursor ||= $self->cursor;
    if (scalar (my @r = $cursor->next) ) {
      $rows = [ \@r ];
    }
  }

  return undef unless @{$rows||[]};

  # sanity check - people are too clever for their own good
  if ($attrs->{collapse} and my $aliastypes = $attrs->{_last_sqlmaker_alias_map} ) {

    my $multiplied_selectors;
    for my $sel_alias ( grep { $_ ne $attrs->{alias} } keys %{ $aliastypes->{selecting} } ) {
      if (
        $aliastypes->{multiplying}{$sel_alias}
          or
        $aliastypes->{premultiplied}{$sel_alias}
      ) {
        $multiplied_selectors->{$_} = 1 for values %{$aliastypes->{selecting}{$sel_alias}{-seen_columns}}
      }
    }

    for my $i (0 .. $#{$attrs->{as}} ) {
      my $sel = $attrs->{select}[$i];

      if (ref $sel eq 'SCALAR') {
        $sel = $$sel;
      }
      elsif( ref $sel eq 'REF' and ref $$sel eq 'ARRAY' ) {
        $sel = $$sel->[0];
      }

      $self->throw_exception(
        'Result collapse not possible - selection from a has_many source redirected to the main object'
      ) if ($multiplied_selectors->{$sel} and $attrs->{as}[$i] !~ /\./);
    }
  }

  # hotspot - skip the setter
  my $res_class = $self->_result_class;

  my $inflator_cref = $self->{_result_inflator}{cref} ||= do {
    $res_class->can ('inflate_result')
      or $self->throw_exception("Inflator $res_class does not provide an inflate_result() method");
  };

  my $infmap = $attrs->{as};

  $self->{_result_inflator}{is_core_row} = ( (
    $inflator_cref
      ==
    ( \&DBIx::Class::Row::inflate_result || die "No ::Row::inflate_result() - can't happen" )
  ) ? 1 : 0 ) unless defined $self->{_result_inflator}{is_core_row};

  $self->{_result_inflator}{is_hri} = ( (
    ! $self->{_result_inflator}{is_core_row}
      and
    $inflator_cref == (
      require DBIx::Class::ResultClass::HashRefInflator
        &&
      DBIx::Class::ResultClass::HashRefInflator->can('inflate_result')
    )
  ) ? 1 : 0 ) unless defined $self->{_result_inflator}{is_hri};


  if ($attrs->{_simple_passthrough_construction}) {
    # construct a much simpler array->hash folder for the one-table HRI cases right here
    if ($self->{_result_inflator}{is_hri}) {
      for my $r (@$rows) {
        $r = { map { $infmap->[$_] => $r->[$_] } 0..$#$infmap };
      }
    }
    # FIXME SUBOPTIMAL this is a very very very hot spot
    # while rather optimal we can *still* do much better, by
    # building a smarter Row::inflate_result(), and
    # switch to feeding it data via a much leaner interface
    #
    # crude unscientific benchmarking indicated the shortcut eval is not worth it for
    # this particular resultset size
    elsif ( $self->{_result_inflator}{is_core_row} and @$rows < 60 ) {
      for my $r (@$rows) {
        $r = $inflator_cref->($res_class, $rsrc, { map { $infmap->[$_] => $r->[$_] } (0..$#$infmap) } );
      }
    }
    else {
      eval sprintf (
        ( $self->{_result_inflator}{is_core_row}
          ? '$_ = $inflator_cref->($res_class, $rsrc, { %s }) for @$rows'
          # a custom inflator may be a multiplier/reductor - put it in direct list ctx
          : '@$rows = map { $inflator_cref->($res_class, $rsrc, { %s } ) } @$rows'
        ),
        ( join (', ', map { "\$infmap->[$_] => \$_->[$_]" } 0..$#$infmap ) )
      ) . '; 1' or die;
    }
  }
  else {
    my $parser_type =
        $self->{_result_inflator}{is_hri}       ? 'hri'
      : $self->{_result_inflator}{is_core_row}  ? 'classic_pruning'
      :                                           'classic_nonpruning'
    ;

    # $args and $attrs to _mk_row_parser are separated to delineate what is
    # core collapser stuff and what is dbic $rs specific
    @{$self->{_row_parser}{$parser_type}}{qw(cref nullcheck)} = $rsrc->_mk_row_parser({
      eval => 1,
      inflate_map => $infmap,
      collapse => $attrs->{collapse},
      premultiplied => $attrs->{_main_source_premultiplied},
      hri_style => $self->{_result_inflator}{is_hri},
      prune_null_branches => $self->{_result_inflator}{is_hri} || $self->{_result_inflator}{is_core_row},
    }, $attrs) unless $self->{_row_parser}{$parser_type}{cref};

    # column_info metadata historically hasn't been too reliable.
    # We need to start fixing this somehow (the collapse resolver
    # can't work without it). Add an explicit check for the *main*
    # result, hopefully this will gradually weed out such errors
    #
    # FIXME - this is a temporary kludge that reduces performance
    # It is however necessary for the time being
    my ($unrolled_non_null_cols_to_check, $err);

    if (my $check_non_null_cols = $self->{_row_parser}{$parser_type}{nullcheck} ) {

      $err =
        'Collapse aborted due to invalid ResultSource metadata - the following '
      . 'selections are declared non-nullable but NULLs were retrieved: '
      ;

      my @violating_idx;
      COL: for my $i (@$check_non_null_cols) {
        ! defined $_->[$i] and push @violating_idx, $i and next COL for @$rows;
      }

      $self->throw_exception( $err . join (', ', map { "'$infmap->[$_]'" } @violating_idx ) )
        if @violating_idx;

      $unrolled_non_null_cols_to_check = join (',', @$check_non_null_cols);

      utf8::upgrade($unrolled_non_null_cols_to_check)
        if DBIx::Class::_ENV_::STRESSTEST_UTF8_UPGRADE_GENERATED_COLLAPSER_SOURCE;
    }

    my $next_cref =
      ($did_fetch_all or ! $attrs->{collapse})  ? undef
    : defined $unrolled_non_null_cols_to_check  ? eval sprintf <<'EOS', $unrolled_non_null_cols_to_check
sub {
  # FIXME SUBOPTIMAL - we can do better, cursor->next/all (well diff. methods) should return a ref
  my @r = $cursor->next or return;
  if (my @violating_idx = grep { ! defined $r[$_] } (%s) ) {
    $self->throw_exception( $err . join (', ', map { "'$infmap->[$_]'" } @violating_idx ) )
  }
  \@r
}
EOS
    : sub {
        # FIXME SUBOPTIMAL - we can do better, cursor->next/all (well diff. methods) should return a ref
        my @r = $cursor->next or return;
        \@r
      }
    ;

    $self->{_row_parser}{$parser_type}{cref}->(
      $rows,
      $next_cref ? ( $next_cref, $self->{_stashed_rows} = [] ) : (),
    );

    # simple in-place substitution, does not regrow $rows
    if ($self->{_result_inflator}{is_core_row}) {
      $_ = $inflator_cref->($res_class, $rsrc, @$_) for @$rows
    }
    # Special-case multi-object HRI - there is no $inflator_cref pass at all
    elsif ( ! $self->{_result_inflator}{is_hri} ) {
      # the inflator may be a multiplier/reductor - put it in list ctx
      @$rows = map { $inflator_cref->($res_class, $rsrc, @$_) } @$rows;
    }
  }

  # The @$rows check seems odd at first - why wouldn't we want to warn
  # regardless? The issue is things like find() etc, where the user
  # *knows* only one result will come back. In these cases the ->all
  # is not a pessimization, but rather something we actually want
  carp_unique(
    'Unable to properly collapse has_many results in iterator mode due '
  . 'to order criteria - performed an eager cursor slurp underneath. '
  . 'Consider using ->all() instead'
  ) if ( ! $fetch_all and @$rows > 1 );

  return $rows;
}

=head2 result_source

=over 4

=item Arguments: L<$result_source?|DBIx::Class::ResultSource>

=item Return Value: L<$result_source|DBIx::Class::ResultSource>

=back

An accessor for the primary ResultSource object from which this ResultSet
is derived.

=head2 result_class

=over 4

=item Arguments: $result_class?

=item Return Value: $result_class

=back

An accessor for the class to use when creating result objects. Defaults to
C<< result_source->result_class >> - which in most cases is the name of the
L<"table"|DBIx::Class::Manual::Glossary/"ResultSource"> class.

Note that changing the result_class will also remove any components
that were originally loaded in the source class via
L<load_components|Class::C3::Componentised/load_components( @comps )>.
Any overloaded methods in the original source class will not run.

=cut

sub result_class {
  my ($self, $result_class) = @_;
  if ($result_class) {

    # don't fire this for an object
    $self->ensure_class_loaded($result_class)
      unless ref($result_class);

    if ($self->get_cache) {
      carp_unique('Changing the result_class of a ResultSet instance with cached results is a noop - the cache contents will not be altered');
    }
    # FIXME ENCAPSULATION - encapsulation breach, cursor method additions pending
    elsif ($self->{cursor} && $self->{cursor}{_pos}) {
      $self->throw_exception('Changing the result_class of a ResultSet instance with an active cursor is not supported');
    }

    $self->_result_class($result_class);

    delete $self->{_result_inflator};
  }
  $self->_result_class;
}

=head2 count

=over 4

=item Arguments: L<$cond|DBIx::Class::SQLMaker>, L<\%attrs?|/ATTRIBUTES>

=item Return Value: $count

=back

Performs an SQL C<COUNT> with the same query as the resultset was built
with to find the number of elements. Passing arguments is equivalent to
C<< $rs->search ($cond, \%attrs)->count >>

=cut

sub count {
  my $self = shift;
  return $self->search(@_)->count if @_ and defined $_[0];
  return scalar @{ $self->get_cache } if $self->get_cache;

  my $attrs = { %{ $self->_resolved_attrs } };

  # this is a little optimization - it is faster to do the limit
  # adjustments in software, instead of a subquery
  my ($rows, $offset) = delete @{$attrs}{qw/rows offset/};

  my $crs;
  if ($self->_has_resolved_attr (qw/collapse group_by/)) {
    $crs = $self->_count_subq_rs ($attrs);
  }
  else {
    $crs = $self->_count_rs ($attrs);
  }
  my $count = $crs->next;

  $count -= $offset if $offset;
  $count = $rows if $rows and $rows < $count;
  $count = 0 if ($count < 0);

  return $count;
}

=head2 count_rs

=over 4

=item Arguments: L<$cond|DBIx::Class::SQLMaker>, L<\%attrs?|/ATTRIBUTES>

=item Return Value: L<$count_rs|DBIx::Class::ResultSetColumn>

=back

Same as L</count> but returns a L<DBIx::Class::ResultSetColumn> object.
This can be very handy for subqueries:

  ->search( { amount => $some_rs->count_rs->as_query } )

As with regular resultsets the SQL query will be executed only after
the resultset is accessed via L</next> or L</all>. That would return
the same single value obtainable via L</count>.

=cut

sub count_rs {
  my $self = shift;
  return $self->search(@_)->count_rs if @_;

  # this may look like a lack of abstraction (count() does about the same)
  # but in fact an _rs *must* use a subquery for the limits, as the
  # software based limiting can not be ported if this $rs is to be used
  # in a subquery itself (i.e. ->as_query)
  if ($self->_has_resolved_attr (qw/collapse group_by offset rows/)) {
    return $self->_count_subq_rs($self->{_attrs});
  }
  else {
    return $self->_count_rs($self->{_attrs});
  }
}

#
# returns a ResultSetColumn object tied to the count query
#
sub _count_rs {
  my ($self, $attrs) = @_;

  my $rsrc = $self->result_source;

  my $tmp_attrs = { %$attrs };
  # take off any limits, record_filter is cdbi, and no point of ordering nor locking a count
  delete @{$tmp_attrs}{qw/rows offset order_by record_filter for/};

  # overwrite the selector (supplied by the storage)
  $rsrc->resultset_class->new($rsrc, {
    %$tmp_attrs,
    select => $rsrc->storage->_count_select ($rsrc, $attrs),
    as => 'count',
  })->get_column ('count');
}

#
# same as above but uses a subquery
#
sub _count_subq_rs {
  my ($self, $attrs) = @_;

  my $rsrc = $self->result_source;

  my $sub_attrs = { %$attrs };
  # extra selectors do not go in the subquery and there is no point of ordering it, nor locking it
  delete @{$sub_attrs}{qw/collapse columns as select order_by for/};

  # if we multi-prefetch we group_by something unique, as this is what we would
  # get out of the rs via ->next/->all. We *DO WANT* to clobber old group_by regardless
  if ( $attrs->{collapse}  ) {
    $sub_attrs->{group_by} = [ map { "$attrs->{alias}.$_" } @{
      $rsrc->_identifying_column_set || $self->throw_exception(
        'Unable to construct a unique group_by criteria properly collapsing the '
      . 'has_many prefetch before count()'
      );
    } ]
  }

  # Calculate subquery selector
  if (my $g = $sub_attrs->{group_by}) {

    my $sql_maker = $rsrc->storage->sql_maker;

    # necessary as the group_by may refer to aliased functions
    my $sel_index;
    for my $sel (@{$attrs->{select}}) {
      $sel_index->{$sel->{-as}} = $sel
        if (ref $sel eq 'HASH' and $sel->{-as});
    }

    # anything from the original select mentioned on the group-by needs to make it to the inner selector
    # also look for named aggregates referred in the having clause
    # having often contains scalarrefs - thus parse it out entirely
    my @parts = @$g;
    if ($attrs->{having}) {
      local $sql_maker->{having_bind};
      local $sql_maker->{quote_char} = $sql_maker->{quote_char};
      local $sql_maker->{name_sep} = $sql_maker->{name_sep};
      unless (defined $sql_maker->{quote_char} and length $sql_maker->{quote_char}) {
        $sql_maker->{quote_char} = [ "\x00", "\xFF" ];
        # if we don't unset it we screw up retarded but unfortunately working
        # 'MAX(foo.bar)' => { '>', 3 }
        $sql_maker->{name_sep} = '';
      }

      my ($lquote, $rquote, $sep) = map { quotemeta $_ } ($sql_maker->_quote_chars, $sql_maker->name_sep);

      my $having_sql = $sql_maker->_parse_rs_attrs ({ having => $attrs->{having} });
      my %seen_having;

      # search for both a proper quoted qualified string, for a naive unquoted scalarref
      # and if all fails for an utterly naive quoted scalar-with-function
      while ($having_sql =~ /
        $rquote $sep $lquote (.+?) $rquote
          |
        [\s,] \w+ \. (\w+) [\s,]
          |
        [\s,] $lquote (.+?) $rquote [\s,]
      /gx) {
        my $part = $1 || $2 || $3;  # one of them matched if we got here
        unless ($seen_having{$part}++) {
          push @parts, $part;
        }
      }
    }

    for (@parts) {
      my $colpiece = $sel_index->{$_} || $_;

      # unqualify join-based group_by's. Arcane but possible query
      # also horrible horrible hack to alias a column (not a func.)
      # (probably need to introduce SQLA syntax)
      if ($colpiece =~ /\./ && $colpiece !~ /^$attrs->{alias}\./) {
        my $as = $colpiece;
        $as =~ s/\./__/;
        $colpiece = \ sprintf ('%s AS %s', map { $sql_maker->_quote ($_) } ($colpiece, $as) );
      }
      push @{$sub_attrs->{select}}, $colpiece;
    }
  }
  else {
    my @pcols = map { "$attrs->{alias}.$_" } ($rsrc->primary_columns);
    $sub_attrs->{select} = @pcols ? \@pcols : [ 1 ];
  }

  return $rsrc->resultset_class
               ->new ($rsrc, $sub_attrs)
                ->as_subselect_rs
                 ->search ({}, { columns => { count => $rsrc->storage->_count_select ($rsrc, $attrs) } })
                  ->get_column ('count');
}


=head2 count_literal

B<CAVEAT>: C<count_literal> is provided for Class::DBI compatibility and
should only be used in that context. See L</search_literal> for further info.

=over 4

=item Arguments: $sql_fragment, @standalone_bind_values

=item Return Value: $count

=back

Counts the results in a literal query. Equivalent to calling L</search_literal>
with the passed arguments, then L</count>.

=cut

sub count_literal { shift->search_literal(@_)->count; }

=head2 all

=over 4

=item Arguments: none

=item Return Value: L<@result_objs|DBIx::Class::Manual::ResultClass>

=back

Returns all elements in the resultset.

=cut

sub all {
  my $self = shift;
  if(@_) {
    $self->throw_exception("all() doesn't take any arguments, you probably wanted ->search(...)->all()");
  }

  delete @{$self}{qw/_stashed_rows _stashed_results/};

  if (my $c = $self->get_cache) {
    return @$c;
  }

  $self->cursor->reset;

  my $objs = $self->_construct_results('fetch_all') || [];

  $self->set_cache($objs) if $self->{attrs}{cache};

  return @$objs;
}

=head2 reset

=over 4

=item Arguments: none

=item Return Value: $self

=back

Resets the resultset's cursor, so you can iterate through the elements again.
Implicitly resets the storage cursor, so a subsequent L</next> will trigger
another query.

=cut

sub reset {
  my ($self) = @_;

  delete @{$self}{qw/_stashed_rows _stashed_results/};
  $self->{all_cache_position} = 0;
  $self->cursor->reset;
  return $self;
}

=head2 first

=over 4

=item Arguments: none

=item Return Value: L<$result|DBIx::Class::Manual::ResultClass> | undef

=back

L<Resets|/reset> the resultset (causing a fresh query to storage) and returns
an object for the first result (or C<undef> if the resultset is empty).

=cut

sub first {
  return $_[0]->reset->next;
}


# _rs_update_delete
#
# Determines whether and what type of subquery is required for the $rs operation.
# If grouping is necessary either supplies its own, or verifies the current one
# After all is done delegates to the proper storage method.

sub _rs_update_delete {
  my ($self, $op, $values) = @_;

  my $rsrc = $self->result_source;
  my $storage = $rsrc->schema->storage;

  my $attrs = { %{$self->_resolved_attrs} };

  my $join_classifications;
  my ($existing_group_by) = delete @{$attrs}{qw(group_by _grouped_by_distinct)};

  # do we need a subquery for any reason?
  my $needs_subq = (
    defined $existing_group_by
      or
    # if {from} is unparseable wrap a subq
    ref($attrs->{from}) ne 'ARRAY'
      or
    # limits call for a subq
    $self->_has_resolved_attr(qw/rows offset/)
  );

  # simplify the joinmap, so we can further decide if a subq is necessary
  if (!$needs_subq and @{$attrs->{from}} > 1) {

    ($attrs->{from}, $join_classifications) =
      $storage->_prune_unused_joins ($attrs);

    # any non-pruneable non-local restricting joins imply subq
    $needs_subq = defined List::Util::first { $_ ne $attrs->{alias} } keys %{ $join_classifications->{restricting} || {} };
  }

  # check if the head is composite (by now all joins are thrown out unless $needs_subq)
  $needs_subq ||= (
    (ref $attrs->{from}[0]) ne 'HASH'
      or
    ref $attrs->{from}[0]{ $attrs->{from}[0]{-alias} }
  );

  my ($cond, $guard);
  # do we need anything like a subquery?
  if (! $needs_subq) {
    # Most databases do not allow aliasing of tables in UPDATE/DELETE. Thus
    # a condition containing 'me' or other table prefixes will not work
    # at all. Tell SQLMaker to dequalify idents via a gross hack.
    $cond = do {
      my $sqla = $rsrc->storage->sql_maker;
      local $sqla->{_dequalify_idents} = 1;
      \[ $sqla->_recurse_where($self->{cond}) ];
    };
  }
  else {
    # we got this far - means it is time to wrap a subquery
    my $idcols = $rsrc->_identifying_column_set || $self->throw_exception(
      sprintf(
        "Unable to perform complex resultset %s() without an identifying set of columns on source '%s'",
        $op,
        $rsrc->source_name,
      )
    );

    # make a new $rs selecting only the PKs (that's all we really need for the subq)
    delete $attrs->{$_} for qw/select as collapse/;
    $attrs->{columns} = [ map { "$attrs->{alias}.$_" } @$idcols ];

    # this will be consumed by the pruner waaaaay down the stack
    $attrs->{_force_prune_multiplying_joins} = 1;

    my $subrs = (ref $self)->new($rsrc, $attrs);

    if (@$idcols == 1) {
      $cond = { $idcols->[0] => { -in => $subrs->as_query } };
    }
    elsif ($storage->_use_multicolumn_in) {
      # no syntax for calling this properly yet
      # !!! EXPERIMENTAL API !!! WILL CHANGE !!!
      $cond = $storage->sql_maker->_where_op_multicolumn_in (
        $idcols, # how do I convey a list of idents...? can binds reside on lhs?
        $subrs->as_query
      ),
    }
    else {
      # if all else fails - get all primary keys and operate over a ORed set
      # wrap in a transaction for consistency
      # this is where the group_by/multiplication starts to matter
      if (
        $existing_group_by
          or
        # we do not need to check pre-multipliers, since if the premulti is there, its
        # parent (who is multi) will be there too
        keys %{ $join_classifications->{multiplying} || {} }
      ) {
        # make sure if there is a supplied group_by it matches the columns compiled above
        # perfectly. Anything else can not be sanely executed on most databases so croak
        # right then and there
        if ($existing_group_by) {
          my @current_group_by = map
            { $_ =~ /\./ ? $_ : "$attrs->{alias}.$_" }
            @$existing_group_by
          ;

          if (
            join ("\x00", sort @current_group_by)
              ne
            join ("\x00", sort @{$attrs->{columns}} )
          ) {
            $self->throw_exception (
              "You have just attempted a $op operation on a resultset which does group_by"
              . ' on columns other than the primary keys, while DBIC internally needs to retrieve'
              . ' the primary keys in a subselect. All sane RDBMS engines do not support this'
              . ' kind of queries. Please retry the operation with a modified group_by or'
              . ' without using one at all.'
            );
          }
        }

        $subrs = $subrs->search({}, { group_by => $attrs->{columns} });
      }

      $guard = $storage->txn_scope_guard;

      for my $row ($subrs->cursor->all) {
        push @$cond, { map
          { $idcols->[$_] => $row->[$_] }
          (0 .. $#$idcols)
        };
      }
    }
  }

  my $res = $cond ? $storage->$op (
    $rsrc,
    $op eq 'update' ? $values : (),
    $cond,
  ) : '0E0';

  $guard->commit if $guard;

  return $res;
}

=head2 update

=over 4

=item Arguments: \%values

=item Return Value: $underlying_storage_rv

=back

Sets the specified columns in the resultset to the supplied values in a
single query. Note that this will not run any accessor/set_column/update
triggers, nor will it update any result object instances derived from this
resultset (this includes the contents of the L<resultset cache|/set_cache>
if any). See L</update_all> if you need to execute any on-update
triggers or cascades defined either by you or a
L<result component|DBIx::Class::Manual::Component/WHAT IS A COMPONENT>.

The return value is a pass through of what the underlying
storage backend returned, and may vary. See L<DBI/execute> for the most
common case.

=head3 CAVEAT

Note that L</update> does not process/deflate any of the values passed in.
This is unlike the corresponding L<DBIx::Class::Row/update>. The user must
ensure manually that any value passed to this method will stringify to
something the RDBMS knows how to deal with. A notable example is the
handling of L<DateTime> objects, for more info see:
L<DBIx::Class::Manual::Cookbook/Formatting DateTime objects in queries>.

=cut

sub update {
  my ($self, $values) = @_;
  $self->throw_exception('Values for update must be a hash')
    unless ref $values eq 'HASH';

  return $self->_rs_update_delete ('update', $values);
}

=head2 update_all

=over 4

=item Arguments: \%values

=item Return Value: 1

=back

Fetches all objects and updates them one at a time via
L<DBIx::Class::Row/update>. Note that C<update_all> will run DBIC defined
triggers, while L</update> will not.

=cut

sub update_all {
  my ($self, $values) = @_;
  $self->throw_exception('Values for update_all must be a hash')
    unless ref $values eq 'HASH';

  my $guard = $self->result_source->schema->txn_scope_guard;
  $_->update({%$values}) for $self->all;  # shallow copy - update will mangle it
  $guard->commit;
  return 1;
}

=head2 delete

=over 4

=item Arguments: none

=item Return Value: $underlying_storage_rv

=back

Deletes the rows matching this resultset in a single query. Note that this
will not run any delete triggers, nor will it alter the
L<in_storage|DBIx::Class::Row/in_storage> status of any result object instances
derived from this resultset (this includes the contents of the
L<resultset cache|/set_cache> if any). See L</delete_all> if you need to
execute any on-delete triggers or cascades defined either by you or a
L<result component|DBIx::Class::Manual::Component/WHAT IS A COMPONENT>.

The return value is a pass through of what the underlying storage backend
returned, and may vary. See L<DBI/execute> for the most common case.

=cut

sub delete {
  my $self = shift;
  $self->throw_exception('delete does not accept any arguments')
    if @_;

  return $self->_rs_update_delete ('delete');
}

=head2 delete_all

=over 4

=item Arguments: none

=item Return Value: 1

=back

Fetches all objects and deletes them one at a time via
L<DBIx::Class::Row/delete>. Note that C<delete_all> will run DBIC defined
triggers, while L</delete> will not.

=cut

sub delete_all {
  my $self = shift;
  $self->throw_exception('delete_all does not accept any arguments')
    if @_;

  my $guard = $self->result_source->schema->txn_scope_guard;
  $_->delete for $self->all;
  $guard->commit;
  return 1;
}

=head2 populate

=over 4

=item Arguments: [ \@column_list, \@row_values+ ] | [ \%col_data+ ]

=item Return Value: L<\@result_objects|DBIx::Class::Manual::ResultClass> (scalar context) | L<@result_objects|DBIx::Class::Manual::ResultClass> (list context)

=back

Accepts either an arrayref of hashrefs or alternatively an arrayref of
arrayrefs.

=over

=item NOTE

The context of this method call has an important effect on what is
submitted to storage. In void context data is fed directly to fastpath
insertion routines provided by the underlying storage (most often
L<DBI/execute_for_fetch>), bypassing the L<new|DBIx::Class::Row/new> and
L<insert|DBIx::Class::Row/insert> calls on the
L<Result|DBIx::Class::Manual::ResultClass> class, including any
augmentation of these methods provided by components. For example if you
are using something like L<DBIx::Class::UUIDColumns> to create primary
keys for you, you will find that your PKs are empty.  In this case you
will have to explicitly force scalar or list context in order to create
those values.

=back

In non-void (scalar or list) context, this method is simply a wrapper
for L</create>. Depending on list or scalar context either a list of
L<Result|DBIx::Class::Manual::ResultClass> objects or an arrayref
containing these objects is returned.

When supplying data in "arrayref of arrayrefs" invocation style, the
first element should be a list of column names and each subsequent
element should be a data value in the earlier specified column order.
For example:

  $schema->resultset("Artist")->populate([
    [ qw( artistid name ) ],
    [ 100, 'A Formally Unknown Singer' ],
    [ 101, 'A singer that jumped the shark two albums ago' ],
    [ 102, 'An actually cool singer' ],
  ]);

For the arrayref of hashrefs style each hashref should be a structure
suitable for passing to L</create>. Multi-create is also permitted with
this syntax.

  $schema->resultset("Artist")->populate([
     { artistid => 4, name => 'Manufactured Crap', cds => [
        { title => 'My First CD', year => 2006 },
        { title => 'Yet More Tweeny-Pop crap', year => 2007 },
      ],
     },
     { artistid => 5, name => 'Angsty-Whiny Girl', cds => [
        { title => 'My parents sold me to a record company', year => 2005 },
        { title => 'Why Am I So Ugly?', year => 2006 },
        { title => 'I Got Surgery and am now Popular', year => 2007 }
      ],
     },
  ]);

If you attempt a void-context multi-create as in the example above (each
Artist also has the related list of CDs), and B<do not> supply the
necessary autoinc foreign key information, this method will proxy to the
less efficient L</create>, and then throw the Result objects away. In this
case there are obviously no benefits to using this method over L</create>.

=cut

sub populate {
  my $self = shift;

  # this is naive and just a quick check
  # the types will need to be checked more thoroughly when the
  # multi-source populate gets added
  my $data = (
    ref $_[0] eq 'ARRAY'
      and
    ( @{$_[0]} or return )
      and
    ( ref $_[0][0] eq 'HASH' or ref $_[0][0] eq 'ARRAY' )
      and
    $_[0]
  ) or $self->throw_exception('Populate expects an arrayref of hashrefs or arrayref of arrayrefs');

  # FIXME - no cref handling
  # At this point assume either hashes or arrays

  if(defined wantarray) {
    my (@results, $guard);

    if (ref $data->[0] eq 'ARRAY') {
      # column names only, nothing to do
      return if @$data == 1;

      $guard = $self->result_source->schema->storage->txn_scope_guard
        if @$data > 2;

      @results = map
        { my $vals = $_; $self->new_result({ map { $data->[0][$_] => $vals->[$_] } 0..$#{$data->[0]} })->insert }
        @{$data}[1 .. $#$data]
      ;
    }
    else {

      $guard = $self->result_source->schema->storage->txn_scope_guard
        if @$data > 1;

      @results = map { $self->new_result($_)->insert } @$data;
    }

    $guard->commit if $guard;
    return wantarray ? @results : \@results;
  }

  # we have to deal with *possibly incomplete* related data
  # this means we have to walk the data structure twice
  # whether we want this or not
  # jnap, I hate you ;)
  my $rsrc = $self->result_source;
  my $rel_info = { map { $_ => $rsrc->relationship_info($_) } $rsrc->relationships };

  my ($colinfo, $colnames, $slices_with_rels);
  my $data_start = 0;

  DATA_SLICE:
  for my $i (0 .. $#$data) {

    my $current_slice_seen_rel_infos;

### Determine/Supplement collists
### BEWARE - This is a hot piece of code, a lot of weird idioms were used
    if( ref $data->[$i] eq 'ARRAY' ) {

      # positional(!) explicit column list
      if ($i == 0) {
        # column names only, nothing to do
        return if @$data == 1;

        $colinfo->{$data->[0][$_]} = { pos => $_, name => $data->[0][$_] } and push @$colnames, $data->[0][$_]
          for 0 .. $#{$data->[0]};

        $data_start = 1;

        next DATA_SLICE;
      }
      else {
        for (values %$colinfo) {
          if ($_->{is_rel} ||= (
            $rel_info->{$_->{name}}
              and
            (
              ref $data->[$i][$_->{pos}] eq 'ARRAY'
                or
              ref $data->[$i][$_->{pos}] eq 'HASH'
                or
              ( defined blessed $data->[$i][$_->{pos}] and $data->[$i][$_->{pos}]->isa('DBIx::Class::Row') )
            )
              and
            1
          )) {

            # moar sanity check... sigh
            for ( ref $data->[$i][$_->{pos}] eq 'ARRAY' ? @{$data->[$i][$_->{pos}]} : $data->[$i][$_->{pos}] ) {
              if ( defined blessed $_ and $_->isa('DBIx::Class::Row' ) ) {
                carp_unique("Fast-path populate() with supplied related objects is not possible - falling back to regular create()");
                return my $throwaway = $self->populate(@_);
              }
            }

            push @$current_slice_seen_rel_infos, $rel_info->{$_->{name}};
          }
        }
      }

     if ($current_slice_seen_rel_infos) {
        push @$slices_with_rels, { map { $colnames->[$_] => $data->[$i][$_] } 0 .. $#$colnames };

        # this is needed further down to decide whether or not to fallback to create()
        $colinfo->{$colnames->[$_]}{seen_null} ||= ! defined $data->[$i][$_]
          for 0 .. $#$colnames;
      }
    }
    elsif( ref $data->[$i] eq 'HASH' ) {

      for ( sort keys %{$data->[$i]} ) {

        $colinfo->{$_} ||= do {

          $self->throw_exception("Column '$_' must be present in supplied explicit column list")
            if $data_start; # it will be 0 on AoH, 1 on AoA

          push @$colnames, $_;

          # RV
          { pos => $#$colnames, name => $_ }
        };

        if ($colinfo->{$_}{is_rel} ||= (
          $rel_info->{$_}
            and
          (
            ref $data->[$i]{$_} eq 'ARRAY'
              or
            ref $data->[$i]{$_} eq 'HASH'
              or
            ( defined blessed $data->[$i]{$_} and $data->[$i]{$_}->isa('DBIx::Class::Row') )
          )
            and
          1
        )) {

          # moar sanity check... sigh
          for ( ref $data->[$i]{$_} eq 'ARRAY' ? @{$data->[$i]{$_}} : $data->[$i]{$_} ) {
            if ( defined blessed $_ and $_->isa('DBIx::Class::Row' ) ) {
              carp_unique("Fast-path populate() with supplied related objects is not possible - falling back to regular create()");
              return my $throwaway = $self->populate(@_);
            }
          }

          push @$current_slice_seen_rel_infos, $rel_info->{$_};
        }
      }

      if ($current_slice_seen_rel_infos) {
        push @$slices_with_rels, $data->[$i];

        # this is needed further down to decide whether or not to fallback to create()
        $colinfo->{$_}{seen_null} ||= ! defined $data->[$i]{$_}
          for keys %{$data->[$i]};
      }
    }
    else {
      $self->throw_exception('Unexpected populate() data structure member type: ' . ref $data->[$i] );
    }

    if ( grep
      { $_->{attrs}{is_depends_on} }
      @{ $current_slice_seen_rel_infos || [] }
    ) {
      carp_unique("Fast-path populate() of belongs_to relationship data is not possible - falling back to regular create()");
      return my $throwaway = $self->populate(@_);
    }
  }

  if( $slices_with_rels ) {

    # need to exclude the rel "columns"
    $colnames = [ grep { ! $colinfo->{$_}{is_rel} } @$colnames ];

    # extra sanity check - ensure the main source is in fact identifiable
    # the localizing of nullability is insane, but oh well... the use-case is legit
    my $ci = $rsrc->columns_info($colnames);

    $ci->{$_} = { %{$ci->{$_}}, is_nullable => 0 }
      for grep { ! $colinfo->{$_}{seen_null} } keys %$ci;

    unless( $rsrc->_identifying_column_set($ci) ) {
      carp_unique("Fast-path populate() of non-uniquely identifiable rows with related data is not possible - falling back to regular create()");
      return my $throwaway = $self->populate(@_);
    }
  }

### inherit the data locked in the conditions of the resultset
  my ($rs_data) = $self->_merge_with_rscond({});
  delete @{$rs_data}{@$colnames};  # passed-in stuff takes precedence

  # if anything left - decompose rs_data
  my $rs_data_vals;
  if (keys %$rs_data) {
     push @$rs_data_vals, $rs_data->{$_}
      for sort keys %$rs_data;
  }

### start work
  my $guard;
  $guard = $rsrc->schema->storage->txn_scope_guard
    if $slices_with_rels;

### main source data
  # FIXME - need to switch entirely to a coderef-based thing,
  # so that large sets aren't copied several times... I think
  $rsrc->storage->_insert_bulk(
    $rsrc,
    [ @$colnames, sort keys %$rs_data ],
    [ map {
      ref $data->[$_] eq 'ARRAY'
      ? (
          $slices_with_rels ? [ @{$data->[$_]}[0..$#$colnames], @{$rs_data_vals||[]} ]  # the collist changed
        : $rs_data_vals     ? [ @{$data->[$_]}, @$rs_data_vals ]
        :                     $data->[$_]
      )
      : [ @{$data->[$_]}{@$colnames}, @{$rs_data_vals||[]} ]
    } $data_start .. $#$data ],
  );

### do the children relationships
  if ( $slices_with_rels ) {
    my @rels = grep { $colinfo->{$_}{is_rel} } keys %$colinfo
      or die 'wtf... please report a bug with DBIC_TRACE=1 output (stacktrace)';

    for my $sl (@$slices_with_rels) {

      my ($main_proto, $main_proto_rs);
      for my $rel (@rels) {
        next unless defined $sl->{$rel};

        $main_proto ||= {
          %$rs_data,
          (map { $_ => $sl->{$_} } @$colnames),
        };

        unless (defined $colinfo->{$rel}{rs}) {

          $colinfo->{$rel}{rs} = $rsrc->related_source($rel)->resultset;

          $colinfo->{$rel}{fk_map} = { reverse %{ $rsrc->_resolve_relationship_condition(
            rel_name => $rel,
            self_alias => "\xFE", # irrelevant
            foreign_alias => "\xFF", # irrelevant
          )->{identity_map} || {} } };

        }

        $colinfo->{$rel}{rs}->search({ map # only so that we inherit them values properly, no actual search
          {
            $_ => { '=' =>
              ( $main_proto_rs ||= $rsrc->resultset->search($main_proto) )
                ->get_column( $colinfo->{$rel}{fk_map}{$_} )
                 ->as_query
            }
          }
          keys %{$colinfo->{$rel}{fk_map}}
        })->populate( ref $sl->{$rel} eq 'ARRAY' ? $sl->{$rel} : [ $sl->{$rel} ] );

        1;
      }
    }
  }

  $guard->commit if $guard;
}

=head2 pager

=over 4

=item Arguments: none

=item Return Value: L<$pager|Data::Page>

=back

Returns a L<Data::Page> object for the current resultset. Only makes
sense for queries with a C<page> attribute.

To get the full count of entries for a paged resultset, call
C<total_entries> on the L<Data::Page> object.

=cut

sub pager {
  my ($self) = @_;

  return $self->{pager} if $self->{pager};

  my $attrs = $self->{attrs};
  if (!defined $attrs->{page}) {
    $self->throw_exception("Can't create pager for non-paged rs");
  }
  elsif ($attrs->{page} <= 0) {
    $self->throw_exception('Invalid page number (page-numbers are 1-based)');
  }
  $attrs->{rows} ||= 10;

  # throw away the paging flags and re-run the count (possibly
  # with a subselect) to get the real total count
  my $count_attrs = { %$attrs };
  delete @{$count_attrs}{qw/rows offset page pager/};

  my $total_rs = (ref $self)->new($self->result_source, $count_attrs);

  require DBIx::Class::ResultSet::Pager;
  return $self->{pager} = DBIx::Class::ResultSet::Pager->new(
    sub { $total_rs->count },  #lazy-get the total
    $attrs->{rows},
    $self->{attrs}{page},
  );
}

=head2 page

=over 4

=item Arguments: $page_number

=item Return Value: L<$resultset|/search>

=back

Returns a resultset for the $page_number page of the resultset on which page
is called, where each page contains a number of rows equal to the 'rows'
attribute set on the resultset (10 by default).

=cut

sub page {
  my ($self, $page) = @_;
  return (ref $self)->new($self->result_source, { %{$self->{attrs}}, page => $page });
}

=head2 new_result

=over 4

=item Arguments: \%col_data

=item Return Value: L<$result|DBIx::Class::Manual::ResultClass>

=back

Creates a new result object in the resultset's result class and returns
it. The row is not inserted into the database at this point, call
L<DBIx::Class::Row/insert> to do that. Calling L<DBIx::Class::Row/in_storage>
will tell you whether the result object has been inserted or not.

Passes the hashref of input on to L<DBIx::Class::Row/new>.

=cut

sub new_result {
  my ($self, $values) = @_;

  $self->throw_exception( "new_result takes only one argument - a hashref of values" )
    if @_ > 2;

  $self->throw_exception( "Result object instantiation requires a hashref as argument" )
    unless (ref $values eq 'HASH');

  my ($merged_cond, $cols_from_relations) = $self->_merge_with_rscond($values);

  my $new = $self->result_class->new({
    %$merged_cond,
    ( @$cols_from_relations
      ? (-cols_from_relations => $cols_from_relations)
      : ()
    ),
    -result_source => $self->result_source, # DO NOT REMOVE THIS, REQUIRED
  });

  if (
    reftype($new) eq 'HASH'
      and
    ! keys %$new
      and
    blessed($new)
  ) {
    carp_unique (sprintf (
      "%s->new returned a blessed empty hashref - a strong indicator something is wrong with its inheritance chain",
      $self->result_class,
    ));
  }

  $new;
}

# _merge_with_rscond
#
# Takes a simple hash of K/V data and returns its copy merged with the
# condition already present on the resultset. Additionally returns an
# arrayref of value/condition names, which were inferred from related
# objects (this is needed for in-memory related objects)
sub _merge_with_rscond {
  my ($self, $data) = @_;

  my ($implied_data, @cols_from_relations);

  my $alias = $self->{attrs}{alias};

  if (! defined $self->{cond}) {
    # just massage $data below
  }
  elsif ($self->{cond} eq UNRESOLVABLE_CONDITION) {
    $implied_data = $self->{attrs}{related_objects};  # nothing might have been inserted yet
    @cols_from_relations = keys %{ $implied_data || {} };
  }
  else {
    my $eqs = $self->result_source->schema->storage->_extract_fixed_condition_columns($self->{cond}, 'consider_nulls');
    $implied_data = { map {
      ( ($eqs->{$_}||'') eq UNRESOLVABLE_CONDITION ) ? () : ( $_ => $eqs->{$_} )
    } keys %$eqs };
  }

  return (
    { map
      { %{ $self->_remove_alias($_, $alias) } }
      # precedence must be given to passed values over values inherited from
      # the cond, so the order here is important.
      ( $implied_data||(), $data)
    },
    \@cols_from_relations
  );
}

# _has_resolved_attr
#
# determines if the resultset defines at least one
# of the attributes supplied
#
# used to determine if a subquery is necessary
#
# supports some virtual attributes:
#   -join
#     This will scan for any joins being present on the resultset.
#     It is not a mere key-search but a deep inspection of {from}
#

sub _has_resolved_attr {
  my ($self, @attr_names) = @_;

  my $attrs = $self->_resolved_attrs;

  my %extra_checks;

  for my $n (@attr_names) {
    if (grep { $n eq $_ } (qw/-join/) ) {
      $extra_checks{$n}++;
      next;
    }

    my $attr =  $attrs->{$n};

    next if not defined $attr;

    if (ref $attr eq 'HASH') {
      return 1 if keys %$attr;
    }
    elsif (ref $attr eq 'ARRAY') {
      return 1 if @$attr;
    }
    else {
      return 1 if $attr;
    }
  }

  # a resolved join is expressed as a multi-level from
  return 1 if (
    $extra_checks{-join}
      and
    ref $attrs->{from} eq 'ARRAY'
      and
    @{$attrs->{from}} > 1
  );

  return 0;
}

# _remove_alias
#
# Remove the specified alias from the specified query hash. A copy is made so
# the original query is not modified.

sub _remove_alias {
  my ($self, $query, $alias) = @_;

  my %orig = %{ $query || {} };
  my %unaliased;

  foreach my $key (keys %orig) {
    if ($key !~ /\./) {
      $unaliased{$key} = $orig{$key};
      next;
    }
    $unaliased{$1} = $orig{$key}
      if $key =~ m/^(?:\Q$alias\E\.)?([^.]+)$/;
  }

  return \%unaliased;
}

=head2 as_query

=over 4

=item Arguments: none

=item Return Value: \[ $sql, L<@bind_values|/DBIC BIND VALUES> ]

=back

Returns the SQL query and bind vars associated with the invocant.

This is generally used as the RHS for a subquery.

=cut

sub as_query {
  my $self = shift;

  my $attrs = { %{ $self->_resolved_attrs } };

  my $aq = $self->result_source->storage->_select_args_to_query (
    $attrs->{from}, $attrs->{select}, $attrs->{where}, $attrs
  );

  $aq;
}

=head2 find_or_new

=over 4

=item Arguments: \%col_data, { key => $unique_constraint, L<%attrs|/ATTRIBUTES> }?

=item Return Value: L<$result|DBIx::Class::Manual::ResultClass>

=back

  my $artist = $schema->resultset('Artist')->find_or_new(
    { artist => 'fred' }, { key => 'artists' });

  $cd->cd_to_producer->find_or_new({ producer => $producer },
                                   { key => 'primary' });

Find an existing record from this resultset using L</find>. if none exists,
instantiate a new result object and return it. The object will not be saved
into your storage until you call L<DBIx::Class::Row/insert> on it.

You most likely want this method when looking for existing rows using a unique
constraint that is not the primary key, or looking for related rows.

If you want objects to be saved immediately, use L</find_or_create> instead.

B<Note>: Make sure to read the documentation of L</find> and understand the
significance of the C<key> attribute, as its lack may skew your search, and
subsequently result in spurious new objects.

B<Note>: Take care when using C<find_or_new> with a table having
columns with default values that you intend to be automatically
supplied by the database (e.g. an auto_increment primary key column).
In normal usage, the value of such columns should NOT be included at
all in the call to C<find_or_new>, even when set to C<undef>.

=cut

sub find_or_new {
  my $self     = shift;
  my $attrs    = (@_ > 1 && ref $_[$#_] eq 'HASH' ? pop(@_) : {});
  my $hash     = ref $_[0] eq 'HASH' ? shift : {@_};
  if (keys %$hash and my $row = $self->find($hash, $attrs) ) {
    return $row;
  }
  return $self->new_result($hash);
}

=head2 create

=over 4

=item Arguments: \%col_data

=item Return Value: L<$result|DBIx::Class::Manual::ResultClass>

=back

Attempt to create a single new row or a row with multiple related rows
in the table represented by the resultset (and related tables). This
will not check for duplicate rows before inserting, use
L</find_or_create> to do that.

To create one row for this resultset, pass a hashref of key/value
pairs representing the columns of the table and the values you wish to
store. If the appropriate relationships are set up, foreign key fields
can also be passed an object representing the foreign row, and the
value will be set to its primary key.

To create related objects, pass a hashref of related-object column values
B<keyed on the relationship name>. If the relationship is of type C<multi>
(L<DBIx::Class::Relationship/has_many>) - pass an arrayref of hashrefs.
The process will correctly identify columns holding foreign keys, and will
transparently populate them from the keys of the corresponding relation.
This can be applied recursively, and will work correctly for a structure
with an arbitrary depth and width, as long as the relationships actually
exists and the correct column data has been supplied.

Instead of hashrefs of plain related data (key/value pairs), you may
also pass new or inserted objects. New objects (not inserted yet, see
L</new_result>), will be inserted into their appropriate tables.

Effectively a shortcut for C<< ->new_result(\%col_data)->insert >>.

Example of creating a new row.

  $person_rs->create({
    name=>"Some Person",
    email=>"somebody@someplace.com"
  });

Example of creating a new row and also creating rows in a related C<has_many>
or C<has_one> resultset.  Note Arrayref.

  $artist_rs->create(
     { artistid => 4, name => 'Manufactured Crap', cds => [
        { title => 'My First CD', year => 2006 },
        { title => 'Yet More Tweeny-Pop crap', year => 2007 },
      ],
     },
  );

Example of creating a new row and also creating a row in a related
C<belongs_to> resultset. Note Hashref.

  $cd_rs->create({
    title=>"Music for Silly Walks",
    year=>2000,
    artist => {
      name=>"Silly Musician",
    }
  });

=over

=item WARNING

When subclassing ResultSet never attempt to override this method. Since
it is a simple shortcut for C<< $self->new_result($attrs)->insert >>, a
lot of the internals simply never call it, so your override will be
bypassed more often than not. Override either L<DBIx::Class::Row/new>
or L<DBIx::Class::Row/insert> depending on how early in the
L</create> process you need to intervene. See also warning pertaining to
L</new>.

=back

=cut

sub create {
  #my ($self, $col_data) = @_;
  DBIx::Class::_ENV_::ASSERT_NO_INTERNAL_INDIRECT_CALLS and fail_on_internal_call;
  return shift->new_result(shift)->insert;
}

=head2 find_or_create

=over 4

=item Arguments: \%col_data, { key => $unique_constraint, L<%attrs|/ATTRIBUTES> }?

=item Return Value: L<$result|DBIx::Class::Manual::ResultClass>

=back

  $cd->cd_to_producer->find_or_create({ producer => $producer },
                                      { key => 'primary' });

Tries to find a record based on its primary key or unique constraints; if none
is found, creates one and returns that instead.

  my $cd = $schema->resultset('CD')->find_or_create({
    cdid   => 5,
    artist => 'Massive Attack',
    title  => 'Mezzanine',
    year   => 2005,
  });

Also takes an optional C<key> attribute, to search by a specific key or unique
constraint. For example:

  my $cd = $schema->resultset('CD')->find_or_create(
    {
      artist => 'Massive Attack',
      title  => 'Mezzanine',
    },
    { key => 'cd_artist_title' }
  );

B<Note>: Make sure to read the documentation of L</find> and understand the
significance of the C<key> attribute, as its lack may skew your search, and
subsequently result in spurious row creation.

B<Note>: Because find_or_create() reads from the database and then
possibly inserts based on the result, this method is subject to a race
condition. Another process could create a record in the table after
the find has completed and before the create has started. To avoid
this problem, use find_or_create() inside a transaction.

B<Note>: Take care when using C<find_or_create> with a table having
columns with default values that you intend to be automatically
supplied by the database (e.g. an auto_increment primary key column).
In normal usage, the value of such columns should NOT be included at
all in the call to C<find_or_create>, even when set to C<undef>.

See also L</find> and L</update_or_create>. For information on how to declare
unique constraints, see L<DBIx::Class::ResultSource/add_unique_constraint>.

If you need to know if an existing row was found or a new one created use
L</find_or_new> and L<DBIx::Class::Row/in_storage> instead. Don't forget
to call L<DBIx::Class::Row/insert> to save the newly created row to the
database!

  my $cd = $schema->resultset('CD')->find_or_new({
    cdid   => 5,
    artist => 'Massive Attack',
    title  => 'Mezzanine',
    year   => 2005,
  });

  if( !$cd->in_storage ) {
      # do some stuff
      $cd->insert;
  }

=cut

sub find_or_create {
  my $self     = shift;
  my $attrs    = (@_ > 1 && ref $_[$#_] eq 'HASH' ? pop(@_) : {});
  my $hash     = ref $_[0] eq 'HASH' ? shift : {@_};
  if (keys %$hash and my $row = $self->find($hash, $attrs) ) {
    return $row;
  }
  return $self->new_result($hash)->insert;
}

=head2 update_or_create

=over 4

=item Arguments: \%col_data, { key => $unique_constraint, L<%attrs|/ATTRIBUTES> }?

=item Return Value: L<$result|DBIx::Class::Manual::ResultClass>

=back

  $resultset->update_or_create({ col => $val, ... });

Like L</find_or_create>, but if a row is found it is immediately updated via
C<< $found_row->update (\%col_data) >>.


Takes an optional C<key> attribute to search on a specific unique constraint.
For example:

  # In your application
  my $cd = $schema->resultset('CD')->update_or_create(
    {
      artist => 'Massive Attack',
      title  => 'Mezzanine',
      year   => 1998,
    },
    { key => 'cd_artist_title' }
  );

  $cd->cd_to_producer->update_or_create({
    producer => $producer,
    name => 'harry',
  }, {
    key => 'primary',
  });

B<Note>: Make sure to read the documentation of L</find> and understand the
significance of the C<key> attribute, as its lack may skew your search, and
subsequently result in spurious row creation.

B<Note>: Take care when using C<update_or_create> with a table having
columns with default values that you intend to be automatically
supplied by the database (e.g. an auto_increment primary key column).
In normal usage, the value of such columns should NOT be included at
all in the call to C<update_or_create>, even when set to C<undef>.

See also L</find> and L</find_or_create>. For information on how to declare
unique constraints, see L<DBIx::Class::ResultSource/add_unique_constraint>.

If you need to know if an existing row was updated or a new one created use
L</update_or_new> and L<DBIx::Class::Row/in_storage> instead. Don't forget
to call L<DBIx::Class::Row/insert> to save the newly created row to the
database!

=cut

sub update_or_create {
  my $self = shift;
  my $attrs = (@_ > 1 && ref $_[$#_] eq 'HASH' ? pop(@_) : {});
  my $cond = ref $_[0] eq 'HASH' ? shift : {@_};

  my $row = $self->find($cond, $attrs);
  if (defined $row) {
    $row->update($cond);
    return $row;
  }

  return $self->new_result($cond)->insert;
}

=head2 update_or_new

=over 4

=item Arguments: \%col_data, { key => $unique_constraint, L<%attrs|/ATTRIBUTES> }?

=item Return Value: L<$result|DBIx::Class::Manual::ResultClass>

=back

  $resultset->update_or_new({ col => $val, ... });

Like L</find_or_new> but if a row is found it is immediately updated via
C<< $found_row->update (\%col_data) >>.

For example:

  # In your application
  my $cd = $schema->resultset('CD')->update_or_new(
    {
      artist => 'Massive Attack',
      title  => 'Mezzanine',
      year   => 1998,
    },
    { key => 'cd_artist_title' }
  );

  if ($cd->in_storage) {
      # the cd was updated
  }
  else {
      # the cd is not yet in the database, let's insert it
      $cd->insert;
  }

B<Note>: Make sure to read the documentation of L</find> and understand the
significance of the C<key> attribute, as its lack may skew your search, and
subsequently result in spurious new objects.

B<Note>: Take care when using C<update_or_new> with a table having
columns with default values that you intend to be automatically
supplied by the database (e.g. an auto_increment primary key column).
In normal usage, the value of such columns should NOT be included at
all in the call to C<update_or_new>, even when set to C<undef>.

See also L</find>, L</find_or_create> and L</find_or_new>.

=cut

sub update_or_new {
    my $self  = shift;
    my $attrs = ( @_ > 1 && ref $_[$#_] eq 'HASH' ? pop(@_) : {} );
    my $cond  = ref $_[0] eq 'HASH' ? shift : {@_};

    my $row = $self->find( $cond, $attrs );
    if ( defined $row ) {
        $row->update($cond);
        return $row;
    }

    return $self->new_result($cond);
}

=head2 get_cache

=over 4

=item Arguments: none

=item Return Value: L<\@result_objs|DBIx::Class::Manual::ResultClass> | undef

=back

Gets the contents of the cache for the resultset, if the cache is set.

The cache is populated either by using the L</prefetch> attribute to
L</search> or by calling L</set_cache>.

=cut

sub get_cache {
  shift->{all_cache};
}

=head2 set_cache

=over 4

=item Arguments: L<\@result_objs|DBIx::Class::Manual::ResultClass>

=item Return Value: L<\@result_objs|DBIx::Class::Manual::ResultClass>

=back

Sets the contents of the cache for the resultset. Expects an arrayref
of objects of the same class as those produced by the resultset. Note that
if the cache is set, the resultset will return the cached objects rather
than re-querying the database even if the cache attr is not set.

The contents of the cache can also be populated by using the
L</prefetch> attribute to L</search>.

=cut

sub set_cache {
  my ( $self, $data ) = @_;
  $self->throw_exception("set_cache requires an arrayref")
      if defined($data) && (ref $data ne 'ARRAY');
  $self->{all_cache} = $data;
}

=head2 clear_cache

=over 4

=item Arguments: none

=item Return Value: undef

=back

Clears the cache for the resultset.

=cut

sub clear_cache {
  shift->set_cache(undef);
}

=head2 is_paged

=over 4

=item Arguments: none

=item Return Value: true, if the resultset has been paginated

=back

=cut

sub is_paged {
  my ($self) = @_;
  return !!$self->{attrs}{page};
}

=head2 is_ordered

=over 4

=item Arguments: none

=item Return Value: true, if the resultset has been ordered with C<order_by>.

=back

=cut

sub is_ordered {
  my ($self) = @_;
  return scalar $self->result_source->storage->_extract_order_criteria($self->{attrs}{order_by});
}

=head2 related_resultset

=over 4

=item Arguments: $rel_name

=item Return Value: L<$resultset|/search>

=back

Returns a related resultset for the supplied relationship name.

  $artist_rs = $schema->resultset('CD')->related_resultset('Artist');

=cut

sub related_resultset {
  my ($self, $rel) = @_;

  return $self->{related_resultsets}{$rel}
    if defined $self->{related_resultsets}{$rel};

  return $self->{related_resultsets}{$rel} = do {
    my $rsrc = $self->result_source;
    my $rel_info = $rsrc->relationship_info($rel);

    $self->throw_exception(
      "search_related: result source '" . $rsrc->source_name .
        "' has no such relationship $rel")
      unless $rel_info;

    my $attrs = $self->_chain_relationship($rel);

    my $join_count = $attrs->{seen_join}{$rel};

    my $alias = $self->result_source->storage
        ->relname_to_table_alias($rel, $join_count);

    # since this is search_related, and we already slid the select window inwards
    # (the select/as attrs were deleted in the beginning), we need to flip all
    # left joins to inner, so we get the expected results
    # read the comment on top of the actual function to see what this does
    $attrs->{from} = $rsrc->schema->storage->_inner_join_to_node ($attrs->{from}, $alias);


    #XXX - temp fix for result_class bug. There likely is a more elegant fix -groditi
    delete @{$attrs}{qw(result_class alias)};

    my $rel_source = $rsrc->related_source($rel);

    my $new = do {

      # The reason we do this now instead of passing the alias to the
      # search_rs below is that if you wrap/overload resultset on the
      # source you need to know what alias it's -going- to have for things
      # to work sanely (e.g. RestrictWithObject wants to be able to add
      # extra query restrictions, and these may need to be $alias.)

      my $rel_attrs = $rel_source->resultset_attributes;
      local $rel_attrs->{alias} = $alias;

      $rel_source->resultset
                 ->search_rs(
                     undef, {
                       %$attrs,
                       where => $attrs->{where},
                   });
    };

    if (my $cache = $self->get_cache) {
      my @related_cache = map
        { $_->related_resultset($rel)->get_cache || () }
        @$cache
      ;

      $new->set_cache([ map @$_, @related_cache ]) if @related_cache == @$cache;
    }

    $new;
  };
}

=head2 current_source_alias

=over 4

=item Arguments: none

=item Return Value: $source_alias

=back

Returns the current table alias for the result source this resultset is built
on, that will be used in the SQL query. Usually it is C<me>.

Currently the source alias that refers to the result set returned by a
L</search>/L</find> family method depends on how you got to the resultset: it's
C<me> by default, but eg. L</search_related> aliases it to the related result
source name (and keeps C<me> referring to the original result set). The long
term goal is to make L<DBIx::Class> always alias the current resultset as C<me>
(and make this method unnecessary).

Thus it's currently necessary to use this method in predefined queries (see
L<DBIx::Class::Manual::Cookbook/Predefined searches>) when referring to the
source alias of the current result set:

  # in a result set class
  sub modified_by {
    my ($self, $user) = @_;

    my $me = $self->current_source_alias;

    return $self->search({
      "$me.modified" => $user->id,
    });
  }

The alias of L<newly created resultsets|/search> can be altered by the
L<alias attribute|/alias>.

=cut

sub current_source_alias {
  return (shift->{attrs} || {})->{alias} || 'me';
}

=head2 as_subselect_rs

=over 4

=item Arguments: none

=item Return Value: L<$resultset|/search>

=back

Act as a barrier to SQL symbols.  The resultset provided will be made into a
"virtual view" by including it as a subquery within the from clause.  From this
point on, any joined tables are inaccessible to ->search on the resultset (as if
it were simply where-filtered without joins).  For example:

 my $rs = $schema->resultset('Bar')->search({'x.name' => 'abc'},{ join => 'x' });

 # 'x' now pollutes the query namespace

 # So the following works as expected
 my $ok_rs = $rs->search({'x.other' => 1});

 # But this doesn't: instead of finding a 'Bar' related to two x rows (abc and
 # def) we look for one row with contradictory terms and join in another table
 # (aliased 'x_2') which we never use
 my $broken_rs = $rs->search({'x.name' => 'def'});

 my $rs2 = $rs->as_subselect_rs;

 # doesn't work - 'x' is no longer accessible in $rs2, having been sealed away
 my $not_joined_rs = $rs2->search({'x.other' => 1});

 # works as expected: finds a 'table' row related to two x rows (abc and def)
 my $correctly_joined_rs = $rs2->search({'x.name' => 'def'});

Another example of when one might use this would be to select a subset of
columns in a group by clause:

 my $rs = $schema->resultset('Bar')->search(undef, {
   group_by => [qw{ id foo_id baz_id }],
 })->as_subselect_rs->search(undef, {
   columns => [qw{ id foo_id }]
 });

In the above example normally columns would have to be equal to the group by,
but because we isolated the group by into a subselect the above works.

=cut

sub as_subselect_rs {
  my $self = shift;

  my $attrs = $self->_resolved_attrs;

  my $fresh_rs = (ref $self)->new (
    $self->result_source
  );

  # these pieces will be locked in the subquery
  delete $fresh_rs->{cond};
  delete @{$fresh_rs->{attrs}}{qw/where bind/};

  return $fresh_rs->search( {}, {
    from => [{
      $attrs->{alias} => $self->as_query,
      -alias  => $attrs->{alias},
      -rsrc   => $self->result_source,
    }],
    alias => $attrs->{alias},
  });
}

# This code is called by search_related, and makes sure there
# is clear separation between the joins before, during, and
# after the relationship. This information is needed later
# in order to properly resolve prefetch aliases (any alias
# with a relation_chain_depth less than the depth of the
# current prefetch is not considered)
#
# The increments happen twice per join. An even number means a
# relationship specified via a search_related, whereas an odd
# number indicates a join/prefetch added via attributes
#
# Also this code will wrap the current resultset (the one we
# chain to) in a subselect IFF it contains limiting attributes
sub _chain_relationship {
  my ($self, $rel) = @_;
  my $source = $self->result_source;
  my $attrs = { %{$self->{attrs}||{}} };

  # we need to take the prefetch the attrs into account before we
  # ->_resolve_join as otherwise they get lost - captainL
  my $join = $self->_merge_joinpref_attr( $attrs->{join}, $attrs->{prefetch} );

  delete @{$attrs}{qw/join prefetch collapse group_by distinct _grouped_by_distinct select as columns +select +as +columns/};

  my $seen = { %{ (delete $attrs->{seen_join}) || {} } };

  my $from;
  my @force_subq_attrs = qw/offset rows group_by having/;

  if (
    ($attrs->{from} && ref $attrs->{from} ne 'ARRAY')
      ||
    $self->_has_resolved_attr (@force_subq_attrs)
  ) {
    # Nuke the prefetch (if any) before the new $rs attrs
    # are resolved (prefetch is useless - we are wrapping
    # a subquery anyway).
    my $rs_copy = $self->search;
    $rs_copy->{attrs}{join} = $self->_merge_joinpref_attr (
      $rs_copy->{attrs}{join},
      delete $rs_copy->{attrs}{prefetch},
    );

    $from = [{
      -rsrc   => $source,
      -alias  => $attrs->{alias},
      $attrs->{alias} => $rs_copy->as_query,
    }];
    delete @{$attrs}{@force_subq_attrs, qw/where bind/};
    $seen->{-relation_chain_depth} = 0;
  }
  elsif ($attrs->{from}) {  #shallow copy suffices
    $from = [ @{$attrs->{from}} ];
  }
  else {
    $from = [{
      -rsrc  => $source,
      -alias => $attrs->{alias},
      $attrs->{alias} => $source->from,
    }];
  }

  my $jpath = ($seen->{-relation_chain_depth})
    ? $from->[-1][0]{-join_path}
    : [];

  my @requested_joins = $source->_resolve_join(
    $join,
    $attrs->{alias},
    $seen,
    $jpath,
  );

  push @$from, @requested_joins;

  $seen->{-relation_chain_depth}++;

  # if $self already had a join/prefetch specified on it, the requested
  # $rel might very well be already included. What we do in this case
  # is effectively a no-op (except that we bump up the chain_depth on
  # the join in question so we could tell it *is* the search_related)
  my $already_joined;

  # we consider the last one thus reverse
  for my $j (reverse @requested_joins) {
    my ($last_j) = keys %{$j->[0]{-join_path}[-1]};
    if ($rel eq $last_j) {
      $j->[0]{-relation_chain_depth}++;
      $already_joined++;
      last;
    }
  }

  unless ($already_joined) {
    push @$from, $source->_resolve_join(
      $rel,
      $attrs->{alias},
      $seen,
      $jpath,
    );
  }

  $seen->{-relation_chain_depth}++;

  return {%$attrs, from => $from, seen_join => $seen};
}

sub _resolved_attrs {
  my $self = shift;
  return $self->{_attrs} if $self->{_attrs};

  my $attrs  = { %{ $self->{attrs} || {} } };
  my $source = $attrs->{result_source} = $self->result_source;
  my $alias  = $attrs->{alias};

  $self->throw_exception("Specifying distinct => 1 in conjunction with collapse => 1 is unsupported")
    if $attrs->{collapse} and $attrs->{distinct};

  # default selection list
  $attrs->{columns} = [ $source->columns ]
    unless List::Util::first { exists $attrs->{$_} } qw/columns cols select as/;

  # merge selectors together
  for (qw/columns select as/) {
    $attrs->{$_} = $self->_merge_attr($attrs->{$_}, delete $attrs->{"+$_"})
      if $attrs->{$_} or $attrs->{"+$_"};
  }

  # disassemble columns
  my (@sel, @as);
  if (my $cols = delete $attrs->{columns}) {
    for my $c (ref $cols eq 'ARRAY' ? @$cols : $cols) {
      if (ref $c eq 'HASH') {
        for my $as (sort keys %$c) {
          push @sel, $c->{$as};
          push @as, $as;
        }
      }
      else {
        push @sel, $c;
        push @as, $c;
      }
    }
  }

  # when trying to weed off duplicates later do not go past this point -
  # everything added from here on is unbalanced "anyone's guess" stuff
  my $dedup_stop_idx = $#as;

  push @as, @{ ref $attrs->{as} eq 'ARRAY' ? $attrs->{as} : [ $attrs->{as} ] }
    if $attrs->{as};
  push @sel, @{ ref $attrs->{select} eq 'ARRAY' ? $attrs->{select} : [ $attrs->{select} ] }
    if $attrs->{select};

  # assume all unqualified selectors to apply to the current alias (legacy stuff)
  $_ = (ref $_ or $_ =~ /\./) ? $_ : "$alias.$_" for @sel;

  # disqualify all $alias.col as-bits (inflate-map mandated)
  $_ = ($_ =~ /^\Q$alias.\E(.+)$/) ? $1 : $_ for @as;

  # de-duplicate the result (remove *identical* select/as pairs)
  # and also die on duplicate {as} pointing to different {select}s
  # not using a c-style for as the condition is prone to shrinkage
  my $seen;
  my $i = 0;
  while ($i <= $dedup_stop_idx) {
    if ($seen->{"$sel[$i] \x00\x00 $as[$i]"}++) {
      splice @sel, $i, 1;
      splice @as, $i, 1;
      $dedup_stop_idx--;
    }
    elsif ($seen->{$as[$i]}++) {
      $self->throw_exception(
        "inflate_result() alias '$as[$i]' specified twice with different SQL-side {select}-ors"
      );
    }
    else {
      $i++;
    }
  }

  $attrs->{select} = \@sel;
  $attrs->{as} = \@as;

  $attrs->{from} ||= [{
    -rsrc   => $source,
    -alias  => $self->{attrs}{alias},
    $self->{attrs}{alias} => $source->from,
  }];

  if ( $attrs->{join} || $attrs->{prefetch} ) {

    $self->throw_exception ('join/prefetch can not be used with a custom {from}')
      if ref $attrs->{from} ne 'ARRAY';

    my $join = (delete $attrs->{join}) || {};

    if ( defined $attrs->{prefetch} ) {
      $join = $self->_merge_joinpref_attr( $join, $attrs->{prefetch} );
    }

    $attrs->{from} =    # have to copy here to avoid corrupting the original
      [
        @{ $attrs->{from} },
        $source->_resolve_join(
          $join,
          $alias,
          { %{ $attrs->{seen_join} || {} } },
          ( $attrs->{seen_join} && keys %{$attrs->{seen_join}})
            ? $attrs->{from}[-1][0]{-join_path}
            : []
          ,
        )
      ];
  }

  for my $attr (qw(order_by group_by)) {

    if ( defined $attrs->{$attr} ) {
      $attrs->{$attr} = (
        ref( $attrs->{$attr} ) eq 'ARRAY'
        ? [ @{ $attrs->{$attr} } ]
        : [ $attrs->{$attr} || () ]
      );

      delete $attrs->{$attr} unless @{$attrs->{$attr}};
    }
  }

  # generate selections based on the prefetch helper
  my ($prefetch, @prefetch_select, @prefetch_as);
  $prefetch = $self->_merge_joinpref_attr( {}, delete $attrs->{prefetch} )
    if defined $attrs->{prefetch};

  if ($prefetch) {

    $self->throw_exception("Unable to prefetch, resultset contains an unnamed selector $attrs->{_dark_selector}{string}")
      if $attrs->{_dark_selector};

    $self->throw_exception("Specifying prefetch in conjunction with an explicit collapse => 0 is unsupported")
      if defined $attrs->{collapse} and ! $attrs->{collapse};

    $attrs->{collapse} = 1;

    # this is a separate structure (we don't look in {from} directly)
    # as the resolver needs to shift things off the lists to work
    # properly (identical-prefetches on different branches)
    my $join_map = {};
    if (ref $attrs->{from} eq 'ARRAY') {

      my $start_depth = $attrs->{seen_join}{-relation_chain_depth} || 0;

      for my $j ( @{$attrs->{from}}[1 .. $#{$attrs->{from}} ] ) {
        next unless $j->[0]{-alias};
        next unless $j->[0]{-join_path};
        next if ($j->[0]{-relation_chain_depth} || 0) < $start_depth;

        my @jpath = map { keys %$_ } @{$j->[0]{-join_path}};

        my $p = $join_map;
        $p = $p->{$_} ||= {} for @jpath[ ($start_depth/2) .. $#jpath]; #only even depths are actual jpath boundaries
        push @{$p->{-join_aliases} }, $j->[0]{-alias};
      }
    }

    my @prefetch = $source->_resolve_prefetch( $prefetch, $alias, $join_map );

    # save these for after distinct resolution
    @prefetch_select = map { $_->[0] } @prefetch;
    @prefetch_as = map { $_->[1] } @prefetch;
  }

  # run through the resulting joinstructure (starting from our current slot)
  # and unset collapse if proven unnecessary
  #
  # also while we are at it find out if the current root source has
  # been premultiplied by previous related_source chaining
  #
  # this allows to predict whether a root object with all other relation
  # data set to NULL is in fact unique
  if ($attrs->{collapse}) {

    if (ref $attrs->{from} eq 'ARRAY') {

      if (@{$attrs->{from}} == 1) {
        # no joins - no collapse
        $attrs->{collapse} = 0;
      }
      else {
        # find where our table-spec starts
        my @fromlist = @{$attrs->{from}};
        while (@fromlist) {
          my $t = shift @fromlist;

          my $is_multi;
          # me vs join from-spec distinction - a ref means non-root
          if (ref $t eq 'ARRAY') {
            $t = $t->[0];
            $is_multi ||= ! $t->{-is_single};
          }
          last if ($t->{-alias} && $t->{-alias} eq $alias);
          $attrs->{_main_source_premultiplied} ||= $is_multi;
        }

        # no non-singles remaining, nor any premultiplication - nothing to collapse
        if (
          ! $attrs->{_main_source_premultiplied}
            and
          ! List::Util::first { ! $_->[0]{-is_single} } @fromlist
        ) {
          $attrs->{collapse} = 0;
        }
      }
    }

    else {
      # if we can not analyze the from - err on the side of safety
      $attrs->{_main_source_premultiplied} = 1;
    }
  }

  # generate the distinct induced group_by before injecting the prefetched select/as parts
  if (delete $attrs->{distinct}) {
    if ($attrs->{group_by}) {
      carp_unique ("Useless use of distinct on a grouped resultset ('distinct' is ignored when a 'group_by' is present)");
    }
    else {
      $attrs->{_grouped_by_distinct} = 1;
      # distinct affects only the main selection part, not what prefetch may add below
      ($attrs->{group_by}, my $new_order) = $source->storage->_group_over_selection($attrs);

      # FIXME possibly ignore a rewritten order_by (may turn out to be an issue)
      # The thinking is: if we are collapsing the subquerying prefetch engine will
      # rip stuff apart for us anyway, and we do not want to have a potentially
      # function-converted external order_by
      # ( there is an explicit if ( collapse && _grouped_by_distinct ) check in DBIHacks )
      $attrs->{order_by} = $new_order unless $attrs->{collapse};
    }
  }

  # inject prefetch-bound selection (if any)
  push @{$attrs->{select}}, @prefetch_select;
  push @{$attrs->{as}}, @prefetch_as;

  $attrs->{_simple_passthrough_construction} = !(
    $attrs->{collapse}
      or
    grep { $_ =~ /\./ } @{$attrs->{as}}
  );

  # if both page and offset are specified, produce a combined offset
  # even though it doesn't make much sense, this is what pre 081xx has
  # been doing
  if (my $page = delete $attrs->{page}) {
    $attrs->{offset} =
      ($attrs->{rows} * ($page - 1))
            +
      ($attrs->{offset} || 0)
    ;
  }

  return $self->{_attrs} = $attrs;
}

sub _rollout_attr {
  my ($self, $attr) = @_;

  if (ref $attr eq 'HASH') {
    return $self->_rollout_hash($attr);
  } elsif (ref $attr eq 'ARRAY') {
    return $self->_rollout_array($attr);
  } else {
    return [$attr];
  }
}

sub _rollout_array {
  my ($self, $attr) = @_;

  my @rolled_array;
  foreach my $element (@{$attr}) {
    if (ref $element eq 'HASH') {
      push( @rolled_array, @{ $self->_rollout_hash( $element ) } );
    } elsif (ref $element eq 'ARRAY') {
      #  XXX - should probably recurse here
      push( @rolled_array, @{$self->_rollout_array($element)} );
    } else {
      push( @rolled_array, $element );
    }
  }
  return \@rolled_array;
}

sub _rollout_hash {
  my ($self, $attr) = @_;

  my @rolled_array;
  foreach my $key (keys %{$attr}) {
    push( @rolled_array, { $key => $attr->{$key} } );
  }
  return \@rolled_array;
}

sub _calculate_score {
  my ($self, $a, $b) = @_;

  if (defined $a xor defined $b) {
    return 0;
  }
  elsif (not defined $a) {
    return 1;
  }

  if (ref $b eq 'HASH') {
    my ($b_key) = keys %{$b};
    $b_key = '' if ! defined $b_key;
    if (ref $a eq 'HASH') {
      my ($a_key) = keys %{$a};
      $a_key = '' if ! defined $a_key;
      if ($a_key eq $b_key) {
        return (1 + $self->_calculate_score( $a->{$a_key}, $b->{$b_key} ));
      } else {
        return 0;
      }
    } else {
      return ($a eq $b_key) ? 1 : 0;
    }
  } else {
    if (ref $a eq 'HASH') {
      my ($a_key) = keys %{$a};
      return ($b eq $a_key) ? 1 : 0;
    } else {
      return ($b eq $a) ? 1 : 0;
    }
  }
}

sub _merge_joinpref_attr {
  my ($self, $orig, $import) = @_;

  return $import unless defined($orig);
  return $orig unless defined($import);

  $orig = $self->_rollout_attr($orig);
  $import = $self->_rollout_attr($import);

  my $seen_keys;
  foreach my $import_element ( @{$import} ) {
    # find best candidate from $orig to merge $b_element into
    my $best_candidate = { position => undef, score => 0 }; my $position = 0;
    foreach my $orig_element ( @{$orig} ) {
      my $score = $self->_calculate_score( $orig_element, $import_element );
      if ($score > $best_candidate->{score}) {
        $best_candidate->{position} = $position;
        $best_candidate->{score} = $score;
      }
      $position++;
    }
    my ($import_key) = ( ref $import_element eq 'HASH' ) ? keys %{$import_element} : ($import_element);
    $import_key = '' if not defined $import_key;

    if ($best_candidate->{score} == 0 || exists $seen_keys->{$import_key}) {
      push( @{$orig}, $import_element );
    } else {
      my $orig_best = $orig->[$best_candidate->{position}];
      # merge orig_best and b_element together and replace original with merged
      if (ref $orig_best ne 'HASH') {
        $orig->[$best_candidate->{position}] = $import_element;
      } elsif (ref $import_element eq 'HASH') {
        my ($key) = keys %{$orig_best};
        $orig->[$best_candidate->{position}] = { $key => $self->_merge_joinpref_attr($orig_best->{$key}, $import_element->{$key}) };
      }
    }
    $seen_keys->{$import_key} = 1; # don't merge the same key twice
  }

  return @$orig ? $orig : ();
}

{
  my $hm;

  sub _merge_attr {
    $hm ||= do {
      my $hm = Hash::Merge->new;

      $hm->specify_behavior({
        SCALAR => {
          SCALAR => sub {
            my ($defl, $defr) = map { defined $_ } (@_[0,1]);

            if ($defl xor $defr) {
              return [ $defl ? $_[0] : $_[1] ];
            }
            elsif (! $defl) {
              return [];
            }
            elsif (__HM_DEDUP and $_[0] eq $_[1]) {
              return [ $_[0] ];
            }
            else {
              return [$_[0], $_[1]];
            }
          },
          ARRAY => sub {
            return $_[1] if !defined $_[0];
            return $_[1] if __HM_DEDUP and List::Util::first { $_ eq $_[0] } @{$_[1]};
            return [$_[0], @{$_[1]}]
          },
          HASH  => sub {
            return [] if !defined $_[0] and !keys %{$_[1]};
            return [ $_[1] ] if !defined $_[0];
            return [ $_[0] ] if !keys %{$_[1]};
            return [$_[0], $_[1]]
          },
        },
        ARRAY => {
          SCALAR => sub {
            return $_[0] if !defined $_[1];
            return $_[0] if __HM_DEDUP and List::Util::first { $_ eq $_[1] } @{$_[0]};
            return [@{$_[0]}, $_[1]]
          },
          ARRAY => sub {
            my @ret = @{$_[0]} or return $_[1];
            return [ @ret, @{$_[1]} ] unless __HM_DEDUP;
            my %idx = map { $_ => 1 } @ret;
            push @ret, grep { ! defined $idx{$_} } (@{$_[1]});
            \@ret;
          },
          HASH => sub {
            return [ $_[1] ] if ! @{$_[0]};
            return $_[0] if !keys %{$_[1]};
            return $_[0] if __HM_DEDUP and List::Util::first { $_ eq $_[1] } @{$_[0]};
            return [ @{$_[0]}, $_[1] ];
          },
        },
        HASH => {
          SCALAR => sub {
            return [] if !keys %{$_[0]} and !defined $_[1];
            return [ $_[0] ] if !defined $_[1];
            return [ $_[1] ] if !keys %{$_[0]};
            return [$_[0], $_[1]]
          },
          ARRAY => sub {
            return [] if !keys %{$_[0]} and !@{$_[1]};
            return [ $_[0] ] if !@{$_[1]};
            return $_[1] if !keys %{$_[0]};
            return $_[1] if __HM_DEDUP and List::Util::first { $_ eq $_[0] } @{$_[1]};
            return [ $_[0], @{$_[1]} ];
          },
          HASH => sub {
            return [] if !keys %{$_[0]} and !keys %{$_[1]};
            return [ $_[0] ] if !keys %{$_[1]};
            return [ $_[1] ] if !keys %{$_[0]};
            return [ $_[0] ] if $_[0] eq $_[1];
            return [ $_[0], $_[1] ];
          },
        }
      } => 'DBIC_RS_ATTR_MERGER');
      $hm;
    };

    return $hm->merge ($_[1], $_[2]);
  }
}

sub STORABLE_freeze {
  my ($self, $cloning) = @_;
  my $to_serialize = { %$self };

  # A cursor in progress can't be serialized (and would make little sense anyway)
  # the parser can be regenerated (and can't be serialized)
  delete @{$to_serialize}{qw/cursor _row_parser _result_inflator/};

  # nor is it sensical to store a not-yet-fired-count pager
  if ($to_serialize->{pager} and ref $to_serialize->{pager}{total_entries} eq 'CODE') {
    delete $to_serialize->{pager};
  }

  Storable::nfreeze($to_serialize);
}

# need this hook for symmetry
sub STORABLE_thaw {
  my ($self, $cloning, $serialized) = @_;

  %$self = %{ Storable::thaw($serialized) };

  $self;
}


=head2 throw_exception

See L<DBIx::Class::Schema/throw_exception> for details.

=cut

sub throw_exception {
  my $self=shift;

  if (ref $self and my $rsrc = $self->result_source) {
    $rsrc->throw_exception(@_)
  }
  else {
    DBIx::Class::Exception->throw(@_);
  }
}

1;

__END__

# XXX: FIXME: Attributes docs need clearing up

=head1 ATTRIBUTES

Attributes are used to refine a ResultSet in various ways when
searching for data. They can be passed to any method which takes an
C<\%attrs> argument. See L</search>, L</search_rs>, L</find>,
L</count>.

Default attributes can be set on the result class using
L<DBIx::Class::ResultSource/resultset_attributes>.  (Please read
the CAVEATS on that feature before using it!)

These are in no particular order:

=head2 order_by

=over 4

=item Value: ( $order_by | \@order_by | \%order_by )

=back

Which column(s) to order the results by.

[The full list of suitable values is documented in
L<SQL::Abstract/"ORDER BY CLAUSES">; the following is a summary of
common options.]

If a single column name, or an arrayref of names is supplied, the
argument is passed through directly to SQL. The hashref syntax allows
for connection-agnostic specification of ordering direction:

 For descending order:

  order_by => { -desc => [qw/col1 col2 col3/] }

 For explicit ascending order:

  order_by => { -asc => 'col' }

The old scalarref syntax (i.e. order_by => \'year DESC') is still
supported, although you are strongly encouraged to use the hashref
syntax as outlined above.

=head2 columns

=over 4

=item Value: \@columns | \%columns | $column

=back

Shortcut to request a particular set of columns to be retrieved. Each
column spec may be a string (a table column name), or a hash (in which
case the key is the C<as> value, and the value is used as the C<select>
expression). Adds the L</current_source_alias> onto the start of any column without a C<.> in
it and sets C<select> from that, then auto-populates C<as> from
C<select> as normal. (You may also use the C<cols> attribute, as in
earlier versions of DBIC, but this is deprecated)

Essentially C<columns> does the same as L</select> and L</as>.

    columns => [ 'some_column', { dbic_slot => 'another_column' } ]

is the same as

    select => [qw(some_column another_column)],
    as     => [qw(some_column dbic_slot)]

If you want to individually retrieve related columns (in essence perform
manual L</prefetch>) you have to make sure to specify the correct inflation slot
chain such that it matches existing relationships:

    my $rs = $schema->resultset('Artist')->search({}, {
        # required to tell DBIC to collapse has_many relationships
        collapse => 1,
        join     => { cds => 'tracks' },
        '+columns'  => {
          'cds.cdid'         => 'cds.cdid',
          'cds.tracks.title' => 'tracks.title',
        },
    });

Like elsewhere, literal SQL or literal values can be included by using a
scalar reference or a literal bind value, and these values will be available
in the result with C<get_column> (see also
L<SQL::Abstract/Literal SQL and value type operators>):

    # equivalent SQL: SELECT 1, 'a string', IF(my_column,?,?) ...
    # bind values: $true_value, $false_value
    columns => [
        {
            foo => \1,
            bar => \q{'a string'},
            baz => \[ 'IF(my_column,?,?)', $true_value, $false_value ],
        }
    ]

=head2 +columns

B<NOTE:> You B<MUST> explicitly quote C<'+columns'> when using this attribute.
Not doing so causes Perl to incorrectly interpret C<+columns> as a bareword
with a unary plus operator before it, which is the same as simply C<columns>.

=over 4

=item Value: \@extra_columns

=back

Indicates additional columns to be selected from storage. Works the same as
L</columns> but adds columns to the current selection. (You may also use the
C<include_columns> attribute, as in earlier versions of DBIC, but this is
deprecated)

  $schema->resultset('CD')->search(undef, {
    '+columns' => ['artist.name'],
    join => ['artist']
  });

would return all CDs and include a 'name' column to the information
passed to object inflation. Note that the 'artist' is the name of the
column (or relationship) accessor, and 'name' is the name of the column
accessor in the related table.

=head2 select

=over 4

=item Value: \@select_columns

=back

Indicates which columns should be selected from the storage. You can use
column names, or in the case of RDBMS back ends, function or stored procedure
names:

  $rs = $schema->resultset('Employee')->search(undef, {
    select => [
      'name',
      { count => 'employeeid' },
      { max => { length => 'name' }, -as => 'longest_name' }
    ]
  });

  # Equivalent SQL
  SELECT name, COUNT( employeeid ), MAX( LENGTH( name ) ) AS longest_name FROM employee

B<NOTE:> You will almost always need a corresponding L</as> attribute when you
use L</select>, to instruct DBIx::Class how to store the result of the column.

Also note that the L</as> attribute has B<nothing to do> with the SQL-side
C<AS> identifier aliasing. You B<can> alias a function (so you can use it e.g.
in an C<ORDER BY> clause), however this is done via the C<-as> B<select
function attribute> supplied as shown in the example above.

=head2 +select

B<NOTE:> You B<MUST> explicitly quote C<'+select'> when using this attribute.
Not doing so causes Perl to incorrectly interpret C<+select> as a bareword
with a unary plus operator before it, which is the same as simply C<select>.

=over 4

=item Value: \@extra_select_columns

=back

Indicates additional columns to be selected from storage.  Works the same as
L</select> but adds columns to the current selection, instead of specifying
a new explicit list.

=head2 as

=over 4

=item Value: \@inflation_names

=back

Indicates DBIC-side names for object inflation. That is L</as> indicates the
slot name in which the column value will be stored within the
L<Row|DBIx::Class::Row> object. The value will then be accessible via this
identifier by the C<get_column> method (or via the object accessor B<if one
with the same name already exists>) as shown below.

The L</as> attribute has B<nothing to do> with the SQL-side identifier
aliasing C<AS>. See L</select> for details.

  $rs = $schema->resultset('Employee')->search(undef, {
    select => [
      'name',
      { count => 'employeeid' },
      { max => { length => 'name' }, -as => 'longest_name' }
    ],
    as => [qw/
      name
      employee_count
      max_name_length
    /],
  });

If the object against which the search is performed already has an accessor
matching a column name specified in C<as>, the value can be retrieved using
the accessor as normal:

  my $name = $employee->name();

If on the other hand an accessor does not exist in the object, you need to
use C<get_column> instead:

  my $employee_count = $employee->get_column('employee_count');

You can create your own accessors if required - see
L<DBIx::Class::Manual::Cookbook> for details.

=head2 +as

B<NOTE:> You B<MUST> explicitly quote C<'+as'> when using this attribute.
Not doing so causes Perl to incorrectly interpret C<+as> as a bareword
with a unary plus operator before it, which is the same as simply C<as>.

=over 4

=item Value: \@extra_inflation_names

=back

Indicates additional inflation names for selectors added via L</+select>. See L</as>.

=head2 join

=over 4

=item Value: ($rel_name | \@rel_names | \%rel_names)

=back

Contains a list of relationships that should be joined for this query.  For
example:

  # Get CDs by Nine Inch Nails
  my $rs = $schema->resultset('CD')->search(
    { 'artist.name' => 'Nine Inch Nails' },
    { join => 'artist' }
  );

Can also contain a hash reference to refer to the other relation's relations.
For example:

  package MyApp::Schema::Track;
  use base qw/DBIx::Class/;
  __PACKAGE__->table('track');
  __PACKAGE__->add_columns(qw/trackid cd position title/);
  __PACKAGE__->set_primary_key('trackid');
  __PACKAGE__->belongs_to(cd => 'MyApp::Schema::CD');
  1;

  # In your application
  my $rs = $schema->resultset('Artist')->search(
    { 'track.title' => 'Teardrop' },
    {
      join     => { cd => 'track' },
      order_by => 'artist.name',
    }
  );

You need to use the relationship (not the table) name in  conditions,
because they are aliased as such. The current table is aliased as "me", so
you need to use me.column_name in order to avoid ambiguity. For example:

  # Get CDs from 1984 with a 'Foo' track
  my $rs = $schema->resultset('CD')->search(
    {
      'me.year' => 1984,
      'tracks.name' => 'Foo'
    },
    { join => 'tracks' }
  );

If the same join is supplied twice, it will be aliased to <rel>_2 (and
similarly for a third time). For e.g.

  my $rs = $schema->resultset('Artist')->search({
    'cds.title'   => 'Down to Earth',
    'cds_2.title' => 'Popular',
  }, {
    join => [ qw/cds cds/ ],
  });

will return a set of all artists that have both a cd with title 'Down
to Earth' and a cd with title 'Popular'.

If you want to fetch related objects from other tables as well, see L</prefetch>
below.

 NOTE: An internal join-chain pruner will discard certain joins while
 constructing the actual SQL query, as long as the joins in question do not
 affect the retrieved result. This for example includes 1:1 left joins
 that are not part of the restriction specification (WHERE/HAVING) nor are
 a part of the query selection.

For more help on using joins with search, see L<DBIx::Class::Manual::Joining>.

=head2 collapse

=over 4

=item Value: (0 | 1)

=back

When set to a true value, indicates that any rows fetched from joined has_many
relationships are to be aggregated into the corresponding "parent" object. For
example, the resultset:

  my $rs = $schema->resultset('CD')->search({}, {
    '+columns' => [ qw/ tracks.title tracks.position / ],
    join => 'tracks',
    collapse => 1,
  });

While executing the following query:

  SELECT me.*, tracks.title, tracks.position
    FROM cd me
    LEFT JOIN track tracks
      ON tracks.cdid = me.cdid

Will return only as many objects as there are rows in the CD source, even
though the result of the query may span many rows. Each of these CD objects
will in turn have multiple "Track" objects hidden behind the has_many
generated accessor C<tracks>. Without C<< collapse => 1 >>, the return values
of this resultset would be as many CD objects as there are tracks (a "Cartesian
product"), with each CD object containing exactly one of all fetched Track data.

When a collapse is requested on a non-ordered resultset, an order by some
unique part of the main source (the left-most table) is inserted automatically.
This is done so that the resultset is allowed to be "lazy" - calling
L<< $rs->next|/next >> will fetch only as many rows as it needs to build the next
object with all of its related data.

If an L</order_by> is already declared, and orders the resultset in a way that
makes collapsing as described above impossible (e.g. C<< ORDER BY
has_many_rel.column >> or C<ORDER BY RANDOM()>), DBIC will automatically
switch to "eager" mode and slurp the entire resultset before constructing the
first object returned by L</next>.

Setting this attribute on a resultset that does not join any has_many
relations is a no-op.

For a more in-depth discussion, see L</PREFETCHING>.

=head2 prefetch

=over 4

=item Value: ($rel_name | \@rel_names | \%rel_names)

=back

This attribute is a shorthand for specifying a L</join> spec, adding all
columns from the joined related sources as L</+columns> and setting
L</collapse> to a true value. It can be thought of as a rough B<superset>
of the L</join> attribute.

For example, the following two queries are equivalent:

  my $rs = $schema->resultset('Artist')->search({}, {
    prefetch => { cds => ['genre', 'tracks' ] },
  });

and

  my $rs = $schema->resultset('Artist')->search({}, {
    join => { cds => ['genre', 'tracks' ] },
    collapse => 1,
    '+columns' => [
      (map
        { +{ "cds.$_" => "cds.$_" } }
        $schema->source('Artist')->related_source('cds')->columns
      ),
      (map
        { +{ "cds.genre.$_" => "genre.$_" } }
        $schema->source('Artist')->related_source('cds')->related_source('genre')->columns
      ),
      (map
        { +{ "cds.tracks.$_" => "tracks.$_" } }
        $schema->source('Artist')->related_source('cds')->related_source('tracks')->columns
      ),
    ],
  });

Both producing the following SQL:

  SELECT  me.artistid, me.name, me.rank, me.charfield,
          cds.cdid, cds.artist, cds.title, cds.year, cds.genreid, cds.single_track,
          genre.genreid, genre.name,
          tracks.trackid, tracks.cd, tracks.position, tracks.title, tracks.last_updated_on, tracks.last_updated_at
    FROM artist me
    LEFT JOIN cd cds
      ON cds.artist = me.artistid
    LEFT JOIN genre genre
      ON genre.genreid = cds.genreid
    LEFT JOIN track tracks
      ON tracks.cd = cds.cdid
  ORDER BY me.artistid

While L</prefetch> implies a L</join>, it is ok to mix the two together, as
the arguments are properly merged and generally do the right thing. For
example, you may want to do the following:

  my $artists_and_cds_without_genre = $schema->resultset('Artist')->search(
    { 'genre.genreid' => undef },
    {
      join => { cds => 'genre' },
      prefetch => 'cds',
    }
  );

Which generates the following SQL:

  SELECT  me.artistid, me.name, me.rank, me.charfield,
          cds.cdid, cds.artist, cds.title, cds.year, cds.genreid, cds.single_track
    FROM artist me
    LEFT JOIN cd cds
      ON cds.artist = me.artistid
    LEFT JOIN genre genre
      ON genre.genreid = cds.genreid
  WHERE genre.genreid IS NULL
  ORDER BY me.artistid

For a more in-depth discussion, see L</PREFETCHING>.

=head2 alias

=over 4

=item Value: $source_alias

=back

Sets the source alias for the query.  Normally, this defaults to C<me>, but
nested search queries (sub-SELECTs) might need specific aliases set to
reference inner queries.  For example:

   my $q = $rs
      ->related_resultset('CDs')
      ->related_resultset('Tracks')
      ->search({
         'track.id' => { -ident => 'none_search.id' },
      })
      ->as_query;

   my $ids = $self->search({
      -not_exists => $q,
   }, {
      alias    => 'none_search',
      group_by => 'none_search.id',
   })->get_column('id')->as_query;

   $self->search({ id => { -in => $ids } })

This attribute is directly tied to L</current_source_alias>.

=head2 page

=over 4

=item Value: $page

=back

Makes the resultset paged and specifies the page to retrieve. Effectively
identical to creating a non-pages resultset and then calling ->page($page)
on it.

If L</rows> attribute is not specified it defaults to 10 rows per page.

When you have a paged resultset, L</count> will only return the number
of rows in the page. To get the total, use the L</pager> and call
C<total_entries> on it.

=head2 rows

=over 4

=item Value: $rows

=back

Specifies the maximum number of rows for direct retrieval or the number of
rows per page if the page attribute or method is used.

=head2 offset

=over 4

=item Value: $offset

=back

Specifies the (zero-based) row number for the  first row to be returned, or the
of the first row of the first page if paging is used.

=head2 software_limit

=over 4

=item Value: (0 | 1)

=back

When combined with L</rows> and/or L</offset> the generated SQL will not
include any limit dialect stanzas. Instead the entire result will be selected
as if no limits were specified, and DBIC will perform the limit locally, by
artificially advancing and finishing the resulting L</cursor>.

This is the recommended way of performing resultset limiting when no sane RDBMS
implementation is available (e.g.
L<Sybase ASE|DBIx::Class::Storage::DBI::Sybase::ASE> using the
L<Generic Sub Query|DBIx::Class::SQLMaker::LimitDialects/GenericSubQ> hack)

=head2 group_by

=over 4

=item Value: \@columns

=back

A arrayref of columns to group by. Can include columns of joined tables.

  group_by => [qw/ column1 column2 ... /]

=head2 having

=over 4

=item Value: $condition

=back

The HAVING operator specifies a B<secondary> condition applied to the set
after the grouping calculations have been done. In other words it is a
constraint just like L</where> (and accepting the same
L<SQL::Abstract syntax|SQL::Abstract/WHERE CLAUSES>) applied to the data
as it exists after GROUP BY has taken place. Specifying L</having> without
L</group_by> is a logical mistake, and a fatal error on most RDBMS engines.

E.g.

  having => { 'count_employee' => { '>=', 100 } }

or with an in-place function in which case literal SQL is required:

  having => \[ 'count(employee) >= ?', 100 ]

=head2 distinct

=over 4

=item Value: (0 | 1)

=back

Set to 1 to automatically generate a L</group_by> clause based on the selection
(including intelligent handling of L</order_by> contents). Note that the group
criteria calculation takes place over the B<final> selection. This includes
any L</+columns>, L</+select> or L</order_by> additions in subsequent
L</search> calls, and standalone columns selected via
L<DBIx::Class::ResultSetColumn> (L</get_column>). A notable exception are the
extra selections specified via L</prefetch> - such selections are explicitly
excluded from group criteria calculations.

If the final ResultSet also explicitly defines a L</group_by> attribute, this
setting is ignored and an appropriate warning is issued.

=head2 where

=over 4

Adds to the WHERE clause.

  # only return rows WHERE deleted IS NULL for all searches
  __PACKAGE__->resultset_attributes({ where => { deleted => undef } });

Can be overridden by passing C<< { where => undef } >> as an attribute
to a resultset.

For more complicated where clauses see L<SQL::Abstract/WHERE CLAUSES>.

=back

=head2 cache

Set to 1 to cache search results. This prevents extra SQL queries if you
revisit rows in your ResultSet:

  my $resultset = $schema->resultset('Artist')->search( undef, { cache => 1 } );

  while( my $artist = $resultset->next ) {
    ... do stuff ...
  }

  $rs->first; # without cache, this would issue a query

By default, searches are not cached.

For more examples of using these attributes, see
L<DBIx::Class::Manual::Cookbook>.

=head2 for

=over 4

=item Value: ( 'update' | 'shared' | \$scalar )

=back

Set to 'update' for a SELECT ... FOR UPDATE or 'shared' for a SELECT
... FOR SHARED. If \$scalar is passed, this is taken directly and embedded in the
query.

=head1 PREFETCHING

DBIx::Class supports arbitrary related data prefetching from multiple related
sources. Any combination of relationship types and column sets are supported.
If L<collapsing|/collapse> is requested, there is an additional requirement of
selecting enough data to make every individual object uniquely identifiable.

Here are some more involved examples, based on the following relationship map:

  # Assuming:
  My::Schema::CD->belongs_to( artist      => 'My::Schema::Artist'     );
  My::Schema::CD->might_have( liner_note  => 'My::Schema::LinerNotes' );
  My::Schema::CD->has_many(   tracks      => 'My::Schema::Track'      );

  My::Schema::Artist->belongs_to( record_label => 'My::Schema::RecordLabel' );

  My::Schema::Track->has_many( guests => 'My::Schema::Guest' );



  my $rs = $schema->resultset('Tag')->search(
    undef,
    {
      prefetch => {
        cd => 'artist'
      }
    }
  );

The initial search results in SQL like the following:

  SELECT tag.*, cd.*, artist.* FROM tag
  JOIN cd ON tag.cd = cd.cdid
  JOIN artist ON cd.artist = artist.artistid

L<DBIx::Class> has no need to go back to the database when we access the
C<cd> or C<artist> relationships, which saves us two SQL statements in this
case.

Simple prefetches will be joined automatically, so there is no need
for a C<join> attribute in the above search.

The L</prefetch> attribute can be used with any of the relationship types
and multiple prefetches can be specified together. Below is a more complex
example that prefetches a CD's artist, its liner notes (if present),
the cover image, the tracks on that CD, and the guests on those
tracks.

  my $rs = $schema->resultset('CD')->search(
    undef,
    {
      prefetch => [
        { artist => 'record_label'},  # belongs_to => belongs_to
        'liner_note',                 # might_have
        'cover_image',                # has_one
        { tracks => 'guests' },       # has_many => has_many
      ]
    }
  );

This will produce SQL like the following:

  SELECT cd.*, artist.*, record_label.*, liner_note.*, cover_image.*,
         tracks.*, guests.*
    FROM cd me
    JOIN artist artist
      ON artist.artistid = me.artistid
    JOIN record_label record_label
      ON record_label.labelid = artist.labelid
    LEFT JOIN track tracks
      ON tracks.cdid = me.cdid
    LEFT JOIN guest guests
      ON guests.trackid = track.trackid
    LEFT JOIN liner_notes liner_note
      ON liner_note.cdid = me.cdid
    JOIN cd_artwork cover_image
      ON cover_image.cdid = me.cdid
  ORDER BY tracks.cd

Now the C<artist>, C<record_label>, C<liner_note>, C<cover_image>,
C<tracks>, and C<guests> of the CD will all be available through the
relationship accessors without the need for additional queries to the
database.

=head3 CAVEATS

Prefetch does a lot of deep magic. As such, it may not behave exactly
as you might expect.

=over 4

=item *

Prefetch uses the L</cache> to populate the prefetched relationships. This
may or may not be what you want.

=item *

If you specify a condition on a prefetched relationship, ONLY those
rows that match the prefetched condition will be fetched into that relationship.
This means that adding prefetch to a search() B<may alter> what is returned by
traversing a relationship. So, if you have C<< Artist->has_many(CDs) >> and you do

  my $artist_rs = $schema->resultset('Artist')->search({
      'cds.year' => 2008,
  }, {
      join => 'cds',
  });

  my $count = $artist_rs->first->cds->count;

  my $artist_rs_prefetch = $artist_rs->search( {}, { prefetch => 'cds' } );

  my $prefetch_count = $artist_rs_prefetch->first->cds->count;

  cmp_ok( $count, '==', $prefetch_count, "Counts should be the same" );

That cmp_ok() may or may not pass depending on the datasets involved. In other
words the C<WHERE> condition would apply to the entire dataset, just like
it would in regular SQL. If you want to add a condition only to the "right side"
of a C<LEFT JOIN> - consider declaring and using a L<relationship with a custom
condition|DBIx::Class::Relationship::Base/condition>

=back

=head1 DBIC BIND VALUES

Because DBIC may need more information to bind values than just the column name
and value itself, it uses a special format for both passing and receiving bind
values.  Each bind value should be composed of an arrayref of
C<< [ \%args => $val ] >>.  The format of C<< \%args >> is currently:

=over 4

=item dbd_attrs

If present (in any form), this is what is being passed directly to bind_param.
Note that different DBD's expect different bind args.  (e.g. DBD::SQLite takes
a single numerical type, while DBD::Pg takes a hashref if bind options.)

If this is specified, all other bind options described below are ignored.

=item sqlt_datatype

If present, this is used to infer the actual bind attribute by passing to
C<< $resolved_storage->bind_attribute_by_data_type() >>.  Defaults to the
"data_type" from the L<add_columns column info|DBIx::Class::ResultSource/add_columns>.

Note that the data type is somewhat freeform (hence the sqlt_ prefix);
currently drivers are expected to "Do the Right Thing" when given a common
datatype name.  (Not ideal, but that's what we got at this point.)

=item sqlt_size

Currently used to correctly allocate buffers for bind_param_inout().
Defaults to "size" from the L<add_columns column info|DBIx::Class::ResultSource/add_columns>,
or to a sensible value based on the "data_type".

=item dbic_colname

Used to fill in missing sqlt_datatype and sqlt_size attributes (if they are
explicitly specified they are never overridden).  Also used by some weird DBDs,
where the column name should be available at bind_param time (e.g. Oracle).

=back

For backwards compatibility and convenience, the following shortcuts are
supported:

  [ $name => $val ] === [ { dbic_colname => $name }, $val ]
  [ \$dt  => $val ] === [ { sqlt_datatype => $dt }, $val ]
  [ undef,   $val ] === [ {}, $val ]
  $val              === [ {}, $val ]

=head1 FURTHER QUESTIONS?

Check the list of L<additional DBIC resources|DBIx::Class/GETTING HELP/SUPPORT>.

=head1 COPYRIGHT AND LICENSE

This module is free software L<copyright|DBIx::Class/COPYRIGHT AND LICENSE>
by the L<DBIx::Class (DBIC) authors|DBIx::Class/AUTHORS>. You can
redistribute it and/or modify it under the same terms as the
L<DBIx::Class library|DBIx::Class/COPYRIGHT AND LICENSE>.

=cut
