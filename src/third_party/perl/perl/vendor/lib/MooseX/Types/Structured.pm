package MooseX::Types::Structured; # git description: v0.35-8-gc2cf3da
# ABSTRACT: Structured Type Constraints for Moose

our $VERSION = '0.36';

use 5.008;
use Moose::Util::TypeConstraints 1.06 'find_type_constraint';
use MooseX::Meta::TypeConstraint::Structured;
use MooseX::Meta::TypeConstraint::Structured::Optional;
use MooseX::Types::Structured::OverflowHandler;
use MooseX::Types::Structured::MessageStack;
use Devel::PartialDump 0.13;
use Scalar::Util qw(blessed);
use namespace::clean 0.19;
use MooseX::Types 0.22 -declare => [qw(Dict Map Tuple Optional)];
use Sub::Exporter 0.982 -setup => {
    exports => [ qw(Dict Map Tuple Optional slurpy) ],
};
use if MooseX::Types->VERSION >= 0.42,
    'namespace::autoclean' => -except => 'import'; # TODO: https://github.com/rjbs/Sub-Exporter/issues/8

#pod =head1 SYNOPSIS
#pod
#pod The following is example usage for this module.
#pod
#pod     package Person;
#pod
#pod     use Moose;
#pod     use MooseX::Types::Moose qw(Str Int HashRef);
#pod     use MooseX::Types::Structured qw(Dict Tuple Optional);
#pod
#pod     ## A name has a first and last part, but middle names are not required
#pod     has name => (
#pod         isa=>Dict[
#pod             first => Str,
#pod             last => Str,
#pod             middle => Optional[Str],
#pod         ],
#pod     );
#pod
#pod     ## description is a string field followed by a HashRef of tagged data.
#pod     has description => (
#pod       isa=>Tuple[
#pod         Str,
#pod         Optional[HashRef],
#pod      ],
#pod     );
#pod
#pod     ## Remainder of your class attributes and methods
#pod
#pod Then you can instantiate this class with something like:
#pod
#pod     my $john = Person->new(
#pod         name => {
#pod             first => 'John',
#pod             middle => 'James'
#pod             last => 'Napiorkowski',
#pod         },
#pod         description => [
#pod             'A cool guy who loves Perl and Moose.', {
#pod                 married_to => 'Vanessa Li',
#pod                 born_in => 'USA',
#pod             };
#pod         ]
#pod     );
#pod
#pod Or with:
#pod
#pod     my $vanessa = Person->new(
#pod         name => {
#pod             first => 'Vanessa',
#pod             last => 'Li'
#pod         },
#pod         description => ['A great student!'],
#pod     );
#pod
#pod But all of these would cause a constraint error for the C<name> attribute:
#pod
#pod     ## Value for 'name' not a HashRef
#pod     Person->new( name => 'John' );
#pod
#pod     ## Value for 'name' has incorrect hash key and missing required keys
#pod     Person->new( name => {
#pod         first_name => 'John'
#pod     });
#pod
#pod     ## Also incorrect keys
#pod     Person->new( name => {
#pod         first_name => 'John',
#pod         age => 39,
#pod     });
#pod
#pod     ## key 'middle' incorrect type, should be a Str not a ArrayRef
#pod     Person->new( name => {
#pod         first => 'Vanessa',
#pod         middle => [1,2],
#pod         last => 'Li',
#pod     });
#pod
#pod And these would cause a constraint error for the C<description> attribute:
#pod
#pod     ## Should be an ArrayRef
#pod     Person->new( description => 'Hello I am a String' );
#pod
#pod     ## First element must be a string not a HashRef.
#pod     Person->new (description => [{
#pod         tag1 => 'value1',
#pod         tag2 => 'value2'
#pod     }]);
#pod
#pod Please see the test cases for more examples.
#pod
#pod =head1 DESCRIPTION
#pod
#pod A structured type constraint is a standard container L<Moose> type constraint,
#pod such as an C<ArrayRef> or C<HashRef>, which has been enhanced to allow you to
#pod explicitly name all the allowed type constraints inside the structure.  The
#pod generalized form is:
#pod
#pod     TypeConstraint[@TypeParameters or %TypeParameters]
#pod
#pod Where C<TypeParameters> is an array reference or hash references of
#pod L<Moose::Meta::TypeConstraint> objects.
#pod
#pod This type library enables structured type constraints. It is built on top of the
#pod L<MooseX::Types> library system, so you should review the documentation for that
#pod if you are not familiar with it.
#pod
#pod =head2 Comparing Parameterized types to Structured types
#pod
#pod Parameterized constraints are built into core Moose and you are probably already
#pod familiar with the type constraints C<HashRef> and C<ArrayRef>.  Structured types
#pod have similar functionality, so their syntax is likewise similar. For example,
#pod you could define a parameterized constraint like:
#pod
#pod     subtype ArrayOfInts,
#pod      as ArrayRef[Int];
#pod
#pod which would constrain a value to something like [1,2,3,...] and so on.  On the
#pod other hand, a structured type constraint explicitly names all it's allowed
#pod 'internal' type parameter constraints.  For the example:
#pod
#pod     subtype StringFollowedByInt,
#pod      as Tuple[Str,Int];
#pod
#pod would constrain its value to things like C<< ['hello', 111] >>  but C<< ['hello', 'world'] >>
#pod would fail, as well as C<< ['hello', 111, 'world'] >> and so on.  Here's another
#pod example:
#pod
#pod 	package MyApp::Types;
#pod
#pod     use MooseX::Types -declare [qw(StringIntOptionalHashRef)];
#pod     use MooseX::Types::Moose qw(Str Int);
#pod     use MooseX::Types::Structured qw(Tuple Optional);
#pod
#pod     subtype StringIntOptionalHashRef,
#pod      as Tuple[
#pod         Str, Int,
#pod         Optional[HashRef]
#pod      ];
#pod
#pod This defines a type constraint that validates values like:
#pod
#pod     ['Hello', 100, {key1 => 'value1', key2 => 'value2'}];
#pod     ['World', 200];
#pod
#pod Notice that the last type constraint in the structure is optional.  This is
#pod enabled via the helper C<Optional> type constraint, which is a variation of the
#pod core Moose type constraint C<Maybe>.  The main difference is that C<Optional> type
#pod constraints are required to validate if they exist, while C<Maybe> permits
#pod undefined values.  So the following example would not validate:
#pod
#pod     StringIntOptionalHashRef->validate(['Hello Undefined', 1000, undef]);
#pod
#pod Please note the subtle difference between undefined and null.  If you wish to
#pod allow both null and undefined, you should use the core Moose C<Maybe> type
#pod constraint instead:
#pod
#pod     package MyApp::Types;
#pod
#pod     use MooseX::Types -declare [qw(StringIntMaybeHashRef)];
#pod     use MooseX::Types::Moose qw(Str Int Maybe);
#pod     use MooseX::Types::Structured qw(Tuple);
#pod
#pod     subtype StringIntMaybeHashRef,
#pod      as Tuple[
#pod         Str, Int, Maybe[HashRef]
#pod      ];
#pod
#pod This would validate the following:
#pod
#pod     ['Hello', 100, {key1 => 'value1', key2 => 'value2'}];
#pod     ['World', 200, undef];
#pod     ['World', 200];
#pod
#pod Structured constraints are not limited to arrays.  You can define a structure
#pod against a C<HashRef> with the C<Dict> type constraint as in this example:
#pod
#pod     subtype FirstNameLastName,
#pod      as Dict[
#pod         firstname => Str,
#pod         lastname => Str,
#pod      ];
#pod
#pod This would constrain a C<HashRef> that validates something like:
#pod
#pod     {firstname => 'Christopher', lastname => 'Parsons'};
#pod
#pod but all the following would fail validation:
#pod
#pod     ## Incorrect keys
#pod     {first => 'Christopher', last => 'Parsons'};
#pod
#pod     ## Too many keys
#pod     {firstname => 'Christopher', lastname => 'Parsons', middlename => 'Allen'};
#pod
#pod     ## Not a HashRef
#pod     ['Christopher', 'Parsons'];
#pod
#pod These structures can be as simple or elaborate as you wish.  You can even
#pod combine various structured, parameterized and simple constraints all together:
#pod
#pod     subtype Crazy,
#pod      as Tuple[
#pod         Int,
#pod         Dict[name=>Str, age=>Int],
#pod         ArrayRef[Int]
#pod      ];
#pod
#pod Which would match:
#pod
#pod     [1, {name=>'John', age=>25},[10,11,12]];
#pod
#pod Please notice how the type parameters can be visually arranged to your liking
#pod and to improve the clarity of your meaning.  You don't need to run then
#pod altogether onto a single line.  Additionally, since the C<Dict> type constraint
#pod defines a hash constraint, the key order is not meaningful.  For example:
#pod
#pod     subtype AnyKeyOrder,
#pod       as Dict[
#pod         key1=>Int,
#pod         key2=>Str,
#pod         key3=>Int,
#pod      ];
#pod
#pod Would validate both:
#pod
#pod     {key1 => 1, key2 => "Hi!", key3 => 2};
#pod     {key2 => "Hi!", key1 => 100, key3 => 300};
#pod
#pod As you would expect, since underneath it's just a plain old Perl hash at work.
#pod
#pod =head2 Alternatives
#pod
#pod You should exercise some care as to whether or not your complex structured
#pod constraints would be better off contained by a real object as in the following
#pod example:
#pod
#pod     package MyApp::MyStruct;
#pod     use Moose;
#pod
#pod     ## lazy way to make a bunch of attributes
#pod     has $_ for qw(full_name age_in_years);
#pod
#pod     package MyApp::MyClass;
#pod     use Moose;
#pod
#pod     has person => (isa => 'MyApp::MyStruct');
#pod
#pod     my $instance = MyApp::MyClass->new(
#pod         person=>MyApp::MyStruct->new(
#pod             full_name => 'John',
#pod             age_in_years => 39,
#pod         ),
#pod     );
#pod
#pod This method may take some additional time to set up but will give you more
#pod flexibility.  However, structured constraints are highly compatible with this
#pod method, granting some interesting possibilities for coercion.  Try:
#pod
#pod     package MyApp::MyClass;
#pod
#pod     use Moose;
#pod     use MyApp::MyStruct;
#pod
#pod     ## It's recommended your type declarations live in a separate class in order
#pod     ## to promote reusability and clarity.  Inlined here for brevity.
#pod
#pod     use MooseX::Types::DateTime qw(DateTime);
#pod     use MooseX::Types -declare [qw(MyStruct)];
#pod     use MooseX::Types::Moose qw(Str Int);
#pod     use MooseX::Types::Structured qw(Dict);
#pod
#pod     ## Use class_type to create an ISA type constraint if your object doesn't
#pod     ## inherit from Moose::Object.
#pod     class_type 'MyApp::MyStruct';
#pod
#pod     ## Just a shorter version really.
#pod     subtype MyStruct,
#pod      as 'MyApp::MyStruct';
#pod
#pod     ## Add the coercions.
#pod     coerce MyStruct,
#pod      from Dict[
#pod         full_name=>Str,
#pod         age_in_years=>Int
#pod      ], via {
#pod         MyApp::MyStruct->new(%$_);
#pod      },
#pod      from Dict[
#pod         lastname=>Str,
#pod         firstname=>Str,
#pod         dob=>DateTime
#pod      ], via {
#pod         my $name = $_->{firstname} .' '. $_->{lastname};
#pod         my $age = DateTime->now - $_->{dob};
#pod
#pod         MyApp::MyStruct->new(
#pod             full_name=>$name,
#pod             age_in_years=>$age->years,
#pod         );
#pod      };
#pod
#pod     has person => (isa=>MyStruct);
#pod
#pod This would allow you to instantiate with something like:
#pod
#pod     my $obj = MyApp::MyClass->new( person => {
#pod         full_name=>'John Napiorkowski',
#pod         age_in_years=>39,
#pod     });
#pod
#pod Or even:
#pod
#pod     my $obj = MyApp::MyClass->new( person => {
#pod         lastname=>'John',
#pod         firstname=>'Napiorkowski',
#pod         dob=>DateTime->new(year=>1969),
#pod     });
#pod
#pod If you are not familiar with how coercions work, check out the L<Moose> cookbook
#pod entry L<Moose::Cookbook::Recipe5> for an explanation.  The section L</Coercions>
#pod has additional examples and discussion.
#pod
#pod =for stopwords Subtyping
#pod
#pod =head2 Subtyping a Structured type constraint
#pod
#pod You need to exercise some care when you try to subtype a structured type as in
#pod this example:
#pod
#pod     subtype Person,
#pod      as Dict[name => Str];
#pod
#pod     subtype FriendlyPerson,
#pod      as Person[
#pod         name => Str,
#pod         total_friends => Int,
#pod      ];
#pod
#pod This will actually work BUT you have to take care that the subtype has a
#pod structure that does not contradict the structure of it's parent.  For now the
#pod above works, but I will clarify the syntax for this at a future point, so
#pod it's recommended to avoid (should not really be needed so much anyway).  For
#pod now this is supported in an EXPERIMENTAL way.  Your thoughts, test cases and
#pod patches are welcomed for discussion.  If you find a good use for this, please
#pod let me know.
#pod
#pod =head2 Coercions
#pod
#pod Coercions currently work for 'one level' deep.  That is you can do:
#pod
#pod     subtype Person,
#pod      as Dict[
#pod         name => Str,
#pod         age => Int
#pod     ];
#pod
#pod     subtype Fullname,
#pod      as Dict[
#pod         first => Str,
#pod         last => Str
#pod      ];
#pod
#pod     coerce Person,
#pod      ## Coerce an object of a particular class
#pod      from BlessedPersonObject, via {
#pod         +{
#pod             name=>$_->name,
#pod             age=>$_->age,
#pod         };
#pod      },
#pod
#pod      ## Coerce from [$name, $age]
#pod      from ArrayRef, via {
#pod         +{
#pod             name=>$_->[0],
#pod             age=>$_->[1],
#pod         },
#pod      },
#pod      ## Coerce from {fullname=>{first=>...,last=>...}, dob=>$DateTimeObject}
#pod      from Dict[fullname=>Fullname, dob=>DateTime], via {
#pod         my $age = $_->dob - DateTime->now;
#pod         my $firstn = $_->{fullname}->{first};
#pod         my $lastn = $_->{fullname}->{last}
#pod         +{
#pod             name => $_->{fullname}->{first} .' '. ,
#pod             age =>$age->years
#pod         }
#pod      };
#pod
#pod And that should just work as expected.  However, if there are any 'inner'
#pod coercions, such as a coercion on C<Fullname> or on C<DateTime>, that coercion
#pod won't currently get activated.
#pod
#pod Please see the test F<07-coerce.t> for a more detailed example.  Discussion on
#pod extending coercions to support this welcome on the Moose development channel or
#pod mailing list.
#pod
#pod =head2 Recursion
#pod
#pod Newer versions of L<MooseX::Types> support recursive type constraints.  That is
#pod you can include a type constraint as a contained type constraint of itself.  For
#pod example:
#pod
#pod     subtype Person,
#pod      as Dict[
#pod          name=>Str,
#pod          friends=>Optional[
#pod              ArrayRef[Person]
#pod          ],
#pod      ];
#pod
#pod This would declare a C<Person> subtype that contains a name and an optional
#pod C<ArrayRef> of C<Person>s who are friends as in:
#pod
#pod     {
#pod         name => 'Mike',
#pod         friends => [
#pod             { name => 'John' },
#pod             { name => 'Vincent' },
#pod             {
#pod                 name => 'Tracey',
#pod                 friends => [
#pod                     { name => 'Stephenie' },
#pod                     { name => 'Ilya' },
#pod                 ],
#pod             },
#pod         ],
#pod     };
#pod
#pod Please take care to make sure the recursion node is either C<Optional>, or declare
#pod a union with an non-recursive option such as:
#pod
#pod     subtype Value
#pod      as Tuple[
#pod          Str,
#pod          Str|Tuple,
#pod      ];
#pod
#pod Which validates:
#pod
#pod     [
#pod         'Hello', [
#pod             'World', [
#pod                 'Is', [
#pod                     'Getting',
#pod                     'Old',
#pod                 ],
#pod             ],
#pod         ],
#pod     ];
#pod
#pod Otherwise you will define a subtype that is impossible to validate since it is
#pod infinitely recursive.  For more information about defining recursive types,
#pod please see the documentation in L<MooseX::Types> and the test cases.
#pod
#pod =head1 TYPE CONSTRAINTS
#pod
#pod This type library defines the following constraints.
#pod
#pod =head2 Tuple[@constraints]
#pod
#pod This defines an ArrayRef based constraint which allows you to validate a specific
#pod list of contained constraints.  For example:
#pod
#pod     Tuple[Int,Str]; ## Validates [1,'hello']
#pod     Tuple[Str|Object, Int]; ## Validates ['hello', 1] or [$object, 2]
#pod
#pod The Values of @constraints should ideally be L<MooseX::Types> declared type
#pod constraints.  We do support 'old style' L<Moose> string based constraints to a
#pod limited degree but these string type constraints are considered deprecated.
#pod There will be limited support for bugs resulting from mixing string and
#pod L<MooseX::Types> in your structures.  If you encounter such a bug and really
#pod need it fixed, we will required a detailed test case at the minimum.
#pod
#pod =head2 Dict[%constraints]
#pod
#pod This defines a HashRef based constraint which allowed you to validate a specific
#pod hashref.  For example:
#pod
#pod     Dict[name=>Str, age=>Int]; ## Validates {name=>'John', age=>39}
#pod
#pod The keys in C<%constraints> follow the same rules as C<@constraints> in the above
#pod section.
#pod
#pod =head2 Map[ $key_constraint, $value_constraint ]
#pod
#pod This defines a C<HashRef>-based constraint in which both the keys and values are
#pod required to meet certain constraints.  For example, to map hostnames to IP
#pod addresses, you might say:
#pod
#pod   Map[ HostName, IPAddress ]
#pod
#pod The type constraint would only be met if every key was a valid C<HostName> and
#pod every value was a valid C<IPAddress>.
#pod
#pod =head2 Optional[$constraint]
#pod
#pod This is primarily a helper constraint for C<Dict> and C<Tuple> type constraints.  What
#pod this allows is for you to assert that a given type constraint is allowed to be
#pod null (but NOT undefined).  If the value is null, then the type constraint passes
#pod but if the value is defined it must validate against the type constraint.  This
#pod makes it easy to make a Dict where one or more of the keys doesn't have to exist
#pod or a tuple where some of the values are not required.  For example:
#pod
#pod     subtype Name() => as Dict[
#pod         first=>Str,
#pod         last=>Str,
#pod         middle=>Optional[Str],
#pod     ];
#pod
#pod ...creates a constraint that validates against a hashref with the keys 'first' and
#pod 'last' being strings and required while an optional key 'middle' is must be a
#pod string if it appears but doesn't have to appear.  So in this case both the
#pod following are valid:
#pod
#pod     {first=>'John', middle=>'James', last=>'Napiorkowski'}
#pod     {first=>'Vanessa', last=>'Li'}
#pod
#pod If you use the C<Maybe> type constraint instead, your values will also validate
#pod against C<undef>, which may be incorrect for you.
#pod
#pod =head1 EXPORTABLE SUBROUTINES
#pod
#pod This type library makes available for export the following subroutines
#pod
#pod =for stopwords slurpy
#pod
#pod =head2 slurpy
#pod
#pod Structured type constraints by their nature are closed; that is validation will
#pod depend on an exact match between your structure definition and the arguments to
#pod be checked.  Sometimes you might wish for a slightly looser amount of validation.
#pod For example, you may wish to validate the first 3 elements of an array reference
#pod and allow for an arbitrary number of additional elements.  At first thought you
#pod might think you could do it this way:
#pod
#pod     #  I want to validate stuff like: [1,"hello", $obj, 2,3,4,5,6,...]
#pod     subtype AllowTailingArgs,
#pod      as Tuple[
#pod        Int,
#pod        Str,
#pod        Object,
#pod        ArrayRef[Int],
#pod      ];
#pod
#pod However what this will actually validate are structures like this:
#pod
#pod     [10,"Hello", $obj, [11,12,13,...] ]; # Notice element 4 is an ArrayRef
#pod
#pod In order to allow structured validation of, "and then some", arguments, you can
#pod use the L</slurpy> method against a type constraint.  For example:
#pod
#pod     use MooseX::Types::Structured qw(Tuple slurpy);
#pod
#pod     subtype AllowTailingArgs,
#pod      as Tuple[
#pod        Int,
#pod        Str,
#pod        Object,
#pod        slurpy ArrayRef[Int],
#pod      ];
#pod
#pod This will now work as expected, validating ArrayRef structures such as:
#pod
#pod     [1,"hello", $obj, 2,3,4,5,6,...]
#pod
#pod A few caveats apply.  First, the slurpy type constraint must be the last one in
#pod the list of type constraint parameters.  Second, the parent type of the slurpy
#pod type constraint must match that of the containing type constraint.  That means
#pod that a C<Tuple> can allow a slurpy C<ArrayRef> (or children of C<ArrayRef>s, including
#pod another C<Tuple>) and a C<Dict> can allow a slurpy C<HashRef> (or children/subtypes of
#pod HashRef, also including other C<Dict> constraints).
#pod
#pod Please note the technical way this works 'under the hood' is that the
#pod slurpy keyword transforms the target type constraint into a coderef.  Please do
#pod not try to create your own custom coderefs; always use the slurpy method.  The
#pod underlying technology may change in the future but the slurpy keyword will be
#pod supported.
#pod
#pod =head1 ERROR MESSAGES
#pod
#pod Error reporting has been improved to return more useful debugging messages. Now
#pod I will stringify the incoming check value with L<Devel::PartialDump> so that you
#pod can see the actual structure that is tripping up validation.  Also, I report the
#pod 'internal' validation error, so that if a particular element inside the
#pod Structured Type is failing validation, you will see that.  There's a limit to
#pod how deep this internal reporting goes, but you shouldn't see any of the "failed
#pod with ARRAY(XXXXXX)" that we got with earlier versions of this module.
#pod
#pod This support is continuing to expand, so it's best to use these messages for
#pod debugging purposes and not for creating messages that 'escape into the wild'
#pod such as error messages sent to the user.
#pod
#pod Please see the test '12-error.t' for a more lengthy example.  Your thoughts and
#pod preferable tests or code patches very welcome!
#pod
#pod =head1 EXAMPLES
#pod
#pod Here are some additional example usage for structured types.  All examples can
#pod be found also in the 't/examples.t' test.  Your contributions are also welcomed.
#pod
#pod =head2 Normalize a HashRef
#pod
#pod You need a hashref to conform to a canonical structure but are required accept a
#pod bunch of different incoming structures.  You can normalize using the C<Dict> type
#pod constraint and coercions.  This example also shows structured types mixed which
#pod other L<MooseX::Types> libraries.
#pod
#pod     package Test::MooseX::Meta::TypeConstraint::Structured::Examples::Normalize;
#pod
#pod     use Moose;
#pod     use DateTime;
#pod
#pod     use MooseX::Types::Structured qw(Dict Tuple);
#pod     use MooseX::Types::DateTime qw(DateTime);
#pod     use MooseX::Types::Moose qw(Int Str Object);
#pod     use MooseX::Types -declare => [qw(Name Age Person)];
#pod
#pod     subtype Person,
#pod      as Dict[
#pod          name=>Str,
#pod          age=>Int,
#pod      ];
#pod
#pod     coerce Person,
#pod      from Dict[
#pod          first=>Str,
#pod          last=>Str,
#pod          years=>Int,
#pod      ], via { +{
#pod         name => "$_->{first} $_->{last}",
#pod         age => $_->{years},
#pod      }},
#pod      from Dict[
#pod          fullname=>Dict[
#pod              last=>Str,
#pod              first=>Str,
#pod          ],
#pod          dob=>DateTime,
#pod      ],
#pod      ## DateTime needs to be inside of single quotes here to disambiguate the
#pod      ## class package from the DataTime type constraint imported via the
#pod      ## line "use MooseX::Types::DateTime qw(DateTime);"
#pod      via { +{
#pod         name => "$_->{fullname}{first} $_->{fullname}{last}",
#pod         age => ($_->{dob} - 'DateTime'->now)->years,
#pod      }};
#pod
#pod     has person => (is=>'rw', isa=>Person, coerce=>1);
#pod
#pod And now you can instantiate with all the following:
#pod
#pod     __PACKAGE__->new(
#pod         person=>{
#pod             name=>'John Napiorkowski',
#pod             age=>39,
#pod         },
#pod     );
#pod
#pod     __PACKAGE__->new(
#pod         person=>{
#pod             first=>'John',
#pod             last=>'Napiorkowski',
#pod             years=>39,
#pod         },
#pod     );
#pod
#pod     __PACKAGE__->new(
#pod         person=>{
#pod             fullname => {
#pod                 first=>'John',
#pod                 last=>'Napiorkowski'
#pod             },
#pod             dob => 'DateTime'->new(
#pod                 year=>1969,
#pod                 month=>2,
#pod                 day=>13
#pod             ),
#pod         },
#pod     );
#pod
#pod This technique is a way to support various ways to instantiate your class in a
#pod clean and declarative way.
#pod
#pod =cut

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

