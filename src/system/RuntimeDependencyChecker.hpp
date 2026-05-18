#pragma once

#include <QString>
#include <QStringList>

class RuntimeDependencyChecker {
public:
  struct Result {
    bool ok = true;
    QStringList missing;
    QStringList warnings;

    QString userMessage() const;
  };

  Result check() const;

private:
  bool commandAvailable(const QString &command) const;
};
