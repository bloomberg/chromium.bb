package # hide from PAUSE
    MooseX::Declare::Syntax::MethodDeclaration::Parameterized;

our $VERSION = '0.43';

use Moose::Role;
# we actually require MXRP 1.06 if versions 1.03,1.04,1.05 are installed
# (which is where current_metaclass was removed from the API), but this was
# only in the wild for a short time, so it's not worth creating a dynamic
# prereq for.
use MooseX::Role::Parameterized 0.12 ();
use namespace::autoclean;

around register_method_declaration => sub {
    my ($next, $self, $parameterizable_meta, $name, $method) = @_;
    my $meta = $self->metaclass_for_method_application($parameterizable_meta, $name, $method);
    $self->$next($meta, $name, $method);
};

sub metaclass_for_method_application {
    return MooseX::Role::Parameterized->current_metaclass;
}

1;
