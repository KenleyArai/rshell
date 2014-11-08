#include <iostream>
#include <stdio.h>
#include <stdlib.h>

using namespace std;

string cmd_output(string cmd)
{
    string data;
    FILE * stream;

    char buff[BUFSIZ];
    cmd.append(" 2>&1");

    stream = popen(cmd.c_str(), "r");
    if(stream)
    {
        while(!feof(stream))
            if(fgets(buff, BUFSIZ, stream) != NULL)
                data.append(buff);
        pclose(stream);
    }
    return data;
}

int main()
{
    string ls = cmd_output("ls -la");

    cout << "LS: " << ls << endl;

    return 0;
}
