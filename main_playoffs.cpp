#include <cassert>
#include <iomanip>
#include <fstream>
#include <random>

#include "strtk/strtk.hpp"

#include "types.h"

std::mt19937 eng;
const int kGameTotal = 48;

struct Team
{
  std::string name;
  char conf;
  int seed;
  int won;
  int lost;
  int otlost;

  bool operator<(Team const& t) const
  {
    if      (conf < t.conf) return true;
    else if (conf > t.conf) return false;
    else {
      return (seed < t.seed);
    };
  }
};

typedef std::vector<Team>         Teams;
typedef std::map<Team, float>     TeamGamesPlayed;
typedef std::pair<Team, Team>     Matchup;
typedef std::vector<Matchup>      Matchups;
typedef std::pair<int, int>       GameSplit;
typedef std::pair<Player, float>  PlayerPoints;
typedef std::vector<PlayerPoints> PlayerPointsList;

struct PlayerPointsListSorter
{
  bool operator()(PlayerPoints const& p1, PlayerPoints const& p2)
  {
    return p1.second > p2.second;
  }
};

Matchups GetMatchups(Teams& teams)
{
  Matchups matchups;

  // Sort the teams based on conference then seed
  Teams sorted_teams = teams;
  std::sort(sorted_teams.begin(), sorted_teams.end());

  // Only two teams left, so they're the matchup!
  if (sorted_teams.size() == 2) {
    matchups.push_back(std::make_pair(sorted_teams[0], sorted_teams[1]));
    return matchups;
  }

  // Keep pulling teams into matchups
  while (!sorted_teams.empty()) {
    // We'll have the highest seeded team at the top, so find the lowest seeded team of the same conference
    Teams::iterator t1 = sorted_teams.begin();
    Teams::iterator t2;
    for (Teams::iterator it = sorted_teams.begin() + 1; it != sorted_teams.end(); ++it) {
      if (it->conf == t1->conf) {
        t2 = it;
      }
    }

    // Push the matchup
    // IMPORTANT -- erase the second one first, because we know it's after the first
    matchups.push_back(std::make_pair(*t1, *t2));
    sorted_teams.erase(t2);
    sorted_teams.erase(t1);
  }

  return matchups;
}

GameSplit SimulateSeries(Matchup const& matchup)
{
  std::uniform_int<int> rnd_game(1, kGameTotal);
  GameSplit result;

  // Make sure each team has played the target number of games
  Team const& t1 = matchup.first;
  Team const& t2 = matchup.second;
  //int sum1 = t1.won + t1.lost + t1.otwon;
  //assert(sum1 == kGameTotal);
  //int sum2 = t2.won + t2.lost + t2.otwon;
  //assert(sum2 == kGameTotal);

  // Keep simulating until somebody wins 4 games
  // TODO: Ignoring home ice advantage. Include it somehow?
  while (result.first < 4 && result.second < 4) {

    // What we'll do is get a random integer between 1 and the game total
    // We'll see if it's a win for one team and a loss for the other
    
    // Generate two indices to check team records
    int g1 = rnd_game(eng);
    int g2 = rnd_game(eng);

    // Check if each team won
    bool t1win = g1 <= t1.won;
    bool t2win = g2 <= t2.won;

    // If this result doesn't make sense (both won or both lost) then discard it and start again
    if (t1win == t2win) continue;

    // Mark down the results
    result.first  += t1win;
    result.second += t2win;
  }

  return result;
}

void RunRound(Teams const& teams, TeamGamesPlayed& games_played, Teams& teams_remaining)
{
  // Sort by conference, then by seed
  Teams teams_copy = teams;

  // If we have 2 teams left, they'll be from separate conferences, so they'll match each other
  Matchups matchups = GetMatchups(teams_copy);

  // Collect all the winners
  Teams winners;
  
  // For each matchup, simulate a series
  for (auto it = matchups.begin(); it != matchups.end(); ++it) {
    GameSplit gs = SimulateSeries(*it);
    
    // We've got the game results, so we'll eliminate the losing team
    games_played[it->first]  += gs.first;
    games_played[it->second] += gs.second;

    // Add the winning team to the winners set
    if (gs.first > gs.second) {
      winners.push_back(it->first);
    } else {
      winners.push_back(it->second);
    }
  }

  // Set our output
  teams_remaining = winners;
}

