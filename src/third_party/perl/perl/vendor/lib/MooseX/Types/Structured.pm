package MooseX::Types::Structured;
BEGIN {
  $MooseX::Types::Structured::AUTHORITY = 'cpan:JJNAPIORK';
}
{
  $MooseX::Types::Structured::VERSION = '0.28';
}
# ABSTRACT: MooseX::Types::Structured - Structured Type Constraints for Moose

use 5.008;

use Moose::Util::TypeConstraints 1.06;
use MooseX::Meta::TypeConstraint::Structured;
use MooseX::Meta::TypeConstraint::Structured::Optional;
use MooseX::Types::Structured::OverflowHandler;
use MooseX::Types::Structured::MessageStack;
use MooseX::Types 0.22 -declare => [qw(Dict Map Tuple Optional)];
use Sub::Exporter 0.982 -setup => [ qw(Dict Map Tuple Optional slurpy) ];
use Devel::PartialDump 0.10;
use Scalar::Util qw(blessed);


my $Optional = MooseX::Meta::TypeConstraint::Structured::Optional->new(
    name => 'MooseX::Types::Structured::Optional',
    package_defined_in => __PACKAGE__,
    parent => find_type_constraint('Item'),
    constraint => sub { 1 },
    constraint_generator => sub {
        my ($type_parameter, @args) = @_;
        my $check = $type_parameter->_compiled_type_constraint();
        return sub {
            my (@args) = @_;
            ## Does the arg exist?  Something exists if it's a 'real' value
            ## or if it is set to undef.
            if(exists($args[0])) {
                ## If it exists, we need to validate it
                $check->($args[0]);
            } else {
                ## But it's is okay if the value doesn't exists
                return 1;
            }
        }
    }
);

my $IsType = sub {
    my ($obj, $type) = @_;

    return $obj->can('equals')
        ? $obj->equals($type)
        : undef;
};

my $CompiledTC = sub {
    my ($obj) = @_;

    my $method = '_compiled_type_constraint';
    return(
          $obj->$IsType('Any')  ? undef
        : $obj->can($method)    ? $obj->$method
        :                         sub { $obj->check(shift) },
    );
};

Moose::Util::TypeConstraints::register_type_constraint($Optional);
Moose::Util::TypeConstraints::add_parameterizable_type($Optional);

Moose::Util::TypeConstraints::get_type_constraint_registry->add_type_constraint(
    MooseX::Meta::TypeConstraint::Structured->new(
        name => "MooseX::Types::Structured::Tuple" ,
        parent => find_type_constraint('ArrayRef'),
        constraint_generator=> sub {
            ## Get the constraints and values to check
            my ($self, $type_constraints) = @_;
            $type_constraints ||= $self->type_constraints;
            my @type_constraints = defined $type_constraints ?
             @$type_constraints : ();

            my $overflow_handler;
            if($type_constraints[-1] && blessed $type_constraints[-1]
              && $type_constraints[-1]->isa('MooseX::Types::Structured::OverflowHandler')) {
                $overflow_handler = pop @type_constraints;
            }

            my $length = $#type_constraints;
            foreach my $idx (0..$length) {
                unless(blessed $type_constraints[$idx]) {
                    ($type_constraints[$idx] = find_type_constraint($type_constraints[$idx]))
                      || die "$type_constraints[$idx] is not a registered type";
                }
            }

            my (@checks, @optional, $o_check, $is_compiled);
            return sub {
                my ($values, $err) = @_;
                my @values = defined $values ? @$values : ();

                ## initialise on first time run
                unless ($is_compiled) {
                    @checks   = map { $_->$CompiledTC } @type_constraints;
                    @optional = map { $_->is_subtype_of($Optional) } @type_constraints;
                    $o_check  = $overflow_handler->$CompiledTC
                        if $overflow_handler;
                    $is_compiled++;
                }

                ## Perform the checking
              VALUE:
                for my $type_index (0 .. $#checks) {

                    my $type_constraint = $checks[ $type_index ];

                    if(@values) {
                        my $value = shift @values;

                        next VALUE
                            unless $type_constraint;

                        unless($type_constraint->($value)) {
                            if($err) {
                               my $message = $type_constraints[ $type_index ]->validate($value,$err);
                               $err->add_message({message=>$message,level=>$err->level});
                            }
                            return;
                        }
                    } else {
                        ## Test if the TC supports null values
                        unless ($optional[ $type_index ]) {
                            if($err) {
                               my $message = $type_constraints[ $type_index ]->get_message('NULL',$err);
                               $err->add_message({message=>$message,level=>$err->level});
                            }
                            return;
                        }
                    }
                }

                ## Make sure there are no leftovers.
                if(@values) {
                    if($overflow_handler) {
                        return $o_check->([@values], $err);
                    } else {
                        if($err) {
                            my $message = "More values than Type Constraints!";
                            $err->add_message({message=>$message,level=>$err->level});
                        }
                        return;
                    }
                } else {
                    return 1;
                }
            };
        }
    )
);

