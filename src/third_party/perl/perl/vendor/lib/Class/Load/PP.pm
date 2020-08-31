use strict;
use warnings;
package Class::Load::PP;

our $VERSION = '0.25';

use Module::Runtime ();
use Package::Stash 0.14;
use Scalar::Util ();
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

    return 0 unless Module::Runtime::is_module_name($class);

    my $stash = Package::Stash->new($class);

    if ($stash->has_symbol('$VERSION')) {
        my $version = ${ $stash->get_symbol('$VERSION') };
        if (defined $version) {
            return 1 if ! ref $version;
            # Sometimes $VERSION ends up as a reference to undef (weird)
            return 1 if ref $version && Scalar::Util::reftype $version eq 'SCALAR' && defined ${$version};
            # a version object
            return 1 if Scalar::Util::blessed $version;
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
