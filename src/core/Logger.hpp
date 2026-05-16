#pragma once
#include <iostream>
#include <string>

class Logger {
public:
  static void info(const std::string &msg) {
    std::cout << "[INFO] " << msg << std::endl;
  }
  static void warn(const std::string &msg) {
    std::cout << "[WARN] " << msg << std::endl;
  }
  static void error(const std::string &msg) {
    std::cout << "[ERROR] " << msg << std::endl;
  }
};
