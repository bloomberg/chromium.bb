use strict;
use warnings;
package Hook::LexWrap; # git description: v0.25-14-g33c34e7
# vi: noet sts=8 sw=8 ts=8 :
# ABSTRACT: Lexically scoped subroutine wrappers
# KEYWORDS: subroutine function modifier wrapper lexical scope

our $VERSION = '0.26';

use Carp ();

{
no warnings 'redefine';
*CORE::GLOBAL::caller = sub (;$) {
	my ($height) = ($_[0]||0);
	my $i=1;
	my $name_cache;
	while (1) {
		my @caller = CORE::caller() eq 'DB'
			? do { package	# line break to foil [Git::Describe]
				DB; CORE::caller($i++) }
			: CORE::caller($i++);
		return if not @caller;
		$caller[3] = $name_cache if $name_cache;
		$name_cache = $caller[0] eq 'Hook::LexWrap' ? $caller[3] : '';
		next if $name_cache || $height-- != 0;
		return wantarray ? @_ ? @caller : @caller[0..2] : $caller[0];
	}
};
}

sub import { no strict 'refs'; *{caller()."::wrap"} = \&wrap }

sub wrap (*@) {  ## no critic Prototypes
	my ($typeglob, %wrapper) = @_;
	$typeglob = (ref $typeglob || $typeglob =~ /::/)
		? $typeglob
		: caller()."::$typeglob";
	my $original;
	{
	        no strict 'refs';
	        $original = ref $typeglob eq 'CODE' && $typeglob
		     || *$typeglob{CODE}
		     || Carp::croak "Can't wrap non-existent subroutine ", $typeglob;
	}
	Carp::croak "'$_' value is not a subroutine reference"
		foreach grep {$wrapper{$_} && ref $wrapper{$_} ne 'CODE'}
			qw(pre post);
	no warnings 'redefine';
	my ($caller, $unwrap) = *CORE::GLOBAL::caller{CODE};
	my $imposter = sub {
		if ($unwrap) { goto &$original }
		my ($return, $prereturn);
		if (wantarray) {
			$prereturn = $return = [];
			() = $wrapper{pre}->(@_,$return) if $wrapper{pre};
			if (ref $return eq 'ARRAY' && $return == $prereturn && !@$return) {
				$return = [ &$original ];
				() = $wrapper{post}->(@_, $return)
					if $wrapper{post};
			}
			return ref $return eq 'ARRAY' ? @$return : ($return);
		}
		elsif (defined wantarray) {
			$return = bless sub {$prereturn=1}, 'Hook::LexWrap::Cleanup';
			my $dummy = $wrapper{pre}->(@_, $return) if $wrapper{pre};
			unless ($prereturn) {
				$return = &$original;
				$dummy = scalar $wrapper{post}->(@_, $return)
					if $wrapper{post};
			}
			return $return;
		}
		else {
			$return = bless sub {$prereturn=1}, 'Hook::LexWrap::Cleanup';
			$wrapper{pre}->(@_, $return) if $wrapper{pre};
			unless ($prereturn) {
				&$original;
				$wrapper{post}->(@_, $return)
					if $wrapper{post};
			}
			return;
		}
	};
	ref $typeglob eq 'CODE' and return defined wantarray
		? $imposter
		: Carp::carp "Uselessly wrapped subroutine reference in void context";
	{
	        no strict 'refs';
	        *{$typeglob} = $imposter;
	}
	return unless defined wantarray;
	return bless sub{ $unwrap=1 }, 'Hook::LexWrap::Cleanup';
}

package  # hide from PAUSE
	Hook::LexWrap::Cleanup;

sub DESTROY { $_[0]->() }
use overload 
	q{""}   => sub { undef },
	q{0+}   => sub { undef },
	q{bool} => sub { undef },
	q{fallback}=>1; #fallback=1 - like no overloading for other operations

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Hook::LexWrap - Lexically scoped subroutine wrappers

=head1 VERSION

