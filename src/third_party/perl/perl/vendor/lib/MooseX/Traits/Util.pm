package MooseX::Traits::Util;
use strict;
use warnings;

use Sub::Exporter -setup => {
    exports => ['new_class_with_traits'],
};

use Carp qw(confess);

# note: "$class" throughout is "class name" or "instance of class
# name"

sub check_class {
    my $class = shift;

    confess "We can't interact with traits for a class ($class) ".
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
            Class::MOP::load_class($transformed);
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
