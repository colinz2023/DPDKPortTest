#include "access_cmdline.h"
using namespace std;

static std::string& cmdRTrim(std::string& s) {
  return s.erase(s.find_last_not_of(" \t\n\r") + 1);
}

CmdLine::CmdLine() {
  in_fd = STDIN_FILENO;
  out_fd = STDOUT_FILENO;
  prompt = string("> ");
  cmd_buffer = "";
  //光标位置
  m_index = 0;
  m_historyFile = ".cmd_history";
  init();
}

CmdLine::CmdLine(const char* prompt_cstr, const char* history_file) {
  in_fd = STDIN_FILENO;
  out_fd = STDOUT_FILENO;
  prompt = string(prompt_cstr);
  cmd_buffer = "";
  m_index = 0;
  m_historyFile = std::string(history_file);
  init();
  // int flags = fcntl(in_fd, F_GETFL, 0);
  // fcntl(in_fd, F_SETFL, flags | O_NONBLOCK);
}

CmdLine::~CmdLine() {
  tcsetattr(in_fd, TCSANOW, &old_tty_atrr);
  writeBackHistory();
}

void CmdLine::setPrompt(std::string& prompt) { this->prompt = prompt; }

void CmdLine::addHints(std::vector<std::string>& cmds) {
  for (vector<string>::iterator c = cmds.begin(); c != cmds.end(); c++) {
    this->addHints(*c);
  }
}

void CmdLine::addHints(const char* str) {
  std::string cmd(str);
  addHints(cmd);
}

void CmdLine::addHints(string& hint) {
  std::vector<string>::iterator it;
  for (it = hints.begin(); it != hints.end(); it++) {
    if (*it == hint) {
      break;
    }
  }
  if (it == hints.end()) {
    hints.push_back(hint);
  }
  // hints.insert(hint);
}

//光标左移n个字节
void CmdLine::Left(int n) {
  char* b = new char[n + 8];
  memset(b, '\b', n);
  write(out_fd, b, n);
  delete[] b;
}

//输出n个空格
void CmdLine::Space(int n) {
  char* s = new char[n + 8];
  memset(s, ' ', n);
  write(out_fd, s, n);
  delete[] s;
}

//清除当前位置左边的N个字节
void CmdLine::backSpace(int n) {
  Left(n);
  Space(n);
  Left(n);
}

//清除当前行
void CmdLine::clearLine() {
  int r = cmd.size() - m_index;
  Space(r);
  Left(r);
  backSpace(m_index);
}

void CmdLine::writeLine(string& buf) { write(out_fd, buf.c_str(), buf.size()); }

void CmdLine::writeLine(const char* buf) {
  write(out_fd, buf, strlen(buf));
  // fsync(out_fd);
}

void CmdLine::writeCmd() { writeLine(cmd); }

void CmdLine::readHistory() {
  char buf[1024] = {0};
  std::vector<std::string> tmp;
  std::vector<std::string>::reverse_iterator rit;
  FILE* f = fopen(m_historyFile.c_str(), "r");
  if (f != NULL) {
    while (NULL != fgets(buf, sizeof(buf), f)) {
      if (strlen(buf) > 0) {
        std::string tmpStr = std::string(buf);
        cmdRTrim(tmpStr);
        tmp.push_back(tmpStr);
      }
    }
    fclose(f);

    int i = 0;
    for (rit = tmp.rbegin(); rit != tmp.rend(); rit++, i++) {
      history.push_back(*rit);
      if (i > 500) break;
    }

  } else {
    history.push_back("help");
  }
}

void CmdLine::writeBackHistory() {
  FILE* f = fopen(m_historyFile.c_str(), "wb");
  std::vector<std::string>::reverse_iterator rit;
  int i = 0;
  if (NULL != f) {
    for (rit = history.rbegin(); rit != history.rend(); rit++, i++) {
      fprintf(f, "%s\n", rit->c_str());
      if (i > 500) break;
    }
    fflush(f);
    fclose(f);
  }
}

