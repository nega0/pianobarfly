#!/bin/bash
#
# Executes all scripts located in ~/.config/pianobarfly/eventcmd.d/ as if they
# were called by pianobarfly directly

STDIN=`mktemp ${TMPDIR:-/tmp}/pianobarfly.XXXXXX`
cat >> $STDIN

for F in ~/.config/pianobarfly/eventcmd.d/*; do
	if [ -x "$F" ]; then
		"$F" $@ < "$STDIN"
	fi
done

rm "$STDIN"

