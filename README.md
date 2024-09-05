# Primer on static build of Perl:
- build Perl into a single, self-contained, fully statically-linked executable
- builds all builtin modules statically and then links them statically
- showcases using Alpine Linux and musl libc for static linking with libc
- embeds `*.pm` modules into the executable
- showcases embedding a user Perl script
- showcases embedding arbitrary files along
- allows appending an uncompressed ZIP with more custom files and lets Perl access them transparently
- file I/O for the above is enabled by a custom read-only virtual FS [overriding](https://github.com/Perl/perl5/issues/22571) I/O libc/stdio function calls; ZIP is supported via linking with https://github.com/libarchive/libarchive

# Limitations
- not all I/O function calls are reimplemented
- it's only a proof-of-concept, very little tested
- compressed ZIP files are not supported: waiting on https://github.com/libarchive/libarchive/issues/2306

# References and alternatives
- https://metacpan.org/dist/App-Staticperl/view/staticperl.pod
- https://metacpan.org/pod/PAR::Packer
- http://staticperl.schmorp.de/smallperl.html
- http://staticperl.schmorp.de/smallperl.bundle
- http://staticperl.schmorp.de/bigperl.bundle
- https://metacpan.org/pod/PerlIO
- https://perldoc.perl.org/perlembed
- https://perldoc.perl.org/open
- https://perl.mines-albi.fr/perl5.8.5/5.8.5/sun4-solaris/Encode/PerlIO.html
- https://perldoc.perl.org/Encode::PerlIO
- https://linux.die.net/man/1/perliol
- https://docs.mojolicious.org/perliol
- https://stackoverflow.com/questions/12729545/why-is-this-xs-code-that-returns-a-perlio-leaky
- https://perldoc.perl.org/perlembed
- https://perldoc.perl.org/perlguts
- https://www.perlmonks.org/?node_id=385469
- https://github.com/Perl/perl5/commit/0301e899536a22752f40481d8a1d141b7a7dda82
- https://github.com/Perl/perl5/issues/16565
- https://www.postgresql.org/message-id/23260.1527026547%40sss.pgh.pa.us
- https://medium.com/booking-com-development/native-extensions-for-perl-without-smoke-and-mirrors-40479999dfc8
- https://github.com/xsawyerx/xs-fun
- https://perldoc.perl.org/perlxs
- https://perldoc.perl.org/perlapi
- https://stackoverflow.com/questions/10763394/how-to-build-a-c-program-using-a-custom-version-of-glibc-and-static-linking
