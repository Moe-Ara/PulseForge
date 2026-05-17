#pragma once

class AutoStartManager {
public:
  bool enableAutoStart() const;
  bool disableAutoStart() const;
  bool isAutoStartEnabled() const;

private:
  bool runSystemctlUser(const char *action) const;
};
