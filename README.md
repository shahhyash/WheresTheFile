# Where's The File? (CS214 // Assignment 4)
#### Contributors: Seth Karten & Yash Shah

## Overview

## Usage
Call all commands from the directory with the Makefile.
make
bin/WTFserver <PORT>
bin/WTF <args>

make test
bin/WTFtest

## Thread Synchronization Requirements
During the execution of our multithreaded server, we do not want to result in race conditions. To avoid this, we use a table (linked list) of project mutexes. In order for a thread to access any of the contents of a project, it first must fetch the lock for that particular mutex from the table.

First, let us briefly mention a few remarks about the table. The table is a linked list of structs. Each struct contains a mutex, the project name, and a pointer to the next struct in the list. The table itself is locked by a mutex.

So in order to fetch the lock for any project mutex, the table mutex must be unlocked. Thus, the function that fetches the lock for any project mutex first checks that the table is open, then locks it to prevent any other thread from editing the table while it looks up a project mutex. The function unlocks the table before returning a pointer to the project mutex.

We must now make a remark about the individual project mutexes. We want to know how many project's mutexes are locked at any given time so we create a number accessed counter that represents this. When a thread locks a project's mutex, it first locks the access counter's mutex, then it increments the counter, then unlocks the access counter's mutex. Similarly, when a thread unlocks a project's, it locks, decrements, and then unlocks the access counter mutex, ensuring that an accurate number of project mutexes are counted at one time.

The aforementioned access counter is important when the table of project mutexes needs to be edited (e.g. when a project is destroyed or created), we implement a trickle system. First the thread that is going to edit the table locks the table mutex. Then the thread waits until all the threads trickle out, that is, the thread waits until the access counter's value is 0. When it is 0, we know that there are no threads accessing any projects. Thus, we can edit the table safely. When the query is resolved, the table is unlocked and other threads may begin to access the projects from the table again.

## Implementation Notes
