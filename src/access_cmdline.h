#ifndef CMD_LINE_H
#define CMD_LINE_H
#include <unistd.h>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <string.h>
#include <string>
#include <vector>
#include <set>

class CmdLine {
 public:
  CmdLine();
  CmdLine(const char* prompt_cstr, const char* history_file);
  ~CmdLine();
  void init();
  std::string readLine();
  void Left(int n);
  void Space(int n);
  void backSpace(int n);
  void clearLine();
  void writeLine(std::string& buf);
  void writeLine(const char* buf);
  void writeCmd();
  void toNline(int n);
  void addHints(std::string& hint);
  void addHints(const char* str);
  void addHints(std::vector<std::string>& v);
  int compareHints(std::string* possible, std::string* append_str);
  void setPrompt(std::string& prompt);

 private:
  struct termios old_tty_atrr;

  void readHistory();
  void writeBackHistory();

  std::string m_historyFile;

  std::vector<std::string> history;
  std::vector<std::string>::iterator curr;
  // set<string, HintCompare> hints;
  std::vector<std::string> hints;
  std::string prompt;
  std::string cmd;
  std::string cmd_buffer;
  size_t m_index;
  char c;
  int in_fd;
  int out_fd;
  int last_key;
};

#endif
