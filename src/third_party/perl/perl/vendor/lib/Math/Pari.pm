=head1 NAME

Math::Pari - Perl interface to PARI.

=head1 SYNOPSIS

  use Math::Pari;
  $a = PARI 2;
  print $a**10000;

or

  use Math::Pari qw(Mod);
  $a = Mod(3,5);
  print $a**10000;

=head1 DESCRIPTION

This package is a Perl interface to famous library PARI for
numerical/scientific/number-theoretic calculations.  It allows use of
most PARI functions as Perl functions, and (almost) seamless merging
of PARI and Perl data. In what follows we suppose prior knowledge of
what PARI is (see L<ftp://megrez.math.u-bordeaux.fr/pub/pari>, or
L<Math::libPARI>).

=head1 EXPORTed functions

=over 4

=item DEFAULT

By default the package exports functions PARI(), PARIcol(), PARIvar(),
PARImat() and PARImat_tr() which convert their argument(s) to a
PARI object. (In fact PARI() is just an alias for C<new Math::Pari>).
The function PARI() accepts following data as its arguments

=over 17

=item One integer

Is converted to a PARI integer.

=item One float

Is converted to a PARI float.

=item One string

Is executed as a PARI expresion (so should not contain whitespace).

=item PARI object

Is passed unchanged.

=item Reference to a Perl array

Each element is converted using the same rules, PARI vector-row with these
elements is returned.

=item Several of above

The same as with a reference to array.

=back

=item Conflicts of rules in PARI()

In deciding what rule of the above to apply the preference is given to
the uppermost choice of those available I<now>.  If none matches, then
the string rule is used.  So C<PARI(1)> returns integer, C<PARI(1.)>
returns float, C<PARI("1")> evaluates C<1> as a PARI expression (well,
the result is the same as C<PARI(1)>, only slower).

Note that for Perl these data are synonimous, since Perl freely
converts between integers, float and strings.  However, to PARI() only
what the argument I<is now> is important.  If $v is C<1> in the Perl
world, C<PARI($v)> may convert it to an integer, float, or to the
result of evaluating the PARI program C<1> (all depending on how $v
was created and accessed in Perl).

This is a fundamental limitation of creating an interface between two
systems, both with polymorphic objects, but with subtly different
semantic of the flavors of these objects.  In reality, however, this
is rarely a problem.

=item PARIcol(), PARImat() and PARImat_tr()

PARIcol() behaves in the same way as PARI() unless given several
arguments. In the latter case it returns a vector-column instead of
a vector-row.

PARImat() constructs a matrix out of the given arguments. It will work
if PARI() will construct a vector of vectors given the same arguments.
The internal vectors become columns of the matrix.  PARImat_tr()
behaves similarly, but the internal vectors become rows of the matrix.

Since PARI matrices are similar to vector-rows of vector-columns,
PARImat() is quickier, but PARImat_tr() better corresponds to the PARI
input and output forms of matrices:

  print PARImat    [[1,2], [3,4]];	# prints [1,3;2,4]
  print PARImat_tr [[1,2], [3,4]];	# prints [1,2;3,4]

=item C<use> with arguments

If arguments are specified in the C<use Math::Pari> directive, the
PARI functions appearing as arguments are exported in the caller
context. In this case the function PARI() and friends is not exported,
so if you need them, you should include them into export list
explicitely, or include C<:DEFAULT> tag:

  use Math::Pari qw(factorint PARI);
  use Math::Pari qw(:DEFAULT factorint);

or simply do it in two steps

  use Math::Pari;
  use Math::Pari 'factorint';

The other tags recognized are C<:PARI>, C<:all>, C<prec=NUMBER>,
number tags (e.g., C<:4>), overloaded constants tags (C<:int>,
C<:float>, C<:hex>) and section names tags.  The number tags export
functions from the PARI library from the given class (except for
C<:PARI>, which exports all of the classes).  Tag C<:all> exports all of the
exportable symbols and C<:PARI>.

Giving C<?> command to C<gp> (B<PARI> calculator) lists the following classes:

  1: Standard monadic or dyadic OPERATORS
  2: CONVERSIONS and similar elementary functions
  3: TRANSCENDENTAL functions
  4: NUMBER THEORETICAL functions
  5: Functions related to ELLIPTIC CURVES
  6: Functions related to general NUMBER FIELDS
  7: POLYNOMIALS and power series
  8: Vectors, matrices, LINEAR ALGEBRA and sets
  9: SUMS, products, integrals and similar functions
  10: GRAPHIC functions
  11: PROGRAMMING under GP

One can use section names instead of number tags.  Recognized names are

  :standard :conversions :transcendental :number :elliptic
  :fields :polynomials :vectors :sums :graphic :programming