version 0.26

=head1 SYNOPSIS

	use Hook::LexWrap;

	sub doit { print "[doit:", caller, "]"; return {my=>"data"} }

	SCOPED: {
		wrap doit =>
			pre  => sub { print "[pre1: @_]\n" },
			post => sub { print "[post1:@_]\n"; $_[1]=9; };

		my $temporarily = wrap doit =>
			post => sub { print "[post2:@_]\n" },
			pre  => sub { print "[pre2: @_]\n  "};

		@args = (1,2,3);
		doit(@args);	# pre2->pre1->doit->post1->post2
	}

	@args = (4,5,6);
	doit(@args);		# pre1->doit->post1

=head1 DESCRIPTION

Hook::LexWrap allows you to install a pre- or post-wrapper (or both)
around an existing subroutine. Unlike other modules that provide this
capacity (e.g. Hook::PreAndPost and Hook::WrapSub), Hook::LexWrap
implements wrappers in such a way that the standard C<caller> function
works correctly within the wrapped subroutine.

To install a prewrappers, you write:

        use Hook::LexWrap;

        wrap 'subroutine_name', pre => \&some_other_sub;

   #or: wrap *subroutine_name,  pre => \&some_other_sub;

The first argument to C<wrap> is a string containing the name of the
subroutine to be wrapped (or the typeglob containing it, or a
reference to it). The subroutine name may be qualified, and the
subroutine must already be defined. The second argument indicates the
type of wrapper being applied and must be either C<'pre'> or
C<'post'>. The third argument must be a reference to a subroutine that
implements the wrapper.

To install a post-wrapper, you write:

        wrap 'subroutine_name', post => \&yet_another_sub;

   #or: wrap *subroutine_name,  post => \&yet_another_sub;

To install both at once:

        wrap 'subroutine_name',
             pre  => \&some_other_sub,
             post => \&yet_another_sub;

or:

        wrap *subroutine_name,
             post => \&yet_another_sub,  # order in which wrappers are
             pre  => \&some_other_sub;   # specified doesn't matter

Once they are installed, the pre- and post-wrappers will be called before
and after the subroutine itself, and will be passed the same argument list.

The pre- and post-wrappers and the original subroutine also all see the same
(correct!) values from C<caller> and C<wantarray>.

=head2 Short-circuiting and long-circuiting return values

The pre- and post-wrappers both receive an extra argument in their @_
arrays. That extra argument is appended to the original argument list
(i.e. is can always be accessed as $_[-1]) and acts as a place-holder for
the original subroutine's return value.

In a pre-wrapper, $_[-1] is -- for obvious reasons -- C<undef>. However,
$_[-1] may be assigned to in a pre-wrapper, in which case Hook::LexWrap
assumes that the original subroutine has been "pre-empted", and that
neither it, nor the corresponding post-wrapper, nor any wrappers that
were applied I<before> the pre-empting pre-wrapper was installed, need
be run. Note that any post-wrappers that were installed after the
pre-empting pre-wrapper was installed I<will> still be called before the
original subroutine call returns.

In a post-wrapper, $_[-1] contains the return value produced by the
wrapped subroutine. In a scalar return context, this value is the scalar
return value. In an list return context, this value is a reference to
the array of return values. $_[-1] may be assigned to in a post-wrapper,
and this changes the return value accordingly.

Access to the arguments and return value is useful for implementing
techniques such as memoization:

        my %cache;
        wrap fibonacci =>
                pre  => sub { $_[-1] = $cache{$_[0]} if $cache{$_[0]} },
                post => sub { $cache{$_[0]} = $_[-1] };

or for converting arguments and return values in a consistent manner:

	# set_temp expects and returns degrees Fahrenheit,
	# but we want to use Celsius
        wrap set_temp =>
                pre   => sub { splice @_, 0, 1, $_[0] * 1.8 + 32 },
                post  => sub { $_[-1] = ($_[0] - 32) / 1.8 };

