#include <depends>
#include "discord.h"

using namespace qt;
int main(int argc, char *argv[])
{
  core loop(argc, argv);
  Discord d;
  d.start();
  return loop.exec();
}































































































