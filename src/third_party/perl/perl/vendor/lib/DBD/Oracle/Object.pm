#!perl
# ABSTRACT: Wrapper for Oracle objects
use strict;
use warnings;

package DBD::Oracle::Object;
our $VERSION = '1.76'; # VERSION

sub type_name { shift->{type_name} }

sub attributes { @{shift->{attributes}} }

sub attr_hash {
    my $self = shift;
    return $self->{attr_hash} ||= { $self->attributes };
}

sub attr {
    my $self = shift;
    if (@_) {
        my $key = shift;
        return $self->attr_hash->{$key};
    }
    return $self->attr_hash;
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

DBD::Oracle::Object - Wrapper for Oracle objects

=head1 VERSION

version 1.76

=head1 AUTHORS

=over 4

=item *

Tim Bunce <timb@cpan.org>

=item *

John Scoles <byterock@cpan.org>

=item *

Yanick Champoux <yanick@cpan.org>

=item *

Martin J. Evans <mjevans@cpan.org>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2018, 2014, 2013, 2012, 2011, 2010 by Tim Bunce.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
