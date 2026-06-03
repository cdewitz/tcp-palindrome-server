/*
 * PalindromeCheckerServer.c
 *
 * Purpose:
 *   A multi-threaded TCP socket server written in pure C (C99).
 *   Accepts string input from one or more PalindromeCheckerClient
 *   connections, evaluates whether each string is a palindrome
 *   (ignoring case, spaces, and non-alphanumeric characters), and
 *   returns a descriptive result to the client.
 *
 *   - Default port : 50504
 *   - Override port: first command-line argument  (e.g. ./server 50501)
 *   - Ports below 1024 are rejected.
 *   - If the port is already in use the program exits with an error.
 *   - Each client connection is handled in a dedicated POSIX thread so
 *     multiple clients can be served simultaneously.
 *   - The server runs until killed (Ctrl+C / kill).
 *   - A connection closes when the client sends an empty (null) string.
 *
 * Compilation:
 *   gcc -std=c99 -pthread -Wall -o server PalindromeCheckerServer.c
 *
 * Usage:
 *   ./server           -> listens on port 50504 (default)
 *   ./server 50501     -> listens on port 50501
 */
 
#define _POSIX_C_SOURCE 200112L
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
 
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
 
/* -----------------------------------------------------------------------
 * Constants
 * --------------------------------------------------------------------- */
#define DEFAULT_PORT  50504
#define BACKLOG       10
#define BUFFER_SIZE   4096
#define FILTER_SIZE   4096
 
/* -----------------------------------------------------------------------
 * isPalindrome
 *
 * Returns 1 if 'input' is a palindrome, 0 otherwise.
 * Normalises by keeping only letters/digits in lowercase.
 * --------------------------------------------------------------------- */
static int isPalindrome(const char *input)
{
    char filtered[FILTER_SIZE];
    int  flen = 0;
    int  i;
 
    for (i = 0; input[i] != '\0' && flen < FILTER_SIZE - 1; i++) {
        unsigned char ch = (unsigned char)input[i];
        if (isalpha(ch) || isdigit(ch)) {
            filtered[flen++] = (char)tolower(ch);
        }
    }
    filtered[flen] = '\0';
 
    if (flen == 0) return 0;
 
    int left  = 0;
    int right = flen - 1;
    while (left < right) {
        if (filtered[left] != filtered[right]) return 0;
        left++;
        right--;
    }
    return 1;
}
 
/* -----------------------------------------------------------------------
 * ClientInfo  – passed to each thread
 * --------------------------------------------------------------------- */
typedef struct {
    int sockfd;
    int clientId;
} ClientInfo;
 
/* -----------------------------------------------------------------------
 * clientHandler  – thread entry point
 * --------------------------------------------------------------------- */
static void *clientHandler(void *arg)
{
    ClientInfo *info     = (ClientInfo *)arg;
    int         clientFd = info->sockfd;
    int         clientId = info->clientId;
    free(info);
 
    printf("[Server] Client #%d connected (fd=%d).\n", clientId, clientFd);
    fflush(stdout);
 
    char    buffer[BUFFER_SIZE];
    char    response[BUFFER_SIZE + 64];
    ssize_t bytesRead;
 
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        bytesRead = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
 
        if (bytesRead <= 0) {
            printf("[Server] Client #%d disconnected.\n", clientId);
            fflush(stdout);
            break;
        }
 
        /* Strip trailing \r and \n */
        int len = (int)strlen(buffer);
        while (len > 0 && (buffer[len-1] == '\n' || buffer[len-1] == '\r')) {
            buffer[--len] = '\0';
        }
 
        /* Empty string = end of session */
        if (len == 0) {
            printf("[Server] Client #%d sent empty string – closing.\n", clientId);
            fflush(stdout);
            break;
        }
 
        printf("[Server] Client #%d sent: \"%s\"\n", clientId, buffer);
        fflush(stdout);
 
        /* Build response */
        if (isPalindrome(buffer)) {
            snprintf(response, sizeof(response),
                     "\"%s\" IS a palindrome.\n", buffer);
        } else {
            snprintf(response, sizeof(response),
                     "\"%s\" is NOT a palindrome.\n", buffer);
        }
 
        if (send(clientFd, response, strlen(response), 0) < 0) {
            fprintf(stderr, "[Server] send() error: %s\n", strerror(errno));
            break;
        }
    }
 
    close(clientFd);
    printf("[Server] Connection with client #%d closed.\n", clientId);
    fflush(stdout);
    return NULL;
}
 
/* -----------------------------------------------------------------------
 * main
 * --------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
    int port = DEFAULT_PORT;
 
    if (argc >= 2) {
        port = atoi(argv[1]);
        if (port <= 0) {
            fprintf(stderr, "[Server] Invalid port \"%s\".\n", argv[1]);
            return 1;
        }
    }
 
    if (port < 1024) {
        fprintf(stderr, "[Server] Port %d is reserved (< 1024).\n", port);
        return 1;
    }
 
    /* Create socket */
    int listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd < 0) {
        fprintf(stderr, "[Server] socket() failed: %s\n", strerror(errno));
        return 1;
    }
 
    /* Allow port reuse after restart */
    int opt = 1;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
 
    /* Bind */
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family      = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port        = htons((unsigned short)port);
 
    if (bind(listenFd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        if (errno == EADDRINUSE)
            fprintf(stderr, "[Server] Port %d is already in use.\n", port);
        else
            fprintf(stderr, "[Server] bind() failed: %s\n", strerror(errno));
        close(listenFd);
        return 1;
    }
 
    if (listen(listenFd, BACKLOG) < 0) {
        fprintf(stderr, "[Server] listen() failed: %s\n", strerror(errno));
        close(listenFd);
        return 1;
    }
 
    printf("[Server] Palindrome Checker started on port %d.\n", port);
    printf("[Server] Waiting for connections...\n");
    fflush(stdout);
 
    int clientCounter = 0;
 
    while (1) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
 
        int clientFd = accept(listenFd,
                              (struct sockaddr *)&clientAddr,
                              &clientLen);
        if (clientFd < 0) {
            fprintf(stderr, "[Server] accept() failed: %s\n", strerror(errno));
            continue;
        }
 
        clientCounter++;
 
        ClientInfo *info = (ClientInfo *)malloc(sizeof(ClientInfo));
        info->sockfd   = clientFd;
        info->clientId = clientCounter;
 
        pthread_t tid;
        if (pthread_create(&tid, NULL, clientHandler, info) != 0) {
            fprintf(stderr, "[Server] pthread_create() failed.\n");
            close(clientFd);
            free(info);
        } else {
            pthread_detach(tid);
        }
    }
 
    close(listenFd);
    return 0;
}
