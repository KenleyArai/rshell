#include <iostream>
#include <vector>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#include <sys/ioctl.h>
#include <stdio.h>
#include <iomanip>
#include <string.h>
#include <algorithm>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
using namespace std;

enum FLAGS {NO_FLAGS=1, REC=2, ALL_INFO=4, ALL_FILES=8};

#define IS_NO_FLAGS(F) (F & NO_FLAGS) ? 1 : 0
#define IS_REC(F) (F & REC) ? 1 : 0
#define IS_ALL_FILES(F) (F & ALL_FILES) ? 1 : 0
#define IS_ALL_INFO(F) (F & ALL_INFO) ? 1 : 0

#define IS_A_DIR(TYPE) (TYPE & DT_DIR) ? 1 : 0
#define IS_A_FILE(TYPE) (TYPE & DT_REG) ? 1 : 0
#define IS_A_LINK(TYPE) (TYPE & DT_LNK) ? 1 : 0

#define USER_R(MODE) (MODE & 0400) ? cout << 'r' : cout << '-';
#define USER_W(MODE) (MODE & 0200) ? cout << 'w' : cout << '-';
#define USER_X(MODE) (MODE & 0100) ? cout << 'x' : cout << '-';

#define GROUP_R(MODE) (MODE & 040) ? cout << 'r' : cout << '-';
#define GROUP_W(MODE) (MODE & 020) ? cout << 'w' : cout << '-';
#define GROUP_X(MODE) (MODE & 010) ? cout << 'x' : cout << '-';

#define ALL_R(MODE) (MODE & 04) ? cout << 'r' : cout << '-';
#define ALL_W(MODE) (MODE & 02) ? cout << 'w' : cout << '-';
#define ALL_X(MODE) (MODE & 01) ? cout << 'x' : cout << '-';

#define ALL_PERM(MODE) \
            ALL_R(MODE)\
            ALL_W(MODE)\
            ALL_X(MODE)

#define GROUP_PERM(MODE) \
            GROUP_R(MODE)\
            GROUP_W(MODE)\
            GROUP_X(MODE)

#define USER_PERM(MODE) \
            USER_R(MODE)\
            USER_W(MODE)\
            USER_X(MODE)

#define PRINT_ALL_PERM(MODE) \
            USER_PERM(MODE) \
            GROUP_PERM(MODE)\
            ALL_PERM(MODE)
            

FLAGS get_flags(const vector<string> &arg);
vector<string> get_cmds(const vector<string> &arg);
bool open_dir(const string &d, const FLAGS &flag);
int get_screen_width();
int get_longest_str(vector<dirent*> &dir);
void delete_dot_files(vector<dirent*> &all_di);
bool compare_words(dirent* lhs,dirent* rhs);
void sort_dir(vector<dirent*> &all_dir);
void print_all_info(dirent* dir, string target_dir);
double get_file_size(const double &file_size);
vector<dirent*> find_all_dir(const vector<dirent*> &all_dir);
bool any_dirs(vector<dirent*> &all_dir);

int main(int argc, char *argv[])
{
    vector<string> arguments(argv + 1, argv + argc);
    vector<string> commands = get_cmds(arguments);
    FLAGS all_flags = get_flags(arguments);
    
    if(commands.size() == 0)
        commands.insert(commands.begin(), string("./"));

    for(vector<string>::iterator i = commands.begin(); i != commands.end(); ++i)
        open_dir(*i, all_flags); 
    
    return 0;
}

FLAGS get_flags(const vector<string> &arg)
{   
    FLAGS flags = NO_FLAGS;
    for(size_t i = 0; i < arg.size(); i++)
    {
        if(arg[i][0] == '-')
        {
            for(size_t j = 0; j < arg[i].length(); j++)
            {
                switch(arg[i][j])
                {
                    case 'R':
                        flags = (FLAGS)(flags | REC);
                        break;
                    case 'a':
                        flags = (FLAGS)(flags | ALL_FILES);
                        break;
                    case 'l':
                        flags = (FLAGS)(flags | ALL_INFO);
                        break;
                }
            }
        }
    }
    return flags;       
}

vector<string> get_cmds(const vector<string> &arg)
{
    vector<string> cmds;

    for(size_t i = 0; i < arg.size(); i++)
        if(arg[i][0] != '-')
            cmds.push_back(arg[i]);

    return cmds; 
}

