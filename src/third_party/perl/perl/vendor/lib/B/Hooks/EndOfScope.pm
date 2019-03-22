use strict;
use warnings;

package B::Hooks::EndOfScope;
BEGIN {
  $B::Hooks::EndOfScope::AUTHORITY = 'cpan:FLORA';
}
{
  $B::Hooks::EndOfScope::VERSION = '0.11';
}
# ABSTRACT: Execute code after a scope finished compilation

use 5.008000;
use Variable::Magic 0.48;

use Sub::Exporter -setup => {
    exports => ['on_scope_end'],
    groups  => { default => ['on_scope_end'] },
};



{
    my $wiz = Variable::Magic::wizard
        data => sub { [$_[1]] },
        free => sub { $_->() for @{ $_[1] }; () },
        # When someone localise %^H, our magic doesn't want to be copied
        # down. We want it to be around only for the scope we've initially
        # attached ourselfs to. Merely having MGf_LOCAL and a noop svt_local
        # callback achieves this. If anything wants to attach more magic of our
        # kind to a localised %^H, things will continue to just work as we'll be
        # attached with a new and empty callback list.
        local => \undef;

    sub on_scope_end (&) {
        my $cb = shift;

        $^H |= 0x020000;

        if (my $stack = Variable::Magic::getdata %^H, $wiz) {
            push @{ $stack }, $cb;
        }
        else {
            Variable::Magic::cast %^H, $wiz, $cb;
        }
    }
}


1;

__END__
=pod

=encoding utf-8

=head1 NAME

B::Hooks::EndOfScope - Execute code after a scope finished compilation

=head1 SYNOPSIS

    on_scope_end { ... };

=head1 DESCRIPTION

This module allows you to execute code when perl finished compiling the
surrounding scope.

=head1 FUNCTIONS

=head2 on_scope_end

    on_scope_end { ... };

    on_scope_end $code;

Registers C<$code> to be executed after the surrounding scope has been
compiled.

This is exported by default. See L<Sub::Exporter> on how to customize it.

=head1 SEE ALSO

L<Sub::Exporter>

L<Variable::Magic>

=head1 AUTHOR

Florian Ragwitz <rafl@debian.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2012 by Florian Ragwitz.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut

