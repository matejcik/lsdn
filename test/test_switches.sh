#!/bin/bash

myexec=$(realpath $0)
source $(dirname $myexec)/common.sh

if ping_from 1 4; then
	echo "ping works before switches set up, env is broken"
	return 1
fi

./test_switches

ping_from 1 4
