package B::Utils;

use 5.006;
use strict;
use warnings;
use vars qw( @EXPORT_OK %EXPORT_TAGS
    @bad_stashes $TRACE_FH $file $line $sub );

use subs (
    qw( all_starts all_roots anon_sub recalc_sub_cache ),
    qw( walkoptree_simple walkoptree_filtered ),
    qw( walkallops_simple walkallops_filtered ),
    qw( opgrep op_or ),
);
sub croak (@);
sub carp (@);

use Scalar::Util qw( weaken blessed );

=head1 NAME

B::Utils - Helper functions for op tree manipulation

=head1 VERSION

version 0.27

=cut


# NOTE: The pod/code version here and in README are computer checked
# by xt/version.t. Keep them in sync.

our $VERSION = '0.27';



=head1 INSTALLATION

To install this module, run the following commands:

    perl Makefile.PL
    make
    make test
    make install

=cut



use base 'DynaLoader';
bootstrap B::Utils $VERSION;
#bootstrap B::Utils::OP $VERSION;
#B::Utils::OP::boot_B__Utils__OP();
sub dl_load_flags {0x01}

=head1 SYNOPSIS

  use B::Utils;

=cut

use B qw( OPf_KIDS main_start main_root walksymtable class main_cv ppname );

use Exporter ();
@EXPORT_OK = qw(all_starts all_roots anon_subs
    walkoptree_simple walkoptree_filtered
    walkallops_simple walkallops_filtered
    recalc_sub_cache
    opgrep op_or );
%EXPORT_TAGS = ( all => \@EXPORT_OK );
*import      = \&Exporter::import;

@bad_stashes
    = qw(B Carp Exporter warnings Cwd Config CORE blib strict DynaLoader vars XSLoader AutoLoader base);

use List::Util qw( shuffle );

BEGIN {

    # Fake up a TRACE constant and set $TRACE_FH
    BEGIN { $^W = 0 }
    no warnings;
    eval 'sub _TRACE () {' . ( 0 + $ENV{B_UTILS_TRACE} ) . '}';
    die $@ if $@;
    $TRACE_FH ||= \*STDOUT;
}
sub _TRUE ()  { !!1 }
sub _FALSE () { !!0 }

=head1 OP METHODS

=over 4

=cut

# The following functions have been removed because it turns out that
# this breaks stuff like B::Concise which depends on ops lacking
# methods they wouldn't normally have.
#
# =pod
#
# =item C<$op-E<gt>first>
#
# =item C<$oo-E<gt>last>
#
# =item C<$op-E<gt>other>
#
# Normally if you call first, last or other on anything which is not an
# UNOP, BINOP or LOGOP respectively it will die.  This leads to lots of
# code like:
#
#     $op->first if $op->can('first');
#
# B::Utils provided every op with first, last and other methods which
# will simply return nothing if it isn't relevant. But this broke B::Concise
#
# =cut
#
# sub B::OP::first { $_[0]->can("SUPER::first") ? $_[0]->SUPER::first() : () }
# sub B::OP::last  { $_[0]->can("SUPER::last")  ? $_[0]->SUPER::last()  : () }
# sub B::OP::other { $_[0]->can("SUPER::other") ? $_[0]->SUPER::other() : () }

=item C<$op-E<gt>oldname>

Returns the name of the op, even if it is currently optimized to null.
This helps you understand the structure of the op tree.

=cut

sub B::OP::oldname {
    my $op   = shift;
    my $name = $op->name;
    my $targ = $op->targ;

    # This is a an operation which *used* to be a real op but was
    # optimized away. Fetch the old value and ignore the leading pp_.

    # I forget why the original pp # is located in the targ field.
    return $name eq 'null' && $targ
        ? substr( ppname($targ), 3 )
        : $name;

}

=item C<$op-E<gt>kids>

Returns an array of all this op's non-null children, in order.

=cut

