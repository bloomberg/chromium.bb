package Imager::Expr;

use Imager::Regops;
use strict;
use vars qw($VERSION);

$VERSION = "1.005";

my %expr_types;

my $error;

sub error {
  shift if UNIVERSAL::isa($_[0], 'Imager::Expr');
  if (@_) {
    $error = "@_";
  }
  else {
    return $error;
  }
}

# what else?
my %default_constants =
  (
   # too many digits, better than too few
   pi=>3.14159265358979323846264338327950288419716939937510582097494
  );

sub new {
  my ($class, $opts) = @_;

  # possibly this is a very bad idea
  my ($type) = grep exists $expr_types{$_}, keys %$opts;
  die "Imager::Expr: No known expression type"
    if !defined $type;
  my $self = bless {}, $expr_types{$type};
  $self->{variables} = [ @{$opts->{variables}} ];
  $self->{constants} = { %default_constants, %{$opts->{constants} || {}} };
  $self->{ops} = $self->compile($opts->{$type}, $opts)
    or return;
  $self->optimize()
    or return;
  $self->{code} = $self->assemble()
    or return;
  $self;
}

sub register_type {
  my ($pack, $name) = @_;
  $expr_types{$name} = $pack;
}

sub type_registered {
  my ($class, $name) = @_;

  $expr_types{$name};
}

sub _variables {
  return @{$_[0]->{variables}};
}

sub code {
  return $_[0]->{code};
}

sub nregs {
  return $_[0]->{nregs};
}

sub cregs {
  return $_[0]->{cregs};
}

my $numre = '[+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?';

sub numre {
  $numre;
}

# optimize the code
sub optimize {
  my ($self) = @_;

  my @ops = @{$self->{ops}};

  # this function cannot current handle code with jumps
  return 1 if grep $_->[0] =~ /^jump/, @ops;

  # optimization - common sub-expression elimination
  # it's possible to fold this into the code generation - but it will wait

  my $max_opr = $Imager::Regops::MaxOperands;
  my $attr = \%Imager::Regops::Attr;
  my $foundops = 1;
  while ($foundops) {
    $foundops = 0;
    my %seen;
    my $index;
    my @out;
    while (@ops) {
      my $op = shift @ops;
      my $desc = join(",", @{$op}[0..$max_opr]);
      if ($seen{$desc}) {
	push(@out, @ops);
	my $old = $op->[-1];
	my $new = $seen{$desc};
	for $op (@out) {
	  for my $reg (@{$op}[1..$max_opr]) {
	    $reg = $new if $reg eq $old;
	  }
	}
	$foundops=1;
	last;
      }
      $seen{$desc} = $op->[-1];
      push(@out, $op);
    }
    @ops = @out;
  }
  # strength reduction
  for my $op (@ops) {
    # reduce division by a constant to multiplication by a constant
    if ($op->[0] eq 'div' && $op->[2] =~ /^r(\d+)/
       && defined($self->{"nregs"}[$1])) {
      my $newreg = @{$self->{"nregs"}};
      push(@{$self->{"nregs"}}, 1.0/$self->{"nregs"}[$1]);
      $op->[0] = 'mult';
      $op->[2] = 'r'.$newreg;
    }
  }
  $self->{ops} = \@ops;
  1;
}