#pod =head1 SEE ALSO
#pod
#pod The following modules or resources may be of interest.
#pod
#pod L<Moose>, L<MooseX::Types>, L<Moose::Meta::TypeConstraint>,
#pod L<MooseX::Meta::TypeConstraint::Structured>
#pod
#pod =cut

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

MooseX::Types::Structured - Structured Type Constraints for Moose

=head1 VERSION

version 0.36

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

But all of these would cause a constraint error for the C<name> attribute:

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

And these would cause a constraint error for the C<description> attribute:

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
such as an C<ArrayRef> or C<HashRef>, which has been enhanced to allow you to
explicitly name all the allowed type constraints inside the structure.  The
generalized form is:

    TypeConstraint[@TypeParameters or %TypeParameters]

Where C<TypeParameters> is an array reference or hash references of
L<Moose::Meta::TypeConstraint> objects.

This type library enables structured type constraints. It is built on top of the
L<MooseX::Types> library system, so you should review the documentation for that
if you are not familiar with it.

=head2 Comparing Parameterized types to Structured types

Parameterized constraints are built into core Moose and you are probably already
familiar with the type constraints C<HashRef> and C<ArrayRef>.  Structured types
have similar functionality, so their syntax is likewise similar. For example,
you could define a parameterized constraint like:

    subtype ArrayOfInts,
     as ArrayRef[Int];

