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

enum CONNECTOR {NEWLINE, S_COLON, AND, OR};

const string cmd_delimiter = ";|&#";

struct CmdAndConn
{
    string cmd;
    CONNECTOR conn;
    CmdAndConn() : cmd("") {}
    CmdAndConn(string c, CONNECTOR co)
    {
        cmd = c;
        conn = co;
    }
};

bool run_command(CmdAndConn &rc);
void tokenize(vector<char*> &comms, string &input);
void trim_lead_and_trail(string &str);
void splice_input(queue<CmdAndConn> &commands, const string &input);

int main(int argc, char *argv[])
{
    if(argv[1][0] == '-' && argv[1][1] == 'c')
    {
        CmdAndConn c(argv[2], NEWLINE);
        run_command(c);
        return 0;
    }
    string input;
    CmdAndConn running_command;
    queue<CmdAndConn> commands;
    bool running = true;

    char *hostname = new char[20];
    char *username = new char[20];

    username = getlogin();

    if(!username)
        perror("Error getting username!");

    if(gethostname(hostname, 20) == -1)
        perror("Error with getting hostname!");

    //Continue prompting
    while(1)
    {
        cout << username << "@" << hostname << "$ ";
    
        getline(cin, input);
        splice_input(commands, input);

        //After getting input from the user begin dequeing 
        //until the queue of commands is empty or logic
        //returns to stop running
        while(!commands.empty() && running)
        {
            //Get command from queue
            running_command = commands.front();
            commands.pop();

            //Check if input is exit
            //And begin handling command
            if(input.find("exit") == string::npos)
                running = run_command(running_command);
            else
                exit(1);
      
            //Clear queues
            if(!running)
            {
                commands = queue<CmdAndConn>();
            }
        }
        //Reset running
        running = true;
    }

    return 0;
}

//Splits the commands and connectors into seperate queues.
void splice_input(queue<CmdAndConn> &cmds, const string &input)
{
    size_t pos = 0;
    string new_cmd;
    string parse = input;

    while(!parse.empty())
    {
        pos = parse.find_first_of(cmd_delimiter);

        if(parse[pos] == parse[pos + 1])
        {
            cmds.push(CmdAndConn(pos == string::npos ? parse : parse.substr(0,pos), \
                                  parse[pos] == '&' ? AND : OR));
            parse.erase(0, pos + 2);
        }
        else
        {
            cmds.push(CmdAndConn(pos == string::npos ? parse : parse.substr(0, pos) , \
                                 parse[pos] == ';' ? S_COLON : NEWLINE));
            parse.erase(0, pos == string::npos ? string::npos : pos + 1);
        }
    }
}

bool run_command(CmdAndConn &rc)
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
        tokenize(tokens, rc.cmd);
        execvp(&tokens[0][0], &tokens[0]);
        perror("Execvp failed!");
        exit(1);
    }
    else
    {
        wait(&status);

        //Deallocating memory
        for( size_t i = 0; i < tokens.size(); i++  )
            delete [] tokens[i];

        //Cleaning up vector
        tokens.clear();

        //Checking if the connector was AND
        if(rc.conn == AND && status > 0)
            return false;

        //Checking if the connector was OR
        if(rc.conn == OR && status <= 0)
            return false;
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
            comms.push_back(strdup(convert.c_str()));
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

