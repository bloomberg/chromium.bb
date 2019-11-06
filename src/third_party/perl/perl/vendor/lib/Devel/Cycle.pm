package Devel::Cycle;
# $Id: Cycle.pm,v 1.15 2009/08/24 12:51:02 lstein Exp $

use 5.006001;
use strict;
use Carp 'croak','carp';
use warnings;

use Scalar::Util qw(isweak blessed refaddr reftype);

my $SHORT_NAME = 'A';
my %SHORT_NAMES;


require Exporter;

our @ISA = qw(Exporter);
our @EXPORT = qw(find_cycle find_weakened_cycle);
our @EXPORT_OK = qw($FORMATTING);
our $VERSION = '1.12';
our $FORMATTING = 'roasted';
our $QUIET   = 0;

my %import_args = (-quiet =>1,
		   -raw   =>1,
		   -cooked =>1,
		   -roasted=>1);

BEGIN {
  require constant;
  constant->import( HAVE_PADWALKER =>
		    eval {
		      require PadWalker;
		      $PadWalker::VERSION >= 1.0;
		    });
}

sub import {
  my $self = shift;
  my @args = @_;
  my %args = map {$_=>1} @args;
  $QUIET++    if exists $args{-quiet};
  $FORMATTING = 'roasted' if exists $args{-roasted};
  $FORMATTING = 'raw'     if exists $args{-raw};
  $FORMATTING = 'cooked'  if exists $args{-cooked};
  $self->export_to_level(1,$self,grep {!exists $import_args{$_}} @_);
}

sub find_weakened_cycle {
  my $ref      = shift;
  my $callback = shift;
  unless ($callback) {
    my $counter = 0;
    $callback = sub {
      _do_report(++$counter,shift)
    }
  }
  _find_cycle($ref,{},$callback,1,{},());
}

sub find_cycle {
  my $ref      = shift;
  my $callback = shift;
  unless ($callback) {
    my $counter = 0;
    $callback = sub {
      _do_report(++$counter,shift)
    }
  }
  _find_cycle($ref,{},$callback,0,{},());
}

sub _find_cycle {
  my $current   = shift;
  my $seenit    = shift;
  my $callback  = shift;
  my $inc_weak_refs = shift;
  my $complain = shift;
  my @report  = @_;

  return unless ref $current;

  # note: it seems like you could just do:
  #
  #    return if isweak($current);
  #
  # but strangely the weak flag doesn't seem to survive the copying,
  # so the test has to happen directly on the reference in the data
  # structure being scanned.

  if ($seenit->{refaddr $current}) {
    $callback->(\@report);
    return;
  }
  $seenit->{refaddr $current}++;

  _find_cycle_dispatch($current,{%$seenit},$callback,$inc_weak_refs,$complain,@report);
}

sub _find_cycle_dispatch {
  my $type = _get_type($_[0]);

  if (!defined $type) {
    my $ref = reftype $_[0];
    our %already_warned;
    if (!$already_warned{$ref}++) {
	warn "Unhandled type: $ref";
    }
    return;
  }
  my $sub = do { no strict 'refs'; \&{"_find_cycle_$type"} };
  $sub->(@_);
}

sub _find_cycle_SCALAR {
  my $current   = shift;
  my $seenit    = shift;
  my $callback  = shift;
  my $inc_weak_refs = shift;
  my $complain  = shift;
  my @report  = @_;

  return if !$inc_weak_refs && isweak($$current);
  _find_cycle($$current,{%$seenit},$callback,$inc_weak_refs,$complain,
              (@report,['SCALAR',undef,$current => $$current,$inc_weak_refs?isweak($$current):()]));
}

sub _find_cycle_ARRAY {
  my $current   = shift;
  my $seenit    = shift;
  my $callback  = shift;
  my $inc_weak_refs = shift;
  my $complain  = shift;
  my @report  = @_;

  for (my $i=0; $i<@$current; $i++) {
    next if !$inc_weak_refs && isweak($current->[$i]);
    _find_cycle($current->[$i],{%$seenit},$callback,$inc_weak_refs,$complain,
                (@report,['ARRAY',$i,$current => $current->[$i],$inc_weak_refs?isweak($current->[$i]):()]));
    }
}

sub _find_cycle_HASH {
  my $current   = shift;
  my $seenit    = shift;
  my $callback  = shift;
  my $inc_weak_refs = shift;
  my $complain  = shift;
  my @report  = @_;

  for my $key (sort keys %$current) {
    next if !$inc_weak_refs && isweak($current->{$key});
    _find_cycle($current->{$key},{%$seenit},$callback,$inc_weak_refs,$complain,
                (@report,['HASH',$key,$current => $current->{$key},$inc_weak_refs?isweak($current->{$key}):()]));
    }
}

