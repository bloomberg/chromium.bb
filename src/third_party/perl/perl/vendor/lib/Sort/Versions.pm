package Sort::Versions;
$Sort::Versions::VERSION = '1.62';
# Copyright (c) 1996, Kenneth J. Albanowski. All rights reserved.  This
# program is free software; you can redistribute it and/or modify it under
# the same terms as Perl itself.

use 5.006;
use strict;
use warnings;

require Exporter;
our @ISA        = qw(Exporter);
our @EXPORT     = qw(&versions &versioncmp);
our @EXPORT_OK  = qw();

sub versioncmp ($$) {
    my @A = ($_[0] =~ /([-.]|\d+|[^-.\d]+)/g);
    my @B = ($_[1] =~ /([-.]|\d+|[^-.\d]+)/g);

    my ($A, $B);
    while (@A and @B) {
	$A = shift @A;
	$B = shift @B;
	if ($A eq '-' and $B eq '-') {
	    next;
	} elsif ( $A eq '-' ) {
	    return -1;
	} elsif ( $B eq '-') {
	    return 1;
	} elsif ($A eq '.' and $B eq '.') {
	    next;
	} elsif ( $A eq '.' ) {
	    return -1;
	} elsif ( $B eq '.' ) {
	    return 1;
	} elsif ($A =~ /^\d+$/ and $B =~ /^\d+$/) {
	    if ($A =~ /^0/ || $B =~ /^0/) {
		return $A cmp $B if $A cmp $B;
	    } else {
		return $A <=> $B if $A <=> $B;
	    }
	} else {
	    $A = uc $A;
	    $B = uc $B;
	    return $A cmp $B if $A cmp $B;
	}	
    }
    @A <=> @B;
}

sub versions () {
    my $callerpkg = (caller)[0];
    my $caller_a = "${callerpkg}::a";
    my $caller_b = "${callerpkg}::b";
    no strict 'refs';
    return versioncmp($$caller_a, $$caller_b);
}

=encoding utf-8

=head1 NAME

Sort::Versions - a perl 5 module for sorting of revision-like numbers

=head1 SYNOPSIS

	use Sort::Versions;
	@l = sort { versioncmp($a, $b) } qw( 1.2 1.2.0 1.2a.0 1.2.a 1.a 02.a );

	...

	use Sort::Versions;
	print 'lower' if versioncmp('1.2', '1.2a') == -1;

	...

	use Sort::Versions;
	%h = (1 => 'd', 2 => 'c', 3 => 'b', 4 => 'a');
	@h = sort { versioncmp($h{$a}, $h{$b}) } keys %h;

=head1 DESCRIPTION	

Sort::Versions allows easy sorting of mixed non-numeric and numeric strings,
like the 'version numbers' that many shared library systems and revision
control packages use. This is quite useful if you are trying to deal with
shared libraries. It can also be applied to applications that intersperse
variable-width numeric fields within text. Other applications can
undoubtedly be found.

For an explanation of the algorithm, it's simplest to look at these examples:

  1.1   <  1.2
  1.1a  <  1.2
  1.1   <  1.1.1
  1.1   <  1.1a
  1.1.a <  1.1a
  1     <  a
  a     <  b
  1     <  2
  1.1-3 <  1.1-4
  1.1-5 <  1.1.6

More precisely (but less comprehensibly), the two strings are treated
as subunits delimited by periods or hyphens. Each subunit can contain
any number of groups of digits or non-digits. If digit groups are
being compared on both sides, a numeric comparison is used, otherwise
a ASCII ordering is used. A group or subgroup with more units will win
if all comparisons are equal.  A period binds digit groups together
more tightly than a hyphen.

Some packages use a different style of version numbering: a simple
real number written as a decimal. Sort::Versions has limited support
for this style: when comparing two subunits which are both digit
groups, if either subunit has a leading zero, then both are treated
like digits after a decimal point. So for example:

  0002  <  1
  1.06  <  1.5

This wonE<39>t always work, because there wonE<39>t always be a leading zero
in real-number style version numbers. There is no way for
Sort::Versions to know which style was intended. But a lot of the time
it will do the right thing. If you are making up version numbers, the
style with (possibly) more than one dot is the style to use.

=head1 USAGE

The function C<versioncmp()> takes two arguments and compares them like C<cmp>.
With perl 5.6 or later, you can also use this function directly in sorting:

    @l = sort versioncmp qw(1.1 1.2 1.0.3);

The function C<versions()> can be used directly as a sort function even on
perl 5.005 and earlier, but its use is deprecated.

=head1 SEE ALSO

L<version>, L<CPAN::Version> which is part of the L<CPAN> distribution.


=head1 REPOSITORY

L<https://github.com/neilb/Sort-Versions>

=head1 AUTHOR

Ed Avis <ed@membled.com> and Matt Johnson <mwj99@doc.ic.ac.uk> for
recent releases; the original author is Kenneth J. Albanowski
<kjahds@kjahds.com>.  Thanks to Hack Kampbjørn and Slaven Rezic for
patches and bug reports.

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 1996 by Kenneth J. Albanowski.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut

1;

