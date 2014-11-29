//C++ libs
#include <iostream>
#include <string>
#include <vector>
#include <vector>
#include <algorithm>
#include <regex>
#include <map>
#include <csignal>

//C libs
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>

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

void ctrl_c(int signum);
bool my_execvp(const vector<string> &cmds);
int get_exec_path(vector<string> &cmd);
bool file_exists(const string &path);
string concat_filenames(const string &path, const string &file);
bool file_exists(const string &path);
vector<string> get_paths();
string concat_cwd(const vector<string> &cwd);
vector<string> get_cwd();
vector<string> tok_env_var(const char *env_var, const char delim);
bool run_command(CmdAndConn &rc);
void tokenize(vector<char*> &comms, const vector<string> &input);
void trim_lead_and_trail(string &str);
void splice_input(vector<CmdAndConn> &commands, const string &input);
map<int, CONNECTOR> get_all_conns(const string &input);
vector< vector<string> > get_all_cmds(const string &input, const map<int, CONNECTOR> &all_conns);
vector<int> get_all_conns_pos(const string &input);
vector<int> find_conn_pairs(const vector<int> &all_conns);
void delete_conn_pairs(vector<int> &all_conns);
bool run_piped(vector <CmdAndConn> &cmds);
bool run_chained(vector<CmdAndConn> &chained);
void set_io(const vector<int> &saved_io, vector<CmdAndConn> &chained);
void set_input(const vector<int> &saved_io, vector<CmdAndConn> &chained);
void set_output(const vector<int> &saved_io, vector<CmdAndConn> &chained);

int main()
{
    signal(SIGINT, SIG_IGN);
    string input;
    vector<CmdAndConn> commands;
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
        cout << username << "@" << hostname << ":"  << concat_cwd(get_cwd()) << "$ ";

        getline(cin, input);
        splice_input(commands, input);

        //After getting input from the user begin dequeing
        //until the vector of commands is empty or logic
        //returns to stop running

        while(!commands.empty() && running)
        {
            //Check if input is exit
            //And begin handling command
            if(input.find("exit") != string::npos)
                exit(1);
            if(IS_PIPE(commands.front().conn) || IS_REDIRECTION(commands.front().conn))
            {
                vector<CmdAndConn> chained_cmds;
                while(IS_PIPE(commands.front().conn) || IS_REDIRECTION(commands.front().conn))
                {
                    chained_cmds.push_back(commands.front());
                    commands.erase(commands.begin());
                }

                chained_cmds.push_back(commands.front());
                commands.erase(commands.begin());

                running = run_chained(chained_cmds);
            }
            else
            {
                running = run_command(commands.back());
                commands.pop_back();
            }
            //Clear vectors
            if(!running)
                commands.clear();
        }
        //Reset running
        running = true;
    }

    return 0;
}

void ctrl_c(int signum)
{
    kill(getpid(), SIGTERM);
}

bool my_execvp(const vector<string> &cmds)
{
    vector<char*> tokens;
    tokenize(tokens, cmds);
    bool ret = (execv(&tokens[0][0], &tokens[0]) != -1);

    if(!ret)
        perror("execv");

    for(auto &it : tokens)
        delete[] it;


    return ret;
}

int get_exec_path(vector<string> &cmd)
{
    for(const auto &it : get_paths())
    {
        string check = concat_filenames(it, cmd.front());
        if(file_exists(check))
        {
            cmd.front() = check;
            return 1;
        }
    }
    return -1;
}

string concat_filenames(const string &path, const string &file)
{
    string p(path);
    if(p.back() != '/')
        p += '/';
    p += file;
    return p;
}