sub _find_cycle_CODE {
  my $current   = shift;
  my $seenit    = shift;
  my $callback  = shift;
  my $inc_weak_refs = shift;
  my $complain  = shift;
  my @report  = @_;

  unless (HAVE_PADWALKER) {
    if (!$complain->{$current} && !$QUIET) {
      carp "A code closure was detected in but we cannot check it unless the PadWalker module is installed";
    }

    return;
  }

  my $closed_vars = PadWalker::closed_over( $current );
  foreach my $varname ( sort keys %$closed_vars ) {
    my $value = $closed_vars->{$varname};
    _find_cycle_dispatch($value,{%$seenit},$callback,$inc_weak_refs,$complain,
                         (@report,['CODE',$varname,$current => $value]));
  }
}

sub _do_report {
  my $counter = shift;
  my $path    = shift;
  print "Cycle ($counter):\n";
  foreach (@$path) {
    my ($type,$index,$ref,$value,$is_weak) = @$_;
    printf("\t%30s => %-30s\n",($is_weak ? 'w-> ' : '')._format_reference($type,$index,$ref,0),_format_reference(undef,undef,$value,1));
  }
  print "\n";
}

sub _format_reference {
  my ($type,$index,$ref,$deref) = @_;
  $type ||= _get_type($ref);
  return $ref unless $type;
  my $suffix  = defined $index ? _format_index($type,$index) : '';
  if ($FORMATTING eq 'raw') {
    return $ref.$suffix;
  }

  else {
    my $package  = blessed($ref);
    my $prefix   = $package ? ($FORMATTING eq 'roasted' ? "${package}::" : "${package}="  ) : '';
    my $sygil    = $deref ? '\\' : '';
    my $shortname = ($SHORT_NAMES{$ref} ||= $SHORT_NAME++);
    return $sygil . ($sygil ? '$' : '$$'). $prefix . $shortname . $suffix if $type eq 'SCALAR';
    return $sygil . ($sygil ? '@' : '$') . $prefix . $shortname . $suffix  if $type eq 'ARRAY';
    return $sygil . ($sygil ? '%' : '$') . $prefix . $shortname . $suffix  if $type eq 'HASH';
    return $sygil . ($sygil ? '&' : '$') . $prefix . $shortname . $suffix if $type eq 'CODE';
  }
}

# why not Scalar::Util::reftype?
sub _get_type {
  my $thingy = shift;
  return unless ref $thingy;
  return 'SCALAR' if UNIVERSAL::isa($thingy,'SCALAR') || UNIVERSAL::isa($thingy,'REF');
  return 'ARRAY'  if UNIVERSAL::isa($thingy,'ARRAY');
  return 'HASH'   if UNIVERSAL::isa($thingy,'HASH');
  return 'CODE'   if UNIVERSAL::isa($thingy,'CODE');
  undef;
}

sub _format_index {
  my ($type,$index) = @_;
  return "->[$index]" if $type eq 'ARRAY';
  return "->{'$index'}" if $type eq 'HASH';
  return " variable $index" if $type eq 'CODE';
  return;
}

1;
__END__

=head1 NAME

Devel::Cycle - Find memory cycles in objects

=head1 SYNOPSIS

  #!/usr/bin/perl
  use Devel::Cycle;
  my $test = {fred   => [qw(a b c d e)],
	    ethel  => [qw(1 2 3 4 5)],
	    george => {martha => 23,
		       agnes  => 19}
	   };
  $test->{george}{phyllis} = $test;
  $test->{fred}[3]      = $test->{george};
  $test->{george}{mary} = $test->{fred};
  find_cycle($test);
  exit 0;

  # output:

  Cycle (1):
	                $A->{'george'} => \%B
	               $B->{'phyllis'} => \%A

  Cycle (2):
	                $A->{'george'} => \%B
	                  $B->{'mary'} => \@A
	                       $A->[3] => \%B

  Cycle (3):
	                  $A->{'fred'} => \@A
	                       $A->[3] => \%B
	               $B->{'phyllis'} => \%A

  Cycle (4):
	                  $A->{'fred'} => \@A
	                       $A->[3] => \%B
	                  $B->{'mary'} => \@A
  
  # you can also check weakened references
  weaken($test->{george}->{phyllis});
  find_weakened_cycle($test);
  exit 0;

  # output:
  
  Cycle (1):
                        $A->{'george'} => \%B                           
                          $B->{'mary'} => \@C                           
                               $C->[3] => \%B                           

  Cycle (2):
                        $A->{'george'} => \%B                           
                   w-> $B->{'phyllis'} => \%A                           

  Cycle (3):
                          $A->{'fred'} => \@C                           
                               $C->[3] => \%B                           
                          $B->{'mary'} => \@C                           

  Cycle (4):
                          $A->{'fred'} => \@C                           
                               $C->[3] => \%B                           
                   w-> $B->{'phyllis'} => \%A                       

=head1 DESCRIPTION

This is a simple developer's tool for finding circular references in
objects and other types of references.  Because of Perl's
reference-count based memory management, circular references will
cause memory leaks.

