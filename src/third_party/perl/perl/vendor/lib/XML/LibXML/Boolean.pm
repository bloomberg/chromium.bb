# $Id$
#
#
# This is free software, you may use it and distribute it under the same terms as
# Perl itself.
#
# Copyright 2001-2003 AxKit.com Ltd., 2002-2006 Christian Glahn, 2006-2009 Petr Pajas
#
#

package XML::LibXML::Boolean;
use XML::LibXML::Number;
use XML::LibXML::Literal;
use strict;

use vars qw ($VERSION);

$VERSION = "1.98"; # VERSION TEMPLATE: DO NOT CHANGE

use overload
        '""' => \&value,
        '<=>' => \&cmp;

sub new {
    my $class = shift;
    my ($param) = @_;
    my $val = $param ? 1 : 0;
    bless \$val, $class;
}

sub True {
    my $class = shift;
    my $val = 1;
    bless \$val, $class;
}

sub False {
    my $class = shift;
    my $val = 0;
    bless \$val, $class;
}

sub value {
    my $self = shift;
    $$self;
}

sub cmp {
    my $self = shift;
    my ($other, $swap) = @_;
    if ($swap) {
        return $other <=> $$self;
    }
    return $$self <=> $other;
}

sub to_number { XML::LibXML::Number->new($_[0]->value); }
sub to_boolean { $_[0]; }
sub to_literal { XML::LibXML::Literal->new($_[0]->value ? "true" : "false"); }

sub string_value { return $_[0]->to_literal->value; }

1;
__END__

=head1 NAME

XML::LibXML::Boolean - Boolean true/false values

=head1 DESCRIPTION

XML::LibXML::Boolean objects implement simple boolean true/false objects.

=head1 API

=head2 XML::LibXML::Boolean->True

Creates a new Boolean object with a true value.

=head2 XML::LibXML::Boolean->False

Creates a new Boolean object with a false value.

=head2 value()

Returns true or false.

=head2 to_literal()

Returns the string "true" or "false".

=cut
