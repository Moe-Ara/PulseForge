#include <QApplication>

#include "audio/PipeWireBackend.hpp"
#include "gui/MainWindow.hpp"
#include "service/AudioService.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTextStream>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  PipeWireBackend backend;
  AudioService audioService(backend);
  audioService.initialize();

  QFile styleFile(QCoreApplication::applicationDirPath() +
                  "/../src/gui/styleDark.qss");

  if (!styleFile.exists()) {
    qWarning() << "QSS file does not exist:" << styleFile.fileName();
  } else if (!styleFile.open(QFile::ReadOnly | QFile::Text)) {
    qWarning() << "Failed to open QSS file:" << styleFile.errorString();
  } else {
    QTextStream stream(&styleFile);
    app.setStyleSheet(stream.readAll());
    qDebug() << "Loaded QSS:" << styleFile.fileName();
  }

  MainWindow window(audioService);
  window.show();

  return app.exec();
}