Moose::Util::TypeConstraints::get_type_constraint_registry->add_type_constraint(
    MooseX::Meta::TypeConstraint::Structured->new(
        name => "MooseX::Types::Structured::Dict",
        parent => find_type_constraint('HashRef'),
        constraint_generator => sub {
            ## Get the constraints and values to check
            my ($self, $type_constraints) = @_;
            $type_constraints = $self->type_constraints;
            my @type_constraints = defined $type_constraints ?
             @$type_constraints : ();

            my $overflow_handler;
            if($type_constraints[-1] && blessed $type_constraints[-1]
              && $type_constraints[-1]->isa('MooseX::Types::Structured::OverflowHandler')) {
                $overflow_handler = pop @type_constraints;
            }
            my %type_constraints = @type_constraints;
            foreach my $key (keys %type_constraints) {
                unless(blessed $type_constraints{$key}) {
                    ($type_constraints{$key} = find_type_constraint($type_constraints{$key}))
                      || die "$type_constraints{$key} is not a registered type";
                }
            }

            my (%check, %optional, $o_check, $is_compiled);
            return sub {
                my ($values, $err) = @_;
                my %values = defined $values ? %$values: ();

                unless ($is_compiled) {
                    %check    = map { ($_ => $type_constraints{ $_ }->$CompiledTC) } keys %type_constraints;
                    %optional = map { ($_ => $type_constraints{ $_ }->is_subtype_of($Optional)) } keys %type_constraints;
                    $o_check  = $overflow_handler->$CompiledTC
                        if $overflow_handler;
                    $is_compiled++;
                }

                ## Perform the checking
              KEY:
                for my $key (keys %check) {
                    my $type_constraint = $check{ $key };

                    if(exists $values{$key}) {
                        my $value = $values{$key};
                        delete $values{$key};

                        next KEY
                            unless $type_constraint;

                        unless($type_constraint->($value)) {
                            if($err) {
                                my $message = $type_constraints{ $key }->validate($value,$err);
                                $err->add_message({message=>$message,level=>$err->level});
                            }
                            return;
                        }
                    } else {
                        ## Test to see if the TC supports null values
                        unless ($optional{ $key }) {
                            if($err) {
                               my $message = $type_constraints{ $key }->get_message('NULL',$err);
                               $err->add_message({message=>$message,level=>$err->level});
                            }
                            return;
                        }
                    }
                }

                ## Make sure there are no leftovers.
                if(%values) {
                    if($overflow_handler) {
                        return $o_check->(+{%values});
                    } else {
                        if($err) {
                            my $message = "More values than Type Constraints!";
                            $err->add_message({message=>$message,level=>$err->level});
                        }
                        return;
                    }
                } else {
                    return 1;
                }
            }
        },
    )
);

