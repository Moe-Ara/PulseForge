#include "ProcessRunner.hpp"

#include "../core/AppConfig.hpp"
#include "../core/Logger.hpp"

#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <poll.h>
#include <signal.h>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

ProcessRunner::ProcessRunner(int processTimeoutMs)
    : timeoutMs(processTimeoutMs) {}

std::string ProcessRunner::trim(std::string value) {
  while (!value.empty() &&
         (value.back() == '\n' || value.back() == '\r' ||
          value.back() == ' ' || value.back() == '\t')) {
    value.pop_back();
  }

  while (!value.empty() &&
         (value.front() == '\n' || value.front() == '\r' ||
          value.front() == ' ' || value.front() == '\t')) {
    value.erase(value.begin());
  }

  return value;
}

std::string ProcessRunner::resolveExecutable(const std::string &name) const {
  if (name.find('/') != std::string::npos) {
    return name;
  }

  const char *pathValue = std::getenv("PATH");
  const std::string path = pathValue ? pathValue : "";
  std::stringstream pathStream(path);
  std::string directory;
  while (std::getline(pathStream, directory, ':')) {
    const std::filesystem::path candidate =
        std::filesystem::path(directory) / name;
    if (access(candidate.c_str(), X_OK) == 0) {
      return candidate.string();
    }
  }

  for (const auto directory : AppConfig::executableFallbackDirectories) {
    const std::filesystem::path candidate =
        std::filesystem::path(directory) / name;
    if (access(candidate.c_str(), X_OK) == 0) {
      return candidate.string();
    }
  }

  return name;
}

ProcessResult
ProcessRunner::run(const std::vector<std::string> &arguments) const {
  ProcessResult result;
  if (arguments.empty()) {
    Logger::error("Cannot run an empty process command.");
    return result;
  }

  std::vector<std::string> resolvedArguments = arguments;
  resolvedArguments.front() = resolveExecutable(resolvedArguments.front());

  int pipeFds[2];
  if (pipe(pipeFds) != 0) {
    Logger::error("Failed to create process pipe: " +
                  std::string(std::strerror(errno)));
    return result;
  }

  const pid_t pid = fork();
  if (pid == -1) {
    close(pipeFds[0]);
    close(pipeFds[1]);
    Logger::error("Failed to fork process: " +
                  std::string(std::strerror(errno)));
    return result;
  }

  if (pid == 0) {
    dup2(pipeFds[1], STDOUT_FILENO);
    dup2(pipeFds[1], STDERR_FILENO);
    close(pipeFds[0]);
    close(pipeFds[1]);

    std::vector<char *> argv;
    argv.reserve(resolvedArguments.size() + 1);
    for (const auto &argument : resolvedArguments) {
      argv.push_back(const_cast<char *>(argument.c_str()));
    }
    argv.push_back(nullptr);

    execvp(argv.front(), argv.data());
    const std::string error =
        "execvp failed for " + resolvedArguments.front() + ": " +
        std::string(std::strerror(errno)) + "\n";
    write(STDERR_FILENO, error.c_str(), error.size());
    _exit(127);
  }

  close(pipeFds[1]);
  fcntl(pipeFds[0], F_SETFL, fcntl(pipeFds[0], F_GETFL, 0) | O_NONBLOCK);

  std::array<char, 512> buffer{};
  int status = 0;
  int elapsedMs = 0;

  while (true) {
    ssize_t bytesRead = 0;
    while ((bytesRead = read(pipeFds[0], buffer.data(), buffer.size())) > 0) {
      result.output.append(buffer.data(), static_cast<std::size_t>(bytesRead));
    }

    const pid_t waitResult = waitpid(pid, &status, WNOHANG);
    if (waitResult == pid) {
      break;
    }
    if (waitResult == -1) {
      result.output += "waitpid failed: " + std::string(std::strerror(errno));
      break;
    }

    if (elapsedMs >= timeoutMs) {
      result.timedOut = true;
      result.output += "Process timed out after " + std::to_string(timeoutMs) +
                       " ms.";
      kill(pid, SIGKILL);
      waitpid(pid, &status, 0);
      break;
    }

    pollfd readFd{};
    readFd.fd = pipeFds[0];
    readFd.events = POLLIN;
    poll(&readFd, 1, AppConfig::processPollStepMs);
    elapsedMs += AppConfig::processPollStepMs;
  }

  ssize_t bytesRead = 0;
  while ((bytesRead = read(pipeFds[0], buffer.data(), buffer.size())) > 0) {
    result.output.append(buffer.data(), static_cast<std::size_t>(bytesRead));
  }
  close(pipeFds[0]);

  if (WIFEXITED(status)) {
    result.exitCode = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    result.exitCode = 128 + WTERMSIG(status);
  }

  return result;
}

bool ProcessRunner::runChecked(const std::vector<std::string> &arguments,
                               const std::string &errorMessage) const {
  const ProcessResult result = run(arguments);
  if (result.exitCode == 0) {
    return true;
  }

  Logger::error(errorMessage + " Exit code: " +
                std::to_string(result.exitCode) + ". Output: " +
                trim(result.output));
  return false;
}
