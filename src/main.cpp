#include <QApplication>

#include "audio/PipeWireBackend.hpp"
#include "core/AppConfig.hpp"
#include "gui/MainWindow.hpp"
#include "service/AudioService.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QString>
#include <QStringList>
#include <QTextStream>

namespace {

QStringList styleSheetCandidates() {
  QStringList candidates;

#ifdef PULSEFORGE_DATA_DIR
  candidates << QStringLiteral(PULSEFORGE_DATA_DIR) + "/" +
                    QString::fromUtf8(AppConfig::qssFileName.data());
#endif

  candidates << QCoreApplication::applicationDirPath() +
                    QString::fromUtf8(
                        AppConfig::qssDevelopmentRelativePath.data());
  return candidates;
}

void loadStyleSheet(QApplication &app) {
  for (const QString &path : styleSheetCandidates()) {
    QFile styleFile(path);
    if (!styleFile.exists()) {
      continue;
    }

    if (!styleFile.open(QFile::ReadOnly | QFile::Text)) {
      qWarning() << "Failed to open QSS file:" << styleFile.errorString();
      continue;
    }

    QTextStream stream(&styleFile);
    app.setStyleSheet(stream.readAll());
    qDebug() << "Loaded QSS:" << styleFile.fileName();
    return;
  }

  qWarning() << "PulseForge QSS file was not found.";
}

} // namespace

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  QCoreApplication::setApplicationName(
      QString::fromUtf8(AppConfig::applicationName.data()));
  QCoreApplication::setOrganizationName(
      QString::fromUtf8(AppConfig::organizationName.data()));
  QApplication::setDesktopFileName(
      QString::fromUtf8(AppConfig::desktopFileName.data()));

  bool startInBackground = false;
  for (int i = 1; i < argc; ++i) {
    const QString argument = QString::fromLocal8Bit(argv[i]);
    if (argument == "--background" || argument == "--minimized") {
      startInBackground = true;
    }
  }

  PipeWireBackend backend;
  AudioService audioService(backend);
  audioService.initialize();

  loadStyleSheet(app);

  MainWindow window(audioService);
  if (startInBackground) {
    window.showMinimized();
  } else {
    window.show();
  }

  return app.exec();
}
