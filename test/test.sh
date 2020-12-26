#!/bin/sh

n=$(awk '/^DAEMON_WORKER_MAX/ {print $2}' 'cmdld.conf')

for i in $(seq 1 $n)
do
	cmd="/bin/sleep $((i * 2))"
	echo "[Job $i/$n] $cmd"
	./cmdl "$cmd" & 
done
