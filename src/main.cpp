#include <QApplication>

#include "audio/PipeWireBackend.hpp"
#include "gui/MainWindow.hpp"
#include "service/AudioService.hpp"

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  PipeWireBackend backend;
  AudioService audioService(backend);
  audioService.initialize();

  MainWindow window(audioService);
  window.show();

  return app.exec();
}
