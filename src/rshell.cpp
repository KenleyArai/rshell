//C++ libs
#include <iostream>
#include <string>
#include <queue>
#include <vector>
#include <algorithm>
#include <regex>
#include <map>

//C libs
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

using namespace std;

enum CONNECTOR {NEWLINE, COMMENT, S_COLON, AND, OR, PIPE, REDIRECT_INPUT, REDIRECT_OUT, REDIRECT_OUT_APPEND};

#define STDIN 0
#define STDOUT 1

#define IS_DOUBLE_CHAR(X) X == AND || X == PIPE || X == OR || X == REDIRECT_OUT_APPEND
#define IS_R_I(X) X == REDIRECT_INPUT
#define IS_R_O(X) X == REDIRECT_OUT
#define IS_R_OA(X) X == REDIRECT_OUT_APPEND
#define IS_REDIRECTION(X) IS_R_I(X) || IS_R_O(X) || IS_R_OA(X)
#define OUTPUT_ONLY (O_CREAT | O_WRONLY | O_TRUNC)
#define OUTPUT_APND (O_CREAT | O_WRONLY | O_APPEND)
#define OPEN_MODE(X) IS_R_O(X) ? OUTPUT_ONLY : IS_R_OA(X) ? OUTPUT_APND : O_RDONLY 
#define STDIN_OR_STDOUT(X) IS_R_I(X) ? 0 : 1
#define IS_PIPE(X) X == PIPE

struct CmdAndConn
{
    vector<string> cmd;
    CONNECTOR conn;
    CmdAndConn(){};
    CmdAndConn(const vector<string> &c, const CONNECTOR &co)
    {
        cmd = c;
        conn = co;
    }
};

bool run_command(const CmdAndConn &rc);
void tokenize(vector<char*> &comms, const vector<string> &input);
void trim_lead_and_trail(string &str);
void splice_input(queue<CmdAndConn> &commands, const string &input);
map<int, CONNECTOR> get_all_conns(const string &input);
queue<vector<string>> get_all_cmds(const string &input, const map<int, CONNECTOR> &all_conns);
vector<int> get_all_conns_pos(const string &input);
vector<int> find_conn_pairs(const vector<int> &all_conns);
void delete_conn_pairs(vector<int> &all_conns);
bool run_chained(queue<CmdAndConn> &chained);
void set_io(vector<int> &cur_io, vector<vector<int>> &piped_io, const vector<int> &saved_io, const CmdAndConn &cur_cmd, const CmdAndConn &next_cmd);
void set_input(vector<int> &cur_io, vector<int> &piped_io, const vector<int> &saved_io, const CmdAndConn &cur_cmd, const CmdAndConn &next_cmd);
void set_output(vector<int> &cur_io, vector<int> &piped_io, const vector<int> &saved_io, const CmdAndConn &cur_cmd, const CmdAndConn &next_cmd);

int main()
{
    string input;
    CmdAndConn running_command;
    queue<CmdAndConn> commands;
    bool running = true;

    char *hostname = new char[20];
    char *username = new char[20];

    if( !(username = getlogin()) )
        perror("Error getting username!");

    if(gethostname(hostname, 20) == -1)
        perror("Error with getting hostname!");

    //Continue prompting
    while(1)
    {
        cout << username << "@" << hostname << "$ ";
    
        getline(cin, input);
        splice_input(commands, input);

/*        After getting input from the user begin dequeing */
        //until the queue of commands is empty or logic
        /*returns to stop running*/
        while(!commands.empty() && running)
        {
            //Get command from queue
            running_command = commands.front();
            commands.pop();

            //Check if input is exit
            //And begin handling command
            if(input.find("exit") != string::npos)
                exit(1);
            if(IS_PIPE(running_command.conn) || IS_REDIRECTION(running_command.conn))
            {   
                queue<CmdAndConn> chained_piped_commands;
                chained_piped_commands.push(running_command);

                while(IS_PIPE(commands.front().conn) || IS_REDIRECTION(commands.front().conn))
                {
                    chained_piped_commands.push(commands.front());
                    commands.pop();
                }
                
                chained_piped_commands.push(commands.front());
                commands.pop();

                running = run_chained(chained_piped_commands);
            }
            else
                running = run_command(running_command);
      
            //Clear queues
            if(!running)
                commands = queue<CmdAndConn>();
        }
        //Reset running
        running = true;
    }

    return 0;
}

