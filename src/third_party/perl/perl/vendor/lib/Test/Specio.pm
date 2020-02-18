package Test::Specio;

use strict;
use warnings;

our $VERSION = '0.43';

use B ();
use IO::File;
use Scalar::Util qw( blessed looks_like_number openhandle );
use Specio::Library::Builtins;
use Specio::Library::Numeric;
use Specio::Library::Perl;
use Specio::Library::String;

# Loading this will force subification to use Sub::Quote, which can expose
# some bugs.
use Sub::Quote;
use Test::Fatal;
use Test::More 0.96;
use Try::Tiny;

use Exporter qw( import );

our $ZERO    = 0;
our $ONE     = 1;
our $INT     = 100;
our $NEG_INT = -100;
our $NUM     = 42.42;
our $NEG_NUM = -42.42;

our $EMPTY_STRING  = q{};
our $STRING        = 'foo';
our $NUM_IN_STRING = 'has 42 in it';
our $INT_WITH_NL1  = "1\n";
our $INT_WITH_NL2  = "\n1";

our $SCALAR_REF = do {
    ## no critic (Variables::ProhibitUnusedVariables)
    \( my $var );
};
our $SCALAR_REF_REF = \$SCALAR_REF;
our $ARRAY_REF      = [];
our $HASH_REF       = {};
our $CODE_REF       = sub { };

our $GLOB_REF = \*GLOB;

our $FH;
## no critic (InputOutput::RequireBriefOpen)
open $FH, '<', $INC{'Test/Specio.pm'}
    or die "Could not open $INC{'Test/Specio.pm'} for the test";

our $FH_OBJECT = IO::File->new( $INC{'Test/Specio.pm'}, 'r' )
    or die "Could not open $INC{'Test/Specio.pm'} for the test";

our $REGEX      = qr/../;
our $REGEX_OBJ  = bless qr/../, 'BlessedQR';
our $FAKE_REGEX = bless {}, 'Regexp';

our $OBJECT = bless {}, 'FakeObject';

our $UNDEF = undef;

## no critic (Modules::ProhibitMultiplePackages)
{
    package _T::Thing;

    sub foo { }
}

our $CLASS_NAME = '_T::Thing';

{
    package _T::BoolOverload;

    use overload
        'bool'   => sub { ${ $_[0] } },
        fallback => 0;

    sub new {
        my $bool = $_[1];
        bless \$bool, __PACKAGE__;
    }
}

our $BOOL_OVERLOAD_TRUE  = _T::BoolOverload->new(1);
our $BOOL_OVERLOAD_FALSE = _T::BoolOverload->new(0);

{
    package _T::StrOverload;

    use overload
        q{""}    => sub { ${ $_[0] } },
        fallback => 0;

    sub new {
        my $str = $_[1];
        bless \$str, __PACKAGE__;
    }
}

our $STR_OVERLOAD_EMPTY      = _T::StrOverload->new(q{});
our $STR_OVERLOAD_FULL       = _T::StrOverload->new('full');
our $STR_OVERLOAD_CLASS_NAME = _T::StrOverload->new('_T::StrOverload');

{
    package _T::NumOverload;

    use overload
        '0+' => sub { ${ $_[0] } },
        '+'  => sub { ${ $_[0] } + $_[1] },
        fallback => 0;

    sub new {
        my $num = $_[1];
        bless \$num, __PACKAGE__;
    }
}

our $NUM_OVERLOAD_ZERO        = _T::NumOverload->new(0);
our $NUM_OVERLOAD_ONE         = _T::NumOverload->new(1);
our $NUM_OVERLOAD_NEG         = _T::NumOverload->new(-42);
our $NUM_OVERLOAD_DECIMAL     = _T::NumOverload->new(42.42);
our $NUM_OVERLOAD_NEG_DECIMAL = _T::NumOverload->new(42.42);

{
    package _T::CodeOverload;

    use overload
        '&{}'    => sub { ${ $_[0] } },
        fallback => 0;

    sub new {
        my $code = $_[1];
        bless \$code, __PACKAGE__;
    }
}

our $CODE_OVERLOAD = _T::CodeOverload->new( sub { } );

{
    package _T::RegexOverload;

    use overload
        'qr'     => sub { ${ $_[0] } },
        fallback => 0;

    sub new {
        my $regex = $_[1];
        bless \$regex, __PACKAGE__;
    }
}

our $REGEX_OVERLOAD = _T::RegexOverload->new(qr/foo/);

{
    package _T::GlobOverload;

    use overload
        '*{}'    => sub { ${ $_[0] } },
        fallback => 0;

    sub new {
        my $glob = $_[1];
        bless \$glob, __PACKAGE__;
    }
}

{
    package _T::ScalarOverload;

    use overload
        '${}'    => sub { $_[0][0] },
        fallback => 0;

    sub new {
        my $scalar = $_[1];
        bless [$scalar], __PACKAGE__;
    }
}

our $SCALAR_OVERLOAD = _T::ScalarOverload->new('x');

{
    package _T::ArrayOverload;

    use overload
        '@{}'    => sub { $_[0]{array} },
        fallback => 0;

    sub new {
        my $array = $_[1];
        bless { array => $array }, __PACKAGE__;
    }
}

our $ARRAY_OVERLOAD = _T::ArrayOverload->new( [ 1, 2, 3 ] );

{
    package _T::HashOverload;

    use overload
        '%{}'    => sub { $_[0][0] },
        fallback => 0;

    sub new {
        my $hash = $_[1];

        # We use an array-based object so we make sure we test hash
        # overloading as opposed to just treating the object as a hash.
        bless [$hash], __PACKAGE__;
    }
}