One can get the list of all of the functions accessible by C<Math::Pari>,
or the accessible functions from the given section using listPari() function.

Starting from version 5.005 of Perl, three constant-overload tags are
supported: C<:int>, C<:float>, C<:hex>.  If used, all the
integer/float/hex-or-octal-or-binary literals in Perl will be automatically
converted to became PARI objects.  For example,

  use Math::Pari ':int';
  print 2**1000;

is equivalent to

  print PARI(2)**PARI(1000);

(The support for this Perl feature is buggy before the Perl version 5.005_57 -
unless Perl uses mymalloc options; you can check for this with C<perl
-V:usemymalloc>.)  Note also that (at least with some versions of Perl)
one should enable C<':float'> for conversion of long integer literals
(I<Perl> may consider them as floats, since they won't fit into Perl
integers); note that it is PARI which determines which PARI subtype is
assigned to each such literal:

  use Math::Pari ':float', 'type_name';
  print type_name 22222222222222222222222;

prints C<t_INT>.

=back

=head1 Available functions

=head2 Directly accessible from Perl

This package supports I<all> the functions from the PARI library with
a I<signature> which can be recognized by Math::Pari.  This means that
when you update the PARI library, the newly added functions will we
available without any change to this package; only a recompile is
needed.  In fact no recompile will be needed if you link libPARI
dynamically (you need to modify the F<Makefile> manually to do
this).

You can "reach" unsupported functions via going directly to PARI
parser using the string flavor of PARI() function, as in

  3 + PARI('O(x^17)');

For some "unreachable" functions there is a special wrapper functions,
such as C<O(variable,power)>).

The following functions are specific to GP calculator, thus are not
available to Math::Pari in any way:

  default error extern input print print1 printp printp1
  printtex quit read system whatnow write write1 writetex

whatnow() function is useless, since Math::Pari does not support the
"compatibility" mode (with older PARI library).  The functionality of
print(), write() and variants is available via automatic string
translation, and pari_print() function and its variants (see L<Printout functions>).

default() is the only important function with functionality not
supported by the current interface.  Note however, that four most
important default() actions are supported by allocatemem(),
setprimelimit(), setprecision() and setseriesprecision() functions.
(When called without arguments, these functions return the current
values.)

allocatemem($bytes) should not be called from inside Math::Pari
functions (such as forprimes()).

=head2 Arguments

Arguments to PARI functions are automatically converted to C<long> or
a PARI object depending on the signature of the actual library function.
The arguments are I<forced> into the given type, so even if C<gp>
rejects your code similar to

  func(2.5);			# func() takes a long in C

arguing that a particular argument should be of C<type T_INT> (i.e., a
Pari integer), the corresponding code will work in C<Math::Pari>,
since 2.5 is silently converted to C<long>, per the function
signature.

=head2 Return values

PARI functions return a PARI object or a Perl's integer depending on
what the actual library function returns.

=head2 Additional functions

Some PARI functions are available in C<gp> (i.e., in C<PARI>
calculator) via infix notation only. In C<Math::Pari> these functions
are available in functional notations too.  Some other convenience
functions are also made available.

=over 5

=item Infix, prefix and postfix operations

are available under names

  gneg, gadd, gsub, gmul, gdiv, gdivent, gmod, gpui,
  gle, gge, glt, ggt, geq, gne, gegal, gor, gand,
  gcmp, gcmp0, gcmp1, gcmp_1.

C<gdivent> means euclidean quotient, C<gpui> is power, C<gegal> checks
whether two objects are equal, C<gcmp> is applicable to two real
numbers only, C<gcmp0>, C<gcmp1>, C<gcmp_1> compare with 0, 1 and -1
correspondingly (see PARI user manual for details, or
L<Math::libPARI>).  Note that all these functions are more readily
available via operator overloading, so instead of

  gadd(gneg($x), $y)

one can write

  -$x+$y

(as far as overloading may be triggered, see L<overload>, so we assume
that at least one of $x or $y is a PARI object).

=item Conversion functions

  pari2iv, pari2nv, pari2num, pari2pv, pari2bool

convert a PARI object to an integer, float, integer/float (whatever is
better), string, and a boolean value correspondingly. Most the time
you do not need these functions due to automatic conversions.

=item Printout functions

  pari_print, pari_pprint, pari_texprint

perform the same conversions to strings as their PARI counterparts,
but do not print the result.  The difference of pari_print() with
pari2pv() is the number of significant digits they output, and
whitespace in the output.  pari2pv(), which is intended for
"computer-readable strings", outputs as many digits as is supported by
the current precision of the number; while pari_print(), which targets
human-readable strings, takes into account the currently specified
output precision too.