which would constrain a value to something like [1,2,3,...] and so on.  On the
other hand, a structured type constraint explicitly names all it's allowed
'internal' type parameter constraints.  For the example:

    subtype StringFollowedByInt,
     as Tuple[Str,Int];

would constrain its value to things like C<< ['hello', 111] >>  but C<< ['hello', 'world'] >>
would fail, as well as C<< ['hello', 111, 'world'] >> and so on.  Here's another
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
enabled via the helper C<Optional> type constraint, which is a variation of the
core Moose type constraint C<Maybe>.  The main difference is that C<Optional> type
constraints are required to validate if they exist, while C<Maybe> permits
undefined values.  So the following example would not validate:

    StringIntOptionalHashRef->validate(['Hello Undefined', 1000, undef]);

Please note the subtle difference between undefined and null.  If you wish to
allow both null and undefined, you should use the core Moose C<Maybe> type
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
against a C<HashRef> with the C<Dict> type constraint as in this example:

    subtype FirstNameLastName,
     as Dict[
        firstname => Str,
        lastname => Str,
     ];

This would constrain a C<HashRef> that validates something like:

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
altogether onto a single line.  Additionally, since the C<Dict> type constraint
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

As you would expect, since underneath it's just a plain old Perl hash at work.

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

This method may take some additional time to set up but will give you more
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

