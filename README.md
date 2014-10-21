RSHELL
======

RSHELL is an open source project created for CS 100 at the University of California Riverside.

How to install
--------------

Run the following commands to get the source and build the shell:

    git clone https://github.com/KenleyArai/rshell.git
    cd rshell
    make
    bin/rshell

Functionality
-------------

RSHELL has basic logic functionality to combine commands together.

###Example
    ls || pwd
    mkdir tmp && cd tmp && cd ../ && rm -rf tmp

RSHELL uses `;` to end commands or newlines.

###Example
    echo Hello; ls
    
Here the command `echo Hello` and `ls` are treated as two seperate commands independant of each other.

Also there is the ability to add comments to commands by added a `#`.

###Example
    ls # echo This won't print because of the #
    
BUGS
----