=item Constant functions

Some mathematical constants appear as function without arguments in
PARI.  These functions are available in Math::Pari too.  If you export
them as in

  use Math::Pari qw(:DEFAULT Pi I Euler);

they can be used as barewords in your program:

  $x = Pi ** Euler;

=item Low-level functions

For convenience of low-level PARI programmers some low-level functions
are made available as well (all except type_name() and changevalue()
are not exportable):

  typ($x)
  lg($x)
  lgef($x)
  lgefint($x)
  longword($x, $n)
  type_name($x)
  changevalue($name,$newvalue)

Here longword($x,$n) returns C<$n>-th word in the memory
representation of $x (including non-code words).  type_name() differs
from the PARI function type(): type() returns a PARI object, while
type_name() returns a Perl string.  (PARI objects of string type
behave very non-intuitive w.r.t. string comparison functions; remember
that they are compared using lex() to I<the results of evaluation> of
other argument of comparison!)

The function listPari($number) outputs a list of names of PARI
functions in the section $number.  Use listPari(-1) to get the list
across all of the sections.

=item Uncompatible functions

  O

Since implementing C<O(7**6)> would be very tedious, we provide a
two-argument form C<O(7,6)> instead (meaning the same as C<O(7^6)> in
PARI).  Note that with polynomials there is no problem like this one,
both C<O($x,6)> and C<O($x**6)> work.

  ifact(n)

integer factorial functions, available from C<gp> as C<n!>.

=back

=head2 Looping functions

PARI has a big collection of functions which loops over some set.
Such a function takes two I<special> arguments: loop variable, and the
code to execute in the loop.

The code can be either a string (which contains PARI code to execute -
thus should not contain whitespace), or a Perl code reference.  The
loop variable can be a string giving the name of PARI variable (as in

  fordiv(28, 'j', 'a=a+j+j^2');

or

  $j= 'j';
  fordiv(28, $j, 'a=a+j+j^2');

), a PARI monomial (as in

  $j = PARI 'j';
  fordiv(28, $j, sub { $a += $j + $j**2 });

), or a "delayed Math::Pari variable" (as in

  $j = PARIvar 'j';
  fordiv(28, $j, 'a=a+j+j^2');

).  If none of these applies, as in

  my $j;	# Have this in a separate statement
  fordiv(28, $j, sub { $a += $j + $j**2 });

then during the execution of the C<sub>, Math::Pari would autogenerate
a PARI variable, and would put its value in $j; this value of $j is
temporary only, the old contents of $j is restored when fordiv() returns.

Note that since you have no control over this name, you will not be
able to use this variable from your PARI code; e.g.,

  $j = 7.8;
  fordiv(28, $j, 'a=a+j+j^2');

will not make C<j> mirror $j (unless you explicitely set up C<j> to be
a no-argument PARI function mirroring $j, see L<"Accessing Perl functions from PARI code">).

B<Caveats>.  There are 2 flavors of the "code" arguments
(string/C<sub>), and 4 types of the "variable" arguments
(string/monomial/C<PARIvar>/other).  However, not all 8 combinations
make sense.  As we already explained, an "other" variable cannot work
with a "string" code.

B<Useless musing alert! Do not read the rest of this section!> Do not
use "string" variables with C<sub> code, and do not ask I<why>!

Additionally, the following code will not do what you expect

  $x = 0;
  $j = PARI 'j';
  fordiv(28, 'j', sub { $x += $j } );	# Use $j as a loop variable!

since the PARI function C<fordiv> I<localizes> the PARI variable C<j>
inside the loop, but $j will still reference the old value; the old
value is a monomial, not the index of the loop (which is an integer
each time C<sub> is called).  The simplest workaround is not to use
the above syntax (i.e., not mixing literal loop variable with Perl
loop code, just using $j as the second argument to C<fordiv> is
enough):

  $x = 0;
  $j = PARI 'j';
  fordiv(28, $j, sub { $x += $j } );

Alternately, one can make a I<delayed> variable $j which will always
reference the same thing C<j> references in PARI I<now> by using
C<PARIvar> constructor

  $x = 0;
  $j = PARIvar 'j';
  fordiv(28, 'j', sub { $x += $j } );

(This problem is similar to

  $ref = \$_;			# $$ref is going to be old value even after
				# localizing $_ in Perl's grep/map

not accessing localized values of $_ in the plain Perl.)

Another possible quirk is that

  fordiv(28, my $j, sub { $a += $j + $j**2 });

will not work too - by a different reason.  C<my> declarations change
the I<meaning> of $j only I<after> the end of the current statement;
thus $j inside C<sub> will access a I<different> variable $j
(typically a non-lexical, global variable $j) than one you declared on this line.

