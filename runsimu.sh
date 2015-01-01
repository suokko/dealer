#!/bin/sh

FILE=$1
SIMUNR=$2
EXAMPLENR=$3

if [ -z "$FILE" ] ||
	[ ! -e "$FILE" ] ||
	! ./dealer -p 1 < $FILE > /dev/null; then
	echo "*** First parameter must be the simulation file."
	exit 1
fi

if [ -z "$SIMUNR" ]; then
	SIMUNR=400
fi

if [ -z "$EXAMPLENR" ]; then
	EXAMPLENR=30
fi

case $SIMUNR in 
	''|*[!0-9]*)
	echo "*** Second parameter can only be a integer setting simulation production limit"
	exit 1 ;;
esac

case $EXAMPLENR in 
	''|*[!0-9]*)
	echo "*** Third parameter can only be a integer setting example hand production limit"
	exit 1
esac

echo "#***\n" >> $FILE
sed -i -e '/^#\*\*\*/,$d' -e "s/produce .*/produce $EXAMPLENR/" -e 's/^\(\tave\|\tfreq\)/#\1/' -e 's/^#\tprint/\tprint/' $FILE

echo "#***\n#Example deals for the biding\n#" >> $FILE
./dealer < $FILE | sed 's/^/#/' | tee -a $FILE
sed -i -e "s/produce .*/produce $SIMUNR/" -e 's/^#\(\tave\|\tfreq\)/\1/' -e 's/^\tprint/#\tprint/' $FILE

echo "\n#Simulation results\n#" >> $FILE
./dealer < $FILE | sed 's/^/#/' | tee -a $FILE
sed -i -e "s/produce .*/produce $EXAMPLENR/" -e 's/^\(\tave\|\tfreq\)/#\1/' -e 's/^#\tprint/\tprint/' $FILE

exit 0