Moose::Util::TypeConstraints::get_type_constraint_registry->add_type_constraint(
  MooseX::Meta::TypeConstraint::Structured->new(
    name => "MooseX::Types::Structured::Map",
    parent => find_type_constraint('HashRef'),
    constraint_generator=> sub {
      ## Get the constraints and values to check
      my ($self, $type_constraints) = @_;
      $type_constraints = $self->type_constraints;
      my @constraints = defined $type_constraints ? @$type_constraints : ();

      Carp::confess( "too many args for Map type" ) if @constraints > 2;

      my ($key_type, $value_type) = @constraints == 2 ? @constraints
                                  : @constraints == 1 ? (undef, @constraints)
                                  :                     ();

      my ($key_check, $value_check, $is_compiled);
      return sub {
          my ($values, $err) = @_;
          my %values = defined $values ? %$values: ();

          unless ($is_compiled) {
              ($key_check, $value_check)
                = map { $_ ? $_->$CompiledTC : undef }
                      $key_type, $value_type;
              $is_compiled++;
          }

          ## Perform the checking
          if ($value_check) {
            for my $value (values %$values) {
              unless ($value_check->($value)) {
                if($err) {
                  my $message = $value_type->validate($value,$err);
                  $err->add_message({message=>$message,level=>$err->level});
                }
                return;
              }
            }
          }
          if ($key_check) {
            for my $key (keys %$values) {
              unless ($key_check->($key)) {
                if($err) {
                  my $message = $key_type->validate($key,$err);
                  $err->add_message({message=>$message,level=>$err->level});
                }
                return;
              }
            }
          }

          return 1;
      };
    },
  )
);

sub slurpy ($) {
    my ($tc) = @_;
    return MooseX::Types::Structured::OverflowHandler->new(
        type_constraint => $tc,
    );
}


1;

__END__
=pod

=encoding utf-8

=head1 NAME

MooseX::Types::Structured - MooseX::Types::Structured - Structured Type Constraints for Moose

=head1 SYNOPSIS

The following is example usage for this module.

    package Person;

    use Moose;
    use MooseX::Types::Moose qw(Str Int HashRef);
    use MooseX::Types::Structured qw(Dict Tuple Optional);

    ## A name has a first and last part, but middle names are not required
    has name => (
        isa=>Dict[
            first => Str,
            last => Str,
            middle => Optional[Str],
        ],
    );

    ## description is a string field followed by a HashRef of tagged data.
    has description => (
      isa=>Tuple[
        Str,
        Optional[HashRef],
     ],
    );

    ## Remainder of your class attributes and methods

Then you can instantiate this class with something like:

    my $john = Person->new(
        name => {
            first => 'John',
            middle => 'James'
            last => 'Napiorkowski',
        },
        description => [
            'A cool guy who loves Perl and Moose.', {
                married_to => 'Vanessa Li',
                born_in => 'USA',
            };
        ]
    );

Or with:

    my $vanessa = Person->new(
        name => {
            first => 'Vanessa',
            last => 'Li'
        },
        description => ['A great student!'],
    );

But all of these would cause a constraint error for the 'name' attribute:

    ## Value for 'name' not a HashRef
    Person->new( name => 'John' );

    ## Value for 'name' has incorrect hash key and missing required keys
    Person->new( name => {
        first_name => 'John'
    });

    ## Also incorrect keys
    Person->new( name => {
        first_name => 'John',
        age => 39,
    });

    ## key 'middle' incorrect type, should be a Str not a ArrayRef
    Person->new( name => {
        first => 'Vanessa',
        middle => [1,2],
        last => 'Li',
    });

And these would cause a constraint error for the 'description' attribute:

    ## Should be an ArrayRef
    Person->new( description => 'Hello I am a String' );

    ## First element must be a string not a HashRef.
    Person->new (description => [{
        tag1 => 'value1',
        tag2 => 'value2'
    }]);

Please see the test cases for more examples.

=head1 DESCRIPTION

A structured type constraint is a standard container L<Moose> type constraint,
such as an ArrayRef or HashRef, which has been enhanced to allow you to
explicitly name all the allowed type constraints inside the structure.  The
generalized form is:

    TypeConstraint[@TypeParameters or %TypeParameters]

Where 'TypeParameters' is an array reference or hash references of
L<Moose::Meta::TypeConstraint> objects.

This type library enables structured type constraints. It is built on top of the
L<MooseX::Types> library system, so you should review the documentation for that
if you are not familiar with it.

=head2 Comparing Parameterized types to Structured types

Parameterized constraints are built into core Moose and you are probably already
familiar with the type constraints 'HashRef' and 'ArrayRef'.  Structured types
have similar functionality, so their syntax is likewise similar. For example,
you could define a parameterized constraint like:

    subtype ArrayOfInts,
     as ArrayRef[Int];