=head2 Accessing Perl functions from PARI code

Use the same name inside PARI code:

  sub counter { $i += shift; }
  $i = 145;
  PARI 'k=5' ;
  fordiv(28, 'j', 'k=k+counter(j)');
  print PARI('k'), "\n";

prints

   984

Due to a difference in the semantic of
variable-number-of-parameters-functions between PARI and Perl, if the
Perl subroutine takes a variable number of arguments (via C<@> in the
prototype or a missing prototype), up to 6 arguments are supported
when this function is called from PARI.  If called from PARI with
fewer arguments, the rest of arguments will be set to be integers C<PARI 0>.

Note also that no direct import of Perl variables is available yet
(but you can write a function wrapper for this):

  sub getv () {$v}

There is an unsupported (and undocumented ;-) function for explicitely
importing Perl functions into PARI, possibly with a different name,
and possibly with explicitely specifying number of arguments.

=head1 PARI objects

Functions from PARI library may take as arguments and/or return values
the objects of C type C<GEN>. In Perl these data are encapsulated into
special kind of Perl variables: PARI objects. You can check for a
variable C<$obj> to be a PARI object using

  ref $obj and $obj->isa('Math::Pari');

Most the time you do not need this due to automatic conversions and overloading.

=head1 PARI monomials and Perl barewords

If very lazy, one can code in Perl the same way one does it in PARI.
Variables in PARI are denoted by barewords, as in C<x>, and in the
default configuration (no warnings, no strict) Perl allows the same -
up to some extent.  Do not do this, since there are many surprising problems.

Some bareletters denote Perl operators, like C<q>, C<x>, C<y>,
C<s>. This can lead to errors in Perl parsing your expression. E.g.,

  print sin(tan(t))-tan(sin(t))-asin(atan(t))+atan(asin(t));

may parse OK after C<use Math::Pari qw(sin tan asin atan)>.  Why?

After importing, the word C<sin> will denote the PARI function sin(),
not Perl operator sin().  The difference is subtle: the PARI function
I<implicitly> forces its arguments to be converted PARI objects; it
gets C<'t'> as the argument, which is a string, thus is converted to
what C<t> denotes in PARI - a monomial.  While the Perl operator sin()
grants overloading (i.e., it will call PARI function sin() if the
argument is a PARI object), it does not I<force> its argument; given
C<'t'> as argument, it converts it to what sin() understands, a float
(producing C<0.>), so will give C<0.> as the answer.

However

  print sin(tan(y))-tan(sin(y))-asin(atan(y))+atan(asin(y));

would not compile. You should avoid lower-case barewords used as PARI
variables, e.g., do

  $y = PARI 'y';
  print sin(tan($y))-tan(sin($y))-asin(atan($y))+atan(asin($y));

to get

  -1/18*y^9+26/4725*y^11-41/1296*y^13+328721/16372125*y^15+O(y^16)

(BTW, it is a very good exercise to get the leading term by hand).

Well, the same advice again: do not use barewords anywhere in your program!

=head1 Overloading and automatic conversion

Whenever an arithmetic operation includes at least one PARI object,
the other arguments are converted to a PARI object and the corresponding
PARI library functions is used to implement the operation.  Currently
the following arithmetic operations are overloaded:

  unary -
  + - * / % ** abs cos sin exp log sqrt
  << >>
  <= == => <  >  != <=>
  le eq ge lt gt ne cmp
  | & ^ ~

Numeric comparison operations are converted to C<gcmp> and friends, string
comparisons compare in lexicographical order using C<lex>.

Additionally, whenever a PARI object appears in a situation that requires integer,
numeric, boolean or string data, it is converted to the corresponding
type. Boolean conversion is subject to usual PARI pitfalls related to
imprecise zeros (see documentation of C<gcmp0> in PARI reference).

For details on overloading, see L<overload>.

Note that a check for equality is subject to same pitfalls as in PARI
due to imprecise values.  PARI may also refuse to compare data of
different types for equality if it thinks this may lead to
counterintuitive results.

Note also that in PARI the numeric ordering is not defined for some
types of PARI objects.  For string comparison operations we use
PARI-lexicographical ordering.

=head1 PREREQUISITES

=head2 Perl

In the versions of perl earlier than 5.003 overloading used a
different interface, so you may need to convert C<use overload> line
to C<%OVERLOAD>, or, better, upgrade.

=head2 PARI

Starting from version 2.0, this module comes without a PARI library included.

For the source of PARI library see
L<ftp://megrez.math.u-bordeaux.fr/pub/pari>.

=head1 Perl vs. PARI: different syntax

