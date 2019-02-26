package Parse::Method::Signatures::Param::Unpacked;

use Moose::Role;
use Parse::Method::Signatures::Types qw/ParamCollection/;

use namespace::clean -except => 'meta';

has _params => (
    is => 'ro',
    isa => ParamCollection,
    init_arg => 'params',
    predicate => 'has_params',
    coerce => 1,
    handles => {
        params => 'params',
    },
);

1;
