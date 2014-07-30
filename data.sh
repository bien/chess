#!/bin/sh

mkdir -p games
cd games

if test ! -f pgnfiles.html; then 
    curl http://www.pgnmentor.com/files.html -o pgnfiles.html
fi

for i in `perl -ne 'if (m[href="(players/.*?\.zip)"]) { print "http://www.pgnmentor.com/$1\n"; }' < pgnfiles.html` ; do
    curl $i -o $(basename $i)
done