Note that the PARI notations should be used in the string arguments to
PARI() function, while the Perl notations should be used otherwise.

=over 4

=item C<^>

Power is denoted by C<**> in Perl.

=item C<\> and C<\/>

There are no such operators in Perl, use the word forms
C<gdivent(x,y)> and C<gdivround(x,y)> instead.

=item C<~>

There is no postfix C<~> Perl operator.  Use mattranspose() instead.

=item C<'>

There is no postfix C<'> Perl operator.  Use deriv() instead.

=item C<!>

There is no postfix C<!> Perl operator.  Use factorial()/ifact() instead
(returning a real or an integer correspondingly).

=item big integers

Perl converts big I<literal> integers to doubles if they could not be
put into B<C> integers (the particular flavor can be found in the
output of C<perl -V> in newer version of Perl, look for
C<ivtype>/C<ivsize>).  If you want to input such an integer, use

  while ($x < PARI('12345678901234567890')) ...

instead of

  while ($x < 12345678901234567890) ...

Why?  Because conversion to double leads to precision loss (typically
above 1e15, see L<perlnumber>), and you will get something like
12345678901234567168 otherwise.

Starting from version 5.005 of Perl, if the tag C<:int> is used on the
'use Math::Pari' line, all of the integer literals in Perl will be
automatically converted to became PARI objects.  E.g.,

  use Math::Pari ':int';
  print 2**1000;

is equivalent to

  print PARI(2)**PARI(1000);

Similarly, large integer literals do not lose precision.

This directive is lexically scoped.  There is a similar tag C<:hex>
which affects hexadecimal, octal and binary constants.  One may
also need to use tag C<:float> for auto-conversion of large integer literals
which Perl considers as floating point literals (see L<C<use> with arguments>
for details).

=item doubles

Doubles in Perl are typically of precision approximately 15 digits
(see L<perlnumber>).  When you use them as arguments to PARI
functions, they are converted to PARI real variables, and due to
intermediate 15-digits-to-binary conversion of Perl variables the
result may be different than with the PARI many-digits-to-binary
conversion.  E.g., C<PARI(0.01)> and C<PARI('0.01')> differ at 19-th
place, as

  setprecision(38);
  print pari_print(0.01),   "\n",
        pari_print('0.01'), "\n";

shows.

Note that setprecision() changes the output format of pari_print() and
friends, as well as the default internal precision.  The generic
PARI===>string conversion does not take into account the output
format, thus

  setprecision(38);
  print PARI(0.01),       "\n",
        PARI('0.01'),     "\n",
        pari_print(0.01), "\n";

will print all the lines with different number of digits after the
point: the first one with 22, since the double 0.01 was converted to a
low-precision PARI object, the second one with 41, since internal form
for precision 38 requires that many digits for representation, and the
last one with 39 to have 38 significant digits.

Starting from version 5.005 of Perl, if the tag C<:float> is used on
the C<use Math::Pari> line, all the float literals in Perl will be
automatically converted to became PARI objects.  E.g.,

  use Math::Pari ':float';
  print atan(1.);

is equivalent to

  print atan(PARI('1.'));

Similarly, large float literals do not lose precision.

This directive is lexically scoped.

=item array base

Arrays are 1-based in PARI, are 0-based in Perl.  So while array
access is possible in Perl, you need to use different indices:

  $nf = PARI 'nf';	# assume that PARI variable nf contains a number field
  $a = PARI('nf[7]');
  $b = $nf->[6];

Now $a and $b contain the same value.

=item matrices

