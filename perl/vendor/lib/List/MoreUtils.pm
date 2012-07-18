package List::MoreUtils;

use 5.00503;
use strict;
use Exporter   ();
use DynaLoader ();

use vars qw{ $VERSION @ISA @EXPORT_OK %EXPORT_TAGS };
BEGIN {
    $VERSION   = '0.33';
    # $VERSION   = eval $VERSION;
    @ISA       = qw{ Exporter DynaLoader };
    @EXPORT_OK = qw{
        any all none notall true false
        firstidx first_index lastidx last_index
        insert_after insert_after_string
        apply indexes
        after after_incl before before_incl
        firstval first_value lastval last_value
        each_array each_arrayref
        pairwise natatime
        mesh zip uniq distinct
        minmax part
    };
    %EXPORT_TAGS = (
        all => \@EXPORT_OK,
    );

    # Load the XS at compile-time so that redefinition warnings will be
    # thrown correctly if the XS versions of part or indexes loaded
    eval {
        # PERL_DL_NONLAZY must be false, or any errors in loading will just
        # cause the perl code to be tested
        local $ENV{PERL_DL_NONLAZY} = 0 if $ENV{PERL_DL_NONLAZY};

        bootstrap List::MoreUtils $VERSION;
        1;

    } unless $ENV{LIST_MOREUTILS_PP};
}

eval <<'END_PERL' unless defined &any;

# Use pure scalar boolean return values for compatibility with XS
use constant YES => ! 0;
use constant NO  => ! 1;

sub any (&@) {
    my $f = shift;
    foreach ( @_ ) {
        return YES if $f->();
    }
    return NO;
}

sub all (&@) {
    my $f = shift;
    foreach ( @_ ) {
        return NO unless $f->();
    }
    return YES;
}

sub none (&@) {
    my $f = shift;
    foreach ( @_ ) {
        return NO if $f->();
    }
    return YES;
}

sub notall (&@) {
    my $f = shift;
    foreach ( @_ ) {
        return YES unless $f->();
    }
    return NO;
}

sub true (&@) {
    my $f     = shift;
    my $count = 0;
    foreach ( @_ ) {
        $count++ if $f->();
    }
    return $count;
}

sub false (&@) {
    my $f     = shift;
    my $count = 0;
    foreach ( @_ ) {
        $count++ unless $f->();
    }
    return $count;
}

sub firstidx (&@) {
    my $f = shift;
    foreach my $i ( 0 .. $#_ ) {
        local *_ = \$_[$i];
        return $i if $f->();
    }
    return -1;
}

sub lastidx (&@) {
    my $f = shift;
    foreach my $i ( reverse 0 .. $#_ ) {
        local *_ = \$_[$i];
        return $i if $f->();
    }
    return -1;
}