bool file_exists(const string &path)
{
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

vector<string> get_paths()
{
    return tok_env_var(getenv("PATH"), ':');
}

string concat_cwd(const vector<string> &cwd)
{
    if(cwd.begin() + 1 == cwd.end())
        return "/" + cwd.front();
    vector<string> v(cwd.begin()+1, cwd.end());
    return "/" + cwd.front() + concat_cwd(v);
}

vector<string> get_cwd()
{
    return tok_env_var(get_current_dir_name(), '/');
}

vector<string> tok_env_var(const char *env_var, const char delim)
{
    vector<string> v;
    string tok;
    istringstream ss(env_var);

    while(getline(ss, tok, delim))
        if(tok.length() >= 1)
            v.push_back(tok);
    return v;
}

//Splits the commands and connectors into seperate vectors.
void splice_input(vector<CmdAndConn> &cmds, const string &input)
{
    map<int, CONNECTOR> all_conns = get_all_conns(input);
    vector<vector<string>> all_cmds = get_all_cmds(input, all_conns);

    for(const auto &it : all_conns)
    {
        vector<string> tmp = all_cmds.front();
        all_cmds.erase(all_cmds.begin());
        cmds.push_back(CmdAndConn(tmp, it.second));
    }
}

bool run_command(CmdAndConn &rc)
{
    if(rc.cmd.front() == "cd")
    {
        vector<string> full_path = get_cwd();
        full_path.push_back(rc.cmd.at(1));

        if(chdir(concat_cwd(full_path).c_str()) == -1)
            perror("Error chdir");

        return true;
    }

    int pid = fork();
    int status = 0;

    if(pid == -1)
    {
        perror("Error with fork()");
        exit(1);
    }
    else if(pid == 0)
    {
        signal(SIGINT, ctrl_c);
        if(get_exec_path(rc.cmd) && my_execvp(rc.cmd))
                perror("Execvp failed!");
        exit(1);
    }
    else
    {
        wait(&status);

        signal(SIGINT, SIG_IGN);

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

vector< vector<string> > get_all_cmds(const string &input, const map<int, CONNECTOR> &all_conns)
{
    vector<vector<string>> ret;
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

        ret.push_back(v_str);

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

bool run_piped(vector <CmdAndConn> &cmds)
{
    int pipes[2];
    if(pipe(pipes) == -1)
        perror("Error creating pipes");

    bool ret;
    cmds.erase(cmds.begin());

    int pid = fork();
    int status = 0;

    if(pid == -1)
    {
        perror("Error with fork()");
        exit(1);
    }
    else if(pid == 0)
    {
        if(dup2(pipes[1], 1) == -1)
            perror("Error writing to pipe");
        if(close(pipes[0]) == -1)
            perror("Error closing read end of pipe");
        if(get_exec_path(cmds.front().cmd) && my_execvp(cmds.front().cmd))
                perror("Execvp failed!!");
        exit(1);
    }
    else
    {
        if(wait(&status) == -1)
            perror("Error wait");

        if(-1 == dup2(pipes[0], 0))
            perror("Error dup2");
        if(close(pipes[1]))
            perror("Error dup2");

        if(IS_PIPE(cmds.front().conn))
            ret = run_piped(cmds);
    }

    //Continue getting commands
    return ret;
}

bool run_chained(vector <CmdAndConn> &chained)
{
    vector<int> saved_io{0,0};
    bool ret = true;

    if((saved_io.front() = dup(STDIN)) == -1)
        perror("Error saving stdin");
    if((saved_io.back() = dup(STDOUT)) == -1)
        perror("Error saving stdout");

    CmdAndConn file;
    file.conn = NEWLINE;

    while(!chained.empty())
    {
        set_io(saved_io, chained);

        if(IS_PIPE(chained.front().conn))
            ret = run_piped(chained);
        else if(IS_REDIRECTION(chained.front().conn))
        {
            if(chained.size() > 2 && IS_PIPE(chained.at(1).conn))
            {
                chained.erase(chained.begin() + 1);
                ret = run_piped(chained);
            }
            else if((chained.size() >= 2 ) && (IS_REDIRECTION(chained.at(1).conn) ))
            {
                chained.erase(chained.begin() + 1);
                chained.erase(chained.begin() + 1);
                ret = run_command(chained.front());
                chained.erase(chained.begin() + 1);

            }
            else if(( chained.size() > 1  ) && ( IS_REDIRECTION(chained.at(0).conn) ))
            {
                chained.erase(chained.begin() + 1);
                ret = run_command(chained.front());
                chained.erase(chained.begin() + 1);
            }
        }
        else
        {
            ret = run_command(chained.front());
            chained.erase(chained.begin());
        }
    }

    if(dup2(saved_io.front(), STDIN) == -1)
        perror("error restoring stdin");
    if(dup2(saved_io.back(), STDOUT) == -1)
        perror("error restoring stdout");

    return ret;
}

void set_io(const vector<int> &saved_io, vector<CmdAndConn> &chained)
{
    set_input(saved_io, chained);
    set_output(saved_io, chained);
}

void set_input(const vector<int> &saved_io, vector<CmdAndConn> &chained)
{
    if(IS_R_I(chained.front().conn))
    {
        int fd;
        if((fd = open(chained.at(1).cmd.front().c_str(), O_RDONLY)) == -1)
            perror("Error opening while setting input");
        if(dup2(fd, STDIN) == -1)
            perror("Error dup2 while setting input");
    }
}

void set_output(const vector<int> &saved_io, vector<CmdAndConn> &chained)
{
    if(IS_R_O(chained.front().conn))
    {
        int fd;
        if(( fd = open(chained.at(1).cmd.front().c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0666) ) == -1)
            perror("Error using open to write to file");
        if(dup2(fd, STDOUT) == -1)
            perror("Error using dup2 to write to file");
    }
    else if(chained.size() > 1 && IS_R_O(chained.at(1).conn))
    {
        int fd;
        if(( fd = open(chained.at(2).cmd.front().c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0666) ) == -1)
            perror("Error using open to write to file");
        if(dup2(fd, STDOUT) == -1)
            perror("Error using dup2 to write to file");
    }
    else if(IS_R_OA(chained.front().conn))
    {
        int fd;
        if(( fd = open(chained.at(1).cmd.front().c_str(), O_CREAT | O_WRONLY | O_APPEND, 0666) ) == -1)
            perror("Error using open to write to file");
        if(dup2(fd, STDOUT) == -1)
            perror("Error using dup2 to write to file");
   }
    else if(chained.size() > 1 && IS_R_OA(chained.at(1).conn))
    {
        int fd;
        if(( fd = open(chained.at(2).cmd.front().c_str(), O_CREAT | O_WRONLY | O_APPEND, 0666) ) == -1)
            perror("Error using open to write to file");
        if(dup2(fd, STDOUT) == -1)
            perror("Error using dup2 to write to file");
    }
    else
        if(dup2(saved_io.back(), STDOUT) == -1)
            perror("error dup2");
}
