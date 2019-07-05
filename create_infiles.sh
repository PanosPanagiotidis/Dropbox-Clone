#!/bin/bash
rootdir=$(pwd)
givendir=$1
rootdir+="/$givendir"
numfiles=$2
numdirs=$3
levels=$4

if [ "$#" -ne 4 ]
then
	echo "Wrong number of arguments"
	exit 1
fi

if ! [[ "$2" =~ ^[0-9]+$ ]]
then
	echo "Incorrect arg for number of files"
	exit 1
fi

if ! [[ "$3" =~ ^[0-9]+$ ]]
then
	echo "Incorrect arg for number of directories"
	exit 1
fi

if ! [[ "$4" =~ ^[0-9]+$ ]]
then
	echo "Incorrect arg for number of levels"
	exit 1
fi

if [[ ( "$2" -lt 0  ||  "$3" -lt 0  ||  "$4" -lt 0 ) ]]
then
	echo "Cannot have negative number of files,directories or levels"
	exit 1
fi

if [[ ( "$3" -eq 0 ) && ( "$4" -gt 0 ) ]]
then
	echo "Cannot have levels if you have 0 directories"
	exit 1
fi

if [ -d "$rootdir" ]
then
	echo "Directory already exists.Deleting..."
	rm -rf $rootdir
	echo "Rebuilding directory"
fi

mkdir $rootdir

rem=0
if [  $levels -le $numdirs  ]
then
	lperdir=1
	upto=$levels
else
	lperdir=$((levels/numdirs))
	rem=$((levels%numdirs))
	upto=$numdirs
fi



for ((i=0; i < upto;i++))
do
	lpdirarray[$i]=$lperdir
done

i=0

while [ $rem -gt 0 ]
do
	lpdirarray[$i]=$((${lpdirarray[$i]}+1))
	rem=$(($rem-1))
	i=$(($i+1))
done




z=0
for ((i=0; i < numdirs+1;i++))
do
	if [ $i -eq 0 ]
	then
		dirpaths[$i]="$rootdir"
		z=$((z+1))
		continue
	fi
	rand=$(cat /dev/urandom | tr -cd 'a-zA-Z0-9' | head -c $(($RANDOM%8+1)))
	mkdir "$rootdir/$rand"
	dirpaths[$z]="$rootdir/$rand"
	z=$((z+1))
	max=${lpdirarray[i-1]}
	for ((j=0 ; j < max ; j++)) 
	do
		rand2=$(cat /dev/urandom | tr -cd 'a-zA-Z0-9' | head -c $(($RANDOM%8+1)))
		mkdir "$rootdir/$rand/$rand2"
		dirpaths[$z]="$rootdir/$rand/$rand2"
		z=$((z+1))
	done
done


rem=$((numfiles%z))

for ((i=0 ; i < z ; i++))
do
	filesperdir[$i]=$((numfiles/z))
done

i=0
while [ $rem -gt 0 ]
do
	filesperdir[$i]=$((filesperdir[i]+1))
	rem=$((rem-1))
	i=$((i+1))

	if [ $i -eq 0 ]
	then
		i=0
	fi
done


i=0
while [ $numfiles -gt 0 ]
do
	filepath="${dirpaths[$i]}/"
	rfilename=$(cat /dev/urandom | tr -cd 'a-zA-Z0-9' | head -c $(($RANDOM%8+1)))
	filename="$rfilename"
	touch "$filepath$filename"
	rand=$(((RANDOM%128)+1))
	rstring=$(cat /dev/urandom | tr -cd 'a-zA-Z0-9' | head -c $((rand))KB )
	echo "$rstring" > "$filepath$filename"
	numfiles=$((numfiles-1))
	i=$((i+1))
	if [ $i -eq $z ]
	then
		i=0
	fi
done