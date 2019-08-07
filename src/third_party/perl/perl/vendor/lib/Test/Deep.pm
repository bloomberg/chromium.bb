use strict;
use warnings;

package Test::Deep;
use Carp qw( confess );

use Test::Deep::Cache;
use Test::Deep::Stack;
use Test::Deep::RegexpVersion;

require overload;
use Scalar::Util;

my $Test;
unless (defined $Test::Deep::NoTest::NoTest)
{
# for people who want eq_deeply but not Test::Builder
	require Test::Builder;
	$Test = Test::Builder->new;
}

our ($Stack, %Compared, $CompareCache, %WrapCache, $Shallow);

our $VERSION = '0.109';
$VERSION = eval $VERSION;

require Exporter;
our @ISA = qw( Exporter );

our $Snobby = 1; # should we compare classes?
our $Expects = 0; # are we comparing got vs expect or expect vs expect

our $DNE = \"";
our $DNE_ADDR = Scalar::Util::refaddr($DNE);

# if no sub name is supplied then we use the package name in lower case
my %constructors = (
  All               => "",
  Any               => "",
  Array             => "",
  ArrayEach         => "array_each",
  ArrayElementsOnly => "",
  ArrayLength       => "",
  ArrayLengthOnly   => "",
  Blessed           => "",
  Boolean           => "bool",
  Code              => "",
  Hash              => "",
  HashEach          => "hash_each",
  HashKeys          => "",
  HashKeysOnly      => "",
  Ignore            => "",
  Isa               => "Isa",
  ListMethods       => "",
  Methods           => "",
  Number            => "num",
  RefType           => "",
  Regexp            => "re",
  RegexpMatches     => "",
  RegexpOnly        => "",
  RegexpRef         => "",
  RegexpRefOnly     => "",
  ScalarRef         => "scalref",
  ScalarRefOnly     => "",
  Shallow           => "",
  String            => "str",
);

our @CONSTRUCTORS_FROM_CLASSES;

while (my ($pkg, $name) = each %constructors)
{
	$name = lc($pkg) unless $name;
	my $full_pkg = "Test::Deep::$pkg";
	my $file = "$full_pkg.pm";
	$file =~ s#::#/#g;
	my $sub = sub {
		require $file;
		return $full_pkg->new(@_);
	};
	{
		no strict 'refs';
		*{$name} = $sub;
	}

  push @CONSTRUCTORS_FROM_CLASSES, $name;
}

{
  our @EXPORT_OK = qw( descend render_stack class_base cmp_details deep_diag );

  our %EXPORT_TAGS;
  $EXPORT_TAGS{v0} = [
    qw(
      Isa

      all any array array_each arrayelementsonly arraylength arraylengthonly
      bag blessed bool cmp_bag cmp_deeply cmp_methods cmp_set code eq_deeply
      hash hash_each hashkeys hashkeysonly ignore isa listmethods methods
      noclass num re reftype regexpmatches regexponly regexpref regexprefonly
      scalarrefonly scalref set shallow str subbagof subhashof subsetof
      superbagof superhashof supersetof useclass
    )
  ];

  our @EXPORT = @{ $EXPORT_TAGS{ v0 } };

  $EXPORT_TAGS{all} = [ @EXPORT, @EXPORT_OK ];
}

# this is ugly, I should never have exported a sub called isa now I
# have to try figure out if the recipient wanted my isa or if a class
# imported us and UNIVERSAL::isa is being called on that class.
# Luckily our isa always expects 1 argument and U::isa always expects
# 2, so we can figure out (assuming the caller is not buggy).
sub isa
{
	if (@_ == 1)
	{
		goto &Isa;
	}
	else
	{
		goto &UNIVERSAL::isa;
	}
}

sub cmp_deeply
{
	my ($d1, $d2, $name) = @_;

	my ($ok, $stack) = cmp_details($d1, $d2);

	if (not $Test->ok($ok, $name))
	{
		my $diag = deep_diag($stack);
		$Test->diag($diag);
	}

	return $ok;
}

sub cmp_details
{
	my ($d1, $d2) = @_;

	local $Stack = Test::Deep::Stack->new;
	local $CompareCache = Test::Deep::Cache->new;
	local %WrapCache;

	my $ok = descend($d1, $d2);

	return ($ok, $Stack);
}

sub eq_deeply
{
	my ($d1, $d2) = @_;

	my ($ok) = cmp_details($d1, $d2);

	return $ok
}

sub eq_deeply_cache
{
	# this is like cross between eq_deeply and descend(). It doesn't start
	# with a new $CompareCache but if the comparison fails it will leave
	# $CompareCache as if nothing happened. However, if the comparison
	# succeeds then $CompareCache retains all the new information

	# this allows Set and Bag to handle circular refs

	my ($d1, $d2, $name) = @_;

	local $Stack = Test::Deep::Stack->new;
	$CompareCache->local;

	my $ok = descend($d1, $d2);

	$CompareCache->finish($ok);

	return $ok;
}