our $HASH_OVERLOAD = _T::HashOverload->new( { x => 42, y => 84 } );

my @vars;

BEGIN {
    open my $fh, '<', $INC{'Test/Specio.pm'} or die $!;
    while (<$fh>) {
        push @vars, $1 if /^our (\$[A-Z0-9_]+)(?: +=|;)/;
    }
}

our @EXPORT_OK = ( @vars, qw( builtins_tests describe test_constraint ) );
our %EXPORT_TAGS = ( vars => \@vars );

sub builtins_tests {
    my $GLOB             = shift;
    my $GLOB_OVERLOAD    = shift;
    my $GLOB_OVERLOAD_FH = shift;

    return {
        Item => {
            accept => [
                $ZERO,
                $ONE,
                $BOOL_OVERLOAD_TRUE,
                $BOOL_OVERLOAD_FALSE,
                $INT,
                $NEG_INT,
                $NUM,
                $NEG_NUM,
                $NUM_OVERLOAD_ZERO,
                $NUM_OVERLOAD_ONE,
                $NUM_OVERLOAD_NEG,
                $NUM_OVERLOAD_NEG_DECIMAL,
                $NUM_OVERLOAD_DECIMAL,
                $EMPTY_STRING,
                $STRING,
                $NUM_IN_STRING,
                $STR_OVERLOAD_EMPTY,
                $STR_OVERLOAD_FULL,
                $INT_WITH_NL1,
                $INT_WITH_NL2,
                $SCALAR_REF,
                $SCALAR_REF_REF,
                $SCALAR_OVERLOAD,
                $ARRAY_REF,
                $ARRAY_OVERLOAD,
                $HASH_REF,
                $HASH_OVERLOAD,
                $CODE_REF,
                $CODE_OVERLOAD,
                $GLOB,
                $GLOB_REF,
                $GLOB_OVERLOAD,
                $GLOB_OVERLOAD_FH,
                $FH,
                $FH_OBJECT,
                $REGEX,
                $REGEX_OBJ,
                $REGEX_OVERLOAD,
                $FAKE_REGEX,
                $OBJECT,
                $UNDEF,
            ],
        },
        Defined => {
            accept => [
                $ZERO,
                $ONE,
                $BOOL_OVERLOAD_TRUE,
                $BOOL_OVERLOAD_FALSE,
                $INT,
                $NEG_INT,
                $NUM,
                $NEG_NUM,
                $NUM_OVERLOAD_ZERO,
                $NUM_OVERLOAD_ONE,
                $NUM_OVERLOAD_NEG,
                $NUM_OVERLOAD_NEG_DECIMAL,
                $NUM_OVERLOAD_DECIMAL,
                $EMPTY_STRING,
                $STRING,
                $NUM_IN_STRING,
                $STR_OVERLOAD_EMPTY,
                $STR_OVERLOAD_FULL,
                $INT_WITH_NL1,
                $INT_WITH_NL2,
                $SCALAR_REF,
                $SCALAR_REF_REF,
                $SCALAR_OVERLOAD,
                $ARRAY_REF,
                $ARRAY_OVERLOAD,
                $HASH_REF,
                $HASH_OVERLOAD,
                $CODE_REF,
                $CODE_OVERLOAD,
                $GLOB,
                $GLOB_REF,
                $GLOB_OVERLOAD,
                $GLOB_OVERLOAD_FH,
                $FH,
                $FH_OBJECT,
                $REGEX,
                $REGEX_OBJ,
                $REGEX_OVERLOAD,
                $FAKE_REGEX,
                $OBJECT,
            ],
            reject => [
                $UNDEF,
            ],
        },
        Undef => {
            accept => [
                $UNDEF,
            ],
            reject => [
                $ZERO,
                $ONE,
                $BOOL_OVERLOAD_TRUE,
                $BOOL_OVERLOAD_FALSE,
                $INT,
                $NEG_INT,
                $NUM,
                $NEG_NUM,
                $NUM_OVERLOAD_ZERO,
                $NUM_OVERLOAD_ONE,
                $NUM_OVERLOAD_NEG,
                $NUM_OVERLOAD_NEG_DECIMAL,
                $NUM_OVERLOAD_DECIMAL,
                $EMPTY_STRING,
                $STRING,
                $NUM_IN_STRING,
                $STR_OVERLOAD_EMPTY,
                $STR_OVERLOAD_FULL,
                $INT_WITH_NL1,
                $INT_WITH_NL2,
                $SCALAR_REF,
                $SCALAR_REF_REF,
                $SCALAR_OVERLOAD,
                $ARRAY_REF,
                $ARRAY_OVERLOAD,
                $HASH_REF,
                $HASH_OVERLOAD,
                $CODE_REF,
                $CODE_OVERLOAD,
                $GLOB,
                $GLOB_REF,
                $GLOB_OVERLOAD,
                $GLOB_OVERLOAD_FH,
                $FH,
                $FH_OBJECT,
                $REGEX,
                $REGEX_OBJ,
                $REGEX_OVERLOAD,
                $FAKE_REGEX,
                $OBJECT,
            ],
        },
        Bool => {
            accept => [
                $ZERO,
                $ONE,
                $BOOL_OVERLOAD_TRUE,
                $BOOL_OVERLOAD_FALSE,
                $EMPTY_STRING,
                $UNDEF,
            ],
            reject => [
                $INT,
                $NEG_INT,
                $NUM,
                $NEG_NUM,
                $NUM_OVERLOAD_ZERO,
                $NUM_OVERLOAD_ONE,
                $NUM_OVERLOAD_NEG,
                $NUM_OVERLOAD_NEG_DECIMAL,
                $NUM_OVERLOAD_DECIMAL,
                $STRING,
                $NUM_IN_STRING,
                $STR_OVERLOAD_EMPTY,
                $STR_OVERLOAD_FULL,
                $INT_WITH_NL1,
                $INT_WITH_NL2,
                $SCALAR_REF,
                $SCALAR_REF_REF,
                $SCALAR_OVERLOAD,
                $ARRAY_REF,
                $ARRAY_OVERLOAD,
                $HASH_REF,
                $HASH_OVERLOAD,
                $CODE_REF,
                $CODE_OVERLOAD,
                $GLOB,
                $GLOB_REF,
                $GLOB_OVERLOAD,
                $GLOB_OVERLOAD_FH,
                $FH,
                $FH_OBJECT,
                $REGEX,
                $REGEX_OBJ,
                $REGEX_OVERLOAD,
                $FAKE_REGEX,
                $OBJECT,
            ],
        },
        Maybe => {
            accept => [
                $ZERO,
                $ONE,
                $BOOL_OVERLOAD_TRUE,
                $BOOL_OVERLOAD_FALSE,
                $INT,
                $NEG_INT,
                $NUM,
                $NEG_NUM,
                $NUM_OVERLOAD_ZERO,
                $NUM_OVERLOAD_ONE,
                $NUM_OVERLOAD_NEG,
                $NUM_OVERLOAD_NEG_DECIMAL,
                $NUM_OVERLOAD_DECIMAL,
                $EMPTY_STRING,
                $STRING,
                $NUM_IN_STRING,
                $STR_OVERLOAD_EMPTY,
                $STR_OVERLOAD_FULL,
                $INT_WITH_NL1,
                $INT_WITH_NL2,
                $SCALAR_REF,
                $SCALAR_REF_REF,
                $SCALAR_OVERLOAD,
                $ARRAY_REF,
                $ARRAY_OVERLOAD,
                $HASH_REF,
                $HASH_OVERLOAD,
                $CODE_REF,
                $CODE_OVERLOAD,
                $GLOB,
                $GLOB_REF,
                $GLOB_OVERLOAD,
                $GLOB_OVERLOAD_FH,
                $FH,
                $FH_OBJECT,
                $REGEX,
                $REGEX_OBJ,
                $REGEX_OVERLOAD,
                $FAKE_REGEX,
                $OBJECT,
                $UNDEF,
            ],
        },
        Value => {
            accept => [
                $ZERO,
                $ONE,
                $INT,
                $NEG_INT,
                $NUM,
                $NEG_NUM,
                $EMPTY_STRING,
                $STRING,
                $NUM_IN_STRING,
                $INT_WITH_NL1,
                $INT_WITH_NL2,
                $GLOB,
            ],
            reject => [
                $BOOL_OVERLOAD_TRUE,
                $BOOL_OVERLOAD_FALSE,
                $STR_OVERLOAD_EMPTY,
                $STR_OVERLOAD_FULL,
                $NUM_OVERLOAD_ZERO,
                $NUM_OVERLOAD_ONE,
                $NUM_OVERLOAD_NEG,
                $NUM_OVERLOAD_NEG_DECIMAL,
                $NUM_OVERLOAD_DECIMAL,
                $SCALAR_REF,
                $SCALAR_REF_REF,
                $SCALAR_OVERLOAD,
                $ARRAY_REF,
                $ARRAY_OVERLOAD,
                $HASH_REF,
                $HASH_OVERLOAD,
                $CODE_REF,
                $CODE_OVERLOAD,
                $GLOB_REF,
                $GLOB_OVERLOAD,
                $GLOB_OVERLOAD_FH,
                $FH,
                $FH_OBJECT,
                $REGEX,
                $REGEX_OBJ,
                $REGEX_OVERLOAD,
                $FAKE_REGEX,
                $OBJECT,
                $UNDEF,
            ],
        },
        Ref => {
            accept => [
                $BOOL_OVERLOAD_TRUE,
                $BOOL_OVERLOAD_FALSE,
                $STR_OVERLOAD_EMPTY,
                $STR_OVERLOAD_FULL,
                $NUM_OVERLOAD_ZERO,
                $NUM_OVERLOAD_ONE,
                $NUM_OVERLOAD_NEG,
                $NUM_OVERLOAD_NEG_DECIMAL,
                $NUM_OVERLOAD_DECIMAL,
                $SCALAR_REF,
                $SCALAR_REF_REF,
                $SCALAR_OVERLOAD,
                $ARRAY_REF,
                $ARRAY_OVERLOAD,
                $HASH_REF,
                $HASH_OVERLOAD,
                $CODE_REF,
                $CODE_OVERLOAD,
                $GLOB_REF,
                $GLOB_OVERLOAD,
                $GLOB_OVERLOAD_FH,
                $FH,
                $FH_OBJECT,
                $REGEX,
                $REGEX_OBJ,
                $REGEX_OVERLOAD,
                $FAKE_REGEX,
                $OBJECT,
            ],
            reject => [
                $ZERO,
                $ONE,
                $INT,
                $NEG_INT,
                $NUM,
                $NEG_NUM,
                $EMPTY_STRING,
                $STRING,
                $NUM_IN_STRING,
                $INT_WITH_NL1,
                $INT_WITH_NL2,
                $GLOB,
                $UNDEF,
            ],
        },
        Num => {
            accept => [
                $ZERO,
                $ONE,
                $INT,
                $NEG_INT,
                $NUM,
                $NEG_NUM,
                $NUM_OVERLOAD_ZERO,
                $NUM_OVERLOAD_ONE,
                $NUM_OVERLOAD_NEG,
                $NUM_OVERLOAD_NEG_DECIMAL,
                $NUM_OVERLOAD_DECIMAL,
                qw(
                    1e10
                    1e-10
                    1.23456e10
                    1.23456e-10
                    1e10
                    1e-10
                    1.23456e10
                    1.23456e-10
                    -1e10
                    -1e-10
                    -1.23456e10
                    -1.23456e-10
                    -1e10
                    -1e-10
                    -1.23456e10
                    -1.23456e-10
                    -1e+10
                    1E10
                    ),
            ],
            reject => [
                $BOOL_OVERLOAD_TRUE,
                $BOOL_OVERLOAD_FALSE,
                $EMPTY_STRING,
                $STRING,
                $NUM_IN_STRING,
                $STR_OVERLOAD_EMPTY,
                $STR_OVERLOAD_FULL,
                $SCALAR_REF,
                $SCALAR_REF_REF,
                $SCALAR_OVERLOAD,
                $ARRAY_REF,
                $ARRAY_OVERLOAD,
                $HASH_REF,
                $HASH_OVERLOAD,
                $CODE_REF,
                $CODE_OVERLOAD,
                $GLOB,
                $GLOB_REF,
                $GLOB_OVERLOAD,
                $GLOB_OVERLOAD_FH,
                $FH,
                $FH_OBJECT,
                $INT_WITH_NL1,
                $INT_WITH_NL2,
                $REGEX,
                $REGEX_OBJ,
                $REGEX_OVERLOAD,
                $FAKE_REGEX,
                $OBJECT,
                $UNDEF,
            ],
        },
        Int => {
            accept => [
                $ZERO,
                $ONE,
                $INT,
                $NEG_INT,
                $NUM_OVERLOAD_ZERO,
                $NUM_OVERLOAD_ONE,
                $NUM_OVERLOAD_NEG,
                qw(
                    1e20
                    1e100
                    -1e10
                    -1e+10
                    1E20
                    ),
            ],
            reject => [
                $BOOL_OVERLOAD_TRUE,
                $BOOL_OVERLOAD_FALSE,
                $NUM,
                $NEG_NUM,
                $NUM_OVERLOAD_NEG_DECIMAL,
                $NUM_OVERLOAD_DECIMAL,
                $EMPTY_STRING,
                $STRING,
                $NUM_IN_STRING,
                $STR_OVERLOAD_EMPTY,
                $STR_OVERLOAD_FULL,
                $INT_WITH_NL1,
                $INT_WITH_NL2,
                $SCALAR_REF,
                $SCALAR_REF_REF,
                $SCALAR_OVERLOAD,
                $ARRAY_REF,
                $ARRAY_OVERLOAD,
                $HASH_REF,
                $HASH_OVERLOAD,
                $CODE_REF,
                $CODE_OVERLOAD,
                $GLOB,
                $GLOB_REF,
                $GLOB_OVERLOAD,
                $GLOB_OVERLOAD_FH,
                $FH,
                $FH_OBJECT,
                $REGEX,
                $REGEX_OBJ,
                $REGEX_OVERLOAD,
                $FAKE_REGEX,
                $OBJECT,
                $UNDEF,
                qw(
                    1e-10
                    -1e-10
                    1.23456e10
                    1.23456e-10
                    -1.23456e10
                    -1.23456e-10
                    -1.23456e+10
                    ),
            ],
        },
        Str => {
            accept => [
                $ZERO,
                $ONE,
                $INT,
                $NEG_INT,
                $NUM,
                $NEG_NUM,
                $EMPTY_STRING,
                $STRING,
                $NUM_IN_STRING,
                $STR_OVERLOAD_EMPTY,
                $STR_OVERLOAD_FULL,
                $INT_WITH_NL1,
                $INT_WITH_NL2,
            ],
            reject => [
                $BOOL_OVERLOAD_TRUE,
                $BOOL_OVERLOAD_FALSE,
                $NUM_OVERLOAD_ZERO,
                $NUM_OVERLOAD_ONE,
                $NUM_OVERLOAD_NEG,
                $NUM_OVERLOAD_NEG_DECIMAL,
                $NUM_OVERLOAD_DECIMAL,
                $SCALAR_REF,
                $SCALAR_REF_REF,
                $SCALAR_OVERLOAD,
                $ARRAY_REF,
                $ARRAY_OVERLOAD,
                $HASH_REF,
                $HASH_OVERLOAD,
                $CODE_REF,
                $CODE_OVERLOAD,
                $GLOB,
                $GLOB_REF,
                $GLOB_OVERLOAD,
                $GLOB_OVERLOAD_FH,
                $FH,
                $FH_OBJECT,
                $REGEX,
                $REGEX_OBJ,
                $REGEX_OVERLOAD,
                $FAKE_REGEX,
                $OBJECT,
                $UNDEF,
            ],
        },
        ScalarRef => {
            accept => [
                $SCALAR_REF,
                $SCALAR_REF_REF,
                $SCALAR_OVERLOAD,
            ],
            reject => [
                $ZERO,
                $ONE,
                $BOOL_OVERLOAD_TRUE,
                $BOOL_OVERLOAD_FALSE,
                $INT,
                $NEG_INT,
                $NUM,
                $NEG_NUM,
                $NUM_OVERLOAD_ZERO,
                $NUM_OVERLOAD_ONE,
                $NUM_OVERLOAD_NEG,
                $NUM_OVERLOAD_NEG_DECIMAL,
                $NUM_OVERLOAD_DECIMAL,
                $EMPTY_STRING,
                $STRING,
                $NUM_IN_STRING,
                $STR_OVERLOAD_EMPTY,
                $STR_OVERLOAD_FULL,
                $INT_WITH_NL1,
                $INT_WITH_NL2,
                $ARRAY_REF,
                $ARRAY_OVERLOAD,
                $HASH_REF,
                $HASH_OVERLOAD,
                $CODE_REF,
                $CODE_OVERLOAD,
                $GLOB,
                $GLOB_REF,
                $GLOB_OVERLOAD,
                $GLOB_OVERLOAD_FH,
                $FH,
                $FH_OBJECT,
                $REGEX,
                $REGEX_OBJ,
                $REGEX_OVERLOAD,
                $FAKE_REGEX,
                $OBJECT,
                $UNDEF,
            ],
        },
        ArrayRef => {
            accept => [
                $ARRAY_REF,
                $ARRAY_OVERLOAD,
            ],
            reject => [
                $ZERO,
                $ONE,
                $BOOL_OVERLOAD_TRUE,
                $BOOL_OVERLOAD_FALSE,
                $INT,
                $NEG_INT,
                $NUM,
                $NEG_NUM,
                $NUM_OVERLOAD_ZERO,
                $NUM_OVERLOAD_ONE,
                $NUM_OVERLOAD_NEG,
                $NUM_OVERLOAD_NEG_DECIMAL,
                $NUM_OVERLOAD_DECIMAL,
                $EMPTY_STRING,
                $STRING,
                $NUM_IN_STRING,
                $STR_OVERLOAD_EMPTY,
                $STR_OVERLOAD_FULL,
                $INT_WITH_NL1,
                $INT_WITH_NL2,
                $SCALAR_REF,
                $SCALAR_REF_REF,
                $SCALAR_OVERLOAD,
                $HASH_REF,
                $HASH_OVERLOAD,
                $CODE_REF,
                $CODE_OVERLOAD,
                $GLOB,
                $GLOB_REF,
                $GLOB_OVERLOAD,
                $GLOB_OVERLOAD_FH,
                $FH,
                $FH_OBJECT,
                $REGEX,
                $REGEX_OBJ,
                $REGEX_OVERLOAD,
                $FAKE_REGEX,
                $OBJECT,
                $UNDEF,
            ],
        },
        HashRef => {
            accept => [
                $HASH_REF,
                $HASH_OVERLOAD,
            ],
            reject => [
                $ZERO,
                $ONE,
                $BOOL_OVERLOAD_TRUE,
                $BOOL_OVERLOAD_FALSE,
                $INT,
                $NEG_INT,
                $NUM,
                $NEG_NUM,
                $NUM_OVERLOAD_ZERO,
                $NUM_OVERLOAD_ONE,
                $NUM_OVERLOAD_NEG,
                $NUM_OVERLOAD_NEG_DECIMAL,
                $NUM_OVERLOAD_DECIMAL,
                $EMPTY_STRING,
                $STRING,
                $NUM_IN_STRING,
                $STR_OVERLOAD_EMPTY,
                $STR_OVERLOAD_FULL,
                $INT_WITH_NL1,
                $INT_WITH_NL2,
                $SCALAR_REF,
                $SCALAR_REF_REF,
                $SCALAR_OVERLOAD,
                $ARRAY_REF,
                $ARRAY_OVERLOAD,
                $CODE_REF,
                $CODE_OVERLOAD,
                $GLOB,
                $GLOB_REF,
                $GLOB_OVERLOAD,
                $GLOB_OVERLOAD_FH,
                $FH,
                $FH_OBJECT,
                $REGEX,
                $REGEX_OBJ,
                $REGEX_OVERLOAD,
                $FAKE_REGEX,
                $OBJECT,
                $UNDEF,
            ],
        },
        CodeRef => {
            accept => [
                $CODE_REF,
                $CODE_OVERLOAD,
            ],
            reject => [
                $ZERO,
                $ONE,
                $BOOL_OVERLOAD_TRUE,
                $BOOL_OVERLOAD_FALSE,
                $INT,
                $NEG_INT,
                $NUM,
                $NEG_NUM,
                $NUM_OVERLOAD_ZERO,
                $NUM_OVERLOAD_ONE,
                $NUM_OVERLOAD_NEG,
                $NUM_OVERLOAD_NEG_DECIMAL,
                $NUM_OVERLOAD_DECIMAL,
                $EMPTY_STRING,
                $STRING,
                $NUM_IN_STRING,
                $STR_OVERLOAD_EMPTY,
                $STR_OVERLOAD_FULL,
                $INT_WITH_NL1,
                $INT_WITH_NL2,
                $SCALAR_REF,
                $SCALAR_REF_REF,
                $SCALAR_OVERLOAD,
                $ARRAY_REF,
                $ARRAY_OVERLOAD,
                $HASH_REF,
                $HASH_OVERLOAD,
                $GLOB,
                $GLOB_REF,
                $GLOB_OVERLOAD,
                $GLOB_OVERLOAD_FH,
                $FH,
                $FH_OBJECT,
                $REGEX,
                $REGEX_OBJ,
                $REGEX_OVERLOAD,
                $FAKE_REGEX,
                $OBJECT,
                $UNDEF,
            ],
        },
        RegexpRef => {
            accept => [
                $REGEX,
                $REGEX_OBJ,
                $REGEX_OVERLOAD,
            ],
            reject => [
                $ZERO,
                $ONE,
                $BOOL_OVERLOAD_TRUE,
                $BOOL_OVERLOAD_FALSE,
                $INT,
                $NEG_INT,
                $NUM,
                $NEG_NUM,
                $NUM_OVERLOAD_ZERO,
                $NUM_OVERLOAD_ONE,
                $NUM_OVERLOAD_NEG,
                $NUM_OVERLOAD_NEG_DECIMAL,
                $NUM_OVERLOAD_DECIMAL,
                $EMPTY_STRING,
                $STRING,
                $NUM_IN_STRING,
                $STR_OVERLOAD_EMPTY,
                $STR_OVERLOAD_FULL,
                $INT_WITH_NL1,
                $INT_WITH_NL2,
                $SCALAR_REF,
                $SCALAR_REF_REF,
                $SCALAR_OVERLOAD,
                $ARRAY_REF,
                $ARRAY_OVERLOAD,
                $HASH_REF,
                $HASH_OVERLOAD,
                $CODE_REF,
                $CODE_OVERLOAD,
                $GLOB,
                $GLOB_REF,
                $GLOB_OVERLOAD,
                $GLOB_OVERLOAD_FH,
                $FH,
                $FH_OBJECT,
                $OBJECT,
                $UNDEF,
                $FAKE_REGEX,
            ],
        },
        GlobRef => {
            accept => [
                $GLOB_REF,
                $GLOB_OVERLOAD,
                $GLOB_OVERLOAD_FH,
                $FH,
            ],
            reject => [
                $ZERO,
                $ONE,
                $BOOL_OVERLOAD_TRUE,
                $BOOL_OVERLOAD_FALSE,
                $INT,
                $NEG_INT,
                $NUM,
                $NEG_NUM,
                $NUM_OVERLOAD_ZERO,
                $NUM_OVERLOAD_ONE,
                $NUM_OVERLOAD_NEG,
                $NUM_OVERLOAD_NEG_DECIMAL,
                $NUM_OVERLOAD_DECIMAL,
                $EMPTY_STRING,
                $STRING,
                $NUM_IN_STRING,
                $STR_OVERLOAD_EMPTY,
                $STR_OVERLOAD_FULL,
                $INT_WITH_NL1,
                $INT_WITH_NL2,
                $SCALAR_REF,
                $SCALAR_REF_REF,
                $SCALAR_OVERLOAD,
                $ARRAY_REF,
                $ARRAY_OVERLOAD,
                $HASH_REF,
                $HASH_OVERLOAD,
                $CODE_REF,
                $CODE_OVERLOAD,
                $GLOB,
                $FH_OBJECT,
                $OBJECT,
                $REGEX,
                $REGEX_OBJ,
                $REGEX_OVERLOAD,
                $FAKE_REGEX,
                $UNDEF,
            ],
        },
        FileHandle => {
            accept => [
                $FH,
                $FH_OBJECT,
                $GLOB_OVERLOAD_FH,
            ],
            reject => [
                $ZERO,
                $ONE,
                $BOOL_OVERLOAD_TRUE,
                $BOOL_OVERLOAD_FALSE,
                $INT,
                $NEG_INT,
                $NUM,
                $NEG_NUM,
                $NUM_OVERLOAD_ZERO,
                $NUM_OVERLOAD_ONE,
                $NUM_OVERLOAD_NEG,
                $NUM_OVERLOAD_NEG_DECIMAL,
                $NUM_OVERLOAD_DECIMAL,
                $EMPTY_STRING,
                $STRING,
                $NUM_IN_STRING,
                $STR_OVERLOAD_EMPTY,
                $STR_OVERLOAD_FULL,
                $INT_WITH_NL1,
                $INT_WITH_NL2,
                $SCALAR_REF,
                $SCALAR_REF_REF,
                $SCALAR_OVERLOAD,
                $ARRAY_REF,
                $ARRAY_OVERLOAD,
                $HASH_REF,
                $HASH_OVERLOAD,
                $CODE_REF,
                $CODE_OVERLOAD,
                $GLOB,
                $GLOB_REF,
                $GLOB_OVERLOAD,
                $OBJECT,
                $REGEX,
                $REGEX_OBJ,
                $REGEX_OVERLOAD,
                $FAKE_REGEX,
                $UNDEF,
            ],
        },
        Object => {
            accept => [
                $BOOL_OVERLOAD_TRUE,
                $BOOL_OVERLOAD_FALSE,
                $STR_OVERLOAD_EMPTY,
                $STR_OVERLOAD_FULL,
                $NUM_OVERLOAD_ZERO,
                $NUM_OVERLOAD_ONE,
                $NUM_OVERLOAD_NEG,
                $NUM_OVERLOAD_NEG_DECIMAL,
                $NUM_OVERLOAD_DECIMAL,
                $CODE_OVERLOAD,
                $FH_OBJECT,
                $REGEX,
                $REGEX_OBJ,
                $REGEX_OVERLOAD,
                $FAKE_REGEX,
                $GLOB_OVERLOAD,
                $GLOB_OVERLOAD_FH,
                $SCALAR_OVERLOAD,
                $ARRAY_OVERLOAD,
                $HASH_OVERLOAD,
                $OBJECT,
            ],
            reject => [
                $ZERO,
                $ONE,
                $INT,
                $NEG_INT,
                $NUM,
                $NEG_NUM,
                $EMPTY_STRING,
                $STRING,
                $NUM_IN_STRING,
                $INT_WITH_NL1,
                $INT_WITH_NL2,
                $SCALAR_REF,
                $SCALAR_REF_REF,
                $ARRAY_REF,
                $HASH_REF,
                $CODE_REF,
                $GLOB,
                $GLOB_REF,
                $FH,
                $UNDEF,
            ],
        },
        ClassName => {
            accept => [
                $CLASS_NAME,
                $STR_OVERLOAD_CLASS_NAME,
            ],
            reject => [
                $ZERO,
                $ONE,
                $BOOL_OVERLOAD_TRUE,
                $BOOL_OVERLOAD_FALSE,
                $INT,
                $NEG_INT,
                $NUM,
                $NEG_NUM,
                $NUM_OVERLOAD_ZERO,
                $NUM_OVERLOAD_ONE,
                $NUM_OVERLOAD_NEG,
                $NUM_OVERLOAD_NEG_DECIMAL,
                $NUM_OVERLOAD_DECIMAL,
                $EMPTY_STRING,
                $STRING,
                $NUM_IN_STRING,
                $STR_OVERLOAD_EMPTY,
                $STR_OVERLOAD_FULL,
                $INT_WITH_NL1,
                $INT_WITH_NL2,
                $SCALAR_REF,
                $SCALAR_REF_REF,
                $SCALAR_OVERLOAD,
                $ARRAY_REF,
                $ARRAY_OVERLOAD,
                $HASH_REF,
                $HASH_OVERLOAD,
                $CODE_REF,
                $CODE_OVERLOAD,
                $GLOB,
                $GLOB_REF,
                $GLOB_OVERLOAD,
                $GLOB_OVERLOAD_FH,
                $FH,
                $FH_OBJECT,
                $REGEX,
                $REGEX_OBJ,
                $REGEX_OVERLOAD,
                $FAKE_REGEX,
                $OBJECT,
                $UNDEF,
            ],
        },
    };
}