=head2 EXPORT

The find_cycle() and find_weakened_cycle() subroutine are exported by default.

=over 4

=item find_cycle($object_reference,[$callback])

The find_cycle() function will traverse the object reference and print
a report to STDOUT identifying any memory cycles it finds.

If an optional callback code reference is provided, then this callback
will be invoked on each cycle that is found.  The callback will be
passed an array reference pointing to a list of lists with the
following format:

 $arg = [ ['REFTYPE',$index,$reference,$reference_value],
          ['REFTYPE',$index,$reference,$reference_value],
          ['REFTYPE',$index,$reference,$reference_value],
           ...
        ]

Each element in the array reference describes one edge in the memory
cycle.  'REFTYPE' describes the type of the reference and is one of
'SCALAR','ARRAY' or 'HASH'.  $index is the index affected by the
reference, and is undef for a scalar, an integer for an array
reference, or a hash key for a hash.  $reference is the memory
reference, and $reference_value is its dereferenced value.  For
example, if the edge is an ARRAY, then the following relationship
holds:

   $reference->[$index] eq $reference_value

The first element of the array reference is the $object_reference that
you pased to find_cycle() and may not be directly involved in the
cycle.

If a reference is a weak ref produced using Scalar::Util's weaken()
function then it won't contribute to cycles.

=item find_weakened_cycle($object_reference,[$callback])

The find_weakened_cycle() function will traverse the object reference and print
a report to STDOUT identifying any memory cycles it finds, I<including> any weakened
cycles produced using Scalar::Util's weaken().

If an optional callback code reference is provided, then this callback
will be invoked on each cycle that is found.  The callback will be
passed an array reference pointing to a list of lists with the
following format:

 $arg = [ ['REFTYPE',$index,$reference,$reference_value,$is_weakened],
          ['REFTYPE',$index,$reference,$reference_value,$is_weakened],
          ['REFTYPE',$index,$reference,$reference_value,$is_weakened],
           ...
        ]

Each element in the array reference describes one edge in the memory
cycle.  'REFTYPE' describes the type of the reference and is one of
'SCALAR','ARRAY' or 'HASH'.  $index is the index affected by the
reference, and is undef for a scalar, an integer for an array
reference, or a hash key for a hash.  $reference is the memory
reference, and $reference_value is its dereferenced value. $is_weakened
is a boolean specifying if the reference is weakened or not. For
example, if the edge is an ARRAY, then the following relationship
holds:

   $reference->[$index] eq $reference_value

The first element of the array reference is the $object_reference that
you pased to find_cycle() and may not be directly involved in the
cycle.

=back

=head2 Cycle Report Formats

The default callback prints out a trace of each cycle it finds.  You
can control the format of the trace by setting the package variable
$Devel::Cycle::FORMATTING to one of "raw," "cooked," or "roasted".

The "raw" format prints out anonymous memory references using standard
Perl memory location nomenclature.  For example, a "Foo::Bar" object
that points to an ordinary hash will appear in the trace like this:

	Foo::Bar=HASH(0x8124394)->{'phyllis'} => HASH(0x81b4a90)

The "cooked" format (the default), uses short names for anonymous
memory locations, beginning with "A" and moving upward with the magic
++ operator.  This leads to a much more readable display:

        $Foo::Bar=B->{'phyllis'} => \%A

The "roasted" format is similar to the "cooked" format, except that
object references are formatted slightly differently:

	$Foo::Bar::B->{'phyllis'} => \%A

If a reference is a weakened ref, then it will have a 'w->' prepended to
it, like this:

	w-> $Foo::Bar::B->{'phyllis'} => \%A

For your convenience, $Devel::Cycle::FORMATTING can be imported:

       use Devel::Cycle qw(:DEFAULT $FORMATTING);
       $FORMATTING = 'raw';

Alternatively, you can control the formatting at compile time by
passing one of the options -raw, -cooked, or -roasted to "use" as
illustrated here:

  use Devel::Cycle -raw;

=head2 Code references (closures)

If the PadWalker module is installed, Devel::Cycle will also report
cycles in code closures. If PadWalker is not installed and
Devel::Cycle detects a CODE reference in one of the data structures,
it will warn (once per data structure) that it cannot inspect the CODE
unless PadWalker is available. You can turn this warning off by
passing -quiet to Devel::Cycle at compile time:

 use Devel::Cycle -quiet;

=head1 SEE ALSO

L<Test::Memory::Cycle>
L<Devel::Leak>
L<Scalar::Util>

=head1 DEVELOPING

https://github.com/lstein/Devel-Cycle. Please contribute to the code
base by sending pull requests. Use GitHub for bug reports and feature
requests.

=head1 AUTHOR

Lincoln Stein, E<lt>lincoln.stein@gmail.comE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2003-2014 by Lincoln Stein

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.8.2 or,
at your option, any later version of Perl 5 you may have available.


=cut
