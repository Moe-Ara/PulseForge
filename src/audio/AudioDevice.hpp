#pragma once
#include <string>

struct AudioDevice {
  std::string id;
  std::string name;
  bool isDefault = false;
};
