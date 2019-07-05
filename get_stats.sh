#!/bin/bash

i=0
totalbytesread=0
totalbyteswritten=0
totalfilesread=0
totalfileswritten=0
in=0

while read statline
do
	line=($statline)
	if [ "${line[0]}" = "START" ]
	then
		continue
	fi

	if [ "$in" -eq 0 ]
	then
		echo "$line"
		ids[$i]=${line[0]}
		i=$((i+1))
		in=$((in+1))
	fi

	if [ "${line[0]}" = "SENT" ]
	then
		totalfileswritten=$((totalfileswritten+1))
		totalbyteswritten=$((totalbyteswritten+${line[2]}))
	fi

	if [ "${line[0]}" = "RECEIVE" ]
	then
		totalfilesread=$((totalfilesread+1))
		totalbytesread=$((totalbytesread+${line[2]}))
	fi

	if [ "${line[0]}" = "END" ]
	then
		in=0
	fi

done


echo "Total ids $i"

for ((x=0 ; x < i ; x++))
do
	if [ "$x" -eq 0 ]
	then
		min=${ids[$x]}
		max=${ids[$x]}
	fi

	if [ "${ids[$x]}" -lt "$min" ]
	then
		min=${ids[$x]}
	fi

	if [ "${ids[$x]}" -gt "$max" ]
	then
		max=${ids[$x]}
	fi

done


echo "All ids during execution"
( IFS=$'\n'; echo "${ids[*]}" )

echo "Max id is"
echo "$max"

echo "Min id is"
echo "$min"

echo "Total bytes read during execution"
echo "$totalbytesread"

echo "Total bytes written during execution"
echo "$totalbyteswritten"

echo "Total files read"
echo "$totalfilesread"

echo "Total files written"
echo "$totalfileswritten"