which would constrain a value to something like [1,2,3,...] and so on.  On the
other hand, a structured type constraint explicitly names all it's allowed
'internal' type parameter constraints.  For the example:

    subtype StringFollowedByInt,
     as Tuple[Str,Int];

would constrain it's value to things like ['hello', 111] but ['hello', 'world']
would fail, as well as ['hello', 111, 'world'] and so on.  Here's another
example:

	package MyApp::Types;

    use MooseX::Types -declare [qw(StringIntOptionalHashRef)];
    use MooseX::Types::Moose qw(Str Int);
    use MooseX::Types::Structured qw(Tuple Optional);

    subtype StringIntOptionalHashRef,
     as Tuple[
        Str, Int,
        Optional[HashRef]
     ];

This defines a type constraint that validates values like:

    ['Hello', 100, {key1 => 'value1', key2 => 'value2'}];
    ['World', 200];

Notice that the last type constraint in the structure is optional.  This is
enabled via the helper Optional type constraint, which is a variation of the
core Moose type constraint 'Maybe'.  The main difference is that Optional type
constraints are required to validate if they exist, while 'Maybe' permits
undefined values.  So the following example would not validate:

    StringIntOptionalHashRef->validate(['Hello Undefined', 1000, undef]);

Please note the subtle difference between undefined and null.  If you wish to
allow both null and undefined, you should use the core Moose 'Maybe' type
constraint instead:

    package MyApp::Types;

    use MooseX::Types -declare [qw(StringIntMaybeHashRef)];
    use MooseX::Types::Moose qw(Str Int Maybe);
    use MooseX::Types::Structured qw(Tuple);

    subtype StringIntMaybeHashRef,
     as Tuple[
        Str, Int, Maybe[HashRef]
     ];

This would validate the following:

    ['Hello', 100, {key1 => 'value1', key2 => 'value2'}];
    ['World', 200, undef];
    ['World', 200];

Structured constraints are not limited to arrays.  You can define a structure
against a HashRef with the 'Dict' type constaint as in this example:

    subtype FirstNameLastName,
     as Dict[
        firstname => Str,
        lastname => Str,
     ];

This would constrain a HashRef that validates something like:

    {firstname => 'Christopher', lastname => 'Parsons'};

but all the following would fail validation:

    ## Incorrect keys
    {first => 'Christopher', last => 'Parsons'};

    ## Too many keys
    {firstname => 'Christopher', lastname => 'Parsons', middlename => 'Allen'};

    ## Not a HashRef
    ['Christopher', 'Parsons'];

These structures can be as simple or elaborate as you wish.  You can even
combine various structured, parameterized and simple constraints all together:

    subtype Crazy,
     as Tuple[
        Int,
        Dict[name=>Str, age=>Int],
        ArrayRef[Int]
     ];

Which would match:

    [1, {name=>'John', age=>25},[10,11,12]];

Please notice how the type parameters can be visually arranged to your liking
and to improve the clarity of your meaning.  You don't need to run then
altogether onto a single line.  Additionally, since the 'Dict' type constraint
defines a hash constraint, the key order is not meaningful.  For example:

    subtype AnyKeyOrder,
      as Dict[
        key1=>Int,
        key2=>Str,
        key3=>Int,
     ];

Would validate both:

    {key1 => 1, key2 => "Hi!", key3 => 2};
    {key2 => "Hi!", key1 => 100, key3 => 300};

As you would expect, since underneath its just a plain old Perl hash at work.

=head2 Alternatives

You should exercise some care as to whether or not your complex structured
constraints would be better off contained by a real object as in the following
example:

    package MyApp::MyStruct;
    use Moose;

    ## lazy way to make a bunch of attributes
    has $_ for qw(full_name age_in_years);

    package MyApp::MyClass;
    use Moose;

    has person => (isa => 'MyApp::MyStruct');

    my $instance = MyApp::MyClass->new(
        person=>MyApp::MyStruct->new(
            full_name => 'John',
            age_in_years => 39,
        ),
    );