sub test_constraint {
    my $type      = shift;
    my $tests     = shift;
    my $describer = shift || \&describe;

    local $Test::Builder::Level = $Test::Builder::Level + 1;

    $type = t($type) unless blessed $type;

    subtest(
        ( $type->name || '<anon>' ),
        sub {
            try {
                my $not_inlined = $type->_constraint_with_parents;

                my $inlined;
                if ( $type->can_be_inlined ) {
                    $inlined = $type->_generated_inline_sub;
                }

                for my $accept ( @{ $tests->{accept} || [] } ) {
                    my $described = $describer->($accept);
                    subtest(
                        "accepts $described",
                        sub {
                            ok(
                                $type->value_is_valid($accept),
                                'using ->value_is_valid'
                            );
                            is(
                                exception { $type->($accept) },
                                undef,
                                'using subref overloading'
                            );
                            ok(
                                $not_inlined->($accept),
                                'using non-inlined constraint'
                            );
                            if ($inlined) {
                                ok(
                                    $inlined->($accept),
                                    'using inlined constraint'
                                );
                            }
                        }
                    );
                }

                for my $reject ( @{ $tests->{reject} || [] } ) {
                    my $described = $describer->($reject);
                    subtest(
                        "rejects $described",
                        sub {
                            ok(
                                !$type->value_is_valid($reject),
                                'using ->value_is_valid'
                            );

                            # I don't love this test, but there's no way to know the
                            # exact content of each type's validation failure
                            # exception. We can, however, reasonably assume (I think)
                            # that the exception thrown will include a trace starting
                            # with Specio::Exception.
                            like(
                                exception { $type->($reject) },
                                qr/\QTrace begun at Specio::Exception->new/,
                                'using subref overloading'
                            );
                            if ($inlined) {
                                ok(
                                    !$inlined->($reject),
                                    'using inlined constraint'
                                );
                            }
                        }
                    );
                }
            }
            catch {
                fail('No exception in test_constraint');
                diag($_);
            };
        }
    );
}