sub deep_diag
{
	my $stack = shift;
	# ick! incArrow and other things expect the stack has to be visible
	# in a well known place . TODO clean this up
	local $Stack = $stack;

	my $where = render_stack('$data', $stack);

	confess "No stack to diagnose" unless $stack;
	my $last = $stack->getLast;

	my $diag;
	my $message;
	my $got;
	my $expected;

	my $exp = $last->{exp};
	if (Scalar::Util::blessed($exp))
	{
		if ($exp->can("diagnostics"))
		{
			$diag = $exp->diagnostics($where, $last);
			$diag =~ s/\n+$/\n/;
		}
		else
		{
			if ($exp->can("diag_message"))
			{
				$message = $exp->diag_message($where);
			}
		}
	}

	if (not defined $diag)
	{
		$got = $exp->renderGot($last->{got}) unless defined $got;
		$expected = $exp->renderExp unless defined $expected;
		$message = "Compared $where" unless defined $message;

		$diag = <<EOM
$message
   got : $got
expect : $expected
EOM
	}

	return $diag;
}

sub render_val
{
	my $val = shift;

	my $rendered;
	if (defined $val)
	{
	 	$rendered = ref($val) ?
	 		(Scalar::Util::refaddr($val) eq $DNE_ADDR ?
	 			"Does not exist" :
				overload::StrVal($val)
			) :
			qq('$val');
	}
	else
	{
		$rendered = "undef";
	}

	return $rendered;
}

sub descend
{
	my ($d1, $d2) = @_;

	if (!ref $d1 and !ref $d2)
	{
    # Shortcut comparison for the non-reference case.
    if (defined $d1)
    {
      return 1 if defined $d2 and $d1 eq $d2;
    }
    else
    {
      return 1 if !defined $d2;
    }
	}

	if (! $Expects and Scalar::Util::blessed($d1) and $d1->isa("Test::Deep::Cmp"))
	{
		my $where = $Stack->render('$data');
		confess "Found a special comparison in $where\nYou can only the specials in the expects structure";
	}

	if (ref $d1 and ref $d2)
	{
		# this check is only done when we're comparing 2 expecteds against each
		# other

		if ($Expects and Scalar::Util::blessed($d1) and $d1->isa("Test::Deep::Cmp"))
		{
			# check they are the same class
			return 0 unless Test::Deep::blessed(Scalar::Util::blessed($d2))->descend($d1);
			if ($d1->can("compare"))
			{
				return $d1->compare($d2);
			}
		}

		my $s1 = Scalar::Util::refaddr($d1);
		my $s2 = Scalar::Util::refaddr($d2);

		if ($s1 eq $s2)
		{
			return 1;
		}
		if ($CompareCache->cmp($d1, $d2))
		{
			# we've tried comparing these already so either they turned out to
			# be the same or we must be in a loop and we have to assume they're
			# the same

			return 1;
		}
		else
		{
			$CompareCache->add($d1, $d2)
		}
	}

	$d2 = wrap($d2);

	$Stack->push({exp => $d2, got => $d1});

	if (ref($d1) and (Scalar::Util::refaddr($d1) == $DNE_ADDR))
	{
		# whatever it was suposed to be, it didn't exist and so it's an
		# automatic fail
		return 0;
	}

	if ($d2->descend($d1))
	{
#		print "d1 = $d1, d2 = $d2\nok\n";
		$Stack->pop;

		return 1;
	}
	else
	{
#		print "d1 = $d1, d2 = $d2\nnot ok\n";
		return 0;
	}
}

sub wrap
{
	my $data = shift;

	return $data if Scalar::Util::blessed($data) and $data->isa("Test::Deep::Cmp");

	my ($class, $base) = class_base($data);

	my $cmp;

	if($base eq '')
	{
		$cmp = shallow($data);
	}
	else
	{
		my $addr = Scalar::Util::refaddr($data);

		return $WrapCache{$addr} if $WrapCache{$addr};

		if($base eq 'ARRAY')
		{
			$cmp = array($data);
		}
		elsif($base eq 'HASH')
		{
			$cmp = hash($data);
		}
		elsif($base eq 'SCALAR' or $base eq 'REF')
		{
			$cmp = scalref($data);
		}
		elsif(($base eq 'Regexp') or ($base eq 'REGEXP'))
		{
			$cmp = regexpref($data);
		}
		else
		{
			$cmp = shallow($data);
		}

		$WrapCache{$addr} = $cmp;
	}
	return $cmp;
}

sub class_base
{
	my $val = shift;

	if (ref $val)
	{
		my $blessed = Scalar::Util::blessed($val);
		$blessed = defined($blessed) ? $blessed : "";
		my $reftype = Scalar::Util::reftype($val);


		if ($Test::Deep::RegexpVersion::OldStyle) {
			if ($blessed eq "Regexp" and $reftype eq "SCALAR")
			{
				$reftype = "Regexp"
			}
		}
		return ($blessed, $reftype);
	}
	else
	{
		return ("", "");
	}
}

sub render_stack
{
	my ($var, $stack) = @_;

	return $stack->render($var);
}

sub cmp_methods
{
	local $Test::Builder::Level = $Test::Builder::Level + 1;
	return cmp_deeply(shift, methods(@{shift()}), shift);
}