sub B::OP::kids {
    my $op = shift;
    return unless defined wantarray;

    my @kids;
    if ( ref $op and $$op and $op->flags & OPf_KIDS ) {
        for (my $kid = $op->first; $$kid; $kid = $kid->sibling) {
            push @kids, $kid;
        }
        ### Assert: $op->children == @kids
    }
    else {
        @kids = (
            ( $op->can('first') ? $op->first : () ),
            ( $op->can('last')  ? $op->last  : () ),
            ( $op->can('other') ? $op->other : () )
        );
    }
    return @kids;
}

=item C<$op-E<gt>parent>

Returns the parent node in the op tree, if possible. Currently
"possible" means "if the tree has already been optimized"; that is, if
we're during a C<CHECK> block. (and hence, if we have valid C<next>
pointers.)

In the future, it may be possible to search for the parent before we
have the C<next> pointers in place, but it'll take me a while to
figure out how to do that.

Warning: Since 5.21.2 B comes with its own version of B::OP::parent
which returns either B::NULL or the real parent when ccflags contains
-DPERL_OP_PARENT.
In this case rather use $op->_parent.

=cut

BEGIN {
  unless ($] >= 5.021002 and exists &B::OP::parent) {
    eval q[
sub B::OP::parent {
    my $op     = shift;
    my $parent = $op->_parent_impl( $op, "" );

    $parent;
}];
  } else {
    eval q[
sub B::OP::_parent {
    my $op     = shift;
    my $parent = $op->_parent_impl( $op, "" );
    $parent;
}];
  }
  if ($] >= 5.021002) {
    eval q[
sub B::NULL::kids { }
    ];
  }
}

sub B::NULL::_parent_impl { }

sub B::OP::_parent_impl {
    my ( $op, $target, $cx ) = @_;

    return if $cx =~ /\b$$op\b/;

    for ( $op->kids ) {
        if ( $$_ == $$target ) {
            return $op;
        }
    }

    return (
        $op->sibling->_parent_impl( $target, "$cx$$op S " )
            || (
              $cx =~ /^(?:\d+ S )*(?:\d+ N )*$/
            ? $op->next->_parent_impl( $target, "$cx$$op N " )
            : ()
            )
            || (
              $op->can('first')
            ? $op->first->_parent_impl( $target, "$cx$$op F " )
            : ()
            )
    );
}

=item C<$op-E<gt>ancestors>

Returns all parents of this node, recursively. The list is ordered
from younger/closer parents to older/farther parents.

=cut

sub B::OP::ancestors {
    my @nodes = shift;

    my $parent;
    push @nodes, $parent while $parent = $nodes[-1]->parent;
    shift @nodes;

    return @nodes;
}

=item C<$op-E<gt>descendants>

Returns all children of this node, recursively. The list is unordered.

=cut

sub B::OP::descendants {
    my $node = shift;
    my @nodes;
    walkoptree_simple( $node,
        sub { push @nodes, $_ if ${ $_[0] } != $$node } );
    return shuffle @nodes;
}

=item C<$op-E<gt>siblings>

Returns all younger siblings of this node. The list is ordered from
younger/closer siblings to older/farther siblings.

=cut

sub B::OP::siblings {
    my @siblings = $_[0];

    my $sibling;
    push @siblings, $siblings[-1]->sibling while $siblings[-1]->can('sibling');
    shift @siblings;

    # Remove any undefined or B::NULL objects
    pop @siblings while
        @siblings
        && !( defined $siblings[-1]
              && ${$siblings[-1]} );

    return @siblings;
}

=item C<$op-E<gt>previous>

Like C< $op-E<gt>next >, but not quite.

=cut