sub assemble {
  my ($self) = @_;
  my $attr = \%Imager::Regops::Attr;
  my $max_opr = $Imager::Regops::MaxOperands;
  my @ops = @{$self->{ops}};
  for my $op (@ops) {
    $op->[0] = $attr->{$op->[0]}{opcode};
    for (@{$op}[1..$max_opr+1]) { s/^[rpj]// }
  }
  my $pack = $Imager::Regops::PackCode x (2+$Imager::Regops::MaxOperands);

  return join("", ,map { pack($pack, @$_, ) } @ops);
}

# converts stack code to register code
sub stack_to_reg {
  my ($self, @st_ops) = @_;
  my @regstack;
  my %nregs;
  my @vars = $self->_variables();
  my @nregs = (0) x scalar(@vars);
  my @cregs;
  my $attr = \%Imager::Regops::Attr;
  my %vars;
  my %names;
  my $max_opr = $Imager::Regops::MaxOperands;
  @vars{@vars} = map { "r$_" } 0..$#vars;

  my @ops;
  for (@st_ops) {
    if (/^$numre$/) {
      # combining constants makes the optimization below work
      if (exists $nregs{$_}) {
	push(@regstack, $nregs{$_});
      }
      else {
	$nregs{$_} = "r".@nregs;
	push(@regstack,"r".@nregs);
	push(@nregs, $_);
      }
    }
    elsif (exists $vars{$_}) {
      push(@regstack, $vars{$_});
    }
    elsif (exists $attr->{$_} && length $attr->{$_}{types}) {
      if (@regstack < $attr->{$_}{parms}) {
	error("Imager::transform2: stack underflow on $_");
	return;
      }
      my @parms = splice(@regstack, -$attr->{$_}{parms});
      my $types = join("", map {substr($_,0,1)} @parms);
      if ($types ne $attr->{$_}{types}) {
	if (exists $attr->{$_.'p'} && $types eq $attr->{$_.'p'}{types}) {
	  $_ .= 'p';
	}
	else {
	  error("Imager::transform2: Call to $_ with incorrect types");
	  return;
	}
      }
      my $result;
      if ($attr->{$_}{result} eq 'r') {
	$result = "r".@nregs;
	push(@nregs, undef);
      }
      else {
	$result = "p".@cregs;
	push(@cregs, -1);
      }
      push(@regstack, $result);
      push(@parms, "0") while @parms < $max_opr;
      push(@ops, [ $_, @parms, $result ]);
      #print "$result <- $_ @parms\n";
    }
    elsif (/^!(\w+)$/) {
      if (!@regstack) {
	error("Imager::transform2: stack underflow with $_");
	return;
      }
      $names{$1} = pop(@regstack);
    }
    elsif (/^\@(\w+)$/) {
      if (exists $names{$1}) {
	push(@regstack, $names{$1});
      }
      else {
	error("Imager::Expr: unknown storage \@$1");
	return;
      }
    }
    else {
      error("Imager::Expr: unknown operator $_");
      return;
    }
  }
  if (@regstack != 1) {
    error("stack must have only one item at end");
    return;
  }
  if ($regstack[0] !~ /^p/) {
    error("you must have a color value at the top of the stack at end");
    return;
  }
  push(@ops, [ "ret", $regstack[0], (-1) x $max_opr ]);

  $self->{"nregs"} = \@nregs;
  $self->{"cregs"} = \@cregs;

  return \@ops;
}

sub dumpops {
  my $result = '';
  for my $op (@{$_[0]->{ops}}) {
    $result .= "@{$op}\n";
  }
  $result;
}

# unassembles the compiled code
sub dumpcode {
  my ($self) = @_;
  my $code = $self->{"code"};
  my $attr = \%Imager::Regops::Attr;
  my @code = unpack("${Imager::Regops::PackCode}*", $code);
  my %names = map { $attr->{$_}{opcode}, $_ } keys %Imager::Regops::Attr;
  my @vars = $self->_variables();
  my $result = '';
  my $index = 0;
  while (my @op = splice(@code, 0, 2+$Imager::Regops::MaxOperands)) {
    my $opcode = shift @op;
    my $name = $names{$opcode};
    if ($name) {
      $result .= "j$index: $name($opcode)";
      my @types = split //, $attr->{$name}{types};
      for my $parm (@types) {
	my $reg = shift @op;
	$result .= " $parm$reg";
	if ($parm eq 'r') {
	  if ($reg < @vars) {
	    $result.= "($vars[$reg])";
	  }
	  elsif (defined $self->{"nregs"}[$reg]) {
	    $result .= "($self->{\"nregs\"}[$reg])";
	  }
	}
      }

      $result .= " -> $attr->{$name}{result}$op[-1]"
	if $attr->{$name}{result};
      $result .= "\n";
    }
    else {
      $result .= "unknown($opcode) @op\n";
    }
    ++$index;
  }

  $result;
}

package Imager::Expr::Postfix;
use vars qw(@ISA);
@ISA = qw(Imager::Expr);

Imager::Expr::Postfix->register_type('rpnexpr');

my %op_names = ( '+'=>'add', '-'=>'subtract', '*'=>'mult', '/' => 'div',
		 '%'=>'mod', '**'=>'pow' );

sub compile {
  my ($self, $expr, $opts) = @_;

  $expr =~ s/#.*//; # remove comments
  my @st_ops = split ' ', $expr;

  for (@st_ops) {
    $_ = $op_names{$_} if exists $op_names{$_};
    $_ = $self->{constants}{$_} if exists $self->{constants}{$_};
  }
  return $self->stack_to_reg(@st_ops);
}

package Imager::Expr::Infix;

use vars qw(@ISA);
@ISA = qw(Imager::Expr);
use Imager::Regops qw(%Attr $MaxOperands);


eval "use Parse::RecDescent;";
__PACKAGE__->register_type('expr') if !$@;

# I really prefer bottom-up parsers
my $grammar = <<'GRAMMAR';

code : assigns 'return' expr
{ $return = [ @item[1,3] ] }

assigns : assign(s?) { $return = [ @{$item[1]} ] }

assign : identifier '=' expr ';'
{ $return = [ @item[1,3] ] }

expr : relation

relation : addition (relstuff)(s?)
{
  $return = $item[1]; 
  for my $op(@{$item[2]}) { $return = [ $op->[0], $return, $op->[1] ] }
  1;
}

relstuff : relop addition { $return = [ @item[1,2] ] }

relop : '<=' { $return = 'le' }
      | '<' { $return = 'lt' }
      | '==' { $return = 'eq' }
      | '>=' { $return = 'ge' }
      | '>' { $return = 'gt' }
      | '!=' { $return = 'ne' }

addition : multiply (addstuff)(s?) 
{ 
  $return = $item[1]; 
#  for my $op(@{$item[2]}) { $return .= " @{$op}[1,0]"; } 
  for my $op(@{$item[2]}) { $return = [ $op->[0], $return, $op->[1] ] }
  1;
}
addstuff : addop multiply { $return = [ @item[1,2] ] }
addop : '+' { $return = 'add' }
      | '-' { $return = 'subtract' }

multiply : power mulstuff(s?)
{ $return = $item[1]; 
#  for my $op(@{$item[2]}) { $return .= " @{$op}[1,0]"; } 
  for my $op(@{$item[2]}) { $return = [ $op->[0], $return, $op->[1] ] }
  1;
}

mulstuff : mulop power { $return = [ @item[1,2] ] }
mulop : '*' { $return = 'mult' }
      | '/' { $return = 'div' }
      | '%' { $return = 'mod' }

power : powstuff(s?) atom
{
  $return = $item[2]; 
  for my $op(reverse @{$item[1]}) { $return = [ @{$op}[1,0], $return ] }
  1;
}
      | atom
powstuff : atom powop { $return = [ @item[1,2] ] }
powop : '**' { $return = 'pow' }

atom: '(' expr ')' { $return = $item[2] }
     | '-' atom    { $return = [ uminus=>$item[2] ] }
     | number
     | funccall
     | identifier

number : /[+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?/

exprlist : expr ',' exprlist { $return = [ $item[1], @{$item[3]} ] }
         | expr { $return = [ $item[1] ] }

funccall : identifier '(' exprlist ')' 
{ $return = [ $item[1], @{$item[3]} ] }

identifier : /[^\W\d]\w*/ { $return = $item[1] }

GRAMMAR

my $parser;

sub init_parser {
  if (!$parser) {
    $parser = Parse::RecDescent->new($grammar);
  }
}

sub compile {
  my ($self, $expr, $opts) = @_;
  if (!$parser) {
    $parser = Parse::RecDescent->new($grammar);
  }
  my $optree = $parser->code($expr);
  if (!$optree) {
    $self->error("Error in $expr\n");
    return;
  }

  @{$self->{inputs}}{$self->_variables} = ();
  $self->{varregs} = {};
  @{$self->{varregs}}{$self->_variables} = map { "r$_" } 0..$self->_variables-1;
  $self->{"nregs"} = [ (undef) x $self->_variables ];
  $self->{"cregs"} = [];
  $self->{"lits"} = {};

  eval {
    # generate code for the assignments
    for my $assign (@{$optree->[0]}) {
      my ($varname, $tree) = @$assign;
      if (exists $self->{inputs}{$varname}) {
	$self->error("$varname is an input - you can't assign to it");
	return;
      }
      $self->{varregs}{$varname} = $self->gencode($tree);
    }

    # generate the final result
    my $result = $self->gencode($optree->[1]);
    if ($result !~ /^p\d+$/) {
      $self->error("You must return a color value");
      return;
    }
    push(@{$self->{genops}}, [ 'ret', $result, (0) x $MaxOperands ])
  };
  if ($@) {
    $self->error($@);
    return;
  }

  return $self->{genops};
}

sub gencode {
  my ($self, $tree) = @_;

  if (ref $tree) {
    my ($op, @parms) = @$tree;

    if (!exists $Attr{$op}) {
      die "Unknown operator or function $op";
    }

    for my $subtree (@parms) {
      $subtree = $self->gencode($subtree);
    }
    my $types = join("", map {substr($_,0,1)} @parms);

    if (length($types) < length($Attr{$op}{types})) {
      die "Too few parameters in call to $op";
    }
    if ($types ne $Attr{$op}{types}) {
      # some alternate operators have the same name followed by p
      my $opp = $op."p";
      if (exists $Attr{$opp} &&
	  $types eq $Attr{$opp}{types}) {
	$op = $opp;
      }
      else {
	die "Call to $_ with incorrect types";
      }
    }
    my $result;
    if ($Attr{$op}{result} eq 'r') {
      $result = "r".@{$self->{nregs}};
      push(@{$self->{nregs}}, undef);
    }
    else {
      $result = "p".@{$self->{cregs}};
      push(@{$self->{cregs}}, undef);
    }
    push(@parms, "0") while @parms < $MaxOperands;
    push(@{$self->{genops}}, [ $op, @parms, $result]);
    return $result;
  }
  elsif (exists $self->{varregs}{$tree}) {
    return $self->{varregs}{$tree};
  }
  elsif ($tree =~ /^$numre$/ || exists $self->{constants}{$tree}) {
    $tree = $self->{constants}{$tree} if exists $self->{constants}{$tree};

    if (exists $self->{lits}{$tree}) {
      return $self->{lits}{$tree};
    }
    my $reg = "r".@{$self->{nregs}};
    push(@{$self->{nregs}}, $tree);
    $self->{lits}{$tree} = $reg;

    return $reg;
  }
}

1;

__END__

=head1 NAME

Imager::Expr - implements expression parsing and compilation for the 
expression evaluation engine used by Imager::transform2()

=head1 SYNOPSIS

my $code = Imager::Expr->new({rpnexpr=>$someexpr})
  or die "Cannot compile $someexpr: ",Imager::Expr::error();

=head1 DESCRIPTION

This module is used internally by the Imager::transform2() function.
You shouldn't have much need to use it directly, but you may want to
extend it.

To create a new Imager::Expr object, call:

 my %options;
 my $expr = Imager::Expr->new(\%options)
   or die Imager::Expr::error();

You will need to set an expression value and you may set any of the
following:

=over

=item *

constants

A hashref defining extra constants for expression parsing.  The names
of the constants must be valid identifiers (/[^\W\d]\w*/) and the
values must be valid numeric constants (that Perl recognizes in
scalars).

Imager::Expr may define it's own constants (currently just pi.)

=item *

variables

A reference to an array of variable names.  These are allocated
numeric registers starting from register zero.

=back

=for stopwords RPN

By default you can define a C<rpnexpr> key (which emulates RPN) or
C<expr> (an infix expression).  It's also possible to write other
expression parsers that will use other keys.  Only one expression key
should be defined.

=head2 Instance methods

The Imager::Expr::error() method is used to retrieve the error if the
expression object cannot be created.

=head2 Methods

Imager::Expr provides only a few simple methods meant for external use:

=for stopwords VM

=over

=item Imager::Expr->type_registered($keyword)

Returns true if the given expression type is available.  The parameter
is the key supplied to the new() method.

  if (Imager::Expr->type_registered('expr')) {
    # use infix expressions
  }

=item $expr->code()

Returns the compiled code.

=item $expr->nregs()

Returns a reference to the array of numeric registers.

=item $expr->cregs()

Returns a reference to the array of color registers.

=item $expr->dumpops()

Returns a string with the generated VM "machine code".

=item $expr->dumpcode()

Returns a string with the disassembled VM "machine code".

=back

=head2 Creating a new parser

I'll write this one day.

Methods used by parsers:

=over

=item compile

This is the main method you'll need to implement in a parser.  See the
existing parsers for a guide.

It's supplied the following parameters:

=over

=item *

$expr - the expression to be parsed

=item *

$options - the options hash supplied to transform2.

=back

Return an array ref of array refs containing opcodes and operands.

=item @vars = $self->_variables()

A list (not a reference) of the input variables.  This should be used
to allocate as many registers as there are variable as input
registers.

=item $self->error($message)

Set the return value of Imager::Expr::error()

=item @ops = $self->stack_to_reg(@stack_ops)

Converts marginally parsed RPN to register code.

=item assemble()

Called to convert op codes into byte code.

=item numre()

Returns a regular expression that matches floating point numbers.

=item optimize()

Optimizes the assembly code, including attempting common subexpression
elimination and strength reducing division by a constant into
multiplication by a constant.

=item register_type()

Called by a new expression parser implementation to register itself,
call as:

  YourClassName->register_type('type code');

where type code is the parameter that will accept the expression.

=back

=head2 Future compatibility

Try to avoid doing your own optimization beyond literal folding - if
we add some sort of jump, the existing optimizer will need to be
rewritten, and any optimization you perform may well be broken too
(well, your code generation will probably be broken anyway <sigh>).

=cut
