#include "Manager.hpp"

int main(int argc, char* argv[])
{
  Manager m;
  if (!m.init()) return EXIT_FAILURE;
  m.run();

  return EXIT_SUCCESS;
}
