require 'nokogiri'
require 'open-uri'

# TODO:
# - check for injuries!
# - convert stats to ruby objects
# - make basic per-player score based on stat lines

params = {
  cat: 'Season',
  pos: 'Player',
  SS: '2017-18',
  st: 'reg',
  lang: 'en',
  page: 1,
  league: 'NHL'
}

uri = URI('https://www.quanthockey.com/scripts/AjaxPaginate.php')
uri.query = URI.encode_www_form(params)

page = Nokogiri::HTML(open(uri))

File.open('stats.html', 'w') do |f|
  f.puts page
end