sub describe {
    my $val = shift;

    return 'undef' unless defined $val;

    if ( !ref $val ) {
        return q{''} if $val eq q{};

        return looks_like_number($val)
            && $val !~ /\n/ ? $val : B::perlstring($val);
    }

    return 'open filehandle'
        if openhandle $val && !blessed $val;

    if ( blessed $val ) {
        my $desc = ( ref $val ) . ' object';
        if ( $val->isa('_T::StrOverload') ) {
            $desc .= ' (' . describe("$val") . ')';
        }
        elsif ( $val->isa('_T::BoolOverload') ) {
            $desc .= ' (' . ( $val ? 'true' : 'false' ) . ')';
        }
        elsif ( $val->isa('_T::NumOverload') ) {
            $desc .= ' (' . describe( ${$val} ) . ')';
        }

        return $desc;
    }
    else {
        return ( ref $val ) . ' reference';
    }
}

1;

# ABSTRACT: Test helpers for Specio

__END__

=pod

=encoding UTF-8

=head1 NAME

Test::Specio - Test helpers for Specio

=head1 VERSION

version 0.43

=head1 SYNOPSIS

  use Test::Specio qw( test_constraint :vars );

  test_constraint(
      t('Foo'), {
          accept => [ 'foo', 'bar' ],
          reject => [ 42,    {}, $EMPTY_STRING, $HASH_REF ],
      }
  );

