# TCP Palindrome Checker

A multi-threaded TCP client-server application written in C (C99) using POSIX sockets and pthreads.

The server accepts connections from multiple simultaneous clients, receives strings, evaluates whether each is a palindrome (ignoring case, spaces, and punctuation), and returns the result. Each client connection is handled in a dedicated detached pthread so no client blocks another.

## Compilation

gcc -std=c99 -pthread -Wall -o server PalindromeCheckerServer.c
gcc -std=c99 -Wall -o client PalindromeCheckerClient.c

## Usage

./server [port]        # default port 50504
./client [host] [port] # default localhost:50504
