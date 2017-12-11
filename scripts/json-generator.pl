#!/bin/perl

use warnings "all";
use strict;
use Getopt::Long;

my %options = ();

#
# Print usage information for this script.
#
sub usage
{
    my ($error) = @_;
    my $FILE = $error ? *STDERR : *STDOUT;

    print $FILE "\n";
    print $FILE "json-generator.pl: Emit contents of a .json file that is\n";
    print $FILE "                   consumed by the generic allocator driver\n";
    print $FILE "                   discovery mechanism.\n";
    print $FILE "\n";
    print $FILE "  perl json-generator.pl [-driver PATH | -help]\n";
    print $FILE "\n";
    print $FILE "PATH can be a relative path, including just a filename, or\n";
    print $FILE "an absolute path.\n";
    print $FILE "\n";

    exit($error);
}

GetOptions(\%options, "help", "driver=s") or usage(1);

if ($options{help})
{
    usage(0);
}

if (!defined $options{driver}) {
    print STDERR "Missing required parameter: -driver\n";
    usage(1);
}

print <<"EOF";
{
    "file_format_version" : "1.0.0",
    "allocator_driver": {
        "library_path": "$options{driver}"
    }
}
EOF
