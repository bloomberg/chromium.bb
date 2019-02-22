package Data::Compare::Plugins::Scalar::Properties;

use strict;
use vars qw($VERSION);
use Data::Compare;

$VERSION = 1.0;

sub register {
    return [
        ['Scalar::Properties', \&sp_scalar_compare],
        ['', 'Scalar::Properties', \&sp_scalar_compare],
    ];
}

# note that when S::Ps are involved we can't use Data::Compare's default
# Compare function, so we use eq to check that values are the same.  But
# we *do* use D::C::Compare whenever possible.

# Compare a S::P and a scalar, or if we figure out that we've got two
# S::Ps, call sp_sp_compare instead

sub sp_scalar_compare {
    my($scalar, $sp) = @_;

    # we don't care what order the two params are, so swap if necessary
    ($scalar, $sp) = ($sp, $scalar) if(ref($scalar));

    # got two S::Ps?
    return sp_sp_compare($scalar, $sp) if(ref($scalar));

    # we've really got a scalar and an S::P, so just compare values
    return 1 if($scalar eq $sp);
    return 0;
}

# Compare two S::Ps

sub sp_sp_compare {
    my($sp1, $sp2) = @_;

    # first check the values
    return 0 unless($sp1 eq $sp2);
    
    # now check that we have all the same properties
    return 0 unless(Data::Compare::Compare([sort $sp1->get_props()], [sort $sp2->get_props()]));

    # and that all properties have the same values
    return 0 if(
        grep { !Data::Compare::Compare(eval "\$sp1->$_()", eval "\$sp2->$_()") } $sp1->get_props()
    );

    # if we get here, all is tickety-boo
    return 1;
}

register();

=head1 NAME

Data::Compare::Plugin::Scalar::Properties - plugin for Data::Compare to
handle Scalar::Properties objects.

=head1 DESCRIPTION

Enables Data::Compare to Do The Right Thing for Scalar::Properties
objects.

=over 4

=item comparing a Scalar::Properties object and an ordinary scalar

If you compare
a scalar and a Scalar::Properties, then they will be considered the same
if the two values are the same, regardless of the presence of properties.

=item comparing two Scalar::Properties objects

If you compare two Scalar::Properties objects, then they will only be
considered the same if the values and the properties match.

=back

=head1 AUTHOR

Copyright (c) 2004 David Cantrell. All rights reserved.
This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=head1 SEE ALSO

L<Data::Compare>

=cut