sub requireclass
{
	require Test::Deep::Class;

	my $val = shift;

	return Test::Deep::Class->new(1, $val);
}

# docs and export say this is call useclass, doh!

*useclass = \&requireclass;

sub noclass
{
	require Test::Deep::Class;

	my $val = shift;

	return Test::Deep::Class->new(0, $val);
}

sub set
{
	require Test::Deep::Set;

	return Test::Deep::Set->new(1, "", @_);
}

sub supersetof
{
	require Test::Deep::Set;

	return Test::Deep::Set->new(1, "sup", @_);
}

sub subsetof
{
	require Test::Deep::Set;

	return Test::Deep::Set->new(1, "sub", @_);
}

sub cmp_set
{
	local $Test::Builder::Level = $Test::Builder::Level + 1;
	return cmp_deeply(shift, set(@{shift()}), shift);
}

sub bag
{
	require Test::Deep::Set;

	return Test::Deep::Set->new(0, "", @_);
}

sub superbagof
{
	require Test::Deep::Set;

	return Test::Deep::Set->new(0, "sup", @_);
}

sub subbagof
{
	require Test::Deep::Set;

	return Test::Deep::Set->new(0, "sub", @_);
}

sub cmp_bag
{
	local $Test::Builder::Level = $Test::Builder::Level + 1;
  my $ref = ref($_[1]) || "";
  confess "Argument 2 to cmp_bag is not an ARRAY ref (".render_val($_[1]).")"
    unless $ref eq "ARRAY";
  return cmp_deeply(shift, bag(@{shift()}), shift);
}

sub superhashof
{
	require Test::Deep::Hash;

	my $val = shift;

	return Test::Deep::SuperHash->new($val);
}

sub subhashof
{
	require Test::Deep::Hash;

	my $val = shift;

	return Test::Deep::SubHash->new($val);
}

sub builder
{
	if (@_)
	{
		$Test = shift;
	}
	return $Test;
}

1;

__END__

=head1 NAME

Test::Deep - Extremely flexible deep comparison

=head1 SYNOPSIS

  use Test::More tests => $Num_Tests;
  use Test::Deep;

  cmp_deeply(
    $actual_horrible_nested_data_structure,
    $expected_horrible_nested_data_structure,
    "got the right horrible nested data structure"
  );

  cmp_deeply(
    $object,
    methods(name => "John", phone => "55378008"),
    "object methods ok"
  );

  cmp_deeply(
    \@array,
    [$hash1, $hash2, ignore()],
    "first 2 elements are as expected, ignoring 3"
  );

  cmp_deeply(
    $object,
    noclass({value => 5}),
    "object looks ok, not checking it's class"
  );

  cmp_deeply(
    \@result,
    bag('a', 'b', {key => [1, 2]}),
    "array has the 3 things we wanted in some order"
  );

=head1 DESCRIPTION

If you don't know anything about automated testing in Perl then you should
probably read about Test::Simple and Test::More before preceding.
Test::Deep uses the Test::Builder framework.

Test::Deep gives you very flexible ways to check that the result you got is
the result you were expecting. At it's simplest it compares two structures
by going through each level, ensuring that the values match, that arrays and
hashes have the same elements and that references are blessed into the
correct class. It also handles circular data structures without getting
caught in an infinite loop.

Where it becomes more interesting is in allowing you to do something besides
simple exact comparisons. With strings, the C<eq> operator checks that 2
strings are exactly equal but sometimes that's not what you want. When you
don't know exactly what the string should be but you do know some things
about how it should look, C<eq> is no good and you must use pattern matching
instead. Test::Deep provides pattern matching for complex data structures

=head1 EXAMPLES

How Test::Deep works is much easier to understand by seeing some examples.

=head2 Without Test::Deep

Say you want to test a function which returns a string. You know that your
string should be a 7 digit number beginning with 0, C<eq> is no good in this
situation, you need a regular expression. So you could use Test::More's
C<like()> function:

  like($string, '/^0d{6}$/', "number looks good");

Similarly, to check that a string looks like a name, you could do:

  like($string, '/^(Mr|Mrs|Miss) \w+ \w+$/',
    "got title, first and last name");

Now imagine your function produces a hash with some personal details in it.
You want to make sure that there are 2 keys, Name and Phone and that the
name looks like a name and the phone number looks like a phone number. You
could do:

  $hash = make_person();
  like($hash->{Name}, '/^(Mr|Mrs|Miss) \w+ \w+$/', "name ok");
  like($hash->{Phone}, '/^0d{6}$/', "phone ok");
  is(scalar keys %$hash, 2, "correct number of keys");

But that's not quite right, what if make_person has a serious problem and
didn't even return a hash? We really need to write

  if (ref($hash) eq "HASH")
  {
    like($hash->{Name}, '/^(Mr|Mrs|Miss) \w+ \w+$/', "name ok");
    like($hash->{Phone}, '/^0d{6}$/', "phone ok");
    is(scalar keys %$hash, 2, "correct number of keys");
  }
  else
  {
    fail("person not a hash");
    fail("person not a hash");
    fail("person not a hash"); # need 3 to keep the plan correct
  }

