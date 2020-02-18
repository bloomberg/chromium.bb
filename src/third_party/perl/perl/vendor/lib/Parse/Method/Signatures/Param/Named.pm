package Parse::Method::Signatures::Param::Named;

use Moose::Role;
use MooseX::Types::Moose qw/Str/;

use namespace::clean -except => 'meta';

has label => (
    is         => 'ro',
    isa        => Str,
    lazy_build => 1,
);

sub _label_from_variable_name {
    my ($self) = @_;
    # strip sigil
    return substr($self->variable_name, 1);
}

sub _build_label {
    my ($self) = @_;
    return $self->_label_from_variable_name;
}

sub _stringify_required {
    my ($self) = @_;
    return $self->required ? q{!} : q{};
}

around _stringify_variable_name => sub {
    my $orig = shift;
    my ($self) = @_;
    my $ret = q{:};
    my ($before, $after) = (q{}) x 2;

    my $implicit_label = $self->_label_from_variable_name if $self->can('variable_name');

    if (!$implicit_label || $self->label ne $implicit_label) {
        $before = $self->label . q{(};
        $after  = q{)};
    }

    $ret .= $before . $orig->(@_) . $after;

    return $ret;
};

1;
