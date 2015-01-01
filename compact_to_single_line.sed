#n
$!{
	:repeat
	N
	s/^\(n.*\)\n\(.*\)\n\(.*\)\n\(.*\)\n\(.*\)$/\5 \1 \2 \3 \4/
	t matched
	b repeat
	:matched
	P
	D
}