=head1 DESCRIPTION

This package provides some helper functions and variables for testing Specio
types.

=head1 EXPORTS

This module provides the following exports:

=head2 test_constraint( $type, $tests, [ $describer ] )

This subroutine accepts two arguments. The first should be a Specio type
object. The second is hashref which can contain the keys C<accept> and
C<reject>. Each key should contain an arrayref of values which the type
accepts or rejects.

The third argument is optional. This is a sub reference which will be called
to generate a description of the value being tested. This defaults to calling
this package's C<describe> sub, but you can provide your own.

=head2 describe($value)

Given a value, this subroutine returns a string describing that value in a
useful way for test output. It know about the various classes used for the
variables exported by this package and will do something intelligent when such
a variable.

=head2 builtins_tests( $GLOB, $GLOB_OVERLOAD, $GLOB_OVERLOAD_FH )

This subroutine returns a hashref containing test variables for all builtin
types. The hashref has a form like this:

  {
      Bool => {
          accept => [
              $ZERO,
              $ONE,
              $BOOL_OVERLOAD_TRUE,
              $BOOL_OVERLOAD_FALSE,
              ...,
          ],
          reject => [
              $INT,
              $NEG_INT,
              $NUM,
              $NEG_NUM,
              ...,
              $OBJECT,
          ],
      },
      Maybe => {...},
  }

