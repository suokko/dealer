#!/usr/bin/perl

my ($exe, $file) = @ARGV;

if ($file eq "") {
  $file = "Descr.*";
}

if ($exe eq "") {
  $exe = "../dealer"
}

#
# Fixed seed so that the output should be the same from run to run
#
$seed = 1;

$exitcode = 0;

sub replaceprogname {
  my ($exe, $file) = @_;
  $cov  = $exe;
  $orig = $exe;
  $cov  =~ s/^.*\/([^\/]*[^vf])(\.cov|\.prof|)(\.exe|)$/\1.cov/;
  $orig =~ s/^(.*\/)([^\/]*[^v](\.cov|)(\.exe|))$/(\1|)\2/;

  open (IN, "<$file");
  @lines = <IN>;
  close IN;

  open (OUT,">$file");
  foreach $line (@lines) {
    $line =~ s/($orig|dealer)([^.\/][^cp])/$cov\3/g;
    print OUT "$line";
  }
  close OUT;
}

foreach $input (`ls $file`) {
  # Loop over all files that start with Descr.
  chop $input;
  print "  TEST   $input\n";

  $output = $input;
  $output =~ s/Descr/Output/;
  $refer  = $input;
  $refer  =~ s/Descr/Refer/;
  $params = $input;
  $params =~ s/Descr/Params/;

  unlink $output;
  unlink "$output.err";

  if (-e $params) {
    open my $info, $params;
    while( my $line = <$info>) {
      $line =~ s/\R//;
      my($rule, $arg) = split(',', $line);
      if (eval($rule)) {
        system ("echo $arg $input >> $output");
        system ("echo $arg $input >> $output.err");
        system ("$exe $arg $input 2>> $output.err >> $output");
      }
    }
  } else {
    system ("$exe -s $seed $input 2> $output.err > $output");
  }

  replaceprogname($exe, "$output.err");
  replaceprogname($exe, $output);

  if (-s "$output.err" == 0) {
    unlink("$output.err");
  }

  if (-e $refer) {
    $diff = `diff -u -Z $refer $output`;
    $diff =~ s/^(.*Time needed.*|.*\[Date ".*|[^+-].*|[+-][+-][+-] .*)\R//mg;
    if (-e "$refer.err" || -e "$output.err") {
      if (-e "$refer.err" && -e "$output.err") {
        $err = `diff -u -Z $refer.err $output.err`;
      } else { if (-e "$output.err") {
          print "Error: $refer.err missing\n";
          $err = `cat "$output.err"`;
          $exitcode = 4;
        } else {
          print "Error: $output.err missing\n";
          $err = `cat $refer.err`;
          $exitcode = 5;
        }
      }
      if ($err ne "") {
        print "Error: stderr ($output.err) not matching\n";
        print $err;
        $exitcode = 2;
      }
    }
    if ($diff ne "") {
      print "Error: output ($output) not matching\n";
      print $diff;
      $exitcode = 1;
    }
  } else {
    print "Error: $refer is missing\n";
    $exitcode = 3
  }

}
exit $exitcode;