This method may take some additional time to setup but will give you more
flexibility.  However, structured constraints are highly compatible with this
method, granting some interesting possibilities for coercion.  Try:

    package MyApp::MyClass;

    use Moose;
    use MyApp::MyStruct;

    ## It's recommended your type declarations live in a separate class in order
    ## to promote reusability and clarity.  Inlined here for brevity.

    use MooseX::Types::DateTime qw(DateTime);
    use MooseX::Types -declare [qw(MyStruct)];
    use MooseX::Types::Moose qw(Str Int);
    use MooseX::Types::Structured qw(Dict);

    ## Use class_type to create an ISA type constraint if your object doesn't
    ## inherit from Moose::Object.
    class_type 'MyApp::MyStruct';

    ## Just a shorter version really.
    subtype MyStruct,
     as 'MyApp::MyStruct';

    ## Add the coercions.
    coerce MyStruct,
     from Dict[
        full_name=>Str,
        age_in_years=>Int
     ], via {
        MyApp::MyStruct->new(%$_);
     },
     from Dict[
        lastname=>Str,
        firstname=>Str,
        dob=>DateTime
     ], via {
        my $name = $_->{firstname} .' '. $_->{lastname};
        my $age = DateTime->now - $_->{dob};

        MyApp::MyStruct->new(
            full_name=>$name,
            age_in_years=>$age->years,
        );
     };

    has person => (isa=>MyStruct);

This would allow you to instantiate with something like:

    my $obj = MyApp::MyClass->new( person => {
        full_name=>'John Napiorkowski',
        age_in_years=>39,
    });

Or even:

    my $obj = MyApp::MyClass->new( person => {
        lastname=>'John',
        firstname=>'Napiorkowski',
        dob=>DateTime->new(year=>1969),
    });

If you are not familiar with how coercions work, check out the L<Moose> cookbook
entry L<Moose::Cookbook::Recipe5> for an explanation.  The section L</Coercions>
has additional examples and discussion.

=head2 Subtyping a Structured type constraint

You need to exercise some care when you try to subtype a structured type as in
this example:

    subtype Person,
     as Dict[name => Str];

    subtype FriendlyPerson,
     as Person[
        name => Str,
        total_friends => Int,
     ];

This will actually work BUT you have to take care that the subtype has a
structure that does not contradict the structure of it's parent.  For now the
above works, but I will clarify the syntax for this at a future point, so
it's recommended to avoid (should not really be needed so much anyway).  For
now this is supported in an EXPERIMENTAL way.  Your thoughts, test cases and
patches are welcomed for discussion.  If you find a good use for this, please
let me know.

=head2 Coercions

Coercions currently work for 'one level' deep.  That is you can do:

    subtype Person,
     as Dict[
        name => Str,
        age => Int
    ];

    subtype Fullname,
     as Dict[
        first => Str,
        last => Str
     ];

    coerce Person,
     ## Coerce an object of a particular class
     from BlessedPersonObject, via {
        +{
            name=>$_->name,
            age=>$_->age,
        };
     },

     ## Coerce from [$name, $age]
     from ArrayRef, via {
        +{
            name=>$_->[0],
            age=>$_->[1],
        },
     },
     ## Coerce from {fullname=>{first=>...,last=>...}, dob=>$DateTimeObject}
     from Dict[fullname=>Fullname, dob=>DateTime], via {
        my $age = $_->dob - DateTime->now;
        my $firstn = $_->{fullname}->{first};
        my $lastn = $_->{fullname}->{last}
        +{
            name => $_->{fullname}->{first} .' '. ,
            age =>$age->years
        }
     };

And that should just work as expected.  However, if there are any 'inner'
coercions, such as a coercion on 'Fullname' or on 'DateTime', that coercion
won't currently get activated.

Please see the test '07-coerce.t' for a more detailed example.  Discussion on
extending coercions to support this welcome on the Moose development channel or
mailing list.

=head2 Recursion

Newer versions of L<MooseX::Types> support recursive type constraints.  That is
you can include a type constraint as a contained type constraint of itself.  For
example:

    subtype Person,
     as Dict[
         name=>Str,
         friends=>Optional[
             ArrayRef[Person]
         ],
     ];

This would declare a Person subtype that contains a name and an optional
ArrayRef of Persons who are friends as in:

    {
        name => 'Mike',
        friends => [
            { name => 'John' },
            { name => 'Vincent' },
            {
                name => 'Tracey',
                friends => [
                    { name => 'Stephenie' },
                    { name => 'Ilya' },
                ],
            },
        ],
    };