## sub B::OP::previous {
##     return unless defined wantarray;
##
##     my $target = shift;
##
##     my $start = $target;
##     my (%deadend, $search);
##     $search = sub {
##         my $node = $_[0];
##
##         unless ( defined $node ) {
##             # If I've been asked to search nothing, just return. The
##             # ->parent call might do this to me.
##             return _FALSE;
##         }
##         elsif ( exists $deadend{$node} ) {
##             # If this node has been seen already, try again as its
##             # parent.
##             return $search->( $node->parent );
##         }
##         elsif ( eval { ${$node->next} == $$target } ) {
##             return $node;
##         }
##
##         # When searching the children, do it in reverse order because
##         # pointers back up are more likely to be farther down the
##         # stack. This works without reversing but I can avoid some
##         # work by ordering the work this way.
##         my @kids = reverse $node->kids;
##
##         # Search this node's direct children for the ->next pointer
##         # that points to this node.
##         eval { ${$_->can('next')} == $$target } and return $_->next
##           for @kids;
##
##         # For each child, check it for a match.
##         my $found;
##         $found = $search->($_) and return $found
##           for @kids;
##
##         # Not in this subtree.
##         $deadend{$node} = _TRUE;
##         return _FALSE;
##     };
##
##     my $next = $target;
##     while ( eval { $next = $next->next } ) {
##         my $result;
##         $result = $search->( $next )
##           and return $result;
##     }
##
##     return _FALSE;
## }

=item C<$op-E<gt>stringify>

Returns a nice stringification of an opcode.

=cut

sub B::OP::stringify {
    my $op = shift;

    return sprintf "%s-%s=(0x%07x)", $op->name, class($op), $$op;
}

=item C<$op-E<gt>as_opgrep_pattern(%options)>

From the op tree it is called on, C<as_opgrep_pattern()>
generates a data structure suitable for use as a condition pattern
for the C<opgrep()> function described below in detail.
I<Beware>: When using such generated patterns, there may be
false positives: The pattern will most likely not match I<only>
the op tree it was generated from since by default, not all properties
of the op are reproduced.

You can control which properties of the op to include in the pattern
by passing named arguments. The default behaviour is as if you
passed in the following options:

  my $pattern = $op->as_opgrep_pattern(
    attributes          => [qw(name flags)],
    max_recursion_depth => undef,
  );

So obviously, you can set C<max_recursion_depth> to a number to
limit the maximum depth of recursion into the op tree. Setting
it to C<0> will limit the dump to the current op.

C<attributes> is a list of attributes to include in the produced
pattern. The attributes that can be checked against in this way
are:

  name targ type seq flags private pmflags pmpermflags.

=cut

sub B::OP::as_opgrep_pattern {
  my $op = shift;
  my $opt = (@_ == 1 and ref($_[0]) eq 'HASH') ? shift() : {@_};

  my $attribs = $opt->{attributes};
  $attribs ||= [qw(name flags)];

  my $pattern = {};
  foreach my $attr (@$attribs) {
    $pattern->{$attr} = $op->$attr() if $op->can($attr);
  }

  my $recursion_limit = $opt->{max_recursion_depth};
  if ( (not defined $recursion_limit or $recursion_limit > 0)
       and ref($op)
       and $$op
       and $op->flags & OPf_KIDS
  ) {
    $opt->{max_recursion_depth}-- if defined $recursion_limit;

    $pattern->{kids} = [
      map { $_->as_opgrep_pattern($opt) } $op->kids()
    ];
  }

  # reset the option structure in case we got a hash ref passed in.
  $opt->{max_recursion_depth} = $recursion_limit
    if exists $opt->{max_recursion_depth};

  return $pattern;
}

=back

=head1 EXPORTABLE FUNCTIONS

=over 4

=item C<all_starts>

=item C<all_roots>

Returns a hash of all of the starting ops or root ops of optrees, keyed
to subroutine name; the optree for main program is simply keyed to C<__MAIN__>.

B<Note>: Certain "dangerous" stashes are not scanned for subroutines:
the list of such stashes can be found in
C<@B::Utils::bad_stashes>. Feel free to examine and/or modify this to
suit your needs. The intention is that a simple program which uses no
modules other than C<B> and C<B::Utils> would show no addition
symbols.

This does B<not> return the details of ops in anonymous subroutines
compiled at compile time. For instance, given

    $a = sub { ... };

