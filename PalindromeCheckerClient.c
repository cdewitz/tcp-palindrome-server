/*
 * PalindromeCheckerClient.c
 *
 * Purpose:
 *   An interactive TCP socket client for the Palindrome Checker service,
 *   written in pure C (C99).
 *   The user is prompted to enter strings one at a time; each string is
 *   sent to the server and the result is printed to the console.
 *
 *   - Default server : localhost
 *   - Default port   : 50504
 *   - Override via command-line: ./client [hostname [port]]
 *       ./client                            -> localhost:50504
 *       ./client 192.168.1.10               -> 192.168.1.10:50504
 *       ./client courses.brockport.edu 50501
 *   - The client terminates when the user presses Enter on an empty line.
 *
 * Compilation:
 *   gcc -std=c99 -Wall -o client PalindromeCheckerClient.c
 *
 * Usage:
 *   ./client [hostname] [port]
 */
 
#define _POSIX_C_SOURCE 200112L
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
 
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
 
#define DEFAULT_PORT  50504
#define DEFAULT_HOST  "localhost"
#define BUFFER_SIZE   4096
 
int main(int argc, char *argv[])
{
    const char *hostname = DEFAULT_HOST;
    int         port     = DEFAULT_PORT;
 
    if (argc >= 2) hostname = argv[1];
    if (argc >= 3) {
        port = atoi(argv[2]);
        if (port <= 0) {
            fprintf(stderr, "[Client] Invalid port \"%s\".\n", argv[2]);
            return 1;
        }
    }
 
    printf("[Client] Connecting to %s on port %d...\n", hostname, port);
 
    /* Resolve hostname */
    struct hostent *server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr, "[Client] Could not resolve host \"%s\".\n", hostname);
        return 1;
    }
 
    /* Create socket */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "[Client] socket() failed: %s\n", strerror(errno));
        return 1;
    }
 
    /* Connect */
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port   = htons((unsigned short)port);
    memcpy(&serverAddr.sin_addr.s_addr,
           server->h_addr_list[0],
           (size_t)server->h_length);
 
    if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        fprintf(stderr, "[Client] connect() failed: %s\n", strerror(errno));
        close(sockfd);
        return 1;
    }
 
    printf("[Client] Connected to Palindrome Checker server.\n");
    printf("[Client] Type a string and press Enter to check it.\n");
    printf("[Client] Press Enter on an empty line to quit.\n\n");
 
    char    inputBuf[BUFFER_SIZE];
    char    sendBuf[BUFFER_SIZE + 2];
    char    recvBuf[BUFFER_SIZE];
    ssize_t bytesRecv;
 
    while (1) {
        printf("Enter string: ");
        fflush(stdout);
 
        /* Read a line from stdin */
        if (fgets(inputBuf, sizeof(inputBuf), stdin) == NULL) {
            /* EOF (Ctrl+D) */
            inputBuf[0] = '\0';
        }
 
        /* Strip trailing newline */
        int len = (int)strlen(inputBuf);
        while (len > 0 && (inputBuf[len-1] == '\n' || inputBuf[len-1] == '\r')) {
            inputBuf[--len] = '\0';
        }
 
        /* Empty input = quit */
        if (len == 0) {
            printf("[Client] Empty string entered. Closing connection.\n");
            /* Send bare newline so server detects the empty string */
            send(sockfd, "\n", 1, 0);
            break;
        }
 
        /* Send string + newline delimiter */
        snprintf(sendBuf, sizeof(sendBuf), "%s\n", inputBuf);
        if (send(sockfd, sendBuf, strlen(sendBuf), 0) < 0) {
            fprintf(stderr, "[Client] send() failed: %s\n", strerror(errno));
            break;
        }
 
        /* Receive response */
        memset(recvBuf, 0, sizeof(recvBuf));
        bytesRecv = recv(sockfd, recvBuf, sizeof(recvBuf) - 1, 0);
        if (bytesRecv <= 0) {
            fprintf(stderr, "[Client] Server closed connection unexpectedly.\n");
            break;
        }
 
        printf("Result : %s", recvBuf);
    }
 
    close(sockfd);
    printf("[Client] Session ended.\n");
    return 0;
}