//Splits the commands and connectors into seperate queues.
void splice_input(queue<CmdAndConn> &cmds, const string &input)
{
    map<int, CONNECTOR> all_conns = get_all_conns(input);
    queue<vector<string>> all_cmds = get_all_cmds(input, all_conns);

    for(auto it : all_conns)
    {
        vector<string> tmp = all_cmds.front();
        
        all_cmds.pop();
        cmds.push(CmdAndConn(tmp, it.second));
    }
}

bool run_command(const CmdAndConn &rc)
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
        
        if(rc.conn == COMMENT)
            return false;

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

void tokenize(vector<char*> &comms, const vector<string> &input)
{
    for( auto it : input )
        comms.push_back(strdup(it.c_str()));
    comms.push_back(NULL);

    return;
}

void trim_lead_and_trail(string &str)
{
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), std::bind1st(std::not_equal_to<char>(), ' ')));
    str.erase(std::find_if(str.rbegin(), str.rend(), std::bind1st(std::not_equal_to<char>(), ' ')).base(), str.end());
}

map<int, CONNECTOR> get_all_conns(const string &input)
{
    map<int, CONNECTOR>conns_with_pos;
    vector<int> conn_pos = get_all_conns_pos(input);
    
    for(auto it : find_conn_pairs(conn_pos))
    {
        if(input[it] == '&')
            conns_with_pos[it] = AND;
        else if(input[it] == '|')
            conns_with_pos[it] = OR;
        else if(input[it] == '>')
            conns_with_pos[it] = REDIRECT_OUT_APPEND;   
    }
    
    delete_conn_pairs(conn_pos);

    for(auto it : conn_pos)
    {
        if(input[it] == '#')
            conns_with_pos[it] = COMMENT;
        else if(input[it] == '|')
            conns_with_pos[it] = PIPE;
        else if(input[it] == '>')
            conns_with_pos[it] = REDIRECT_OUT;
        else if(input[it] == '<')
            conns_with_pos[it] = REDIRECT_INPUT;
        else if(input[it] == ';')
            conns_with_pos[it] = S_COLON;
        else if(input[it] == '\n')
            conns_with_pos[it] = NEWLINE;
    }
    conns_with_pos[input.length()] = NEWLINE;
    return conns_with_pos;
}

queue< vector<string> > get_all_cmds(const string &input, const map<int, CONNECTOR> &all_conns)
{
    queue<vector<string>> ret;
    int prev = 0;
    string str;

    for(auto it : all_conns)
    {
        string tmp;
        vector<string> v_str;
        str = input.substr(prev, it.first - prev);
        auto new_end = unique(str.begin(), str.end(), [](const char &lhs, const char &rhs) -> bool{ return (lhs == rhs) && lhs == ' '; });
        str.erase(new_end, str.end());
        trim_lead_and_trail(str);
        
        for(stringstream s(str); s >> tmp; )
            v_str.push_back(tmp);
        
        ret.push(v_str);

        if(IS_DOUBLE_CHAR(it.second))
            prev = it.first + 2;
        else
            prev = it.first + 1;
                
    }

    return ret;
}

vector<int> get_all_conns_pos(const string &input)
{
    vector<int> ret;
    vector<string> conns{"&", "|", ">", "<", ";", "#", "\n"};

    for(auto it : conns)
    {
        size_t pos = 0;
        while(string::npos != (pos = input.find(it,pos)))
        {
            ret.push_back(pos);
            ++pos;
        }
    }
    
    sort(ret.begin(), ret.end());
    ret.erase(unique(ret.begin(), ret.end()), ret.end());

    return ret;
}

vector<int> find_conn_pairs(const vector<int> &all_conns)
{
    vector<int> ret;
    int prev = -1;

    for(auto it : all_conns)
    {
        if((it - prev) == 1)
            ret.push_back(it - 1);
        prev = it;
    }
    return ret;
}

void delete_conn_pairs(vector<int> &all_conns)
{
    vector<int> all_pairs = find_conn_pairs(all_conns);
    
    for(auto & it : all_pairs)
    {
        auto itt = find(all_conns.begin(), all_conns.end(), it);

        if(itt != all_conns.end())
            all_conns.erase(itt);
        
        itt = find(all_conns.begin(), all_conns.end(), it +  1);

        if(itt != all_conns.end())
            all_conns.erase(itt);
    }

}