void CmdLine::init() {
  struct termios tty_attr;
  tcgetattr(in_fd, &tty_attr);
  old_tty_atrr = tty_attr;
  tty_attr.c_lflag &= (~(ICANON | ECHO));
  tty_attr.c_cc[VTIME] = 0;
  tty_attr.c_cc[VMIN] = 1;
  tcsetattr(in_fd, TCSANOW, &tty_attr);
  writeLine(prompt);
  readHistory();
  curr = history.end();
}

// 命令匹配, hints为匹配列表, 支持以空格间隔的二级命令
//
// @possible,   匹配到的命令(匹配到多个的情况)
// @append_str, 补全的命令(追加到当前命令后)
// 返回值: 匹配到的个数
int CmdLine::compareHints(std::string* possible, std::string* append_str) {
  size_t len_min = 65535;
  std::vector<string> common;
  std::vector<string>::iterator it;
  size_t i = 0;
  int will_sub_cmd = 0;
  int have_space = 0;
  std::string::size_type n;

  if (cmd.length() == 0) {
    return 0;
  }

  n = cmd.find(' ');
  if (std::string::npos != n) {
    //补全二级命令
    will_sub_cmd = 1;
  }

  //匹配命令
  possible->clear();
  for (it = hints.begin(); it != hints.end(); it++) {
    n = it->find(' ');
    if (std::string::npos != n) {
      have_space = 1;
    }
    if (it->length() > cmd.length()) {
      if (0 == it->compare(0, cmd.length(), cmd)) {
        if (will_sub_cmd || (!will_sub_cmd && !have_space)) {
          common.push_back(*it);
          possible->append(*it);
          possible->append(" ");
          i++;
          if (i % 4 == 0) {
            possible->append("\n");
          }
          if (it->length() < len_min) {
            len_min = it->length();
          }
        }
      }
    }
  }

  if (common.size() < 2) {
    /*        if (common.size() == 1) {
                *append_str = *possible;
                append_str->erase(0, cmd.size() - 1);
            }*/
    return common.size();
  }

  if (will_sub_cmd) {
    std::string main_cmd = cmd.substr(0, cmd.find(' '));
    for (size_t ii = 0; ii < i; ii++) {
      int n = possible->find(main_cmd.c_str());
      possible->erase(n, main_cmd.length());
    }
  }

  //多匹配的情况, 计算补全字符
  string common_str("");
  char x = 0, xx = 0;
  for (size_t i = cmd.length(); i < len_min; i++) {
    it = common.begin();
    x = (*it)[i];
    for (; it != common.end(); it++) {
      xx = (*it)[i];
      if (xx != x) {
        x = 0;
        break;
      }
    }
    if (x) {
      common_str.append(1, x);
    } else {
      break;
    }
  }
  append_str->append(common_str);
  return common.size();
}

