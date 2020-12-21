#!/bin/sh

while read line
do
    echo "Launching job '$line'"
    ./cmdl "$line"&
done < $1
