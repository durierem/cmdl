#!/bin/sh

n=$(awk '/^DAEMON_WORKER_MAX/ {print $2}' 'cmdld.conf')

for i in $(seq 1 $n)
do
	cmd="sleep $((i * 2))"
	echo "[$i/$n] $cmd"
	./cmdl "$cmd" & 
done