string CmdLine::readLine() {
  string res("");
  int len = read(in_fd, &c, 1);
  if (len != 1) {
    return res;
  }

  if (c == '\n') {
    //回车键
    write(out_fd, &c, 1);
    cmd.erase(0, cmd.find_first_not_of(" "));
    cmd.erase(cmd.find_last_not_of(" \t\n\r") + 1);
    if (cmd.empty()) {
      writeLine(prompt);
    } else if (*(history.end() - 1) != cmd) {
      if (cmd != "exit") history.push_back(cmd);
      curr = history.end();
    } else {
      curr = history.end();
    }
    res = string(cmd);
    cmd.clear();
    cmd_buffer.clear();
    m_index = 0;
  } else if (c == '\t') {
    // TAB键
    int count = 0;
    string possible_cmd, append_str;
    possible_cmd.clear();
    append_str.clear();
    count = compareHints(&possible_cmd, &append_str);
    if (count > 1 && last_key == c) {
      possible_cmd.insert(0, "\n");
      possible_cmd.append("\n");
      writeLine(possible_cmd);
      writeLine(prompt);
      cmd.append(append_str);
      m_index = cmd.size();
      writeLine(cmd);
      // cmd.clear();
    } else if (1 == count) {
      clearLine();
      writeLine(possible_cmd);
      cmd = possible_cmd;
      m_index = cmd.size();
      last_key = 0;
    } else {
      // last_key = 0;
    }
  } else if (c == 127 || c == 8) {
    //退格键
    if (m_index > 0) {
      clearLine();
      std::string last_part = cmd.substr(m_index);
      m_index--;
      cmd.erase(m_index, 1);
      writeLine(cmd);
      Left(last_part.size());
    }
  } else if (c == 0x1B) {
    char b[2];
    len = read(in_fd, b, sizeof(b));
    if (b[0] == 0x5B && b[1] == 0x41 && history.size() > 0) {
      //上光标键
      if (curr == history.begin()) {
      } else {
        curr--;
        if (*curr == cmd && curr != history.begin()) {
          curr--;
        }
      }
      clearLine();
      writeLine(*curr);
      cmd = string(*curr);
      m_index = cmd.size();
    } else if (b[0] == 0x5B && b[1] == 0x42 && history.size() > 0) {
      //下光标键
      if (curr == history.end()) {
      } else if (curr == history.end() - 1) {
        if (cmd_buffer.size() != 0) {
          clearLine();
          writeLine(cmd_buffer);
          cmd = cmd_buffer;
          m_index = cmd.size();
        }
        curr++;
      } else {
        curr++;
        clearLine();
        writeLine(*curr);
        cmd = string(*curr);
        m_index = cmd.size();
      }
    } else if (b[0] == 0x5B && b[1] == 0x44 && m_index > 0) {
      //左光标键
      char b = '\b';
      write(out_fd, &b, 1);
      m_index--;
    } else if (b[0] == 0x5B && b[1] == 0x43 && m_index < cmd.size()) {
      //右光标键
      char b = cmd[m_index];
      write(out_fd, &b, 1);
      m_index++;
    } else {
      // printf("\n%02x, %02x", b[0], b[1]);
      // fflush(stdout);
    }

  } else if (isprint(c)) {
    /*        insert
            write(out_fd, &c, 1);
            cmd.push_back(c);
            cmd_buffer = cmd;
            m_index++;*/
    clearLine();
    std::string last_part = cmd.substr(m_index);
    cmd.insert(m_index, 1, c);
    cmd_buffer = cmd;
    writeLine(cmd);
    Left(last_part.size());
    m_index++;
  } else if (c == 0x0c) {
    // Ctrl + l
    // clear screen
    writeLine("\033[2J\033[0;0H");
    writeLine(prompt);
    writeLine(cmd);
  } else if (c == 0x15) {
    // Ctrl + u
    std::string sub_str = cmd.substr(m_index);
    clearLine();
    cmd = sub_str;
    cmd_buffer = sub_str;
    writeLine(sub_str);
    Left(sub_str.size());
    m_index = 0;
  } else if (c == 0x01) {
    // Ctrl + a
    Left(m_index);
    m_index = 0;
  } else if (c == 0x05) {
    // Ctrl + e
    std::string sub_str = cmd.substr(m_index);
    m_index = cmd.size();
    writeLine(sub_str);
  } else if (c == 0x0b) {
    // Ctrl + k
    std::string sub_str = cmd.substr(0, m_index);
    clearLine();
    cmd = sub_str;
    cmd_buffer = sub_str;
    writeLine(sub_str);
    m_index = cmd.size();
  } else if (c == 0x02) {
    // Ctrl + b
    if (m_index > 0) {
      char b = '\b';
      write(out_fd, &b, 1);
      m_index--;
    }
  } else if (c == 0x06) {
    // Ctrl + f
    if (m_index < cmd.size()) {
      char b = cmd[m_index];
      write(out_fd, &b, 1);
      m_index++;
    }
  } else if (c == 0x04) {
    // Ctrl + d
  } else if (c == 0x19) {
    // Ctrl + y
  } else if (c == 0x17) {
    // Ctrl +  w
  } else {
    // just for fun, do not support
    // printf("\n%02x", c);
    // fflush(stdout);
  }
  last_key = c;
  return res;
}
