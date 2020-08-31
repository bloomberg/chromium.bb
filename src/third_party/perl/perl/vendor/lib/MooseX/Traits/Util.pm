package MooseX::Traits::Util;
use strict;
use warnings;

our $VERSION = '0.13';

use Sub::Exporter -setup => {
    exports => ['new_class_with_traits'],
};

use Class::Load ();
use Carp ();

# note: "$class" throughout is "class name" or "instance of class
# name"

sub check_class {
    my $class = shift;

    Carp::confess "We can't interact with traits for a class ($class) ".
      "that does not do MooseX::Traits" unless $class->does('MooseX::Traits');
}

sub transform_trait {
    my ($class, $name) = @_;
    return $1 if $name =~ /^[+](.+)$/;

    check_class($class);

    my $namespace = $class->meta->find_attribute_by_name('_trait_namespace');
    my $base;
    if($namespace->has_default){
        $base = $namespace->default;
        if(ref $base eq 'CODE'){
            $base = $base->();
        }
    }

    return $name unless $base;
    return join '::', $base, $name;
}

sub resolve_traits {
    my ($class, @traits) = @_;

    check_class($class);

    return map {
        my $orig = $_;
        if(!ref $orig){
            my $transformed = transform_trait($class, $orig);
            Class::Load::load_class($transformed);
            $transformed;
        }
        else {
            $orig;
        }
    } @traits;
}

my $anon_serial = 0;

sub new_class_with_traits {
    my ($class, @traits) = @_;

    check_class($class);

    my $meta;
    @traits = resolve_traits($class, @traits);
    if (@traits) {
        $meta = $class->meta->create(
            join(q{::} => 'MooseX::Traits::__ANON__::SERIAL', ++$anon_serial),
            superclasses => [ $class->meta->name ],
            roles        => \@traits,
            cache        => 1,
        );
    }

    # if no traits were given just return the class meta
    return $meta ? $meta : $class->meta;
}

1;

=for Pod::Coverage check_class new_class_with_traits resolve_traits transform_trait

=cut
