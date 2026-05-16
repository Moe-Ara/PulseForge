#include <QApplication>

#include "audio/PipeWireBackend.hpp"
#include "gui/MainWindow.hpp"
#include "service/AudioService.hpp"

#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QDir>


int main(int argc, char *argv[])
{
  QApplication app(argc, argv);

  PipeWireBackend backend;
  AudioService audioService(backend);
  audioService.initialize();
  QFile styleFile("../src/gui/styleDark.qss");

  if (!styleFile.exists())
  {
    qWarning() << "QSS file does not exist:" << styleFile.fileName();
  }
  else if (!styleFile.open(QFile::ReadOnly | QFile::Text))
  {
    qWarning() << "Failed to open QSS file:" << styleFile.errorString();
  }
  else
  {
    QTextStream stream(&styleFile);
    app.setStyleSheet(stream.readAll());
    qDebug() << "Loaded QSS:" << styleFile.fileName();
  }

  if (styleFile.open(QFile::ReadOnly | QFile::Text))
  {
    QTextStream stream(&styleFile);
    app.setStyleSheet(stream.readAll());
  }
  MainWindow window(audioService);
  window.show();

  return app.exec();
}