TeamGamesPlayed RunPlayoffs(Teams const& teams, int runs, TeamGamesPlayed& win_perc)
{
  TeamGamesPlayed games_played;

  // Run it a bunch of times
  for (auto i = 0; i < runs; ++i) {
    
    static int perc = -1;
    int new_perc = i * 100 / runs;
    if (new_perc != perc) {
      std::cout << "Simulation " << new_perc << "% done." << std::endl;
        perc = new_perc;
    }

    Teams teams_in = teams;
    Teams teams_out;

    // Go through each round -- we'll eliminate teams until there's one winner
    while (teams_in.size() > 1) {
      RunRound(teams_in, games_played, teams_out);
      teams_in = teams_out;
    }

    // We have found a winner!
    win_perc[teams_out.front()] += 1;
  }

  // Normalize by the number of runs we've done...
  for (auto it = games_played.begin(); it != games_played.end(); ++it) {
    it->second /= runs;
  }
  for (auto it = win_perc.begin(); it != win_perc.end(); ++it) {
    it->second /= runs;
  }

  std::cout << "Simulation done." << std::endl;

  return games_played;
}

Teams GetTeams(strtk::token_grid const& grid)
{
  Teams teams;
  for (size_t i = 0; i < grid.row_count(); ++i) {
    strtk::token_grid::row_type r = grid.row(i);
    Team t;
    t.name = r.get<std::string>(0);
    t.conf = r.get<char>(1);
    t.seed = r.get<int>(2);
    t.won = r.get<int>(3);
    t.lost = r.get<int>(4);
    t.otlost = r.get<int>(5);
    teams.push_back(t);
  }
  return teams;
}

Players GetPlayers(strtk::token_grid const& grid, std::string const& pos)
{
  Players players;
  for (size_t i = 0; i < grid.row_count(); ++i) {
    strtk::token_grid::row_type r = grid.row(i);
    Player p;
    p.name = r.get<std::string>(0);
    p.team = r.get<std::string>(1);
    p.gp = r.get<int>(2);
    p.pts = r.get<int>(3);
    p.pos = pos; // copy inputted position
    players.push_back(p);
  }
  return players;
}

bool GetTeam(std::string const& team, Teams const& teams, Team& player_team)
{
  for (auto it = teams.cbegin(); it != teams.cend(); ++it) {
    if (!it->name.compare(team)) {
      player_team = *it;
      return true;
    }
  }

  return false;
}

PlayerPointsList ScorePlayers(Players const& players, Teams const& teams, TeamGamesPlayed const& tgp)
{
  PlayerPointsList pp;

  // Go through each player and calculate an expected number of points by multiplying team games played
  // by points per game!
  for (auto it = players.cbegin(); it != players.cend(); ++it) {
    // Get the expected points per game from a player
    float player_ppg = it->gp != 0 ? static_cast<float>(it->pts) / it->gp : 0.f;
    // Get the team
    Team player_team;
    bool found = GetTeam(it->team, teams, player_team);
    // Get the number of playoff games for this team
    float pgp = found ? tgp.find(player_team)->second : 0.f;
    // Get percentage of expected games player will play (injuries, etc.)
    float exp_game_perc = 1.f;//static_cast<float>(it->gp) / kGameTotal;
    // Get the estimated number of points for this player
    float player_pts = pgp * player_ppg * exp_game_perc;
    // Add to our data
    pp.push_back(std::make_pair(*it, player_pts));
  }

  // Sort based on player points
  std::sort(pp.begin(), pp.end(), PlayerPointsListSorter());

  return pp;
}

int main_playoffs(int argc, char* argv[]) 
{
  strtk::token_grid teams_csv("teamdata.csv");
  strtk::token_grid forwards_csv("forwards.csv");
  strtk::token_grid defense_csv("defense.csv");

  Teams t = GetTeams(teams_csv);
  Players fwd = GetPlayers(forwards_csv, "F");
  Players def = GetPlayers(defense_csv, "D");
  
  // Combine the players into one big list
  Players all;
  all.insert(all.end(), fwd.begin(), fwd.end());
  all.insert(all.end(), def.begin(), def.end());

  // Run the playoffs some number of times to get average number of games played per team
  TeamGamesPlayed win_perc;
  TeamGamesPlayed tgp = RunPlayoffs(t, 100000, win_perc);

  // Generate player scores based on the number of games we expect the team to play
  PlayerPointsList ppl = ScorePlayers(all, t, tgp);

  // Write out the results
  std::ofstream picks("scores.txt");
  for (auto it = ppl.begin(); it != ppl.end(); ++it) {
    picks << std::left << std::setw(30) << it->first.name << it->first.pos << " " << std::left << std::setw(4) << it->first.team << std::left << std::setw(3) << it->first.gp << it->second << std::endl;
  }
  picks.close();

  std::ofstream winners("winners.txt");
  for (auto it = win_perc.begin(); it != win_perc.end(); ++it) {
    winners << std::left << std::setfill(' ') << std::setw(4) << it->first.name << std::internal << std::fixed << std::setprecision(2) << std::setfill('0') << std::setw(5) << it->second * 100 << "%" << std::endl;
  }
  winners.close();

  return EXIT_SUCCESS;
}