Please take care to make sure the recursion node is either Optional, or declare
a Union with an non recursive option such as:

    subtype Value
     as Tuple[
         Str,
         Str|Tuple,
     ];

Which validates:

    [
        'Hello', [
            'World', [
                'Is', [
                    'Getting',
                    'Old',
                ],
            ],
        ],
    ];

Otherwise you will define a subtype thatis impossible to validate since it is
infinitely recursive.  For more information about defining recursive types,
please see the documentation in L<MooseX::Types> and the test cases.

=head1 TYPE CONSTRAINTS

This type library defines the following constraints.

=head2 Tuple[@constraints]

This defines an ArrayRef based constraint which allows you to validate a specific
list of contained constraints.  For example:

    Tuple[Int,Str]; ## Validates [1,'hello']
    Tuple[Str|Object, Int]; ## Validates ['hello', 1] or [$object, 2]

The Values of @constraints should ideally be L<MooseX::Types> declared type
constraints.  We do support 'old style' L<Moose> string based constraints to a
limited degree but these string type constraints are considered deprecated.
There will be limited support for bugs resulting from mixing string and
L<MooseX::Types> in your structures.  If you encounter such a bug and really
need it fixed, we will required a detailed test case at the minimum.

=head2 Dict[%constraints]

This defines a HashRef based constraint which allowed you to validate a specific
hashref.  For example:

    Dict[name=>Str, age=>Int]; ## Validates {name=>'John', age=>39}

The keys in %constraints follow the same rules as @constraints in the above
section.

=head2 Map[ $key_constraint, $value_constraint ]

This defines a HashRef based constraint in which both the keys and values are
required to meet certain constraints.  For example, to map hostnames to IP
addresses, you might say:

  Map[ HostName, IPAddress ]

The type constraint would only be met if every key was a valid HostName and
every value was a valid IPAddress.

=head2 Optional[$constraint]

This is primarily a helper constraint for Dict and Tuple type constraints.  What
this allows is for you to assert that a given type constraint is allowed to be
null (but NOT undefined).  If the value is null, then the type constraint passes
but if the value is defined it must validate against the type constraint.  This
makes it easy to make a Dict where one or more of the keys doesn't have to exist
or a tuple where some of the values are not required.  For example:

    subtype Name() => as Dict[
        first=>Str,
        last=>Str,
        middle=>Optional[Str],
    ];

Creates a constraint that validates against a hashref with the keys 'first' and
'last' being strings and required while an optional key 'middle' is must be a
string if it appears but doesn't have to appear.  So in this case both the
following are valid:

    {first=>'John', middle=>'James', last=>'Napiorkowski'}
    {first=>'Vanessa', last=>'Li'}

If you use the 'Maybe' type constraint instead, your values will also validate
against 'undef', which may be incorrect for you.

=head1 EXPORTABLE SUBROUTINES

This type library makes available for export the following subroutines

=head2 slurpy

Structured type constraints by their nature are closed; that is validation will
depend on an exact match between your structure definition and the arguments to
be checked.  Sometimes you might wish for a slightly looser amount of validation.
For example, you may wish to validate the first 3 elements of an array reference
and allow for an arbitrary number of additional elements.  At first thought you
might think you could do it this way:

    #  I want to validate stuff like: [1,"hello", $obj, 2,3,4,5,6,...]
    subtype AllowTailingArgs,
     as Tuple[
       Int,
       Str,
       Object,
       ArrayRef[Int],
     ];

However what this will actually validate are structures like this:

    [10,"Hello", $obj, [11,12,13,...] ]; # Notice element 4 is an ArrayRef

In order to allow structured validation of, "and then some", arguments, you can
use the L</slurpy> method against a type constraint.  For example:

    use MooseX::Types::Structured qw(Tuple slurpy);

    subtype AllowTailingArgs,
     as Tuple[
       Int,
       Str,
       Object,
       slurpy ArrayRef[Int],
     ];

This will now work as expected, validating ArrayRef structures such as:

    [1,"hello", $obj, 2,3,4,5,6,...]

