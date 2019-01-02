package Imager::Expr::Assem;
use strict;
use Imager::Expr;
use Imager::Regops;
use vars qw($VERSION);

$VERSION = "1.003";

use vars qw(@ISA);
@ISA = qw(Imager::Expr);

__PACKAGE__->register_type('assem');

sub compile {
  my ($self, $expr, $opts) = @_;
  my %nregs;
  my @vars = $self->_variables();
  my @nregs = (0) x @vars;
  my @cregs;
  my %vars;
  @vars{@vars} = map { "r$_" } 0..$#vars;
  my %labels;
  my @ops;
  my @msgs;
  my $attr = \%Imager::Regops::Attr;

  # initially produce [ $linenum, $result, $opcode, @parms ]
  my $lineno = 0;
  while ($expr =~ s/^([^\n]+)(?:\n|$)//) {
    ++$lineno;
    my $line = $1;
    $line =~ s/#.*//;
    next if $line =~ /^\s*$/;
    for my $op (split /;/, $line) {
      if (my ($name, $type) = $op =~ /^\s*var\s+([^:]+):(\S+)\s*$/) {
	if (exists $vars{$name}) {
	  push(@msgs, "$lineno: duplicate variable name '$name'");
	  next;
	}
	if ($type eq 'num' || $type eq 'n') {
	  $vars{$name} = 'r'.@nregs;
	  push(@nregs, undef);
	  next;
	}
	elsif ($type eq 'pixel' || $type eq 'p' || $type eq 'c') {
	  $vars{$name} = 'p'.@cregs;
	  push(@cregs, undef);
	  next;
	}
	push(@msgs, "$lineno: unknown variable type $type");
	next;
      }
      # any statement can have a label
      if ($op =~ s/^\s*(\w+):\s*//) {
	if ($labels{$1}) {
	  push(@msgs, 
	       "$lineno: duplicate label $1 (previous on $labels{$1}[1])");
	  next;
	}
	$labels{$1} = [ scalar @ops, $lineno ];
      }
      next if $op =~ /^\s*$/;
      # jumps have special operand handling
      if ($op =~ /^\s*jump\s+(\w+)\s*$/) {
	push(@ops, [$lineno, "", "jump", $1]);
      }
      elsif (my ($code, $reg, $targ) =
	     ($op =~ /^\s*(jumpz|jumpnz)\s+(\S+)\s+(\S+)\s*$/)) {
	push(@ops, [$lineno, "", $code, $reg, $targ]);
      }
      elsif ($op =~ /^\s*print\s+(\S+)\s*/) {
	push(@ops, [$lineno, "", 'print', $1 ]);
      }
      elsif ($op =~ /^\s*ret\s+(\S+)\s*/) {
	push(@ops, [$lineno, "", 'ret', $1]);
      }
      elsif ($op =~ s/\s*(\S+)\s*=\s*(\S+)\s*$//) {
	# simple assignment
	push(@ops, [$lineno, $1, "set", $2]);
      }
      elsif ($op =~ s/\s*(\S+)\s*=\s*(\S+)\s*//) {
	# some normal ops finally
	my ($result, $opcode) = ($1, $2);
	unless ($attr->{$opcode}) {
	  push(@msgs, "$lineno: unknown operator $opcode");
	  next;
	}
	my @oper;
	while ($op =~ s/(\S+)\s*//) {
	  push(@oper, $1);
	}
	push(@ops, [$lineno, $result, $opcode, @oper]);
      }
      else {
	push(@msgs, "$lineno: invalid statement '$op'");  
      }
    }
  }

  my $max_opr = $Imager::Regops::MaxOperands;
  my $numre = $self->numre;
  my $trans =
    sub {
      # translate a name/number to a <type><digits>
      my ($name) = @_;
      $name = $self->{constants}{$name}
	if exists $self->{constants}{$name};
      if ($vars{$name}) {
	return $vars{$name};
      }
      elsif ($name =~ /^$numre$/) {
	$vars{$name} = 'r'.@nregs;
	push(@nregs, $name);
	return $vars{$name};
      }
      else {
	push(@msgs, "$lineno: undefined variable $name");
	return '';
      }
    };
  # now to translate symbols and so on
 OP: for my $op (@ops) {
    $lineno = shift @$op;
    if ($op->[1] eq 'jump') {
      unless (exists $labels{$op->[2]}) {
	push(@msgs, "$lineno: unknown label $op->[2]");
	next;
      }
      $op = [ 'jump', "j$labels{$op->[2]}[0]", (0) x $max_opr ];
    }
    elsif ($op->[1] =~ /^jump/) {
      unless (exists $labels{$op->[3]}) {
	push(@msgs, "$lineno: unknown label $op->[2]");
	next;
      }
      $op = [ $op->[1], $trans->($op->[2]), "j$labels{$op->[3]}[0]",
	      (0) x ($max_opr-1) ];
    }
    elsif ($op->[1] eq 'print') {
      $op = [ $op->[1], $trans->($op->[2]), (0) x $max_opr ];
    }
    elsif ($op->[1] eq 'ret') {
      $op = [ 'ret', $trans->($op->[2]), (0) x $max_opr ];
    }
    else {
      # a normal operator
      my ($result, $name, @parms) = @$op;

      if ($result =~ /^$numre$/) {
	push(@msgs, "$lineno: target of operator cannot be a constant");
	next;
      }
      $result = $trans->($result);
      for my $parm (@parms) {
	$parm = $trans->($parm);
      }
      push(@parms, (0) x ($max_opr-@parms));
      $op = [ $op->[1], @parms, $result ];
    }
  }

  # more validation than a real assembler
  # not trying to solve the halting problem...
  if (@ops && $ops[-1][0] ne 'ret' && $ops[-1][0] ne 'jump') {
    push(@msgs, ": the last instruction must be ret or jump");
  }

  $self->{nregs} = \@nregs;
  $self->{cregs} = \@cregs;

  if (@msgs) {
    $self->error(join("\n", @msgs));
    return 0;
  }

  return \@ops;
}