Already this is getting messy, now imagine another entry in the hash, an
array of children's names. This would require


  if (ref($hash) eq "HASH")
  {
    like($hash->{Name}, $name_pat, "name ok");
    like($hash->{Phone}, '/^0d{6}$/', "phone ok");
    my $cn = $hash->{ChildNames};
    if (ref($cn) eq "ARRAY")
    {
      foreach my $child (@$cn)
      {
        like($child, $name_pat);
      }
    }
    else
    {
        fail("child names not an array")
    }
  }
  else
  {
    fail("person not a hash");
  }

This is a horrible mess and because we don't know in advance how many
children's names there will be, we can't make a plan for our test anymore
(actually, we could but it would make things even more complicated).

Test::Deep to the rescue.

=head2 With Test::Deep

  my $name_re = re('^(Mr|Mrs|Miss) \w+ \w+$');
  cmp_deeply(
    $person,
    {
      Name => $name_re,
      Phone => re('^0d{6}$'),
      ChildNames => array_each($name_re)
    },
    "person ok"
  );

This will do everything that the messy code above does and it will give a
sensible message telling you exactly what went wrong if it finds a part of
$person that doesn't match the pattern. C<re()> and C<array_each()> are
special function imported from Test::Deep. They create a marker that tells
Test::Deep that something different is happening here. Instead of just doing
a simple comparison and checking are two things exactly equal, it should do
something else.

If a person was asked to check that 2 structures are equal, they could print
them both out and compare them line by line. The markers above are similar
to writing a note in red pen on one of the printouts telling the person that
for this piece of the structure, they should stop doing simple line by line
comparison and do something else.

C<re($regex)> means that Test::Deep should check that the current piece of
data matches the regex in C<$regex>. C<array_each($struct)> means that
Test::Deep should expect the current piece of data to be an array and it
should check that every element of that array matches C<$struct>.
In this case, every element of C<< $person->{ChildNames} >> should look like a
name. If say the 3rd one didn't you would get an error message something
like

  Using Regexp on $data->{ChildNames}[3]
     got    : 'Queen John Paul Sartre'
     expect : /^(Mr|Mrs|Miss) \w+ \w+$/

There are lots of other special comparisons available, see
L<SPECIAL COMPARISONS PROVIDED> below for the full list.

=head2 Reusing structures

Test::Deep is good for reusing test structures so you can do this

  my $name_re = re('^(Mr|Mrs|Miss) \w+ \w+$');
  my $person_cmp = {
    Name => $name_re,
    Phone => re('^0d{6}$'),
    ChildNames => array_each($name_re)
  };

  cmp_deeply($person1, $person_cmp, "person ok");
  cmp_deeply($person2, $person_cmp, "person ok");
  cmp_deeply($person3, $person_cmp, "person ok");

You can even put $person_cmp in a module and let other people use it when
they are writing test scripts for modules that use your modules.

To make things a little more difficult, lets change the person data
structure so that instead of a list of ChildNames, it contains a list of
hashes, one for each child. So in fact our person structure will contain
other person structures which may contain other person structures and so on.
This is easy to handle with Test::Deep because Test::Deep structures can
include themselves. Simply do

  my $name_re = re('^(Mr|Mrs|Miss) \w+ \w+$');
  my $person_cmp = {
    Name => $name_re,
    Phone => re('^0d{6}$'),
    # note no mention of Children here
  };

  $person_cmp->{Children} = each_array($person_cmp);

  cmp_deeply($person, $person_cmp, "person ok");

This will now check that $person->{Children} is an array and that every
element of that array also matches C<$person_cmp>, this includes checking
that it's children also match the same pattern and so on.

=head2 Circular data structures

A circular data structure is one which loops back on itself, you can make
one easily by doing

  my @b;
  my @a = (1, 2, 3, \@b);
  push(@b, \@a);

now @a contains a reference to be @b and @b contains a reference to @a. This
causes problems if you have a program that wants to look inside @a and keep
looking deeper and deeper at every level, it could get caught in an infinite
loop looking into @a then @b then @a then @b and so on.

Test::Deep avoids this problem so we can extend our example further by
saying that a person should also list their parents.

  my $name_re = re('^(Mr|Mrs|Miss) \w+ \w+$');
  my $person_cmp = {
    Name => $name_re,
    Phone => re('^0d{6}$'),
    # note no mention of Children here
  };

  $person_cmp->{Children} = each_array($person_cmp);
  $person_cmp->{Parents} = each_array($person_cmp);

  cmp_deeply($person, $person_cmp, "person ok");

So this will check that for each child C<$child> in C<< $person->{Children} >>
that the C<< $child->{Parents} >> matches C<$person_cmp> however it is smart
enough not to get caught in an infinite loop where it keeps bouncing between
the same Parent and Child.

=head1 TERMINOLOGY

C<cmp_deeply($got, $expected, $name)> takes 3 arguments. C<$got> is the
structure that you are checking, you must not include any special
comparisons in this structure or you will get a fatal error. C<$expected>
describes what Test::Deep will be looking for in $got. You can put special
comparisons in $expected if you want to.

