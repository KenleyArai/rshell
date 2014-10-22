//C++ libs
#include <iostream>
#include <string>
#include <queue>
#include <vector>
#include <algorithm>

//C libs
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
bool run_command(string &input, char &conn);
void tokenize(vector<char*> &comms, string &input);
void trim_lead_and_trail(string &str);


int main()
{
  string input;
  char logic;
  queue<string> commands;
  queue<char> connectors;
  bool running = true;

  char *hostname = new char[20];

  if(gethostname(hostname, 20) == -1)
  {
    perror("Error with getting hostname!");
  }

  //Continue prompting
  while(1)
  {
    cout << getlogin() << "@" << hostname << "$ ";
    
    getline(cin, input);
    splice_input(commands, connectors, input);

    //After getting input from the user begin dequeing 
    //until the queue of commands is empty or logic
    //returns to stop running
    while(!commands.empty() && running)
    {
      //Get command from queue
      input = commands.front();
      commands.pop();
      //Get connector from queue
      if(!connectors.empty())
      {
        logic = connectors.front();
        connectors.pop();
      }

      //Check if input is exit
      //And begin handling command
      if(input.find("exit") == string::npos)
        running = run_command(input, logic);
      else
        exit(1);
      
      //Clear queues
      if(!running)
      {
        connectors = queue<char>();
        commands = queue<string>();
      }
    }
    //Reset running
    running = true;
  }

  return 0;
}

//Splits the commands and connectors into seperate queues.
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

    trim_lead_and_trail(new_cmd);

    if(logic == '&' || logic == '|')
    {
      if(parse[pos + 1] == logic)
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
  int pid = fork();
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

    //Deallocating memory
    for( size_t i = 0; i < tokens.size(); i++  )
      delete tokens[i];

    //Cleaning up vector
    tokens.clear();

    //Checking if the connector was AND
    if(conn == '&')
    {
      //If the previous cmd failed stop running
      if(status > 0)
        return false;
    }

    //Checking if the connector was OR
    if(conn == '|')
    {
      //No need to continue running since first cmd was true
      if(status <= 0)
        return false;
    }
  }

  //Continue getting commands
  return true;
}

void tokenize(vector<char*> &comms, string &input)
{
  string convert;
  string tokenizer = input;
  size_t pos = 0;
 
  trim_lead_and_trail(tokenizer);

  while(pos != string::npos )
  {
    pos = tokenizer.find(' ');
    convert = pos == string::npos ? tokenizer \
                                  : tokenizer.substr(0, pos);

    trim_lead_and_trail(convert);

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

void trim_lead_and_trail(string &str)
{
  str.erase(str.begin(), std::find_if(str.begin(), str.end(), std::bind1st(std::not_equal_to<char>(), ' ')));
  str.erase(std::find_if(str.rbegin(), str.rend(), std::bind1st(std::not_equal_to<char>(), ' ')).base(), str.end());
}