the subroutine will not appear in the hash. This is just as well,
since they're anonymous... If you want to get at them, use...

=cut

my ( %starts, %roots );
sub all_starts { _init_sub_cache(); wantarray ? %starts : \%starts }
sub all_roots  { _init_sub_cache(); wantarray ? %roots  : \%roots }

=item C<anon_subs>

This returns an array of hash references. Each element has the keys
"start" and "root". These are the starting and root ops of all of the
anonymous subroutines in the program.

=cut

my @anon_subs;
sub anon_subs { _init_sub_cache(); wantarray ? @anon_subs : \@anon_subs }

=item C<recalc_sub_cache>

If PL_sub_generation has changed or you have some other reason to want
to force the re-examination of the optrees, everywhere, call this
function.

=cut

my $subs_cached = _FALSE;

sub recalc_sub_cache {
    $subs_cached = _FALSE;

    %starts = %roots = @anon_subs = ();

    _init_sub_cache();
    return;
}

sub _init_sub_cache {

    # Allow this function to be run only once.
    return if $subs_cached;

    %starts = ( __MAIN__ => main_start() );
    %roots  = ( __MAIN__ => main_root() );

    # Through the magic of B::'s ugly callback system, %starts and
    # %roots will be populated.
    walksymtable(
        \%main::,
        _B_Utils_init_sub_cache => sub {

            # Do not eat our own children!
            $_[0] eq "$_\::" && return _FALSE for @bad_stashes;

            return _TRUE;
        },
        ''
    );

    # Some sort of file-scoped anonymous code refs are found here. In
    # general, when a function has anonymous functions, they can be
    # found in the scratchpad.
    push @anon_subs,
        map( (
            'CV' eq class($_)
            ? { root  => $_->ROOT,
                start => $_->START
                }
            : ()
        ),
        main_cv()->PADLIST->ARRAY->ARRAY );

    $subs_cached = _TRUE;
    return;
}

sub B::GV::_B_Utils_init_sub_cache {

    # This is a callback function called from B::Utils::_init via
    # B::walksymtable.

    my $gv = shift;
    my $cv = $gv->CV;

    # If the B::CV object is a pointer to nothing, ignore it.
    return unless $$cv;

    # Simon was originally using $gv->SAFENAME but I don't think
    # that's a "correct" decision because then oddly named functions
    # can't be disambiguated. If a function were actually named ^G, I
    # couldn't tell it apart from one named after the control
    # character ^G.
    my $name = $gv->STASH->NAME . "::" . $gv->NAME;

    # When does a CV not fulfill ->ARRAY->ARRAY? Some time during
    # initialization?
    if (    $cv->can('PADLIST')
        and $cv->PADLIST->can('ARRAY')
        and $cv->PADLIST->ARRAY->can('ARRAY') )
    {
        push @anon_subs,
            map( (
                'CV' eq class($_)
                ? { root  => $_->ROOT,
                    start => $_->START
                    }
                : ()
            ),
            $cv->PADLIST->ARRAY->ARRAY );
    }

    return unless ( ( my $start = $cv->START )
        and ( my $root = $cv->ROOT ) );

    $starts{$name} = $start;
    $roots{$name}  = $root;

    #    return _TRUE;
    return;
}

# sub B::SPECIAL::_B_Utils_init_sub_cache {
#
#     # This is a callback function called from B::Utils::_init via
#     # B::walksymtable.
#
#     # JJ: I'm not sure why this callback function exists.
#
#     return _TRUE;
# }

=item C<walkoptree_simple($op, \&callback, [$data])>

The C<B> module provides various functions to walk the op tree, but
they're all rather difficult to use, requiring you to inject methods
into the C<B::OP> class. This is a very simple op tree walker with
more expected semantics.

All the C<walk> functions set C<$B::Utils::file>, C<$B::Utils::line>,
and C<$B::Utils::sub> to the appropriate values of file, line number,
and sub name in the program being examined.

=cut

$B::Utils::file = '__none__';
$B::Utils::line = 0;
$B::Utils::sub  = undef;