Note that C<PARImat([[...],...,[...])> constructor creates a matrix
with specified columns, while in PARI the command C<[1,2,3;4,5,6]>
creates a matrix with specified rows.  Use a convenience function
PARImat_tr() which will transpose a matrix created by PARImat() to use
the same order of elements as in PARI.

=item builtin perl functions

Some PARI functions, like C<length> and C<eval>, are Perl
(semi-)reserved words.  To reach these functions, one should either
import them:

  use Math::Pari qw(length eval);

or call them with prefix (like C<&length>) or the full name (like
C<Math::Pari::length>).

=back

=head1 High-resolution graphics

If you have Term::Gnuplot Perl module installed, you may use high-resolution
graphic primitives of B<PARI>.  Before the usage you need to establish
a link between Math::Pari and Term::Gnuplot by calling link_gnuplot().
You can change the output filehandle by calling set_plot_fh(), and
output terminal by calling plotterm(), as in

    use Math::Pari qw(:graphic asin);

    open FH, '>out.tex' or die;
    link_gnuplot();		# automatically loads Term::Gnuplot
    set_plot_fh(\*FH);
    plotterm('emtex');
    ploth($x, .5, .999, sub {asin $x});
    close FH or die;

=head1 libPARI documentation

libPARI documentation is included, see L<Math::libPARI>.  It is converted
from Chapter 3 of B<PARI/GP> documentation by the F<gphelp> script of GP/PARI.

=head1 ENVIRONMENT

No environment variables are used.

=head1 BUGS

=over 5

=item *

A few of PARI functions are available indirectly only.

=item *

Using overloading constants with the Perl versions below 5.005_57 could lead to
segfaults (at least without C<-D usemymalloc>), as in:

  use Math::Pari ':int';
  for ( $i = 0; $i < 10 ; $i++ ) { print "$i\n" }

=item *

It may be possible that conversion of a Perl value which has both the
integer slot and the floating slot set may create a PARI integer, even
if the actual value is not an integer.

=item *

problems with refcounting of array elements and Mod().

Workaround: make the modulus live longer than the result of Mod().
Until Perl version C<5.6.1>, one should exercise a special care so
that the modulus goes out of scope on a different statement than the
result:

  { my $modulus = 125;
    { my $res = Mod(34, $modulus);
      print $res;
    }
    $fake = 1;		# A (fake) statement here is required
  }

Here $res is destructed before the C<$fake = 1> statement, $modulus is
destructed before the first statement after the provided block.
However, if you remove the C<$fake = 1> statement, both these
variables are destructed on the first statement after the provided
block (and in a wrong order!).

In C<5.6.1> declaring $modulus before $res is all that is needed to
circumvent the same problem:

  { my $modulus = 125;
    my $res = Mod(34, $modulus);
    print $res;
  }			# destruction will happen in a correct order.

Access to array elements may result in similar problems.  Hard to fix
since in PARI the data is not refcounted.

=item *

Legacy implementations of dynalinking require the code of DLL to be
compiled to be "position independent" code (PIC).  This slows down the
execution, while allowing sharing the loaded copy of the DLL between
different processes.  [On contemeporary architectures the same effect
is allowed without the position-independent hack.]

Currently, PARI assembler files are not position-independent.  When
compiled for the dynamic linking on legacy systems, this creates a DLL
which cannot be shared between processes.  Some legacy systems are
reported to recognize this situation, and load the DLL as a non-shared
module.  However, there may be systems (are there?) on which this can
cause some "problems".

Summary: if the dynaloading on your system requires some kind of C<-fPIC> flag, using "assembler" compiles (anything but C<machine=none>) *may* force you to do a static build (i.e., creation of a custom Perl executable with

 perl Makefile.PL static
 make perl
 make test_static

).

=back

=head1 INITIALIZATION

When Math::Pari is loaded, it examines variables $Math::Pari::initmem
and $Math::Pari::initprimes.  They specify up to which number the
initial list of primes should be precalculated, and how large should
be the arena for PARI calculations (in bytes).  (These values have
safe defaults.)

Since setting these values before loading requires either a C<BEGIN>
block, or postponing the loading (C<use> vs. C<require>), it may be
more convenient to set them via Math::PariInit:

  use Math::PariInit qw( primes=12000000 stack=1e8 );

C<use Math::PariInit> also accepts arbitrary Math::Pari import directives,
see L<Math::PariInit>.

These values may be changed at runtime too, via allocatemem() and
setprimelimit(), with performance penalties for recalculation/reallocation.

=head1 AUTHOR

Ilya Zakharevich, I<ilyaz@cpan.org>

=cut

# $Id: Pari.pm,v 1.3 1994/11/25 23:40:52 ilya Exp ilya $
package Math::Pari::Arr;

#sub TIEARRAY { $_[0] }
sub STORE { die "Storing into array elements unsupported" }

package Math::Pari;

require Exporter;
require DynaLoader;
#use autouse Carp => 'croak';

@ISA = qw(Exporter DynaLoader);
@Math::Pari::Ep::ISA = qw(Math::Pari);

# Items to export into callers namespace by default
# (move infrequently used names to @EXPORT_OK below)

@EXPORT = qw(
PARI PARIcol PARImat PARIvar PARImat_tr
);

# Other items we are prepared to export if requested (may be extended during
# ->import. )
@EXPORT_OK = qw(
  sv2pari sv2parimat pari2iv pari2nv pari2num pari2pv pari2bool loadPari _bool
  listPari pari_print pari_pprint pari_texprint O ifact gdivent gdivround
  changevalue set_plot_fh link_gnuplot setprecision setseriesprecision
  setprimelimit allocatemem type_name pari2num_
);

use subs qw(
   _gneg
   _gadd
   _gsub
   _gmul
   _gdiv
   _gmod
   _gpui
   _gle
   _gge
   _glt
   _ggt
   _geq
   _gne
   _gcmp
   _lex
   _2bool
   pari2pv
   pari2num
   pari2num_
   _abs
   _cos
   _sin
   _exp
   _log
   _sqrt
   _gbitand
   _gbitor
   _gbitxor
   _gbitneg
   _to_int
   _gbitshiftr
   _gbitshiftl
);		# Otherwise overload->import would complain...

my $two;

sub _shiftl {
  my ($left,$right) = (shift,shift);
  ($left,$right) = ($right, $left) if shift;
  $left * $two**$right;
}

sub _shiftr {
  my ($left,$right) = (shift,shift);
  ($left,$right) = ($right, $left) if shift;
  floor($left / $two**$right);
}

$initmem ||= 4000000;		# How much memory for the stack
$initprimes ||= 500000;		# Calculate primes up to this number

$VERSION = '2.01080605';

my $true = 1;
# Propagate sv_true, sv_false to SvIOK:
my $dummy = pack 'ii', $true == 1, $true == 0;

bootstrap Math::Pari;

use overload qw(
   neg _gneg
   + _gadd
   - _gsub
   * _gmul
   / _gdiv
   % _gmod
   ** _gpui
   <= _gle
   >= _gge
   < _glt
   > _ggt
   == _geq
   != _gne
   <=> _gcmp
   cmp _lex
   bool _2bool
   "" pari2pv
   0+ pari2num_
   abs _abs
   cos _cos
   sin _sin
   exp _exp
   log _log
   sqrt _sqrt
   int _to_int
);

if (pari_version_exp() >= 2000018) {
  'overload'->import( qw(
			 | _gbitor
			 & _gbitand
			 ^ _gbitxor
			 ~ _gbitneg
			) );
}

if (pari_version_exp() >= 2002001) {
  'overload'->import( qw( << _gbitshiftl ) );
} else {
  'overload'->import( qw( << _shiftl ) );
}
if (pari_version_exp() >= 2002001 && pari_version_exp() <= 2002007) {
  'overload'->import( qw( >> _gbitshiftr ) );
} else {
  'overload'->import( qw( >> _shiftr ) );
}

sub AUTOLOAD {
  $AUTOLOAD =~ /^(?:Math::Pari::)?(.*)/;
  # warn "Autoloading $1...\n";
  # exit 4 if $1 eq 'loadPari';
  my $cv = loadPari($1);

#  goto &$cv;
#  goto &$AUTOLOAD;
#  &$cv;
  &$1;
#  &$AUTOLOAD;
}

# Needed this guy to circumwent autoloading while no XS definition

#### sub DESTROY {}


# sub AUTOLOAD {
#     if ((caller(0))[4]) {
# 	$AutoLoader::AUTOLOAD = $AUTOLOAD;
# 	goto &AutoLoader::AUTOLOAD;
#     }
#     local($constname);
#     ($constname = $AUTOLOAD) =~ s/.*:://;
#     $val = constant($constname, @_ ? $_[0] : 0);
#     if ($! != 0) {
# 	if ($! =~ /Invalid/) {
# 	    $AutoLoader::AUTOLOAD = $AUTOLOAD;
# 	    goto &AutoLoader::AUTOLOAD;
# 	}
# 	else {
# 	    ($pack,$file,$line) = caller;
# 	    die "Your vendor has not defined Math::Pari macro $constname, used at $file line $line.
# ";
# 	}
#     }
#     eval "sub $AUTOLOAD { $val }";
#     goto &$AUTOLOAD;
# }

# Preloaded methods go here.  Autoload methods go after __END__, and are
# processed by the autosplit program.

sub new {
  shift;
  if (@_>1) {my(@t)=@_;return sv2pari(\@t)}
  return sv2pari(shift);
}

###sub PARI {new Math::Pari @_}

%names    = qw(
	       1 standard
	       2 conversions
	       3 transcendental
	       4 number
	       5 elliptic
	       6 fields
	       7 polynomials
	       8 vectors
	       9 sums
	       10 graphic
	       11 programming
	      );
@sections{values %names} = keys %names;

@converted{split /,\s+/, qq(buchimag, buchreal,
    buchgen, buchgenforcefu, buchgenfu, buchinit, buchinitforcefu, buchinitfu,
    plotstring, addhelp, kill)} = (1) x 100;

# Now even tested...
sub _cvt { PARI(shift) }
sub _hex_cvt {
  my $in = shift;
  my $mult = PARI(1);
  my $ret = 0;
  my $shift = 1<<(4*7);

  $in =~ s/^0([xb])?// or die;
  my $hex = $1;
  if ($hex and $1 eq 'b') {
    my $b = '0' x (15 * length($in) % 16) . $in;
    $hex = '';
    while ($b) {
      my $s = substr $b, 0, 16;
      substr($b, 0, 16) = '';
      $hex .= unpack 'H4', pack 'B16', $s;
    }
    $in = $hex;
  }
  $shift = 1<<(3*7) unless $hex;
  while ($in =~ s/([a-fA-F\d]{1,7})$//) {
    # In 5.6.0 hex() can return a floating number:
    my $part = int($hex ? hex $1 : oct $1);

    $ret += $part * $mult;
    $mult *= $shift;
  }
  die "Cannot hex '$in'" if length $in;
  return $ret;
}
%overloaded_const = ( 'int' => \&_cvt, float => \&_cvt, 'hex' => \&_hex_cvt);
%overloaded_const_word
  = ( 'int' => 'integer', float => 'float', 'hex' => 'binary');

sub import {
  my $p=shift;
  my @consts;			# Need to do it outside any block!
  @_ = map {
    if (/^:(?!DEFAULT)(.*)/) {
      my $tag = $1;
      my $sect = $tag;
      my @pre;
      $tag = -1, @pre = (@EXPORT_OK,@EXPORT) if ($tag eq 'all');
      $tag = -1 if ($tag eq 'PARI');
      $tag = $sections{$tag} if $tag !~ /^-?\d+$/ and exists $sections{$tag};
      push @pre, 'link_gnuplot', 'set_plot_fh' if $tag eq 10;
      if ($tag =~ /^prec=(\d+)$/) {
	setprecision($1);
	();
      } elsif ($tag =~ /^(int|hex|float)$/) {
	die "Overloaded constants are not supported in this version of Perl"
	  if $] < 5.004_69;
	push @const, $overloaded_const_word{$tag} => $overloaded_const{$tag};
	# print "Constants: $overloaded_const_word{$tag} => $overloaded_const{$tag} \n";
	();
      } elsif (defined $tag and $tag =~ /^-?\d+$/) {
	(@pre, listPari($tag));
      } else {
	die "Unknown section '$sect' specified";
      }
    } else {
      ($_);
    }
  } @_;

  overload::constant(splice @const, 0, 2) while @const;

  # print "EXPORT_OK: @EXPORT_OK\n";
  push @EXPORT_OK,
      grep( ($_ ne ':DEFAULT'
	     and not $export_ok{$_}++
	     and (eval {loadPari($_), 1} or warn $@), !$@) ,
	    @_);
  # Invalidate Exporter cache, so that new %EXPORT_OK is noticed:
  undef %EXPORT;
  # print "EXPORT_OK: @EXPORT_OK\n";
  &Exporter::export($p,(caller(0))[0],@_);
}