You need to pass in a glob, an object which overloads globification, and an
object which overloads globification to return an open filehandle. See below
for more details on how to create these things.

=head2 Variables

This module also exports many variables containing values which are useful for
testing constraints. Note that references are always empty unless stated
otherwise. You can import these variables individually or import all of them
with the C<:vars> import tag.

=over 4

=item * C<$ZERO>

=item * C<$ONE>

=item * C<$INT>

An arbitrary positive integer.

=item * C<$NEG_INT>

An arbitrary negative integer.

=item * C<$NUM>

An arbitrary positive non-integer number.

=item * C<$NEG_NUM>

An arbitrary negative non-integer number.

=item * C<$EMPTY_STRING>

=item * C<$STRING>

An arbitrary non-empty string.

=item * C<$NUM_IN_STRING>

An arbitrary string which contains a number.

=item * C<$INT_WITH_NL1>

An string containing an integer followed by a newline.

=item * C<$INT_WITH_NL2>

An string containing a newline followed by an integer.

=item * C<$SCALAR_REF>

=item * C<$SCALAR_REF_REF>

A reference containing a reference to a scalar.

=item * C<$ARRAY_REF>

=item * C<$HASH_REF>

=item * C<$CODE_REF>

=item * C<$GLOB_REF>

=item * C<$FH>