1;

__END__

=head1 NAME

  Imager::Expr::Assem - an assembler for producing code for the Imager
  register machine

=head1 SYNOPSIS

  use Imager::Expr::Assem;
  my $expr = Imager::Expr->new(assem=>'...', ...)

=head1 DESCRIPTION

This module is a simple Imager::Expr compiler that compiles a
low-level language that has a nearly 1-to-1 relationship to the
internal representation used for compiled register machine code.

=head2 Syntax

Each line can contain multiple statements separated by semi-colons.

Anything after '#' in a line is ignored.

Types of statements:

=over 4

=item variable definition

=over 4

C<var> I<name>:I<type>

=back

defines variable I<name> to have I<type>, which can be any of C<n> or
C<num> for a numeric type or C<pixel>, C<p> or C<c> for a pixel or
color type.

Variable names cannot include white-space.

=item operators

Operators can be split into 3 basic types, those that have a result
value, those that don't and the null operator, eg. jump has no value.

The format for operators that return a value is typically:

=over 4

I<result> = I<operator> I<operand> ...

=back

and for those that don't return a value:

=over 4

I<operator> I<operand>

=back

where operator is any valid register machine operator, result is any
variable defined with C<var>, and operands are variables, constants or
literals, or for jump operators, labels.

The set operator can be simplified to:

=over 4

I<result> = I<operator>

=back

All operators maybe preceded by a label, which is any non-white-space
text immediately followed by a colon (':').

=back

=head1 BUGS

Note that the current optimizer may produce incorrect optimization for
your code, fortunately the optimizer will disable itself if you
include any jump operator in your code.  A single jump to anywhere
after your final C<ret> operator can be used to disable the optimizer
without slowing down your code.

There's currently no high-level code generation that can generate code
with loops or real conditions.

=head1 SEE ALSO

Imager(3), F<transform.perl>, F<regmach.c>

=head1 AUTHOR

Tony Cook <tony@develop-help.com>

=cut
