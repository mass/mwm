#include "Manager.hpp"

#include <getopt.h>

int main(int argc, char* argv[])
{
  static struct option long_options[] =
  {
    {"display", required_argument, NULL, 'd'},
    {"screen", required_argument, NULL, 's'},
    {"screenshot-dir", required_argument, NULL, 'S'},
    {NULL, 0, NULL, 0}
  };

  std::string display;
  std::map<int,Point> screens;
  std::string screenshotDir = "${HOME}";

  int ch;
  while ((ch = getopt_long(argc, argv, "d:s:S:", long_options, NULL)) != -1) {
    switch (ch) {
      case 'd':
        display = optarg;
        break;
      case 's': {
        int screen;
        int x = 0, y = 0;
        if (sscanf(optarg, "%i(%i,%i)", &screen, &x, &y) < 1)
          throw std::invalid_argument("invalid screen argument");
        screens[screen] = Point(x,y);
        break;
      }
      case 'S':
        screenshotDir = optarg;
        break;
    }
  }

  Manager m(display, screens, screenshotDir);
  if (!m.init())
    return EXIT_FAILURE;
  m.run();

  return EXIT_SUCCESS;
}
