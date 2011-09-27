#!/usr/bin/env ruby

destination = '/Users/myusername/Dropbox/Music/'

trigger = ARGV.shift

songinfo = {}

STDIN.each_line { |line| songinfo.store(*line.chomp.split('=', 2))}

#uncomment next line if you want a log of events output to /tmp/pianobar.log
#File.open("/tmp/pianobar.log", 'a') {|file| file.write("\n\n#{trigger}: \n#{songinfo.inspect}\n\n===")}

if trigger == 'songfinish'
  file_path = songinfo['filePath']
  dir = File.dirname(file_path)
  filename = File.basename(file_path)
  destination += "#{dir}"
  `growlnotify -t "Copying Files" -m "Copying #{filename} to #{destination}"`
  `mkdir -p #{destination}`
  `cp #{file_path} #{destination}`
end

if trigger == 'songstart'
  `growlnotify -t "Now Playing" -m "#{songinfo['title']}\nby #{songinfo['artist']}"`
end