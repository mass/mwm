#include "Manager.hpp"

#include <getopt.h>

int main(int argc, char* argv[])
{
  static struct option long_options[] =
  {
    {"display", required_argument, NULL, 'd'},
    {"screen", required_argument, NULL, 's'},
    {NULL, 0, NULL, 0}
  };

  std::string display;
  std::vector<int> screens;

  char ch;
  while ((ch = getopt_long(argc, argv, "d:s:", long_options, NULL)) != -1) {
    switch (ch) {
      case 'd':
        display = optarg;
        break;
      case 's':
        screens.push_back(atoi(optarg));
        break;
    }
  }

  Manager m(display, screens);
  if (!m.init()) return EXIT_FAILURE;
  m.run();

  return EXIT_SUCCESS;
}
