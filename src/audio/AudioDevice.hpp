#pragma once
#include <string>

struct AudioDevice {
  std::string id;
  std::string name;
  std::string sinkName;
  bool isDefault = false;
};
