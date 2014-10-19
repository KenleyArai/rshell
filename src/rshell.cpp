#include <iostream>
#include <string>
#include <queue>
#include <vector>
#include <algorithm>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

using namespace std;

const string cmd_delimiter = ";|&#";

void splice_input(queue<string> &cmds, queue<char> &conns, const string &input);
string get_input(const string &input, int &start, int &end);
int find_delimiter(const string &input, int &start);
bool run_command(string &input, char &conn);
void tokenize(vector<char*> &comms, string &input);

int main()
{
  string input;
  char logic;
  queue<string> commands;
  queue<char> connectors;
  bool running = true;

  while(1)
  {
    cout << "$ ";
    getline(cin, input);
    splice_input(commands, connectors, input);

    while(!commands.empty() && running)
    {
      input = commands.front();
      commands.pop();

      if(!connectors.empty())
      {
        logic = connectors.front();
        connectors.pop();
      }

      if(input.find("exit") == string::npos)
        running = run_command(input, logic);
      else
        exit(1);

      if(!running)
      {
        connectors = queue<char>();
        commands = queue<string>();
      }
    }
    running = true;

  }

  return 0;
}

void splice_input(queue<string> &cmds, queue<char> &conns, const string &input)
{
  int pos = 0;
  char logic;
  string new_cmd;
  string parse = input;

  while(pos != -1)
  {
    pos = parse.find_first_of(cmd_delimiter);
    new_cmd = parse.substr(0, pos);
    logic = parse[pos];

    while(new_cmd[0] == ' ')
      new_cmd = new_cmd.substr(1, new_cmd.length());

    if(logic == '&' || logic == '|')
    {
      cmds.push(new_cmd);
      parse.erase(0, pos + 2);
    }
    else if(logic == ';')
    {
      cmds.push(new_cmd);
      parse.erase(0, pos + 1);
    }
    else
      cmds.push(new_cmd);
    
    if(logic == '#')
      return;

    conns.push(logic);
  }

}

bool run_command(string &input, char &conn)
{
  pid_t pid = fork();
  vector<char*> tokens;
  int status = 0;

  if(pid == -1)
  {
    perror("Error with fork()");
    exit(1);
  }
  else if(pid == 0)
  {
    tokenize(tokens, input);
    char **cmds = &tokens[0];
    execvp(cmds[0], cmds);
    perror("Execvp failed!");
    exit(1);
  }
  else
  {
    wait(&status);

    tokens.clear();

    if(conn == '&')
    {
      if(status > 0)
        return false;
    }
  }

  return true;
}

void tokenize(vector<char*> &comms, string &input)
{
  string convert;
  string tokenizer = input;
  size_t pos = 0;

  while(pos != string::npos)
  {
    pos = tokenizer.find(' ');
    convert = pos == string::npos ? tokenizer \
                                  : tokenizer.substr(0, pos);
    convert.erase(std::remove_if(convert.begin(), convert.end(), ::isspace), convert.end());
    if(!convert.empty())
    {
      char *tmp = new char[convert.length() + 1];
      strcpy(tmp, convert.c_str());
      comms.push_back(tmp);
      tokenizer.erase(0, pos + 1);
    }
  }

  comms.push_back(NULL);

  return;
}


