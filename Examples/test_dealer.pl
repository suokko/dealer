#!/usr/bin/perl

$exe = "../dealer";
#
# Fixed seed so that the output should be the same from run to run
#
$seed = 1;

foreach $input (`ls Descr.*`) {
  # Loop over all files that start with Descr.
  chop $input;
  print "------------------------------------------------------------------\n";
  print "Now processing $input\n";

  $output = $input;
  $output =~ s/Descr/Output/;
  $refer  = $input;
  $refer  =~ s/Descr/Refer/;

  system ("$exe -s $seed $input >$output");

  print "Done, output in $output\n";

  if (-e $refer) {
     print "Comparing against reference output from $refer\n";
     print `diff $output $refer`;
  }
 
  $seed++;

}

