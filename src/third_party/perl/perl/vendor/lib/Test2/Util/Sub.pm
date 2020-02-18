package Test2::Util::Sub;
use strict;
use warnings;

our $VERSION = '0.000122';

use Carp qw/croak carp/;
use B();
use Sub::Info;

our @EXPORT_OK = qw{
    sub_info
    sub_name

    gen_reader gen_writer gen_accessor
};
use base 'Exporter';

sub gen_reader {
    my $field = shift;
    return sub { $_[0]->{$field} };
}

sub gen_writer {
    my $field = shift;
    return sub { $_[0]->{$field} = $_[1] };
}

sub gen_accessor {
    my $field = shift;
    return sub {
        my $self = shift;
        ($self->{$field}) = @_ if @_;
        return $self->{$field};
    };
}

sub sub_name {
    my ($sub) = @_;

    croak "sub_name requires a coderef as its only argument"
        unless ref($sub) eq 'CODE';

    my $cobj = B::svref_2object($sub);
    my $name = $cobj->GV->NAME;
    return $name;
}

sub sub_info {
    carp "Test2::Util::Sub::sub_info() is deprecated, use Sub::Info::sub_info() instead";
    Sub::Info::sub_info(@_);
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Util::Sub - Tools for inspecting and manipulating subs.

=head1 DESCRIPTION

Utilities used by Test2::Tools to inspect and manipulate subroutines.

=head1 EXPORTS

All exports are optional, you must specify subs to import.

=over 4

=item $name = sub_name(\&sub)

Get the name of the sub.

=item my $hr = sub_info(\&code)

This returns a hashref with information about the sub:

    {
        ref        => \&code,
        cobj       => $cobj,
        name       => "Some::Mod::code",
        file       => "Some/Mod.pm",
        package    => "Some::Mod",

        # Note: These have been adjusted based on guesswork.
        start_line => 22,
        end_line   => 42,
        lines      => [22, 42],

        # Not a bug, these lines are different!
        all_lines  => [23, 25, ..., 39, 41],
    };

=over 4

=item $info->{ref} => \&code

This is the original sub passed to C<sub_info()>.

=item $info->{cobj} => $cobj

This is the c-object representation of the coderef.

=item $info->{name} => "Some::Mod::code"

This is the name of the coderef. For anonymous coderefs this may end with
C<'__ANON__'>. Also note that the package 'main' is special, and 'main::' may
be omitted.

=item $info->{file} => "Some/Mod.pm"

The file in which the sub was defined.

=item $info->{package} => "Some::Mod"

The package in which the sub was defined.

=item $info->{start_line} => 22

=item $info->{end_line} => 42

=item $info->{lines} => [22, 42]

These three fields are the I<adjusted> start line, end line, and array with both.
It is important to note that these lines have been adjusted and may not be
accurate.

The lines are obtained by walking the ops. As such, the first line is the line
of the first statement, and the last line is the line of the last statement.
This means that in multi-line subs the lines are usually off by 1.  The lines
in these keys will be adjusted for you if it detects a multi-line sub.

=item $info->{all_lines} => [23, 25, ..., 39, 41]

This is an array with the lines of every statement in the sub. Unlike the other
line fields, these have not been adjusted for you.

=back

=back

=head1 SOURCE

The source code repository for Test2-Suite can be found at
F<https://github.com/Test-More/Test2-Suite/>.

=head1 MAINTAINERS

=over 4

=item Chad Granum E<lt>exodist@cpan.orgE<gt>

=back

=head1 AUTHORS

=over 4

=item Chad Granum E<lt>exodist@cpan.orgE<gt>

=item Kent Fredric E<lt>kentnl@cpan.orgE<gt>

=back

=head1 COPYRIGHT

Copyright 2018 Chad Granum E<lt>exodist@cpan.orgE<gt>.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

See F<http://dev.perl.org/licenses/>

=cut