=for stopwords Subtyping

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
coercions, such as a coercion on C<Fullname> or on C<DateTime>, that coercion
won't currently get activated.

Please see the test F<07-coerce.t> for a more detailed example.  Discussion on
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

This would declare a C<Person> subtype that contains a name and an optional
C<ArrayRef> of C<Person>s who are friends as in:

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

Please take care to make sure the recursion node is either C<Optional>, or declare
a union with an non-recursive option such as:

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

Otherwise you will define a subtype that is impossible to validate since it is
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

The keys in C<%constraints> follow the same rules as C<@constraints> in the above
section.

=head2 Map[ $key_constraint, $value_constraint ]

This defines a C<HashRef>-based constraint in which both the keys and values are
required to meet certain constraints.  For example, to map hostnames to IP
addresses, you might say:

  Map[ HostName, IPAddress ]

The type constraint would only be met if every key was a valid C<HostName> and
every value was a valid C<IPAddress>.

=head2 Optional[$constraint]

This is primarily a helper constraint for C<Dict> and C<Tuple> type constraints.  What
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

...creates a constraint that validates against a hashref with the keys 'first' and
'last' being strings and required while an optional key 'middle' is must be a
string if it appears but doesn't have to appear.  So in this case both the
following are valid:

    {first=>'John', middle=>'James', last=>'Napiorkowski'}
    {first=>'Vanessa', last=>'Li'}

If you use the C<Maybe> type constraint instead, your values will also validate
against C<undef>, which may be incorrect for you.

=head1 EXPORTABLE SUBROUTINES

This type library makes available for export the following subroutines

=for stopwords slurpy

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
that a C<Tuple> can allow a slurpy C<ArrayRef> (or children of C<ArrayRef>s, including
another C<Tuple>) and a C<Dict> can allow a slurpy C<HashRef> (or children/subtypes of
HashRef, also including other C<Dict> constraints).

Please note the technical way this works 'under the hood' is that the
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
bunch of different incoming structures.  You can normalize using the C<Dict> type
constraint and coercions.  This example also shows structured types mixed which
other L<MooseX::Types> libraries.

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

=head1 SUPPORT

Bugs may be submitted through L<the RT bug tracker|https://rt.cpan.org/Public/Dist/Display.html?Name=MooseX-Types-Structured>
(or L<bug-MooseX-Types-Structured@rt.cpan.org|mailto:bug-MooseX-Types-Structured@rt.cpan.org>).

There is also a mailing list available for users of this distribution, at
L<http://lists.perl.org/list/moose.html>.

There is also an irc channel available for users of this distribution, at
L<C<#moose> on C<irc.perl.org>|irc://irc.perl.org/#moose>.

=head1 AUTHORS

=over 4

=item *

John Napiorkowski <jjnapiork@cpan.org>

=item *

Florian Ragwitz <rafl@debian.org>

=item *

יובל קוג'מן (Yuval Kogman) <nothingmuch@woobling.org>

=item *

Tomas (t0m) Doran <bobtfish@bobtfish.net>

=item *

Robert Sedlacek <rs@474.at>

=back

=head1 CONTRIBUTORS

=for stopwords Karen Etheridge Ricardo Signes Dave Rolsky Ansgar Burchardt Stevan Little arcanez Jesse Luehrs D. Ilmari Mannsåker

=over 4

=item *

Karen Etheridge <ether@cpan.org>

=item *

Ricardo Signes <rjbs@cpan.org>

=item *

Dave Rolsky <autarch@urth.org>

=item *

Ansgar Burchardt <ansgar@43-1.org>

=item *

Stevan Little <stevan.little@iinteractive.com>

=item *

arcanez <justin.d.hunter@gmail.com>

=item *

Jesse Luehrs <doy@tozt.net>

=item *

D. Ilmari Mannsåker <ilmari@cpan.org>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2008 by John Napiorkowski.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