sub _can {			# Without taking into account inheritance...
  my ($obj, $meth) = (shift, shift);
  return \&$meth if defined &$meth;
  return \&$meth if eval {loadPari($meth), 1};
  return;
}

sub can {
  my ($obj, $meth) = (@_);
  my $f = $obj->SUPER::can($meth);
  return $f if defined $f;
  # There is no "usual" way to get the function; try loadPari()
  $f = eval { loadPari($meth) };
  return $f if defined $f;
  return;
}

sub O ($;$) {
  return PARI("O($_[0]^$_[1])") if @_ == 2;
  return PARI("O($_[0])") if typ($_[0]) == 10; # Poly
  Carp::croak("O(number**power) not implemented, use O(number,power) instead");
}

sub PARImat_tr {mattranspose(PARImat(@_))}
#sub string ($$) {
#  PARI (qq'string($_[0],"$_[1]")');
#}

sub installPerlFunction {my @a=@_; $a[0] = \&{$a[0]}; installPerlFunctionCV(@a)}

my $name;

for $name (keys %converted) {
  push @EXPORT_OK, $name;
  next if defined &$name;
  # string needs to format numbers to 8.3...
  if ($name eq 'addhelp' or $name eq 'plotstring') {
    *$name = sub { PARI ( qq($name($_[0],"$_[1]")) ) }
  } else {
    *$name = sub { local $"=','; PARI("$name(@_)") }
  }
}

@export_ok{@EXPORT_OK,@EXPORT} = (1) x (@EXPORT_OK + @EXPORT);

sub link_gnuplot {
    eval 'use Term::Gnuplot 0.56; 1' or die;
    int_set_term_ftable(Term::Gnuplot::get_term_ftable());
}

sub set_plot_fh {
  eval 'use Term::Gnuplot 0.4; 1' or die;
  Term::Gnuplot::set_gnuplot_fh(@_);
}

PARI_DEBUG_set($ENV{MATHPARI_DEBUG} || 0);
$two = PARI(2);

1;
__END__
