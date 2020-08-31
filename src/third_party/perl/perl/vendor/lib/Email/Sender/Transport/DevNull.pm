package Email::Sender::Transport::DevNull;
# ABSTRACT: happily throw away your mail
$Email::Sender::Transport::DevNull::VERSION = '1.300031';
use Moo;
with 'Email::Sender::Transport';

#pod =head1 DESCRIPTION
#pod
#pod This class implements L<Email::Sender::Transport>.  Any mail sent through a
#pod DevNull transport will be silently discarded.
#pod
#pod =cut

sub send_email { return $_[0]->success }

no Moo;
1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Email::Sender::Transport::DevNull - happily throw away your mail

=head1 VERSION

version 1.300031

=head1 DESCRIPTION

This class implements L<Email::Sender::Transport>.  Any mail sent through a
DevNull transport will be silently discarded.

=head1 AUTHOR

Ricardo Signes <rjbs@cpan.org>

=head1 COPYRIGHT AND LICENSE

This software is copyright (c) 2017 by Ricardo Signes.

This is free software; you can redistribute it and/or modify it under
the same terms as the Perl 5 programming language system itself.

=cut
