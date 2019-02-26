use strict;
use warnings;

package Parse::Method::Signatures::Types;

use Moose::Util::TypeConstraints;
use MooseX::Types::Moose qw/Str ArrayRef/;
use namespace::clean;

use MooseX::Types -declare => [qw/
    VariableName
    TypeConstraint
    Param
    ParamCollection
    PositionalParam
    NamedParam
    UnpackedParam
/];

subtype VariableName,
    as Str,
    where { /^[\$@%](?:[a-z_][a-z_\d]*)?$/i },
    message { 'not a valid variable name' };

subtype TypeConstraint,
    as 'Moose::Meta::TypeConstraint';

class_type Param, { class => 'Parse::Method::Signatures::Param' };

class_type ParamCollection, { class => 'Parse::Method::Signatures::ParamCollection' };

coerce ParamCollection,
    from ArrayRef,
    via { Parse::Method::Signatures::ParamCollection->new(params => $_) };

role_type PositionalParam, { role => 'Parse::Method::Signatures::Param::Positional' };
role_type NamedParam,      { role => 'Parse::Method::Signatures::Param::Named'      };
role_type UnpackedParam,   { role => 'Parse::Method::Signatures::Param::Unpacked'   };

1;
