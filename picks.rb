require 'nokogiri'
require 'open-uri'
require 'pry'

# TODO:
# - check for injuries!
# - convert stats to ruby objects
# - make basic per-player score based on stat lines

def add_results(result_arr)
end

def important_cols
  {
    name: 3,
    goals: 7,
    assists: 8,
    plus_minus: 11,
    penalty_mins: 10,
    powerplay_pts: 23,
    shorthanded_pts: 24,
    game_winning_goals: 15,
    shots: 44,
    hits: 46
  }
end

def multipliers
  {
    goals: 5,
    assists: 3,
    plus_minus: 1,
    penalty_mins: 1,
    powerplay_pts: 2,
    shorthanded_pts: 1,
    game_winning_goals: 2,
    shots: 1,
    hits: 1
  }
end

def append_page_results(page, results)
  rows = page.xpath('//table/tbody/tr')
  results.concat(rows.map do |row|
    important_cols.map do |stat, col|
      val = row.xpath('./td')[col-1].text
      val = Float(val) rescue val
      [stat, val]
    end.to_h
  end)
end

def gen_params(page_num)
  {
    cat: 'Season',
    pos: 'Player',
    SS: '2017-18',
    st: 'reg',
    lang: 'en',
    page: page_num,
    league: 'NHL'
  }
end

def gen_uri(page_num)
  uri = URI('https://www.quanthockey.com/scripts/AjaxPaginate.php')
  uri.query = URI.encode_www_form(gen_params(page_num))
  uri
end

def get_page(page_num)
  puts "Retrieving page #{page_num}"
  Nokogiri::HTML(open(gen_uri(page_num)))
end

def score_results(results, mults)
  results.map do |player_stats|
    score = mults.inject(0) do |acc, category|
      acc += player_stats[category[0]].to_f * category[1]
    end
    player_stats.merge({ score: score })
  end
end

results = []
page = get_page(1)
append_page_results(page, results)
num_pages = page.xpath('//ul[1]/li[last()-1]/a').text.to_i

(2..num_pages).each do |page_num|
  page = get_page(page_num)
  append_page_results(page, results)
end

scored_results = score_results(results, multipliers).sort_by{ |stats| stats[:score] }.reverse!
File.open('skaters.txt', 'w') do |f|
  scored_results.each do |player|
    f.puts "#{player[:name]} #{player[:score]}"
  end
end
