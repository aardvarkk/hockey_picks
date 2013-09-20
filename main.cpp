#include <map>
#include <string>

#include "strtk/strtk.hpp"

// Last one in the list is the one that will be predicted
// Currently, use the 2011 regular season and 2013 preseason to "predict" the 2012 season
static const char* prefixes[] = {
  "2011r",
  "2012r",
  "2013p"
}; // regular and preseason for each year

static const int num_prefixes = sizeof(prefixes) / sizeof(prefixes[0]);

struct SkaterPerformance
{
  std::string tag;
  int gp;
  int pts;
  double ppg;
};

typedef std::vector<double> Predictors;

typedef std::vector<SkaterPerformance> SkaterPerformances;      // map a player name to a set of performances
typedef std::map<std::string, SkaterPerformances>  SkaterStats; // all of a player's performances

typedef std::vector<strtk::token_grid*> TokenGrids;

void GetSkaterStats(SkaterStats& ss)
{
  // try skaters first
  TokenGrids skater_data;
  for (int i = 0; i < num_prefixes; ++i) {
    skater_data.push_back(new strtk::token_grid(std::string(prefixes[i]) + 's' + ".csv"));

    // Get column locations
    size_t name_col;
    size_t gp_col;
    size_t pts_col;
    strtk::token_grid::row_type r = skater_data[i]->row(0);
    for (size_t j = 0; j < r.size(); ++j) {
      std::string header = r.get<std::string>(j);
      header.erase(std::remove_if(header.begin(), header.end(), [](unsigned char c){return !::isalnum(c);}));
      std::transform(header.begin(), header.end(), header.begin(), ::tolower);
      if (header == "name" || header == "player") {
        name_col = j;
      }
      if (header == "gp") {
        gp_col = j;
      }
      if (header == "pts") {
        pts_col = j;
      }
    }

    // Go through all the rows in this data
    for (size_t j = 1; j < skater_data[i]->row_count(); ++j) {
      strtk::token_grid::row_type r = skater_data[i]->row(j);

      SkaterPerformance sp;
      sp.tag = prefixes[i];
      sp.gp = r.get<int>(gp_col);
      sp.pts = r.get<int>(pts_col);
      sp.ppg = static_cast<double>(sp.pts) / sp.gp;
      ss[r.get<std::string>(name_col)].push_back(sp);
    }
  }
}

void TryPredictorsAtIndex(
  SkaterStats const& ss,
  Predictors const& try_predictors, 
  int idx, 
  double& min_error, 
  Predictors& best_predictors
  )
{
  // Base case -- we have a full predictor vector
  Predictors my_predictors(try_predictors);

  if (idx == my_predictors.size()-1) {
    double sum = 0;
    for (size_t i = 0; i < my_predictors.size() - 1; ++i) {
      sum += my_predictors[i];
    }
    my_predictors[idx] = 1 - sum;

    // Calculate the error!
    double error = 0;

    // Go through each player applying the predictor
    for (auto it : ss) {

      // Our season values
      std::vector<int> vals(num_prefixes);
      bool active = false;

      // Go through each season
      for (auto s : it.second) {
        
        int p_idx = -1;
        // Get the relevant predictor
        for (int i = 0; i < num_prefixes; ++i) {
          if (prefixes[i] == s.tag) {
            p_idx = i;
          }
        }

        // We found stats in the latest year, so this player is active
        if (p_idx == num_prefixes-1) {
          active = true;
        }

        vals[p_idx] = s.ppg;
      }

      // Only care about active players
      if (!active) {
        continue;
      }

      // Make the prediction
      double est_ppg = 0;
      for (size_t i = 0; i < my_predictors.size(); ++i) {
        est_ppg += vals[i] * my_predictors[i];
      }

      // Add to the error!
      error += abs(vals[num_prefixes-1] - est_ppg);
    }
    
    // New best!
    if (error < min_error) {
      min_error = error;
      best_predictors = my_predictors;
    }

    return;
  }

  // Generate values at this level
  int to_try = 100;
  for (int p = 0; p <= to_try; ++p) {
    my_predictors[idx] = static_cast<double>(p) / to_try;
    TryPredictorsAtIndex(ss, my_predictors, idx+1, min_error, best_predictors);
  }
}

void FindBestPredictors(
  SkaterStats const& ss,
  Predictors& best_predictors
  )
{
  // Try a bunch of options
  best_predictors.resize(num_prefixes-1);

  double min_error = DBL_MAX;
  TryPredictorsAtIndex(ss, best_predictors, 0, min_error, best_predictors);
}

int main()
{
  SkaterStats ss;
  GetSkaterStats(ss);

  Predictors best_predictors;
  FindBestPredictors(ss, best_predictors);
}