#!/bin/sh

file='cmdld.conf'
wk=$(awk '/^DAEMON_WORKER_MAX/ {print $2}' $file)

for i in $(seq 1 $wk)
do
	t=$((i * 2))
	echo "[$i/$wk] sleep $t"
	./cmdl "sleep $t" & 
done