An opened filehandle.

=item * C<$FH_OBJECT>

An opened L<IO::File> object.

=item * C<$REGEX>

A regex created with C<qr//>.

=item * C<$REGEX_OBJ>

A regex created with C<qr//> that was then blessed into class.

=item * C<$FAKE_REGEX>

A non-regex blessed into the C<Regexp> class which Perl uses internally for
C<qr//> objects.

=item * C<$OBJECT>

An arbitrary object.

=item * C<$UNDEF>

=item * C<$CLASS_NAME>

A string containing a loaded package name.

=item * C<$BOOL_OVERLOAD_TRUE>

An object which overloads boolification to return true.

=item * C<$BOOL_OVERLOAD_FALSE>

An object which overloads boolification to return false.

=item * C<$STR_OVERLOAD_EMPTY>

An object which overloads stringification to return an empty string.

=item * C<$STR_OVERLOAD_FULL>

An object which overloads stringification to return a non-empty string.

=item * C<$STR_OVERLOAD_CLASS_NAME>

An object which overloads stringification to return a loaded package name.

=item * C<$NUM_OVERLOAD_ZERO>

=item * C<$NUM_OVERLOAD_ONE>

=item * C<$NUM_OVERLOAD_NEG>

=item * C<$NUM_OVERLOAD_DECIMAL>