As Test::Deep descends through the 2 structures, it compares them one piece
at a time, so at any point in the process, Test::Deep is thinking about 2
things - the current value from C<$got> and the current value from
C<$expected>. In the documentation, I call them C<$got_v> and C<exp_v>
respectively.

=head1 COMPARISON FUNCTIONS

=head3 $ok = cmp_deeply($got, $expected, $name)

$got is the result to be checked. $expected is the structure against which
$got will be check. $name is the test name.

This is the main comparison function, the others are just wrappers around
this. Without any special comparisons, it will descend into $expected,
following every reference and comparing C<$expected_v> to C<$got_v> (using
C<eq>) at the same position. If at any stage C<$expected_v> is a special
comparison then Test::Deep may do something else besides a simple string
comparison, exactly what it does depends on which special comparison it is.

=head3 $ok = cmp_bag(\@got, \@bag, $name)

Is shorthand for cmp_deeply(\@got, bag(@bag), $name)

N.B. Both arguments must be array refs. If they aren't an error will
be raised via die.

=head3 $ok = cmp_set(\@got, \@set, $name)

Is shorthand for cmp_deeply(\@got, set(@set), $name)

=head3 $ok = cmp_methods(\@got, \@methods, $name)

Is shorthand for cmp_deeply(\@got, methods(@methods), $name)

=head3 $ok = eq_deeply($got, $expected)

This is the same as cmp_deeply() except it just returns true or
false. It does not create diagnostics or talk to L<Test::Builder>, but
if you want to use it in a non-testing environment then you should
import it through L<Test::Deep::NoTest>. For example

  use Test::Deep::NoTest;
  print "a equals b" unless eq_deeply($a, $b);

otherwise the L<Test::Builder> framework will be loaded and testing messages
will be output when your program ends.

=head3 ($ok, $stack) = cmp_details($got, $expected)

This behaves much like eq_deeply, but it additionally allows you to
produce diagnostics in case of failure by passing the value in C<$stack>
to C<deep_diag>.

Do not make assumptions about the structure or content of C<$stack> and
do not use it if C<$ok> contains a true value.

See L</USING TEST::DEEP WITH TEST::BUILDER> for example uses.

=head1 SPECIAL COMPARISONS PROVIDED

=head3 ignore()