A few caveats apply.  First, the slurpy type constraint must be the last one in
the list of type constraint parameters.  Second, the parent type of the slurpy
type constraint must match that of the containing type constraint.  That means
that a Tuple can allow a slurpy ArrayRef (or children of ArrayRefs, including
another Tuple) and a Dict can allow a slurpy HashRef (or children/subtypes of
HashRef, also including other Dict constraints).

Please note the the technical way this works 'under the hood' is that the
slurpy keyword transforms the target type constraint into a coderef.  Please do
not try to create your own custom coderefs; always use the slurpy method.  The
underlying technology may change in the future but the slurpy keyword will be
supported.

=head1 ERROR MESSAGES

Error reporting has been improved to return more useful debugging messages. Now
I will stringify the incoming check value with L<Devel::PartialDump> so that you
can see the actual structure that is tripping up validation.  Also, I report the
'internal' validation error, so that if a particular element inside the
Structured Type is failing validation, you will see that.  There's a limit to
how deep this internal reporting goes, but you shouldn't see any of the "failed
with ARRAY(XXXXXX)" that we got with earlier versions of this module.

This support is continuing to expand, so it's best to use these messages for
debugging purposes and not for creating messages that 'escape into the wild'
such as error messages sent to the user.

Please see the test '12-error.t' for a more lengthy example.  Your thoughts and
preferable tests or code patches very welcome!

=head1 EXAMPLES

Here are some additional example usage for structured types.  All examples can
be found also in the 't/examples.t' test.  Your contributions are also welcomed.

=head2 Normalize a HashRef

You need a hashref to conform to a canonical structure but are required accept a
bunch of different incoming structures.  You can normalize using the Dict type
constraint and coercions.  This example also shows structured types mixed which
other MooseX::Types libraries.

    package Test::MooseX::Meta::TypeConstraint::Structured::Examples::Normalize;

    use Moose;
    use DateTime;

    use MooseX::Types::Structured qw(Dict Tuple);
    use MooseX::Types::DateTime qw(DateTime);
    use MooseX::Types::Moose qw(Int Str Object);
    use MooseX::Types -declare => [qw(Name Age Person)];

    subtype Person,
     as Dict[
         name=>Str,
         age=>Int,
     ];

    coerce Person,
     from Dict[
         first=>Str,
         last=>Str,
         years=>Int,
     ], via { +{
        name => "$_->{first} $_->{last}",
        age => $_->{years},
     }},
     from Dict[
         fullname=>Dict[
             last=>Str,
             first=>Str,
         ],
         dob=>DateTime,
     ],
     ## DateTime needs to be inside of single quotes here to disambiguate the
     ## class package from the DataTime type constraint imported via the
     ## line "use MooseX::Types::DateTime qw(DateTime);"
     via { +{
        name => "$_->{fullname}{first} $_->{fullname}{last}",
        age => ($_->{dob} - 'DateTime'->now)->years,
     }};

    has person => (is=>'rw', isa=>Person, coerce=>1);

And now you can instantiate with all the following:

    __PACKAGE__->new(
        person=>{
            name=>'John Napiorkowski',
            age=>39,
        },
    );

    __PACKAGE__->new(
        person=>{
            first=>'John',
            last=>'Napiorkowski',
            years=>39,
        },
    );

    __PACKAGE__->new(
        person=>{
            fullname => {
                first=>'John',
                last=>'Napiorkowski'
            },
            dob => 'DateTime'->new(
                year=>1969,
                month=>2,
                day=>13
            ),
        },
    );

This technique is a way to support various ways to instantiate your class in a
clean and declarative way.

=head1 SEE ALSO

The following modules or resources may be of interest.

L<Moose>, L<MooseX::Types>, L<Moose::Meta::TypeConstraint>,
L<MooseX::Meta::TypeConstraint::Structured>

=head1 AUTHORS

=over 4

=item *

John Napiorkowski <jjnapiork@cpan.org>

=item *

Florian Ragwitz <rafl@debian.org>

=item *

Yuval Kogman <nothingmuch@woobling.org>

=item *

Tomas Doran <bobtfish@bobtfish.net>

=item *

Robert Sedlacek <rs@474.at>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2011 by John Napiorkowski.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut

