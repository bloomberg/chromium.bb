package MooseX::Declare::Context::WithOptions;
BEGIN {
  $MooseX::Declare::Context::WithOptions::AUTHORITY = 'cpan:FLORA';
}
{
  $MooseX::Declare::Context::WithOptions::VERSION = '0.35';
}

use Moose::Role;
use Carp qw/croak/;
use MooseX::Types::Moose 0.20 qw/HashRef/;

use namespace::clean -except => 'meta';

has options => (
    is      => 'rw',
    isa     => HashRef,
    default => sub { {} },
    lazy    => 1,
);

sub strip_options {
    my ($self) = @_;
    my %ret;

    # Make errors get reported from right place in source file
    local $Carp::Internal{'MooseX::Declare'} = 1;
    local $Carp::Internal{'Devel::Declare'} = 1;

    $self->skipspace;
    my $linestr = $self->get_linestr;

    while (substr($linestr, $self->offset, 1) !~ /[{;]/) {
        my $key = $self->strip_name;
        if (!defined $key) {
            croak 'expected option name'
              if keys %ret;
            return; # This is the case when { class => 'foo' } happens
        }

        croak "unknown option name '$key'"
            unless $key =~ /^(extends|with|is)$/;

        my $val = $self->strip_name;
        if (!defined $val) {
            if (defined($val = $self->strip_proto)) {
                $val = [split /\s*,\s*/, $val];
            }
            else {
                croak "expected option value after $key";
            }
        }

        $ret{$key} ||= [];
        push @{ $ret{$key} }, ref $val ? @{ $val } : $val;
    } continue {
        $self->skipspace;
        $linestr = $self->get_linestr();
    }

    my $options = { map {
        my $key = $_;
        $key eq 'is'
            ? ($key => { map { ($_ => 1) } @{ $ret{$key} } })
            : ($key => $ret{$key})
    } keys %ret };

    $self->options($options);

    return $options;
}

1;

__END__
=pod

=encoding utf-8

=head1 NAME

MooseX::Declare::Context::WithOptions

=head1 AUTHORS

=over 4

=item *

Florian Ragwitz <rafl@debian.org>

=item *

Ash Berlin <ash@cpan.org>

=item *

Chas. J. Owens IV <chas.owens@gmail.com>

=item *

Chris Prather <chris@prather.org>

=item *

Dave Rolsky <autarch@urth.org>

=item *

Devin Austin <dhoss@cpan.org>

=item *

Hans Dieter Pearcey <hdp@cpan.org>

=item *

Justin Hunter <justin.d.hunter@gmail.com>

=item *

Matt Kraai <kraai@ftbfs.org>

=item *

Michele Beltrame <arthas@cpan.org>

=item *

Nelo Onyiah <nelo.onyiah@gmail.com>

=item *

nperez <nperez@cpan.org>

=item *

Piers Cawley <pdcawley@bofh.org.uk>

=item *

Rafael Kitover <rkitover@io.com>

=item *

Robert 'phaylon' Sedlacek <rs@474.at>

=item *

Stevan Little <stevan.little@iinteractive.com>

=item *

Tomas Doran <bobtfish@bobtfish.net>

=item *

Yanick Champoux <yanick@babyl.dyndns.org>

=back

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2011 by Florian Ragwitz.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut

