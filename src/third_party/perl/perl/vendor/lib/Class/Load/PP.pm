package Class::Load::PP;
{
  $Class::Load::PP::VERSION = '0.19';
}

use strict;
use warnings;
use Module::Runtime 'is_module_name';
use Package::Stash 0.14;
use Scalar::Util 'blessed', 'reftype';
use Try::Tiny;

sub is_class_loaded {
    my $class   = shift;
    my $options = shift;

    my $loaded = _is_class_loaded($class);

    return $loaded if ! $loaded;
    return $loaded unless $options && $options->{-version};

    return try {
        $class->VERSION($options->{-version});
        1;
    }
    catch {
        0;
    };
}

sub _is_class_loaded {
    my $class = shift;

    return 0 unless is_module_name($class);

    my $stash = Package::Stash->new($class);

    if ($stash->has_symbol('$VERSION')) {
        my $version = ${ $stash->get_symbol('$VERSION') };
        if (defined $version) {
            return 1 if ! ref $version;
            # Sometimes $VERSION ends up as a reference to undef (weird)
            return 1 if ref $version && reftype $version eq 'SCALAR' && defined ${$version};
            # a version object
            return 1 if blessed $version;
        }
    }

    if ($stash->has_symbol('@ISA')) {
        return 1 if @{ $stash->get_symbol('@ISA') };
    }

    # check for any method
    return 1 if $stash->list_all_symbols('CODE');

    # fail
    return 0;
}

1;
