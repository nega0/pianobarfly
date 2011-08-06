#!/bin/bash

# create variables
while read L; do
	k="`echo "$L" | cut -d '=' -f 1`"
	v="`echo "$L" | cut -d '=' -f 2`"
	export "$k=$v"
done < <(grep -e '^\(title\|artist\|album\|stationName\|songStationName\|pRet\|pRetStr\|wRet\|wRetStr\|songDuration\|songPlayed\|rating\|coverArt\|stationCount\|station[0-9]*\)=' /dev/stdin) # don't overwrite $1...

case "$1" in
#	songstart)
#		echo 'naughty.notify({title = "pianobarfly", text = "Now playing: ' "$title" ' by ' "$artist" '"})' | awesome-client -

#		echo "$title -- $artist" > $HOME/.config/pianobarfly/nowplaying

#		if [ "$rating" -eq 1 ]
#		then
#			kdialog --title pianobarfly --passivepopup "'$title' by '$artist' on '$album' - LOVED" 10
#		else
#			kdialog --title pianobarfly --passivepopup "'$title' by '$artist' on '$album'" 10
#		fi
#		# or whatever you like...
#		;;

#	songfinish)
#		# scrobble if 75% of song have been played, but only if the song hasn't
#		# been banned
#		if [ -n "$songDuration" ] && [ "$songDuration" -ne 0 ] &&
#				[ $(echo "scale=4; ($songPlayed/$songDuration*100)>50" | bc) -eq 1 ] &&
#				[ "$rating" -ne 2 ]; then
#			# scrobbler-helper is part of the Audio::Scrobble package at cpan
#			# "pia" is the last.fm client identifier of "pianobarfly", don't use
#			# it for anything else, please
#			scrobbler-helper -P pia -V 1.0 "$title" "$artist" "$album" "" "" "" "$((songDuration/1000))" &
#		fi
#		;;

#	songlove)
#		kdialog --title pianobarfly --passivepopup "LOVING '$title' by '$artist' on '$album' on station '$stationName'" 10
#		;;

#	songshelf)
#		kdialog --title pianobarfly --passivepopup "SHELVING '$title' by '$artist' on '$album' on station '$stationName'" 10
#		;;

#	songban)
#		kdialog --title pianobarfly --passivepopup "BANNING '$title' by '$artist' on '$album' on station '$stationName'" 10
#		;;

#	songbookmark)
#		kdialog --title pianobarfly --passivepopup "BOOKMARKING '$title' by '$artist' on '$album'" 10
#		;;

#	artistbookmark)
#		kdialog --title pianobarfly --passivepopup "BOOKMARKING '$artist'" 10
#		;;

	*)
		if [ "$pRet" -ne 1 ]; then
			echo "naughty.notify({title = \"pianobarfly\", text = \"$1 failed: $pRetStr\"})" | awesome-client -
#			kdialog --title pianobarfly --passivepopup "$1 failed: $pRetStr"
		elif [ "$wRet" -ne 1 ]; then
			echo "naughty.notify({title = \"pianobarfly\", text = \"$1 failed: Network error: $wRetStr\"})" | awesome-client -
#			kdialog --title pianobarfly --passivepopup "$1 failed: Network error: $wRetStr"
		fi
		;;
esac

