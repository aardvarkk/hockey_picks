#include <cassert>
#include <iomanip>
#include <fstream>
#include <random>

#include "strtk/strtk.hpp"

std::mt19937 eng;
const int kGameTotal = 48;

struct Team
{
  std::string name;
  char conf;
  int seed;
  int won;
  int lost;
  int otwon;

  bool operator<(Team const& t) const
  {
    if      (conf < t.conf) return true;
    else if (conf > t.conf) return false;
    else {
      return (seed < t.seed);
    };
  }
};

struct Player
{
  std::string name;
  std::string team;
  int gp;
  int pts;

  bool operator<(Player const& p) const
  {
    return name < p.name;
  }
};

typedef std::vector<Team>         Teams;
typedef std::vector<Player>       Players;
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
    bool t1win = g1 <= t1.won + t1.otwon;
    bool t2win = g2 <= t2.won + t2.otwon;

    // If this result doesn't make sense (both won) then discard it and start again
    if (t1win && t2win) continue;

    // Mark down the results
    result.first  += t1win;
    result.second += t2win;
  }

  return result;
}

void RunRound(Teams const& teams, TeamGamesPlayed& games_played, Teams& teams_remaining)
{
  // Sort by conference, then by seed
  Teams sorted_teams = teams;

  // If we have 2 teams left, they'll be from separate conferences, so they'll match each other
  Matchups matchups = GetMatchups(sorted_teams);

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

TeamGamesPlayed RunPlayoffs(Teams const& teams, int runs)
{
  TeamGamesPlayed games_played;

  // Run it a bunch of times
  for (auto i = 0; i < runs; ++i) {
    
    Teams teams_in = teams;
    Teams teams_out;

    // Go through each round -- we'll eliminate teams until there's one winner
    while (teams_in.size() > 1) {
      RunRound(teams_in, games_played, teams_out);
      teams_in = teams_out;
    }
  }

  // Normalize by the number of runs we've done...
  for (auto it = games_played.begin(); it != games_played.end(); ++it) {
    it->second /= runs;
  }

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
    t.otwon = r.get<int>(5);
    teams.push_back(t);
  }
  return teams;
}

Players GetPlayers(strtk::token_grid const& grid)
{
  Players players;
  for (size_t i = 0; i < grid.row_count(); ++i) {
    strtk::token_grid::row_type r = grid.row(i);
    Player p;
    p.name = r.get<std::string>(0);
    p.team = r.get<std::string>(1);
    p.gp = r.get<int>(2);
    p.pts = r.get<int>(3);
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
    // Get the estimated number of points for this player
    float player_pts = pgp * player_ppg;
    // Add to our data
    pp.push_back(std::make_pair(*it, player_pts));
  }

  // Sort based on player points
  std::sort(pp.begin(), pp.end(), PlayerPointsListSorter());

  return pp;
}

int main(int argc, char* argv[]) 
{
  strtk::token_grid teams_csv("teamdata.csv");
  strtk::token_grid players_csv("playerdata.csv");

  Teams t = GetTeams(teams_csv);
  Players p = GetPlayers(players_csv);

  // Run the playoffs some number of times to get average number of games played per team
  TeamGamesPlayed tgp = RunPlayoffs(t, 100000);

  // Generate player scores based on the number of games we expect the team to play
  PlayerPointsList ppl = ScorePlayers(p, t, tgp);

  // Write out the results
  std::ofstream f("scores.txt");
  for (auto it = ppl.begin(); it != ppl.end(); ++it) {
    f << std::left << std::setw(30) << it->first.name << std::left << std::setw(4) << it->first.team << it->second << std::endl;
  }
  f.close();
}