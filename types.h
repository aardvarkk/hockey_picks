#include <string>

struct Player
{
  std::string name;
  std::string team;
  int gp;
  int pts;
  std::string pos;

  bool operator<(Player const& p) const
  {
    return name < p.name;
  }
};

typedef std::vector<Player> Players;