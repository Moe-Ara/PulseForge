#pragma once

class AutoStartManager {
public:
  bool enableAutoStart() const;
  bool disableAutoStart() const;
  bool isAutoStartEnabled() const;

private:
  bool ensureUserServiceFile() const;
  bool runSystemctlUser(const char *action) const;
};