=item * C<$NUM_OVERLOAD_NEG_DECIMAL>

=item * C<$CODE_OVERLOAD>

=item * C<$SCALAR_OVERLOAD>

An object which overloads scalar dereferencing to return a non-empty string.

=item * C<$ARRAY_OVERLOAD>

An object which overloads array dereferencing to return a non-empty array.

=item * C<$HASH_OVERLOAD>

An object which overloads hash dereferencing to return a non-empty hash.

=back

=head2 Globs and the _T::GlobOverload package

To create a glob you can pass around for tests, use this code:

  my $GLOB = do {
      no warnings 'once';
      *SOME_GLOB;
  };

The C<_T::GlobOverload> package is defined when you load C<Test::Specio> so
you can create your own glob overloading objects. Such objects cannot be
exported because the glob they return does not transfer across packages
properly.

You can create such a variable like this:

  local *FOO;
  my $GLOB_OVERLOAD = _T::GlobOverload->new( \*FOO );

If you want to create a glob overloading object that returns a filehandle, do
this:

  local *BAR;
  open BAR, '<', $0 or die "Could not open $0 for the test";
  my $GLOB_OVERLOAD_FH = _T::GlobOverload->new( \*BAR );

=head1 SUPPORT

Bugs may be submitted at L<https://github.com/houseabsolute/Specio/issues>.

I am also usually active on IRC as 'autarch' on C<irc://irc.perl.org>.

=head1 SOURCE

The source code repository for Specio can be found at L<https://github.com/houseabsolute/Specio>.

=head1 AUTHOR

Dave Rolsky <autarch@urth.org>

=head1 COPYRIGHT AND LICENSE

This software is Copyright (c) 2012 - 2018 by Dave Rolsky.

This is free software, licensed under:

  The Artistic License 2.0 (GPL Compatible)

The full text of the license can be found in the
F<LICENSE> file included with this distribution.

=cut