bool run_chained(queue <CmdAndConn> &chained)
{
    vector<int> saved_io{0,0};
    vector<int> current_io{0,0};
    vector<int> tmp{-1,-1};
    vector<vector<int>> piped_io;
    bool ret = true;
    
    piped_io.push_back(tmp);
    piped_io.push_back(tmp);

    if((saved_io.front() = dup(0)) == -1)
        perror("Error saving stdin");
    if((saved_io.back() = dup(1)) == -1)
        perror("Error saving stdout");
    
    current_io.front() = saved_io.front();
    current_io.back() = saved_io.back();
    CmdAndConn file;
    file.conn = NEWLINE;
    CmdAndConn curr_cmd = chained.front();
    chained.pop();

    while(!chained.empty() || curr_cmd.conn == NEWLINE)
    {

        cerr << "saved front\t= " << saved_io.front() << endl \
             << "saved back\t= " << saved_io.back() << endl;
        cerr << "piped 0 front\t= " << piped_io[0].front() << endl \
             << "piped 0 back\t= " << piped_io[0].back() << endl;
        cerr << "piped 1 front\t= " << piped_io[1].front() << endl \
             << "piped 1 back\t= " << piped_io[1].back() << endl;
        cerr << "curr front\t= " << current_io.front() << endl \
             << "curr back\t= " << current_io.back() << endl<< endl;
        cerr << curr_cmd.cmd.front() << endl << endl;

        set_io(current_io, piped_io, saved_io, curr_cmd, chained.front());
        
        cerr << "saved front\t= " << saved_io.front() << endl \
             << "saved back\t= " << saved_io.back() << endl;
        cerr << "piped 0 front\t= " << piped_io[0].front() << endl \
             << "piped 0 back\t= " << piped_io[0].back() << endl;
        cerr << "piped 1 front\t= " << piped_io[1].front() << endl \
             << "piped 1 back\t= " << piped_io[1].back() << endl;
        cerr << "curr front\t= " << current_io.front() << endl \
             << "curr back\t= " << current_io.back() << endl<< endl;
        cerr << curr_cmd.cmd.front() << endl << endl;
        ret = run_command(curr_cmd);
        
        if(piped_io[0].back() != -1 && piped_io[0].front() != -1)
        {
            if(close(piped_io[0].front()) == -1)
                perror("error closing back pipe");
            piped_io.front().front() = -1;
        }
        else if(piped_io[1].back() != -1 && piped_io[1].front() != -1)
        {
            if(close(piped_io[1].front()) == -1)
                perror("error closing back pipe");
            piped_io.front().front() = -1;
        }
        if(curr_cmd.conn == NEWLINE)
            break;
        if(IS_REDIRECTION(curr_cmd.conn) && !chained.empty())
            chained.pop();
        if(!chained.empty())
        {
            curr_cmd = chained.front();
            chained.pop();
        }
    }

    current_io.front() = saved_io.front();
    if(dup2(saved_io.front(), STDIN) == -1)
        perror("error restoring stdin");
    current_io.back() = saved_io.back();
    if(dup2(current_io.back(), STDOUT) == -1)
        perror("error restoring stdout");
    cerr << "saved front\t= " << saved_io.front() << endl \
         << "saved back\t= " << saved_io.back() << endl;
    cerr << "piped 0 front\t= " << piped_io[0].front() << endl \
         << "piped 0 back\t= " << piped_io[0].back() << endl;
    cerr << "piped 1 front\t= " << piped_io[1].front() << endl \
         << "piped 1 back\t= " << piped_io[1].back() << endl;
    cerr << "curr front\t= " << current_io.front() << endl \
         << "curr back\t= " << current_io.back() << endl<< endl;

    return ret;
}

