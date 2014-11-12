#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <vector>
#include <dirent.h>

using namespace std;

enum CP {IOSTREAM, UNIX_ONE, UNIX_BUF};
enum FLAG {NO_FLAG = 1, REC = 2};

#define IS_REC(X) X & REC ? 1 : 0

bool file_exists(const string &file);
string check_end_slash(const string &dir);
vector<string> get_all_reg(const string &file);
vector<string> get_all_dir(const string &dir);
void copy_files(const string &s, const string &d, const FLAG &f);
bool is_a_reg(const string &file);
bool is_a_dir(const string &file);
FLAG get_flags(const vector<string> &args);
string get_src_dir(const vector<string> &args);
string get_dst_dir(const vector<string> &args);
bool is_a_reg(const string &file);
bool is_a_dir(const string &file);
void iostream_cp(const char * file1, const char * file2);
void unix_cp(const char * file1, const char * file2, int buffer = BUFSIZ);
void optimal_cp(const char * file1, const char * file2, CP type = UNIX_BUF);

int main(int argc, char *argv[])
{
    vector<string> arguments(argv + 1, argv + argc);
    FLAG f     = get_flags(arguments);
    string src = get_src_dir(arguments),
           dst = get_dst_dir(arguments);
   
    copy_files(src,dst,f);
    
    return 0;
}

bool file_exists(const string &file)
{
    struct stat info;
    return (stat(file.c_str(), &info) == 0);
}

string check_end_slash(const string &dir)
{
    if(is_a_dir(dir) && dir.at(dir.length() - 1) != '/')
        return (dir + "/");
    return dir;
}

vector<string> get_all_reg(const string &file)
{
    vector<string> ret_vec;
    DIR *dir;
    struct dirent *dp;
    dir = opendir(file.c_str());
    
    if(dir)
        while((dp = readdir(dir)))
            if(is_a_reg(file + dp->d_name))
                ret_vec.push_back(dp->d_name);
    (void) closedir (dir);

    return ret_vec;
}

vector<string> get_all_dir(const string &d)
{
    vector<string> ret_vec;

    DIR *dir;
    struct dirent *dp;
    dir = opendir(d.c_str());

    if(dir)
        while((dp = readdir(dir)))
            if(is_a_dir(d + dp->d_name) && dp->d_name[0] != '.')
                ret_vec.push_back(check_end_slash(string(dp->d_name)));
    (void) closedir (dir);

    return ret_vec;
}

void copy_files(const string &s, const string &d, const FLAG &f)
{
    cout << "s: " << s << endl;
    cout << "d: " << d << endl;
    string src = check_end_slash(s);
    string dst = check_end_slash(d);
 
    cout << "src: " << src << endl;
    cout << "dst: " << dst << endl;
   if(is_a_reg(src) && is_a_reg(dst))
        optimal_cp(src.c_str(), dst.c_str());
    else if(is_a_dir(src) && !file_exists(dst))
    {
        struct stat info;
        if(mkdir(dst.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH ) < 0)
            perror("Error using mkdir");
        if(stat(src.c_str(), &info))
            perror("stat");
        else
            if(chmod(dst.c_str(), info.st_mode & 07777))
                perror("chmod");

        copy_files(src, dst, f);
    }
    else if(IS_REC(f))
    {
        vector<string>::iterator it;
        vector<string> reg_vec = get_all_reg(src);
        vector<string> dir_vec = get_all_dir(src);
        
        for(it = dir_vec.begin(); it != dir_vec.end(); ++it)
            copy_files(check_end_slash(src + *it), check_end_slash(dst + *it), f);    

        for(it = reg_vec.begin(); it != reg_vec.end(); ++it)
            copy_files(src + *it, dst + *it, f);
    }
}

bool is_a_reg(const string &file)
{
    struct stat info;
    stat(file.c_str(), &info);
    return S_ISREG(info.st_mode);
}

bool is_a_dir(const string &file)
{
    struct stat info;
    stat(file.c_str(), &info);
    return S_ISDIR(info.st_mode);
}

FLAG get_flags(const vector<string> &args)
{
    FLAG ret_flag = NO_FLAG;
    vector<string>::const_iterator it;

    for(it = args.begin(); it != args.end(); ++it)
        if((*it)[0] == '-')
            for(size_t i = 0; i < it->length(); i++)
                if((*it)[i] == 'r')
                    ret_flag = (FLAG)(ret_flag | REC);
    return ret_flag;
}

string get_src_dir(const vector<string> &args)
{
    vector<string>::const_iterator it;

    for(it = args.begin(); it != args.end(); ++it)
        if((*it)[0] != '-')
            return *it;
    return string();
}

string get_dst_dir(const vector<string> &args)
{
    vector<string>::const_iterator it;
    bool found_src = false;

    for(it = args.begin(); it != args.end(); ++it)
        if((*it)[0] != '-')
        {
            if(found_src)
                return *it;
            else if(!found_src)
                found_src = !found_src;
        }
    return string();
}

void iostream_cp(const char * file1, const char * file2)
{
    ifstream f1(file1);
    fstream f2(file2);
    
    char c = f1.get();

    while(f1.good())
    {
        f2.put(c);
        c = f1.get();
    }

    f1.close();
    f2.close();
}


void unix_cp(const char * file1, const char * file2, int buffer)
{
    char * str = new char[buffer];
    struct stat info;
    
    int n;

    int f1 = open(file1, O_RDONLY);
    int f2 = open(file2, O_RDWR | O_CREAT | S_IRUSR | S_IRGRP | S_IROTH);

    if(f1 == -1)
        perror("Error opening source");
    if(f2 == -1)
        perror("Error opening dest");

    while( (n = read(f1, str, buffer)) > 0)
    {
        if(write(f2, str, n) == -1)
            perror("write error");
    }
    if(n == -1)
        perror("open error");
    
    if(stat(file1, &info))
        perror("stat");
    else
        if(chmod(file2, info.st_mode & 07777))
            perror("chmod");

}

void optimal_cp(const char * file1, const char * file2, CP type)
{
    switch(type)
    {
        case IOSTREAM:
            iostream_cp(file1, file2);
            break;
        case UNIX_ONE:
            unix_cp(file1, file2, 1);
            break;
        case UNIX_BUF:
            unix_cp(file1, file2);
            break;
    }
}