=head2 Lexically scoped wrappers

Normally, any wrappers installed by C<wrap> remain attached to the 
subroutine until it is undefined. However, it is possible to make
specific wrappers lexically bound, so that they operate only until
the end of the scope in which they're created (or until some other
specific point in the code).

If C<wrap> is called in a I<non-void> context:

        my $lexical = wrap 'sub_name', pre => \&wrapper;

it returns a special object corresponding to the particular wrapper being
placed around the original subroutine. When that object is destroyed
-- when its container variable goes out of scope, or when its
reference count otherwise falls to zero (e.g. C<undef $lexical>), or 
when it is explicitly destroyed (C<$lexical-E<gt>DESTROY>) --
the corresponding wrapper is removed from around
the original subroutine. Note, however, that all other wrappers around the
subroutine are preserved.

=head2 Anonymous wrappers

If the subroutine to be wrapped is passed as a reference (rather than by name
or by typeglob), C<wrap> does not install the wrappers around the 
original subroutine. Instead it generates a new subroutine which acts
as if it were the original with those wrappers around it.
It then returns a reference to that new subroutine. Only calls to the original
through that wrapped reference invoke the wrappers. Direct by-name calls to
the original, or calls through another reference, do not.

If the original is subsequently wrapped by name, the anonymously wrapped
subroutine reference does not see those wrappers. In other words,
wrappers installed via a subroutine reference are completely independent
of those installed via the subroutine's name (or typeglob).

For example:

        sub original { print "ray" }

        # Wrap anonymously...
        my $anon_wrapped = wrap \&original, pre => sub { print "do..." };

        # Show effects...
        original();             # prints "ray"
        $anon_wrapped->();      # prints "do..ray"

        # Wrap nonymously...
        wrap *original,
                pre  => sub { print "fa.." },
                post => sub { print "..mi" };

        # Show effects...
        original();             #   now prints "fa..ray..mi"
        $anon_wrapped->();      # still prints "do...ray"

=head1 DIAGNOSTICS

=over

=item C<Can't wrap non-existent subroutine %s>

An attempt was made to wrap a subroutine that was not defined at the
point of wrapping.

=item C<'pre' value is not a subroutine reference>

The value passed to C<wrap> after the C<'pre'> flag was not
a subroutine reference. Typically, someone forgot the C<sub> on
the anonymous subroutine:

        wrap 'subname', pre => { your_code_here() };

and Perl interpreted the last argument as a hash constructor.

=item C<'post' value is not a subroutine reference>

The value passed to C<wrap> after the C<'post'> flag was not
a subroutine reference.

=item C<Uselessly wrapped subroutine reference in void context> (warning only)

When the subroutine to be wrapped is passed as a subroutine reference,
C<wrap> does not install the wrapper around the original, but instead
returns a reference to a subroutine which wraps the original
(see L<Anonymous wrappers>). 

However, there's no point in doing this if you don't catch the resulting
subroutine reference.

=back

=head1 BLAME

Schwern made me do this (by implying it wasn't possible ;-)

=head1 SEE ALSO

Sub::Prepend

=head1 BUGS

There are undoubtedly serious bugs lurking somewhere in code this funky :-)

Bug reports and other feedback are most welcome.

Bugs may be submitted through L<the RT bug tracker|https://rt.cpan.org/Public/Dist/Display.html?Name=Hook-LexWrap>
(or L<bug-Hook-LexWrap@rt.cpan.org|mailto:bug-Hook-LexWrap@rt.cpan.org>).

=head1 AUTHOR

Damian Conway <damian@conway.org>

=head1 CONTRIBUTORS

=for stopwords Karen Etheridge Alexandr Ciornii Father Chrysostomos

=over 4

=item *

Karen Etheridge <ether@cpan.org>

=item *

Alexandr Ciornii <alexchorny@gmail.com>

=item *

Father Chrysostomos <sprout@cpan.org>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2001 by Damian Conway.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
