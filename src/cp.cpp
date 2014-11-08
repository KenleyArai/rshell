#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <stdio.h>

#include "Timer.h"

using namespace std;

enum CP {IOSTREAM, UNIX_ONE, UNIX_BUF};

void iostream_cp(const char * file1, const char * file2);
void unix_cp(const char * file1, const char * file2, int buffer = BUFSIZ);
void optimal_cp(const char * file1, const char * file2, CP type = UNIX_BUF);

int main()
{
    char f1[256] = "/usr/share/dict/american-english";
    char f2[256] = "file2.txt";

    Timer t;
    double eTime;

    t.start();
    optimal_cp(f1,f2);
    t.elapsedWallclockTime(eTime);
    cout << "Unix buffer:\t" << eTime << endl;
    t.elapsedUserTime(eTime);
    cout << "Usertime:\t" << eTime << endl;
    t.elapsedSystemTime(eTime);
    cout << "SystemTime:\t" << eTime << endl << endl;
 
    ofstream(f2, std::ios::trunc);

    t.start();
    optimal_cp(f1,f2, UNIX_ONE);
    t.elapsedWallclockTime(eTime);
    cout << "Unix one char:\t" << eTime << endl;
    t.elapsedUserTime(eTime);
    cout << "Usertime:\t" << eTime << endl;
    t.elapsedSystemTime(eTime);
    cout << "SystemTime:\t" << eTime << endl << endl;
 
    ofstream(f2, std::ios::trunc);

    t.start();
    optimal_cp(f1,f2, IOSTREAM);
    t.elapsedWallclockTime(eTime);
    cout << "in.get and in.put\t" << eTime << endl;
    t.elapsedUserTime(eTime);
    cout << "Usertime:\t" << eTime << endl;
    t.elapsedSystemTime(eTime);
    cout << "SystemTime:\t" << eTime << endl << endl;
 
    ofstream(f2, std::ios::trunc);
   
    return 0;
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
    
    int n;

    int f1 = open(file1, O_RDONLY);
    int f2 = open(file2, O_WRONLY);
    if(f1 == -1)
        perror("Error f1");
    if(f2 == -1)
        perror("Error f2");

    while( (n = read(f1, str, buffer)) > 0)
    {
        if(write(f2, str, n) == -1)
            perror("write error");
    }
    if(n == -1)
        perror("open error");
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