void set_io(vector<int> &cur_io, vector<vector<int>> &piped_io, const vector<int> &saved_io, const CmdAndConn &cur_cmd, const CmdAndConn &next_cmd)
{
    if(IS_PIPE(cur_cmd.conn) && !(IS_PIPE(next_cmd.conn)))
    {
        if(piped_io[0].front() == -1 && piped_io[0].back() == -1)
        {
            if(pipe(&piped_io[0][0]) == -1)
                perror("Error piping");
        }
        else if(piped_io[1].front() == -1 && piped_io[1].back() == -1)
        {
            if(pipe(&piped_io[1][0]) == -1)
                perror("Error piping");
        }
    }
    else if(IS_PIPE(cur_cmd.conn) && IS_PIPE(next_cmd.conn))
    {
        if(piped_io[0].front() == -1 && piped_io[0].back() == -1)
        {
            if(pipe(&piped_io[0][0]) == -1)
                perror("Error piping");
        }
        else if(piped_io[1].front() == -1 && piped_io[1].back() == -1)
        {
            if(pipe(&piped_io[1][0]) == -1)
                perror("Error piping");
        }
    }

    if(piped_io[0].back() != -1 && piped_io[0].front() == -1)
    {
        set_input(cur_io, piped_io[0], saved_io, cur_cmd, next_cmd);
        set_output(cur_io, piped_io[1], saved_io, cur_cmd, next_cmd);
    }
    else if(piped_io[1].back() != -1 && piped_io[1].front() != -1)
    {
        set_input(cur_io, piped_io[1], saved_io, cur_cmd, next_cmd);
        set_output(cur_io, piped_io[0], saved_io, cur_cmd, next_cmd);
    } 
    else if(piped_io[0].front() != -1 && piped_io[0].back() != -1)
    {
        set_input(cur_io, piped_io[0], saved_io, cur_cmd, next_cmd);
        set_output(cur_io, piped_io[0], saved_io, cur_cmd, next_cmd);
    }
    else if(piped_io[1].front() != -1 && piped_io[1].back() != -1)
    {
        set_input(cur_io, piped_io[1], saved_io, cur_cmd, next_cmd);
        set_output(cur_io, piped_io[1], saved_io, cur_cmd, next_cmd);
    }
}

void set_input(vector<int> &cur_io, vector<int> &piped_io, const vector<int> &saved_io, const CmdAndConn &cur_cmd, const CmdAndConn &next_cmd)
{
    if(IS_R_I(cur_cmd.conn))
    {
        if((cur_io.front() = open(next_cmd.cmd.front().c_str(), O_RDONLY)) == -1)
            perror("Error opening while setting input");
        if(dup2(cur_io.front(), STDIN) == -1)
            perror("Error dup2 while setting input");
    }
    else if(cur_io.back() == piped_io.back())
    {
        if(close(piped_io.back()) == -1)
            perror("error closing pipe");
        piped_io.back() = -1;

        cur_io.front() = piped_io.front();
        if(dup2(cur_io.front(), STDIN) == -1)
            perror("Error using dup2 with pipe on STDIN");
    }
    else
    {
        cur_io.front() = saved_io.front();
        if(dup2(cur_io.front(), STDIN) == -1)
                perror("Error using dup2");
    }
}

void set_output(vector<int> &cur_io, vector<int> &piped_io, const vector<int> &saved_io, const CmdAndConn &cur_cmd, const CmdAndConn &next_cmd)
{
    if(IS_R_O(cur_cmd.conn))
    {
        if(( cur_io.back() = open(next_cmd.cmd.front().c_str(),O_CREAT | O_WRONLY | O_TRUNC, 0666) ) == -1)
            perror("Error using open to write to file");
        if(dup2(cur_io.back(), STDOUT) == -1)
            perror("Error using dup2 to write to file");
    }
    else if(IS_R_OA(cur_cmd.conn))
    {
        if(( cur_io.back() = open(next_cmd.cmd.front().c_str(),O_CREAT | O_WRONLY | O_APPEND, 0666) ) == -1)
            perror("Error using open to append to file");
        if(dup2(cur_io.back(), STDOUT) == -1)
            perror("Error using dup2 to append to file");
    }
    else if(IS_PIPE(cur_cmd.conn) || IS_PIPE(next_cmd.conn))
    {
        
        cur_io.back() = piped_io.back();
        if(dup2(cur_io.back(), STDOUT) == -1)
            perror("Error using pipe to STDOUT");
    }
    else
    {
        cur_io.back() = saved_io.back();
        if(dup2(cur_io.back(), STDOUT) == -1)
            perror("error dup2");
    }
}







