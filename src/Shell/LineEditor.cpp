#include "shell.hpp"

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <vector>

void Shell::handleTab(std::string &line, bool doubleTab) {
  std::vector<std::string> matches;
  size_t lastSpace = line.find_last_of(' ');
  bool isFirstword = (lastSpace == std::string::npos);
  std::string currentWord =
      (lastSpace == std::string::npos) ? line : line.substr(lastSpace + 1);

  if (isFirstword) {
    for (auto &pair : commands) {
      if (pair.first.starts_with(currentWord)) {
        matches.push_back(pair.first);
      }
    }

    const char *pathenv = std::getenv("PATH");
    if (pathenv) {
      std::string pathdir = pathenv;
      std::string dir;
      std::istringstream stream(pathdir);
      while (std::getline(stream, dir, ':')) {
        std::error_code ec;
        std::filesystem::directory_iterator it(dir, ec);
        if (ec) {
          continue;
        }

        for (const auto &entry : it) {
          std::string filepath = entry.path();
          std::string filename = entry.path().filename().string();
          if (filename.rfind(currentWord, 0) == 0) {
            if (entry.is_regular_file() &&
                access(filepath.c_str(), X_OK) == 0) {
              if (std::find(matches.begin(), matches.end(), filename) ==
                  matches.end()) {
                matches.push_back(filename);
              }
            }
          }
        }
      }
    }
  } else {
    size_t slash_pos = currentWord.find_last_of('/');
    std::string path = (slash_pos != std::string::npos)
                           ? currentWord.substr(0, slash_pos + 1)
                           : ".";
    std::string prefix = (slash_pos != std::string::npos)
                             ? currentWord.substr(slash_pos + 1)
                             : currentWord;
    std::error_code ec;
    std::filesystem::directory_iterator it(path, ec);
    if (ec) {
      return;
    }
    for (const auto &entry : it) {
      std::string filename = entry.path().filename().string();
      if (filename.rfind(prefix, 0) == 0) {
        std::string candidate = (path == ".") ? filename : path + filename;
        if (std::find(matches.begin(), matches.end(), candidate) ==
            matches.end()) {
          matches.push_back(candidate);
        }
      }
    }
  }

  std::sort(matches.begin(), matches.end());

  if (matches.empty()) {
    std::cout << "\x07" << std::flush;
    return;
  }

  std::string lcp = matches[0];

  for (size_t i = 1; i < matches.size(); ++i) {
    size_t j = 0;
    while (j < lcp.size() && j < matches[i].size() && lcp[j] == matches[i][j]) {
      ++j;
    }
    lcp.resize(j);
  }

  if (lcp.size() > currentWord.size()) {
    std::string cmp = lcp.substr(currentWord.size());
    line += cmp;
    std::cout << cmp << std::flush;
    return;
  }

  if (matches.size() == 1) {
    std::string cmp = matches[0].substr(currentWord.size());
    line += cmp;
    line += ' ';
    std::cout << cmp << ' ' << std::flush;
    return;
  }

  if (doubleTab) {
    std::cout << "\r\n";
    for (size_t i = 0; i < matches.size(); ++i) {
      std::cout << matches[i];
      if (i + 1 < matches.size()) {
        std::cout << "  ";
      }
    }
    std::cout << "\r\n$ " << line << std::flush;
  } else {
    std::cout << "\x07" << std::flush;
  }
}

std::optional<std::string> Shell::readlineWithTab() {
  std::string line;
  char c;
  historyPos = history.size();

  while (read(STDIN_FILENO, &c, 1) == 1) {
    if (c == '\n' || c == '\r') {
      prevTab = false;
      std::cout << std::endl;
      return line;
    } else if (c == '\x1b') {
      char seq[2];
      if (read(STDIN_FILENO, &seq[0], 1) != 1 ||
          read(STDIN_FILENO, &seq[1], 1) != 1) {
        continue;
      }
      if (seq[0] == '[' && seq[1] == 'A') {
        historyUp(line);
      } else if (seq[0] == '[' && seq[1] == 'B') {
        historyDown(line);
      }
    } else if (c == '\t') {
      bool doubleTab = prevTab && (line == lastTabLine);
      handleTab(line, doubleTab);
      prevTab = true;
      lastTabLine = line;
    } else if (c == '\x7f' || c == '\x08') {
      prevTab = false;
      if (!line.empty()) {
        line.pop_back();
        std::cout << "\b \b" << std::flush;
      }
    } else if (c == 0x03) { // Ctrl-C
      prevTab = false;
      if (foregroundPgid != -1) {
        kill(-foregroundPgid, SIGINT);
      } else {
        std::cout << "^C" << std::endl;
        line.clear();
        std::cout << "$ " << std::flush;
      }
    } else if (c == 0x1a) { // Ctrl-Z
      prevTab = false;
      if (foregroundPgid != -1) {
        kill(-foregroundPgid, SIGTSTP);
      }
    } else {
      prevTab = false;
      line += c;
      std::cout << c << std::flush;
    }
  }
  return std::nullopt;
}

void Shell::historyUp(std::string &line) {
  if (history.empty())
    return;
  prevTab = false;
  if (historyPos == history.size()) {
    savedLine = line;
    historyPos = history.size() - 1;
  } else if (historyPos > 0) {
    historyPos--;
  } else {
    return;
  }
  line = history[historyPos];
  redrawLine(line);
}

void Shell::historyDown(std::string &line) {
  if (historyPos >= history.size())
    return;
  prevTab = false;
  historyPos++;
  line = (historyPos == history.size()) ? savedLine : history[historyPos];
  redrawLine(line);
}

void Shell::redrawLine(const std::string &line) {
  std::cout << "\r\x1b[K$ " << line << std::flush;
}
