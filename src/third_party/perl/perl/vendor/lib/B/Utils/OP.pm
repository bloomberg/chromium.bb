package B::Utils::OP;

require 5.006;
use Carp 'croak';
use strict;
use warnings;
use B::Utils ();

our @ISA = 'Exporter';
require Exporter;
our $VERSION = '0.27';
our @EXPORT = qw(parent_op return_op);


push @ISA, 'DynaLoader';
# the boot symbol is in B::Utils.  bootstrap doesn't like it, so we
# need to load it manually.
my $bootname = 'boot_B__Utils__OP';
if (my $boot_symbol_ref = DynaLoader::dl_find_symbol_anywhere($bootname)) {
    DynaLoader::dl_install_xsub(__PACKAGE__."::bootstrap", $boot_symbol_ref, __FILE__)->(__PACKAGE__, $VERSION);
}

=head1 NAME

B::Utils::OP - op related utility functions for perl

=head1 VERSION

version 0.27

=head1 SYNOPSIS

  use B::Utils::OP qw(parent_op return_op);
  sub foo {
    my $pop = parent_op(0);
    my $rop = return_op(0);
  }

=head1 DESCRIPTION

  sub foo {
    dothis(1);
    find_things();
    return;
  }

has the following optree:

 d  <1> leavesub[1 ref] K/REFC,1 ->(end)
 -     <@> lineseq KP ->d
 1        <;> nextstate(main -371 bah.pl:8) v/2 ->2
 5        <1> entersub[t2] vKS/TARG,3 ->6
 -           <1> ex-list K ->5
 2              <0> pushmark s ->3
 3              <$> const[IV 1] sM ->4
 -              <1> ex-rv2cv sK/3 ->-
 4                 <#> gv[*dothis] s ->5
 6        <;> nextstate(main -371 bah.pl:9) v/2 ->7

 9        <1> entersub[t4] vKS/TARG,3 ->a
 -           <1> ex-list K ->9
 7              <0> pushmark s ->8
 -              <1> ex-rv2cv sK/3 ->-
 8                 <#> gv[*find_things] s/EARLYCV ->9

 a        <;> nextstate(main -371 bah.pl:10) v/2 ->b
 c        <@> return K ->d
 b           <0> pushmark s ->c

The C<find_things> in C<foo> is called in the C<entersub> in #9.  If
you call C<parent_op> function with level 0, you get the C<nextstate>
op that is before the entersub, which is #6.  And C<return_op> gives
you the next op that the caller is returning to, in this case, the
C<nextstate> in #a.

=head2 EXPORTED PERL FUNCTIONS

=over

=item parent_op($lv)

In runtime, returns the L<B::OP> object whose next is the C<entersub> of the current context up level C<$lv>

=item return_op($lv)

In runtime, returns the L<B::OP> object that the current context is returning to at level C<$lv>

=back

=head2 B::CV METHODS

=over

=item $cv->NEW_with_start($root, $start)

Clone the C<$cv> but with different C<$root> and C<$start>

=back

=head1 AUTHORS

Chia-liang Kao E<lt>clkao@clkao.orgE<gt>

=head1 COPYRIGHT

Copyright 2008 by Chia-liang Kao

This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

See L<http://www.perl.com/perl/misc/Artistic.html>

=cut

1;
