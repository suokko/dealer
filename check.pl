#!/usr/bin/perl

for ($i=0; $i<40; $i++) {
  $out=system("./dealer -s ".$i. " check.txt | grep NS: ")
}