sub insert_after (&$\@) {
    my ($f, $val, $list) = @_;
    my $c = -1;
    local *_;
    foreach my $i ( 0 .. $#$list ) {
        $_ = $list->[$i];
        $c = $i, last if $f->();
    }
    @$list = (
        @{$list}[ 0 .. $c ],
        $val,
        @{$list}[ $c + 1 .. $#$list ],
    ) and return 1 if $c != -1;
    return 0;
}

sub insert_after_string ($$\@) {
    my ($string, $val, $list) = @_;
    my $c = -1;
    foreach my $i ( 0 .. $#$list ) {
        local $^W = 0;
        $c = $i, last if $string eq $list->[$i];
    }
    @$list = (
        @{$list}[ 0 .. $c ],
        $val,
        @{$list}[ $c + 1 .. $#$list ],
    ) and return 1 if $c != -1;
    return 0;
}

sub apply (&@) {
    my $action = shift;
    &$action foreach my @values = @_;
    wantarray ? @values : $values[-1];
}

sub after (&@) {
    my $test = shift;
    my $started;
    my $lag;
    grep $started ||= do {
        my $x = $lag;
        $lag = $test->();
        $x
    }, @_;
}

sub after_incl (&@) {
    my $test = shift;
    my $started;
    grep $started ||= $test->(), @_;
}

sub before (&@) {
    my $test = shift;
    my $more = 1;
    grep $more &&= ! $test->(), @_;
}

sub before_incl (&@) {
    my $test = shift;
    my $more = 1;
    my $lag  = 1;
    grep $more &&= do {
        my $x = $lag;
        $lag = ! $test->();
        $x
    }, @_;
}

sub indexes (&@) {
    my $test = shift;
    grep {
        local *_ = \$_[$_];
        $test->()
    } 0 .. $#_;
}

sub lastval (&@) {
    my $test = shift;
    my $ix;
    for ( $ix = $#_; $ix >= 0; $ix-- ) {
        local *_ = \$_[$ix];
        my $testval = $test->();

        # Simulate $_ as alias
        $_[$ix] = $_;
        return $_ if $testval;
    }
    return undef;
}

sub firstval (&@) {
    my $test = shift;
    foreach ( @_ ) {
        return $_ if $test->();
    }
    return undef;
}

sub pairwise (&\@\@) {
    my $op = shift;

    # Symbols for caller's input arrays
    use vars qw{ @A @B };
    local ( *A, *B ) = @_;

    # Localise $a, $b
    my ( $caller_a, $caller_b ) = do {
        my $pkg = caller();
        no strict 'refs';
        \*{$pkg.'::a'}, \*{$pkg.'::b'};
    };

    # Loop iteration limit
    my $limit = $#A > $#B? $#A : $#B;

    # This map expression is also the return value
    local( *$caller_a, *$caller_b );
    map {
        # Assign to $a, $b as refs to caller's array elements
        ( *$caller_a, *$caller_b ) = \( $A[$_], $B[$_] );

        # Perform the transformation
        $op->();
    }  0 .. $limit;
}

sub each_array (\@;\@\@\@\@\@\@\@\@\@\@\@\@\@\@\@\@\@\@\@\@\@\@\@\@) {
    return each_arrayref(@_);
}

sub each_arrayref {
    my @list  = @_; # The list of references to the arrays
    my $index = 0;  # Which one the caller will get next
    my $max   = 0;  # Number of elements in longest array

    # Get the length of the longest input array
    foreach ( @list ) {
        unless ( ref $_ eq 'ARRAY' ) {
            require Carp;
            Carp::croak("each_arrayref: argument is not an array reference\n");
        }
        $max = @$_ if @$_ > $max;
    }

    # Return the iterator as a closure wrt the above variables.
    return sub {
        if ( @_ ) {
            my $method = shift;
            unless ( $method eq 'index' ) {
                require Carp;
                Carp::croak("each_array: unknown argument '$method' passed to iterator.");
            }

            # Return current (last fetched) index
            return undef if $index == 0  ||  $index > $max;
            return $index - 1;
        }

        # No more elements to return
        return if $index >= $max;
        my $i = $index++;

        # Return ith elements
        return map $_->[$i], @list; 
    }
}

sub natatime ($@) {
    my $n    = shift;
    my @list = @_;
    return sub {
        return splice @list, 0, $n;
    }
}

sub mesh (\@\@;\@\@\@\@\@\@\@\@\@\@\@\@\@\@\@\@\@\@\@\@\@\@\@\@\@\@\@\@\@\@) {
    my $max = -1;
    $max < $#$_ && ( $max = $#$_ ) foreach @_;
    map {
        my $ix = $_;
        map $_->[$ix], @_;
    } 0 .. $max; 
}

sub uniq (@) {
    my %seen = ();
    grep { not $seen{$_}++ } @_;
}

sub minmax (@) {
    return unless @_;
    my $min = my $max = $_[0];

    for ( my $i = 1; $i < @_; $i += 2 ) {
        if ( $_[$i-1] <= $_[$i] ) {
            $min = $_[$i-1] if $min > $_[$i-1];
            $max = $_[$i]   if $max < $_[$i];
        } else {
            $min = $_[$i]   if $min > $_[$i];
            $max = $_[$i-1] if $max < $_[$i-1];
        }
    }

    if ( @_ & 1 ) {
        my $i = $#_;
        if ($_[$i-1] <= $_[$i]) {
            $min = $_[$i-1] if $min > $_[$i-1];
            $max = $_[$i]   if $max < $_[$i];
        } else {
            $min = $_[$i]   if $min > $_[$i];
            $max = $_[$i-1] if $max < $_[$i-1];
        }
    }

    return ($min, $max);
}

sub part (&@) {
    my ($code, @list) = @_;
    my @parts;
    push @{ $parts[ $code->($_) ] }, $_  foreach @list;
    return @parts;
}

sub _XScompiled {
    return 0;
}

END_PERL
die $@ if $@;

# Function aliases
*first_index = \&firstidx;
*last_index  = \&lastidx;
*first_value = \&firstval;
*last_value  = \&lastval;
*zip         = \&mesh;
*distinct    = \&uniq;

1;

__END__

=pod

=head1 NAME

List::MoreUtils - Provide the stuff missing in List::Util

=head1 SYNOPSIS

    use List::MoreUtils qw{
        any all none notall true false
        firstidx first_index lastidx last_index
        insert_after insert_after_string
        apply indexes
        after after_incl before before_incl
        firstval first_value lastval last_value
        each_array each_arrayref
        pairwise natatime
        mesh zip uniq distinct minmax part
    };

=head1 DESCRIPTION

B<List::MoreUtils> provides some trivial but commonly needed functionality on
lists which is not going to go into L<List::Util>.

All of the below functions are implementable in only a couple of lines of Perl
code. Using the functions from this module however should give slightly better
performance as everything is implemented in C. The pure-Perl implementation of
these functions only serves as a fallback in case the C portions of this module
couldn't be compiled on this machine.

=over 4

=item any BLOCK LIST

Returns a true value if any item in LIST meets the criterion given through
BLOCK. Sets C<$_> for each item in LIST in turn:

    print "At least one value undefined"
        if any { ! defined($_) } @list;

Returns false otherwise, or if LIST is empty.

=item all BLOCK LIST

Returns a true value if all items in LIST meet the criterion given through
BLOCK, or if LIST is empty. Sets C<$_> for each item in LIST in turn:

    print "All items defined"
        if all { defined($_) } @list;

Returns false otherwise.

=item none BLOCK LIST

Logically the negation of C<any>. Returns a true value if no item in LIST meets
the criterion given through BLOCK, or if LIST is empty. Sets C<$_> for each item
in LIST in turn:

    print "No value defined"
        if none { defined($_) } @list;

Returns false otherwise.

=item notall BLOCK LIST

Logically the negation of C<all>. Returns a true value if not all items in LIST
meet the criterion given through BLOCK. Sets C<$_> for each item in LIST in
turn:

    print "Not all values defined"
        if notall { defined($_) } @list;

Returns false otherwise, or if LIST is empty.

=item true BLOCK LIST

Counts the number of elements in LIST for which the criterion in BLOCK is true.
Sets C<$_> for  each item in LIST in turn:

    printf "%i item(s) are defined", true { defined($_) } @list;

=item false BLOCK LIST

Counts the number of elements in LIST for which the criterion in BLOCK is false.
Sets C<$_> for each item in LIST in turn:

    printf "%i item(s) are not defined", false { defined($_) } @list;

=item firstidx BLOCK LIST

=item first_index BLOCK LIST

Returns the index of the first element in LIST for which the criterion in BLOCK
is true. Sets C<$_> for each item in LIST in turn:

    my @list = (1, 4, 3, 2, 4, 6);
    printf "item with index %i in list is 4", firstidx { $_ == 4 } @list;
    __END__
    item with index 1 in list is 4
    
Returns C<-1> if no such item could be found.

C<first_index> is an alias for C<firstidx>.

=item lastidx BLOCK LIST

=item last_index BLOCK LIST

Returns the index of the last element in LIST for which the criterion in BLOCK
is true. Sets C<$_> for each item in LIST in turn:

    my @list = (1, 4, 3, 2, 4, 6);
    printf "item with index %i in list is 4", lastidx { $_ == 4 } @list;
    __END__
    item with index 4 in list is 4

Returns C<-1> if no such item could be found.

C<last_index> is an alias for C<lastidx>.

=item insert_after BLOCK VALUE LIST

Inserts VALUE after the first item in LIST for which the criterion in BLOCK is
true. Sets C<$_> for each item in LIST in turn.

    my @list = qw/This is a list/;
    insert_after { $_ eq "a" } "longer" => @list;
    print "@list";
    __END__
    This is a longer list

=item insert_after_string STRING VALUE LIST

Inserts VALUE after the first item in LIST which is equal to STRING. 

    my @list = qw/This is a list/;
    insert_after_string "a", "longer" => @list;
    print "@list";
    __END__
    This is a longer list

=item apply BLOCK LIST

Applies BLOCK to each item in LIST and returns a list of the values after BLOCK
has been applied. In scalar context, the last element is returned.  This
function is similar to C<map> but will not modify the elements of the input
list:

    my @list = (1 .. 4);
    my @mult = apply { $_ *= 2 } @list;
    print "\@list = @list\n";
    print "\@mult = @mult\n";
    __END__
    @list = 1 2 3 4
    @mult = 2 4 6 8

Think of it as syntactic sugar for

    for (my @mult = @list) { $_ *= 2 }

=item before BLOCK LIST

Returns a list of values of LIST upto (and not including) the point where BLOCK
returns a true value. Sets C<$_> for each element in LIST in turn.

=item before_incl BLOCK LIST

Same as C<before> but also includes the element for which BLOCK is true.

=item after BLOCK LIST

Returns a list of the values of LIST after (and not including) the point
where BLOCK returns a true value. Sets C<$_> for each element in LIST in turn.

    @x = after { $_ % 5 == 0 } (1..9);    # returns 6, 7, 8, 9

=item after_incl BLOCK LIST

Same as C<after> but also inclues the element for which BLOCK is true.

=item indexes BLOCK LIST

Evaluates BLOCK for each element in LIST (assigned to C<$_>) and returns a list
of the indices of those elements for which BLOCK returned a true value. This is
just like C<grep> only that it returns indices instead of values:

    @x = indexes { $_ % 2 == 0 } (1..10);   # returns 1, 3, 5, 7, 9

=item firstval BLOCK LIST

=item first_value BLOCK LIST

Returns the first element in LIST for which BLOCK evaluates to true. Each
element of LIST is set to C<$_> in turn. Returns C<undef> if no such element
has been found.

C<first_val> is an alias for C<firstval>.

=item lastval BLOCK LIST

=item last_value BLOCK LIST

Returns the last value in LIST for which BLOCK evaluates to true. Each element
of LIST is set to C<$_> in turn. Returns C<undef> if no such element has been
found.

C<last_val> is an alias for C<lastval>.

=item pairwise BLOCK ARRAY1 ARRAY2

Evaluates BLOCK for each pair of elements in ARRAY1 and ARRAY2 and returns a
new list consisting of BLOCK's return values. The two elements are set to C<$a>
and C<$b>.  Note that those two are aliases to the original value so changing
them will modify the input arrays.

    @a = (1 .. 5);
    @b = (11 .. 15);
    @x = pairwise { $a + $b } @a, @b;	# returns 12, 14, 16, 18, 20

    # mesh with pairwise
    @a = qw/a b c/;
    @b = qw/1 2 3/;
    @x = pairwise { ($a, $b) } @a, @b;	# returns a, 1, b, 2, c, 3

=item each_array ARRAY1 ARRAY2 ...

Creates an array iterator to return the elements of the list of arrays ARRAY1,
ARRAY2 throughout ARRAYn in turn.  That is, the first time it is called, it
returns the first element of each array.  The next time, it returns the second
elements.  And so on, until all elements are exhausted.

This is useful for looping over more than one array at once:

    my $ea = each_array(@a, @b, @c);
    while ( my ($a, $b, $c) = $ea->() )   { .... }

The iterator returns the empty list when it reached the end of all arrays.

If the iterator is passed an argument of 'C<index>', then it retuns
the index of the last fetched set of values, as a scalar.

=item each_arrayref LIST

Like each_array, but the arguments are references to arrays, not the
plain arrays.

=item natatime EXPR, LIST

Creates an array iterator, for looping over an array in chunks of
C<$n> items at a time.  (n at a time, get it?).  An example is
probably a better explanation than I could give in words.

Example:

    my @x = ('a' .. 'g');
    my $it = natatime 3, @x;
    while (my @vals = $it->())
    {
        print "@vals\n";
    }

This prints

    a b c
    d e f
    g

=item mesh ARRAY1 ARRAY2 [ ARRAY3 ... ]

=item zip ARRAY1 ARRAY2 [ ARRAY3 ... ]

Returns a list consisting of the first elements of each array, then
the second, then the third, etc, until all arrays are exhausted.

Examples:

    @x = qw/a b c d/;
    @y = qw/1 2 3 4/;
    @z = mesh @x, @y;	    # returns a, 1, b, 2, c, 3, d, 4

    @a = ('x');
    @b = ('1', '2');
    @c = qw/zip zap zot/;
    @d = mesh @a, @b, @c;   # x, 1, zip, undef, 2, zap, undef, undef, zot

C<zip> is an alias for C<mesh>.

=item uniq LIST

=item distinct LIST

Returns a new list by stripping duplicate values in LIST. The order of
elements in the returned list is the same as in LIST. In scalar context,
returns the number of unique elements in LIST.

    my @x = uniq 1, 1, 2, 2, 3, 5, 3, 4; # returns 1 2 3 5 4
    my $x = uniq 1, 1, 2, 2, 3, 5, 3, 4; # returns 5

=item minmax LIST

Calculates the minimum and maximum of LIST and returns a two element list with
the first element being the minimum and the second the maximum. Returns the
empty list if LIST was empty.

The C<minmax> algorithm differs from a naive iteration over the list where each
element is compared to two values being the so far calculated min and max value
in that it only requires 3n/2 - 2 comparisons. Thus it is the most efficient
possible algorithm.

However, the Perl implementation of it has some overhead simply due to the fact
that there are more lines of Perl code involved. Therefore, LIST needs to be
fairly big in order for C<minmax> to win over a naive implementation. This
limitation does not apply to the XS version.

=item part BLOCK LIST

Partitions LIST based on the return value of BLOCK which denotes into which
partition the current value is put.

Returns a list of the partitions thusly created. Each partition created is a
reference to an array.

    my $i = 0;
    my @part = part { $i++ % 2 } 1 .. 8;   # returns [1, 3, 5, 7], [2, 4, 6, 8]

You can have a sparse list of partitions as well where non-set partitions will
be undef:

    my @part = part { 2 } 1 .. 10;	    # returns undef, undef, [ 1 .. 10 ]

Be careful with negative values, though:

    my @part = part { -1 } 1 .. 10;
    __END__
    Modification of non-creatable array value attempted, subscript -1 ...

Negative values are only ok when they refer to a partition previously created:

    my @idx  = ( 0, 1, -1 );
    my $i    = 0;
    my @part = part { $idx[$++ % 3] } 1 .. 8; # [1, 4, 7], [2, 3, 5, 6, 8]

=back

=head1 EXPORTS

Nothing by default. To import all of this module's symbols, do the conventional

    use List::MoreUtils ':all';

It may make more sense though to only import the stuff your program actually
needs:

    use List::MoreUtils qw{ any firstidx };

=head1 ENVIRONMENT

When C<LIST_MOREUTILS_PP> is set, the module will always use the pure-Perl
implementation and not the XS one. This environment variable is really just
there for the test-suite to force testing the Perl implementation, and possibly
for reporting of bugs. I don't see any reason to use it in a production
environment.

=head1 BUGS

There is a problem with a bug in 5.6.x perls. It is a syntax error to write
things like:

    my @x = apply { s/foo/bar/ } qw{ foo bar baz };

It has to be written as either

    my @x = apply { s/foo/bar/ } 'foo', 'bar', 'baz';

or

    my @x = apply { s/foo/bar/ } my @dummy = qw/foo bar baz/;

Perl 5.5.x and Perl 5.8.x don't suffer from this limitation.

If you have a functionality that you could imagine being in this module, please
drop me a line. This module's policy will be less strict than L<List::Util>'s
when it comes to additions as it isn't a core module.

When you report bugs, it would be nice if you could additionally give me the
output of your program with the environment variable C<LIST_MOREUTILS_PP> set
to a true value. That way I know where to look for the problem (in XS,
pure-Perl or possibly both).

=head1 SUPPORT

Bugs should always be submitted via the CPAN bug tracker.

L<http://rt.cpan.org/NoAuth/ReportBug.html?Queue=List-MoreUtils>

=head1 THANKS

Credits go to a number of people: Steve Purkis for giving me namespace advice
and James Keenan and Terrence Branno for their effort of keeping the CPAN
tidier by making L<List::Utils> obsolete. 

Brian McCauley suggested the inclusion of apply() and provided the pure-Perl
implementation for it.

Eric J. Roode asked me to add all functions from his module C<List::MoreUtil>
into this one. With minor modifications, the pure-Perl implementations of those
are by him.

The bunch of people who almost immediately pointed out the many problems with
the glitchy 0.07 release (Slaven Rezic, Ron Savage, CPAN testers).

A particularly nasty memory leak was spotted by Thomas A. Lowery.

Lars Thegler made me aware of problems with older Perl versions.

Anno Siegel de-orphaned each_arrayref().

David Filmer made me aware of a problem in each_arrayref that could ultimately
lead to a segfault.

Ricardo Signes suggested the inclusion of part() and provided the
Perl-implementation.

Robin Huston kindly fixed a bug in perl's MULTICALL API to make the
XS-implementation of part() work.

=head1 TODO

A pile of requests from other people is still pending further processing in
my mailbox. This includes:

=over 4

=item * List::Util export pass-through

Allow B<List::MoreUtils> to pass-through the regular L<List::Util>
functions to end users only need to C<use> the one module.

=item * uniq_by(&@)

Use code-reference to extract a key based on which the uniqueness is
determined. Suggested by Aaron Crane.

=item * delete_index

=item * random_item

=item * random_item_delete_index

=item * list_diff_hash

=item * list_diff_inboth

=item * list_diff_infirst

=item * list_diff_insecond

These were all suggested by Dan Muey.

=item * listify

Always return a flat list when either a simple scalar value was passed or an
array-reference. Suggested by Mark Summersault.

=back

=head1 SEE ALSO

L<List::Util>

=head1 AUTHOR

Adam Kennedy E<lt>adamk@cpan.orgE<gt>

Tassilo von Parseval E<lt>tassilo.von.parseval@rwth-aachen.deE<gt>

=head1 COPYRIGHT AND LICENSE

Some parts copyright 2011 Aaron Crane.

Copyright 2004 - 2010 by Tassilo von Parseval

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.8.4 or,
at your option, any later version of Perl 5 you may have available.

=cut
