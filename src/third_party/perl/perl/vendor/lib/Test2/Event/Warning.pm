package Test2::Event::Warning;

use strict;
use warnings;

our $VERSION = '0.07';

use parent 'Test2::Event';

use Test2::Util::HashBase qw( causes_fail warning );

sub init {
    $_[0]->{ +CAUSES_FAIL } = 1       unless exists $_[0]->{ +CAUSES_FAIL };
    $_[0]->{ +WARNING }     = 'undef' unless defined $_[0]->{ +WARNING };
}

sub summary { $_[0]->{ +WARNING } }

sub increments_count {1}
sub diagnostics      {1}

1;

# ABSTRACT: A Test2 event for warnings

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Event::Warning - A Test2 event for warnings

=head1 VERSION

version 0.07

=head1 DESCRIPTION

An event representing an unwanted warning. This is treated as a failure.

=for Pod::Coverage init

=head1 ACCESSORS

=over 4

=item $warning = $event->warning

Returns the warning that this event captured.

=back

=head1 SUPPORT

Bugs may be submitted at L<http://rt.cpan.org/Public/Dist/Display.html?Name=Test2-Plugin-NoWarnings> or via email to L<bug-test2-plugin-nowarnings@rt.cpan.org|mailto:bug-test2-plugin-nowarnings@rt.cpan.org>.

I am also usually active on IRC as 'autarch' on C<irc://irc.perl.org>.

=head1 SOURCE

The source code repository for Test2-Plugin-NoWarnings can be found at L<https://github.com/houseabsolute/Test2-Plugin-NoWarnings>.

=head1 AUTHOR

Dave Rolsky <autarch@urth.org>

=head1 COPYRIGHT AND LICENSE

This software is Copyright (c) 2019 by Dave Rolsky.

This is free software, licensed under:

  The Artistic License 2.0 (GPL Compatible)

The full text of the license can be found in the
F<LICENSE> file included with this distribution.

=cut