sub walkoptree_simple {
    $B::Utils::file = '__none__';
    $B::Utils::line = 0;

    _walkoptree_simple( {}, @_ );

    return _TRUE;
}

sub _walkoptree_simple {
    my ( $visited, $op, $callback, $data ) = @_;

    return if $visited->{$$op}++;

    if ( ref $op and $op->isa("B::COP") ) {
        $B::Utils::file = $op->file;
        $B::Utils::line = $op->line;
    }

    $callback->( $op, $data );
    return if $op->isa('B::NULL');
    if ( $op->flags & OPf_KIDS ) {
        # for (my $kid = $op->first; $$kid; $kid = $kid->sibling) {
        #     _walkoptree_simple( $visited, $kid, $callback, $data );
        # }
        _walkoptree_simple( $visited, $_, $callback, $data ) for $op->kids;
    }
    if ( $op->isa('B::PMOP') ) {
        my $maybe_root = $op->pmreplroot;
        if (ref($maybe_root) and $maybe_root->isa("B::OP")) {
            # It really is the root of the replacement, not something
            # else stored here for lack of space elsewhere
            _walkoptree_simple( $visited, $maybe_root, $callback, $data );
        }
    }

    return;

}

=item C<walkoptree_filtered($op, \&filter, \&callback, [$data])>

This is much the same as C<walkoptree_simple>, but will only call the
callback if the C<filter> returns true. The C<filter> is passed the
op in question as a parameter; the C<opgrep> function is fantastic
for building your own filters.

=cut

sub walkoptree_filtered {
    $B::Utils::file = '__none__';
    $B::Utils::line = 0;

    _walkoptree_filtered( {}, @_ );;

    return _TRUE;
}

sub _walkoptree_filtered {
    my ( $visited, $op, $filter, $callback, $data ) = @_;

    if ( $op->isa("B::COP") ) {
        $B::Utils::file = $op->file;
        $B::Utils::line = $op->line;
    }

    $callback->( $op, $data ) if $filter->($op);

    if (    ref $op
        and $$op
        and $op->flags & OPf_KIDS )
    {

        my $kid = $op->first;
        while ( ref $kid
            and $$kid )
        {
            _walkoptree_filtered( $visited, $kid, $filter, $callback, $data );

            $kid = $kid->sibling;
        }
    }

    return _TRUE;
}

=item C<walkallops_simple(\&callback, [$data])>

This combines C<walkoptree_simple> with C<all_roots> and C<anon_subs>
to examine every op in the program. C<$B::Utils::sub> is set to the
subroutine name if you're in a subroutine, C<__MAIN__> if you're in
the main program and C<__ANON__> if you're in an anonymous subroutine.

=cut

sub walkallops_simple {
    $B::Utils::sub = undef;

    &_walkallops_simple;

    return _TRUE;
}

sub _walkallops_simple {
    my ( $callback, $data ) = @_;

    _init_sub_cache();

    for my $sub_name (sort keys %roots) {
	$B::Utils::sub = $sub_name;
	my $root = $roots{$sub_name};
	walkoptree_simple( $root, $callback, $data );
    }

    $B::Utils::sub = "__ANON__";
    walkoptree_simple( $_->{root}, $callback, $data ) for @anon_subs;

    return _TRUE;
}

=item C<walkallops_filtered(\&filter, \&callback, [$data])>

Same as above, but filtered.

=cut

sub walkallops_filtered {
    $B::Utils::sub = undef;

    &_walkallops_filtered;

    return _TRUE;
}

sub _walkallops_filtered {
    my ( $filter, $callback, $data ) = @_;

    _init_sub_cache();

    walkoptree_filtered( $_, $filter, $callback, $data ) for values %roots;

    $B::Utils::sub = "__ANON__";

    walkoptree_filtered( $_->{root}, $filter, $callback, $data )
        for @anon_subs;

    return _TRUE;
}

=item C<opgrep(\%conditions, @ops)>