This makes Test::Deep skip tests on $got_v. No matter what value C<$got_v>
has, Test::Deep will think it's correct. This is useful if some part of the
structure you are testing is very complicated and already tested elsewhere,
or is unpredictable.

  cmp_deeply($got, { name => 'John', random => ignore(), address => ['5 A
    street', 'a town', 'a country'],
  })

is the equivalent of checking

  $got->{name} eq 'John';
  exists $got->{random};
  cmp_deeply($got->{address};
  ['5 A street', 'a town', 'a country']);

=head3 methods(%hash)

%hash is a hash of method call => expected value pairs.

This lets you call methods on an object and check the result of each call.
The methods will be called in the order supplied. If you want to pass
arguments to the method you should wrap the method name and arguments in an
array reference.

  cmp_deeply(
    $obj,
    methods(name => "John", ["favourite", "food"] => "taco")
  );

is roughly the equivalent of checking that

  $obj->name eq "John"
  $obj->favourite("food") eq "taco"

The methods will be called in the order you supply them and will be called
in scalar context. If you need to test methods called in list context then
you should use listmethods().

B<NOTE> Just as in a normal test script, you need to be careful if the
methods you call have side effects like changing the object or other objects
in the structure. Although the order of the methods is fixed, the order of
some other tests is not so if $expected is

  {
    manager => methods(@manager_methods),
    coder => methods(@coder_methods)
  }

there is no way to know which if manager and coder will be tested first. If
the methods you are testing depend on and alter global variables or if
manager and coder are the same object then you may run into problems.

=head3 listmethods(%hash)

%hash is a hash of method call => expected value pairs.

This is almost identical to methods() except the methods are called in list
context instead of scalar context. This means that the expected values
supplied must be an array reference.

  cmp_deeply(
    $obj,
    listmethods(
      name => "John",
      ["favourites", "food"] => ["Mapo tofu", "Gongbao chicken"]
    )
  );

is the equivalent of checking that

  $obj->name eq "John"
  cmp_deeply([$obj->favourites("food")], ["Mapo tofu", "Gongbao chicken"]);

The methods will be called in the order you supply them.

B<NOTE> The same caveats apply as for methods().

=head3 shallow($thing)

$thing is a ref.

This prevents Test::Deep from looking inside $thing. It allows you to
check that $got_v and $thing are references to the same variable. So

  my @a = @b = (1, 2, 3);
  cmp_deeply(\@a, \@b);

will pass because @a and @b have the same elements however

  cmp_deeply(\@a, shallow(\@b))

will fail because although \@a and \@b both contain C<1, 2, 3> they are
references to different arrays.

=head3 noclass($thing)

$thing is a structure to be compared against.

This makes Test::Deep ignore the class of objects, so it just looks at the
data they contain. Class checking will be turned off until Test::Deep is
finished comparing C<$got_v> against C<$thing>. Once Test::Deep comes out of
C<$thing> it will go back to it's previous setting for checking class.

This can be useful when you want to check that objects have been
constructed correctly but you don't want to write lots of
C<bless>es. If \@people is an array of Person objects then

  cmp_deeply(\@people, noclass([
    bless {name => 'John', phone => '555-5555'}, "Person",
    bless {name => 'Anne', phone => '444-4444'}, "Person",
  ]));

can be replaced with

  cmp_deeply(\@people, noclass([
    {name => 'John', phone => '555-5555'},
    {name => 'Anne', phone => '444-4444'}
  ]));

However, this is testing so you should also check that the objects are
blessed correctly. You could use a map to bless all those hashes or you
could do a second test like

  cmp_deeply($people, array_each(isa("Person"));

=head3 useclass($thing)

This turns back on the class comparison while inside a noclass().

  cmp_deeply(
    $got,
    noclass(
      [
        useclass( $object )
      ]
    )
  )

In this example the class of the array reference in C<$got> is ignored but
the class of C<$object> is checked, as is the class of everything inside
C<$object>.

=head3 re($regexp, $capture_data, $flags)

$regexp is either a regular expression reference produced with C<qr/.../> or
a string which will be used to construct a regular expression.

$capture_data is optional and is used to check the strings captured by
an regex. This should can be an array ref or a Test::Deep comparator
that works on array refs.

$flags is an optional string which controls whether the regex runs as a
global match. If $flags is "g" then the regex will run as m/$regexp/g.

Without $capture_data, this simply compares $got_v with the regular expression
provided. So

  cmp_deeply($got, [ re("ferg") ])

is the equivalent of

  $got->[0] =~ /ferg/

With $capture_data

  cmp_deeply($got, [re($regex, $capture_data])

is the equivalent of

  my @data = $got->[0] =~ /$regex/;
  cmp_deeply(\@data, $capture_data);

So you can do something simple like

  cmp_deeply($got, re(qr/(\d\d)(\w\w)/, [25, "ab" ])

to check that (\d\d) was 25 and (\w\w) was "ab" but you can also use
Test::Deep objects to do more complex testing of the captured values

  cmp_deeply("cat=2,dog=67,sheep=3,goat=2,dog=5",
    re(qr/(\D+)=\d+,?/, set(qw( cat sheep dog )), "g"))

here, the regex will match the string and will capture the animal names and
check that they match the specified set, in this case it will fail,
complaining that "goat" is not in the set.

=head3 superhashof(\%hash)

This will check that the hash C<%$got> is a "super-hash" of C<%hash>. That
is that all the key and value pairs in C<%hash> appear in C<%$got> but
C<%$got> can have extra ones also.

For example

  cmp_deeply({a => 1, b => 2}, superhashof({a => 1}))

will pass but

  cmp_deeply({a => 1, b => 2}, superhashof({a => 1, c => 3}))

will fail.

=head3 subhashof(\%hash)

This will check that the hash C<%$got> is a "sub-hash" of C<%hash>. That is
that all the key and value pairs in C<%$got> also appear in C<%hash>.

For example

  cmp_deeply({a => 1}, subhashof({a => 1, b => 2}))

will pass but

  cmp_deeply({a => 1, c => 3}, subhashof({a => 1, b => 2}))

will fail.

=head3 bag(@elements)

@elements is an array of elements.

This does a bag comparison, that is, it compares two arrays but ignores the
order of the elements so

  cmp_deeply([1, 2, 2], bag(2, 2, 1))

will be a pass.

The object returned by bag() has an add() method.

  my $bag = bag(1, 2, 3);
  $bag->add(2, 3, 4);

will result in a bag containing 1, 2, 2, 3, 3, 4.

C<NOTE> If you use certain special comparisons within a bag or set
comparison there is a danger that a test will fail when it should have
passed. It can only happen if two or more special comparisons in the bag are
competing to match elements. Consider this comparison

  cmp_deeply(['furry', 'furball'], bag(re("^fur"), re("furb")))

There are two things that could happen, hopefully C<re("^fur")> is paired with
"furry" and C<re("^furb")> is paired with "furb" and everything is fine but it
could happen that C<re("^fur")> is paired with "furball" and then C<re("^furb")>
cannot find a match and so the test fails. Examples of other competing
comparisons are C<bag(1, 2, 2)> vs C<set(1, 2)> and
C<< methods(m1 => "v1", m2 => "v2") >> vs C<< methods(m1 => "v1") >>

This problem is could be solved by using a slower and more complicated
algorithm for set and bag matching. Something for the future...

=head3 set(@elements)

@elements is an array of elements.

This does a set comparison, that is, it compares two arrays but ignores the
order of the elements and it ignores duplicate elements, so

  cmp_deeply([1, 2, 2, 3], set(3, 2, 1, 1))

will be a pass.

The object returned by set() has an add() method.

  my $set = set(1, 2, 3);
  $set->add(4, 5, 6);

will result in a set containing 1, 2, 3, 4, 5, 5.

C<NOTE> See the NOTE on the bag() comparison for some dangers in using
special comparisons inside set()

=head3 superbagof(@elements), subbagof(@elements), supersetof(@elements) and subsetof(@elements)

@elements is an array of elements.

These do exactly what you'd expect them to do, so for example

  cmp_deeply($data, subbagof(1, 1, 3, 4));

checks that @$data contains at most 2 "1"s, 1 "3" and 1 "4" and

  cmp_deeply($data, supersetof(1, 4));

check that @$data contains at least 1 "1" and 1 "4".

These are just special cases of the Set and Bag comparisons so they also
give you an add() method and they also have the same limitations when using
special comparisons inside them (see the NOTE in the bag() section).

=head3 all(@expecteds)

@expecteds is an array of expected structures.

This allows you to compare data against multiple expected results and make
sure each of them matches.

  cmp_deeply($got, all(isa("Person"), methods(name => 'John')))

is equivalent to

  $got->isa("Person")
  $got->name eq 'John'

If either test fails then the whole thing is considered a fail. This is a
short-circuit test, the testing is stopped after the first failure, although
in the future it may complete all tests so that diagnostics can be output
for all failures. When reporting failure, the parts are counted from 1.

Thanks to the magic of overloading, you can write

  all(isa("Person"), methods(name => 'John'), re("^wi"))

as

  isa("Person") & methods(name => 'John') | re("^wi")

Note B<single> | not double as || cannot be overloaded. This will only work
when there is a special comparison involved. If you write

  "john" | "anne" | "robert"

Perl will turn this into

  "{onort"

which is presumably not what you wanted. This is because Perl |s them
together as strings before Test::Deep gets a chance to do any overload
tricks.

=head3 any(@expecteds)

@expecteds is an array of expected structures.

This can be used to compare data against multiple expected results and make
sure that at least one of them matches. This is a short-circuit test so if
a test passes then none of the tests after that will be attempted.

You can also use overloading with | similarly to all().

=head3 isa($class), Isa($class)

$class is a class name.

This uses UNIVERSAL::isa() to check that $got_v is blessed into the class
$class.

B<NOTE:> Isa() does exactly as documented here, but isa() is slightly
different. If isa() is called with 1 argument it falls through to
Isa(). If isa() called with 2 arguments, it falls through to
UNIVERSAL::isa. This is to prevent breakage when you import isa() into
a package that is used as a class. Without this, anyone calling
C<Class-E<gt>isa($other_class)> would get the wrong answer. This is a
hack to patch over the fact that isa is exported by default.

=head3 array_each($thing)

$thing is a structure to be compared against.

<$got_v> must be an array reference. Each element of it will be compared to
$thing. This is useful when you have an array of similar things, for example
objects of a known type and you don't want to have to repeat the same test
for each one.

  my $common_tests = all(
     isa("MyFile"),
     methods(
       handle => isa("IO::Handle")
       filename => re("^/home/ted/tmp"),
    )
  );

  cmp_deeply($got, array_each($common_tests));

is similar to

  foreach my $got_v (@$got) {
    cmp_deeply($got_v, $common_tests)
  }

Except it will not explode is $got is not an array reference. It will check
that each of the objects in @$got is a MyFile and that each one gives the
correct results for it's methods.

You could go further, if for example there were 3 files and you knew the
size of each one you could do this

  cmp_deeply(
    $got,
    all(
      array_each($common_tests),
      [
        methods(size => 1000),
        methods(size => 200),
        methods(size => 20)
      ]
    )
  )
  cmp_deeply($got, array_each($structure));

=head3 str($string)

$string is a string.

This will stringify C<$got_v> and compare it to C<$string> using C<eq>, even
if $got_v is a ref. It is useful for checking the stringified value of an
overloaded reference.

=head3 num($number, $tolerance)

$number is a number.
$tolerance is an optional number.

This will add 0 to C<$got_v> and check if it's numerically equal to
C<$number>, even if $got_v is a ref. It is useful for checking the numerical
value of an overloaded reference. If $tolerance is supplied then this will
check that $got_v and $exp_v are less than $tolerance apart. This is useful
when comparing floating point numbers as rounding errors can make it hard or
impossible for $got_v to be exactly equal to $exp_v. When $tolerance is
supplied, the test passes if C<abs($got_v - $exp_v) <= $tolerance>.

B<Note> in Perl, C<"12blah" == 12> because Perl will be smart and convert
"12blah" into 12. You may not want this. There was a strict mode but that is
now gone. A "lookslike s number" test will replace it soon. Until then you
can usually just use the string() comparison to be more strict. This will
work fine for almost all situations, however it will not work when <$got_v>
is an overloaded value who's string and numerical values differ.

=head3 bool($value)

$value is anything you like but it's probably best to use 0 or 1

This will check that C<$got_v> and C<$value> have the same truth value, that
is they will give the same result when used in boolean context, like in an
if() statement.

=head3 code(\&subref)

\&subref is a reference to a subroutine which will be passed a single
argument, it then should return a true or false and possibly a string

This will pass C<$got_v> to the subroutine which returns true or false to
indicate a pass or fail. Fails can be accompanied by a diagnostic string
which gives an explanation of why it's a fail.

  sub check_name
  {
    my $name = shift;
    if ($boss->likes($name))
    {
      return 1;
    }
    else
    {
      return (0, "the boss doesn't like your name");
    }
  }

  cmp_deeply("Brian", \&check_name);

=head1 DIAGNOSTIC FUNCTIONS

=head3 my $reason = deep_diag($stack)

C<$stack> is a value returned by cmp_details.  Do not call this function
if cmp_details returned a true value for C<$ok>.

deep_diag() returns a human readable string describing how the
comparison failed.

=head1 ANOTHER EXAMPLE

You've written a module to handle people and their film interests. Say you
have a function that returns an array of people from a query, each person is
a hash with 2 keys: Name and Age and the array is sorted by Name. You can do

  cmp_deeply(
    $result,
    [
      {Name => 'Anne', Age => 26},
      {Name => "Bill", Age => 47}
      {Name => 'John', Age => 25},
    ]
  );

Soon after, your query function changes and all the results now have an ID
field. Now your test is failing again because you left out ID from each of
the hashes. The problem is that the IDs are generated by the database and
you have no way of knowing what each person's ID is. With Test::Deep you can
change your query to

  cmp_deeply(
    $result,
    [
      {Name => 'John', Age => 25, ID => ignore()},
      {Name => 'Anne', Age => 26, ID => ignore()},
      {Name => "Bill", Age => 47, ID => ignore()}
    ]
  );

But your test still fails. Now, because you're using a database, you no
longer know what order the people will appear in. You could add a sort into
the database query but that could slow down your application. Instead you
can get Test::Deep to ignore the order of the array by doing a bag
comparison instead.

  cmp_deeply(
    $result,
    bag(
      {Name => 'John', Age => 25, ID => ignore()},
      {Name => 'Anne', Age => 26, ID => ignore()},
      {Name => "Bill", Age => 47, ID => ignore()}
    )
  );

Finally person gets even more complicated and includes a new field called
Movies, this is a list of movies that the person has seen recently, again
these movies could also come back in any order so we need a bag inside our
other bag comparison, giving us something like

  cmp_deeply(
  $result,
    bag(
      {Name => 'John', Age => 25, ID => ignore(), Movies => bag(...)},
      {Name => 'Anne', Age => 26, ID => ignore(), Movies => bag(...)},
      {Name => "Bill", Age => 47, ID => ignore(), Movies => bag(...)}
    )
  );

=head1 USING TEST::DEEP WITH TEST::BUILDER

Combining C<cmp_details> and C<test_diag> makes it possible to use
Test::Deep in your own test classes.

In a L<Test::Builder> subclass, create a test method in the following
form:

  sub behaves_ok {
    my $self = shift;
    my $expected = shift;
    my $test_name = shift;

    my $got = do_the_important_work_here();

    my ($ok, $stack) = cmp_details($got, $expected);
    unless ($Test->ok($ok, $test_name)) {
      my $diag = deep_diag($stack);
      $Test->diag($diag);
    }
  }

As the subclass defines a test class, not tests themselves, make sure it
uses L<Test::Deep::NoTest>, not C<Test::Deep> itself.

=head1 LIMITATIONS

Currently any CODE, GLOB or IO refs will be compared using shallow(), which
means only their memory addresses are compared.

=head1 BUGS

There is a bug in set and bag compare to do with competing SCs. It only
occurs when you put certain special comparisons inside bag or set
comparisons you don't need to worry about it. The full details are in the
bag() docs. It will be fixed in an upcoming version.

=head1 WHAT ARE SPECIAL COMPARISONS?

A special comparison (SC) is simply an object that inherits from
Test::Deep::Cmp. Whenever C<$expected_v> is an SC then instead of checking
C<$got_v eq $expected_v>, we pass control over to the SC and let it do it's
thing.

Test::Deep exports lots of SC constructors, to make it easy for you to use
them in your test scripts. For example is C<re("hello")> is just a handy way
of creating a Test::Deep::Regexp object that will match any string containing
"hello". So

  cmp_deeply([ 'a', 'b', 'hello world'], ['a', 'b', re("^hello")]);

will check C<'a' eq 'a'>, C<'b' eq 'b'> but when it comes to comparing
C<'hello world'> and C<re("^hello")> it will see that
$expected_v is an SC and so will pass control to the Test::Deep::Regexp class
by do something like C<< $expected_v->descend($got_v) >>. The C<descend()>
method should just return true or false.

This gives you enough to write your own SCs but I haven't documented how
diagnostics works because it's about to get an overhaul.

=head1 SEE ALSO

L<Test::More>

=head1 MAINTAINER

  Ricardo Signes <rjbs@cpan.org>

=head1 AUTHOR

Fergal Daly E<lt>fergal@esatclear.ieE<gt>, with thanks to Michael G Schwern
for Test::More's is_deeply function which inspired this.

B<Please> do not bother Fergal Daly with bug reports.  Send them to the
maintainer (above) or submit them at L<the request
tracker|https://rt.cpan.org/Dist/Display.html?Queue=Test-Deep>.

=head1 COPYRIGHT

Copyright 2003, 2004 by Fergal Daly E<lt>fergal@esatclear.ieE<gt>.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

See F<http://www.perl.com/perl/misc/Artistic.html>

=cut