bool open_dir(const string &d, const FLAGS &flag)
{
    string dir = d;
    struct stat info;
    DIR *target_dir;

    if(stat(dir.c_str(), &info) == -1)
        perror("Error using stat");

    if(S_ISDIR(info.st_mode))
    {
        dir = dir[dir.length() - 1] == '/' ? dir : dir.append("/");
        target_dir = opendir(dir.c_str());
        if(!target_dir)
            perror(string("Error opening [" + dir + "]").c_str());
    }

    vector<dirent*> all_dir;
    dirent *direntp;
    int screen_width = get_screen_width();
    int new_screen_width = 0;
    int longest;
    
    while(S_ISDIR(info.st_mode) && (direntp = readdir(target_dir)))
        direntp ? all_dir.push_back(direntp) : perror("Error using readdir!");
    
    if(!(flag & ALL_FILES))
        delete_dot_files(all_dir);

    sort_dir(all_dir);

    longest = get_longest_str(all_dir);
    
    if(flag & REC && any_dirs(all_dir))
    {
        vector<dirent*> only_dir = find_all_dir(all_dir);
        for(vector<dirent*>::iterator it = only_dir.begin(); it != only_dir.end(); ++it)
        {
            string tmp((*it)->d_name);
            if(tmp != "." && tmp != "..")
            {     
                open_dir(dir + (*it)->d_name, flag);
                cout << endl;
            } 
        }
    }

    if(longest <= 0)
        longest = 1;
    while(new_screen_width < screen_width - 2*longest)
        new_screen_width += longest;

    int tmp = longest;

    if(flag & REC)
        cout << dir << '\n';

    if(S_ISREG(info.st_mode))
        cout << left << setw(longest) <<  dir << ' ';
    
    for(vector<dirent*>::iterator it = all_dir.begin(); it != all_dir.end(); ++it)
    {
       if(flag & ALL_INFO)
            print_all_info(*it, dir);
        else
        {
            cout <<  left << setw(longest);
            cout << (*it)->d_name << " ";
            if(tmp >= new_screen_width)
            {
                tmp = 0;
                cout << "\n";
            }
            tmp += longest;
        }
    }
    cout << endl;

    return 0;
}

int get_screen_width()
{
    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);
    return (int)w.ws_col;
}

int get_longest_str(vector<dirent*> &dir)
{
    int longest = 0;
     for(vector<dirent*>::iterator it = dir.begin(); it != dir.end(); ++it)
        if(longest < (int)strlen((*it)->d_name))
            longest = (int)strlen((*it)->d_name);
     return longest;
    
}

void delete_dot_files(vector<dirent*> &all_dir)
{
    for(vector<dirent*>::iterator it = all_dir.begin(); it != all_dir.end();)
        if(((*it)->d_name)[0] == '.')
            it = all_dir.erase(it);
        else
            it++;
}

bool compare_words(dirent* lhs, dirent* rhs)
{
    string l(lhs->d_name);
    string r(rhs->d_name);
    if(l[0] == '.')
        l = l.substr(1, l.length());
    if(r[0] == '.')
        r = r.substr(1, r.length());
    transform(l.begin(), l.end(), l.begin(), ::toupper);
    transform(r.begin(), r.end(), r.begin(), ::toupper);
    return l < r;
}

void sort_dir(vector<dirent*> &all_dir)
{
    sort(all_dir.begin(), all_dir.end(), compare_words);
}

void print_all_info(dirent* dir, string target_dir)
{
    struct stat *info = new struct stat;
    int tmp = 0;
    string full_path = target_dir + dir->d_name;

    if((tmp = stat(full_path.c_str(), info)) == -1)
    {
        cout << full_path << endl;
        perror("Error using stat!");
        return;
    }
    
    cout << right; 
    if(dir->d_type == DT_REG)
        cout << '-';
    else if(dir->d_type == DT_DIR)
        cout << 'd';
    else if(dir->d_type == DT_LNK)
        cout << 'l';
    
    PRINT_ALL_PERM(info->st_mode);
    
    cout << ' ' << getpwuid(info->st_uid)->pw_name << ' ';
    cout << getgrgid(info->st_gid)->gr_name << ' ';
    
    cout << setw(5)<< setprecision(2) << get_file_size((double)info->st_size) << "K ";
    cout << string(ctime(&info->st_mtime)).substr(0, 16) << ' ';
    cout << dir->d_name << ' ';

    if(dir->d_type == DT_LNK)
    {
        char buff[info->st_size];
        if(readlink(full_path.c_str(), buff, info->st_size + 1) != -1)
            cout << "-> " << buff << ' ';
        else
            perror("Error with readlink");
    }
    cout << endl;
    delete info;
}

double get_file_size(const double &file_size)
{
    double ret_int = file_size/1000;
    return ret_int;
}

vector<dirent*> find_all_dir(const vector<dirent*> &all_dir)
{
    vector<dirent*> ret_vec = all_dir;
    
    for(vector<dirent*>::iterator it = ret_vec.begin(); it != ret_vec.end(); )
        (!IS_A_DIR((*it)->d_type)) ? it = ret_vec.erase(it) : ++it;

    return ret_vec;
}

bool any_dirs(vector<dirent*> &all_dir)
{
    for(vector<dirent*>::iterator it = all_dir.begin(); it != all_dir.end(); ++it)
        if(IS_A_DIR((*it)->d_type))
            return true;
    return false;
}