Returns the ops which meet the given conditions. The conditions should
be specified like this:

    @barewords = opgrep(
                        { name => "const", private => OPpCONST_BARE },
                        @ops
                       );

where the first argument to C<opgrep()> is the condition to be matched against the
op structure. We'll henceforth refer to it as an op-pattern.

You can specify alternation by giving an arrayref of values:

    @svs = opgrep ( { name => ["padsv", "gvsv"] }, @ops)

And you can specify inversion by making the first element of the
arrayref a "!". (Hint: if you want to say "anything", say "not
nothing": C<["!"]>)

You may also specify the conditions to be matched in nearby ops as nested patterns.

    walkallops_filtered(
        sub { opgrep( {name => "exec",
                       next => {
                                 name    => "nextstate",
                                 sibling => { name => [qw(! exit warn die)] }
                               }
                      }, @_)},
        sub {
              carp("Statement unlikely to be reached");
              carp("\t(Maybe you meant system() when you said exec()?)\n");
        }
    )

Get that?

Here are the things that can be tested in this way:

        name targ type seq flags private pmflags pmpermflags
        first other last sibling next pmreplroot pmreplstart pmnext

Additionally, you can use the C<kids> keyword with an array reference
to match the result of a call to C<$op-E<gt>kids()>. An example use is
given in the documentation for C<op_or> below.

For debugging, you can have many properties of an op that is currently being
matched against a given condition dumped to STDERR
by specifying C<dump => 1> in the condition's hash reference.

If you match a complex condition against an op tree, you may want to extract
a specific piece of information from the tree if the condition matches.
This normally entails manually walking the tree a second time down to
the op you wish to extract, investigate or modify. Since this is tedious
duplication of code and information, you can specify a special property
in the pattern of the op you wish to extract to capture the sub-op
of interest. Example:

  my ($result) = opgrep(
    { name => "exec",
      next => { name    => "nextstate",
                sibling => { name => [qw(! exit warn die)]
                             capture => "notreached",
                           },
              }
    },
    $root_op
  );

  if ($result) {
    my $name = $result->{notreached}->name; # result is *not* the root op
    carp("Statement unlikely to be reached (op name: $name)");
    carp("\t(Maybe you meant system() when you said exec()?)\n");
  }

While the above is a terribly contrived example, consider the win for a
deeply nested pattern or worse yet, a pattern with many disjunctions.
If a C<capture> property is found anywhere in
the op pattern, C<opgrep()> returns an unblessed hash reference on success
instead of the tested op. You can tell them apart using L<Scalar::Util>'s
C<blessed()>. That hash reference contains all captured ops plus the
tested root up as the hash entry C<$result-E<gt>{op}>. Note that you cannot
use this feature with C<walkoptree_filtered> since that function was
specifically documented to pass the tested op itself to the callback.

You cannot capture disjunctions, but that doesn't really make sense anyway.

=item C<opgrep( \@conditions, @ops )>

Same as above, except that you don't have to chain the conditions
yourself.  If you pass an array-ref, opgrep will chain the conditions
for you using C<next>.
The conditions can either be strings (taken as op-names), or
hash-refs, with the same testable conditions as given above.

=cut

sub opgrep {
    return unless defined wantarray;

    my $conds_ref = shift;
    $conds_ref = _opgrep_helper($conds_ref)
        if 'ARRAY' eq ref $conds_ref;

    my @grep_ops;

    # Check whether we're dealing with a disjunction of patterns:
    my @conditions = exists($conds_ref->{disjunction}) ? @{$conds_ref->{disjunction}} : ($conds_ref);

OP:
    for my $op (@_) {
        next unless ref $op and $$op;

        # only one condition by default, but if we have a disjunction, there will
        # be several
CONDITION:
        foreach my $condition (@conditions) {
            # nested disjunctions? naughty user!
            # $foo or ($bar or $baz) is $foo or $bar or $baz!
            # ==> flatten
            if (exists($condition->{disjunction})) {
              push @conditions, @{$condition->{disjunction}};
              next CONDITION;
            }

            # structure to hold captured information
            my $capture = {};

            # Debugging aid
            if (exists $condition->{'dump'}) {
                ($op->can($_)
                or next)
                and warn "$_: " . $op->$_ . "\n"
                for
                qw( first other last pmreplroot pmreplstart pmnext pmflags pmpermflags name targ type seq flags private kids);
            }

            # special disjunction case. undef in a disjunction => (child) does not exist
            if (not defined $condition) {
                return _TRUE if not defined $op and not wantarray();
                return();
            }

            # save the op if the user wants flat access to it
            if ($condition->{capture}) {
                $capture->{ $condition->{capture} } = $op;
            }

            # First, let's skim off ops of the wrong type. If they require
            # something that isn't implemented for this kind of object, it
            # must be wrong. These tests are cheap
            exists $condition->{$_}
                and !$op->can($_)
                and next
                for
                qw( first other last pmreplroot pmreplstart pmnext pmflags pmpermflags name targ type seq flags private kids);

#            # Check alternations
#            (   ref( $condition->{$_} )
#                ? ( "!" eq $condition->{$_}[0]
#                    ? ()
#                    : ()
#                    )
#                : ( $op->can($_) && $op->$_ eq $condition->{$_} or next )
#                )
#                for qw( name targ type seq flags private pmflags pmpermflags );

            for my $test (
                qw(name targ type seq flags private pmflags pmpermflags))
            {
                next unless exists $condition->{$test};
                my $val = $op->$test;

                if ( 'ARRAY' eq ref $condition->{$test} ) {

                    # Test a list of valid/invalid values.
                    if ( '!' eq $condition->{$test}[0] ) {

                        # Fail if any entries match.
                        $_ ne $val
                            or next CONDITION
                            for @{ $condition->{$test} }
                            [ 1 .. $#{ $condition->{$test} } ];
                    }
                    else {

                        # Fail if no entries match.
                        my $okay = 0;

                        $_ eq $val and $okay = 1, last
                            for @{ $condition->{$test} };

                        next CONDITION if not $okay;
                    }
                }
                elsif ( 'CODE' eq ref $condition->{$test} ) {
                    local $_ = $val;
                    $condition->{$test}($op)
                        or next CONDITION;
                }
                else {

                    # Test a single value.
                    $condition->{$test} eq $op->$test
                        or next CONDITION;
                }
            } # end for test

            # We know it ->can because that was tested above. It is an
            # error to have anything in this list of tests that isn't
            # tested for ->can above.
            foreach (
              qw( first other last sibling next pmreplroot pmreplstart pmnext )
              ) {
                next unless exists $condition->{$_};
                my ($result) = opgrep( $condition->{$_}, $op->$_ );
                next CONDITION if not $result;

                if (not blessed($result)) {
                    # copy over the captured data/ops from the recursion
                    $capture->{$_} = $result->{$_} foreach keys %$result;
                }
            }

            # Apply all kids conditions. We $op->can(kids) (see above).
            if (exists $condition->{kids}) {
                my $kidno = 0;
                my $kidconditions = $condition->{kids};

                next CONDITION if not @{$kidconditions} == @{$condition->{kids}};

                foreach my $kid ($op->kids()) {
                    # if you put undef in your kid conditions list, we skip one kid
                    next if not defined $kidconditions->[$kidno];

                    my ($result) = opgrep( $kidconditions->[$kidno++], $kid );
                    next CONDITION if not $result;

                    if (not blessed($result)) {
                        # copy over the captured data/ops from the recursion
                        $capture->{$_} = $result->{$_} foreach keys %$result;
                    }
                }
            }

            # Attempt to quit early if possible.
            if (wantarray) {
                if (keys %$capture) {
                    # save all captured information and the main op
                    $capture->{op} = $op;
                    push @grep_ops, $capture;
                }
                else {
                    # save main op
                    push @grep_ops, $op;
                }
                last;
            }
            elsif ( defined wantarray ) {
                return _TRUE;
            }
        } # end for @conditions
        # end of conditions loop should be end of op test
    }

    # Either this was called in list context and then I want to just
    # return everything possible or this is in scalar/void context and
    # @grep_ops will be empty and thus "false."
    return @grep_ops;
}

sub _opgrep_helper {
    my @conds =
        map ref() ? {%$_} : { name => $_ }, @{ $_[0] };

    # Wire this into a list of entries, all ->next
    for ( 1 .. $#conds ) {
        $conds[ $_ - 1 ]{next} = $conds[$_];
    }

    # This is a linked list now so I can return only the head.
    return $conds[0];
}

=item C<op_or( @conditions )>

Unlike the chaining of conditions done by C<opgrep> itself if there are multiple
conditions, this function creates a disjunction (C<$cond1 || $cond2 || ...>) of
the conditions and returns a structure (hash reference) that can be passed to
opgrep as a single condition.

Example:

  my $sub_structure = {
    name => 'helem',
    first => { name => 'rv2hv', },
    'last' => { name => 'const', },
  };

  my @ops = opgrep( {
      name => 'leavesub',
      first => {
        name => 'lineseq',
        kids => [,
          { name => 'nextstate', },
          op_or(
            {
              name => 'return',
              first => { name => 'pushmark' },
              last => $sub_structure,
            },
            $sub_structure,
          ),
        ],
      },
  }, $op_obj );

This example matches the code in a typical simplest-possible
accessor method (albeit not down to the last bit):

  sub get_foo { $_[0]->{foo} }

But by adding an alternation
we can also match optional op layers. In this case, we optionally
match a return statement, so the following implementation is also
recognized:

  sub get_foo { return $_[0]->{foo} }

Essentially, this is syntactic sugar for the following structure
recognized by C<opgrep()>:

  { disjunction => [@conditions] }

=cut

sub op_or {
  my @conditions = @_;
  return({ disjunction => [@conditions] });
}

# TODO
# sub op_pattern_match {
#   my $op = shift;
#   my $pattern = shift;
#
#   my $ret = {};
#
#
#   return $ret;
# }

=item C<carp(@args)>

=item C<croak(@args)>

Warn and die, respectively, from the perspective of the position of
the op in the program. Sounds complicated, but it's exactly the kind
of error reporting you expect when you're grovelling through an op
tree.

=cut

sub carp (@)  { CORE::warn( _preparewarn(@_) ) }
sub croak (@) { CORE::die( _preparewarn(@_) ) }

sub _preparewarn {
    my $args = join '', @_;
    $args = "Something's wrong " unless $args;
    if ( "\n" ne substr $args, -1, 1 ) {
        $args .= " at $B::Utils::file line $B::Utils::line.\n";
    }
    return $args;
}

=back

=head2 EXPORT

None by default.

=head2 XS EXPORT

This modules uses L<ExtUtils::Depends> to export some useful functions
for XS modules to use.  To use those, include in your Makefile.PL:

  my $pkg = ExtUtils::Depends->new("Your::XSModule", "B::Utils");
  WriteMakefile(
    ... # your normal makefile flags
    $pkg->get_makefile_vars,
  );

Your XS module can now include F<BUtils.h> and F<BUtils_op.h>.  To see
document for the functions provided, use:

  perldoc -m B::Utils::Install::BUtils.h
  perldoc -m B::Utils::Install::BUtils_op.h

=head1 AUTHOR

Originally written by Simon Cozens, C<simon@cpan.org>
Maintained by Joshua ben Jore, C<jjore@cpan.org>

Contributions from Mattia Barbon, Jim Cromie, Steffen Mueller, and
Chia-liang Kao, Alexandr Ciornii, Reini Urban.

=head1 LICENSE

This module is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

=head1 SEE ALSO

L<B>, L<B::Generate>.

=cut

"Wow, you're pretty uptight for a guy who worships a multi-armed,
hermaphrodite embodiment of destruction who has a fetish for vaguely
phallic shaped headgear.";
