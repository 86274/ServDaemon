#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <time.h> 
#include <cstring>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <list>
#include <netinet/in.h>
#include <signal.h>
#include <syslog.h>
#include <string>
#include <fstream>

#define BUFFER_SIZE 8192

int listenSocket; 
int serverSocket; 
std::list<std::string> msglist;
pthread_mutex_t mutexList;

int flagSetAnswers = 0;
int flagText = 0;
int flagLink = 0;

int threadJoin(pthread_t thread) {
    int result = 0;
    result = pthread_join(thread, NULL);
    if (result != 0) {
        perror("Ждём поток");
        return EXIT_FAILURE;
    }
    return 0;
}

void* funcGetRequests(void* arg) {
    std::ofstream file;
    file.open("text.txt");
    char rcvbuf[BUFFER_SIZE];
    int reccount = 0;
    flagSetAnswers = 0;
    while (true)
    {
        memset(rcvbuf, 0, BUFFER_SIZE);
        reccount = recv(serverSocket, (char*)rcvbuf, BUFFER_SIZE, 0);//принимаем сообщение
        if (reccount == -1)
        {
            //perror("recv error");
            sleep(1);
        }
        else if (reccount == 0) {
            //Разъединение
            flagSetAnswers = 1;
            flagText = 1;
            pthread_exit(0);
        }
        else {
            int statusLock = 1;
            statusLock = pthread_mutex_trylock(&mutexList);
            file << rcvbuf;
            if (statusLock == 0) {
                msglist.push_front((std::string(rcvbuf)));
            }
            pthread_mutex_unlock(&mutexList);
        }
    }
    file.close();
    pthread_exit(0);

}

void* funcSetAnswers(void* arg){ //Проверка коннекта для клиента

    while (flagSetAnswers == 0)
    {
        int statusLockList = 1;
        statusLockList = pthread_mutex_trylock(&mutexList);
        if (statusLockList == 0)
            if (!msglist.empty()) {
                msglist.pop_back();
                pthread_mutex_unlock(&mutexList);
                char sndbuf[1];
                socklen_t sz = sizeof(sndbuf);
                int sentcount = send(serverSocket, &sndbuf, sz, 0);
                if (sentcount == -1) {
                    //perror("send error")
                }
                else {
                    //message ok
                }
            }
            else {
                pthread_mutex_unlock(&mutexList);
                sleep(1);
            }
    }
    pthread_exit(0);

}

void* funcWaitLink(void* arg) {

    struct sockaddr_in serverSockAddr;
    socklen_t addrlen = (socklen_t)sizeof(serverSockAddr);
    while (flagLink == 0)
    {
        serverSocket = accept(listenSocket, (struct sockaddr*)&serverSockAddr, &addrlen);
        if (serverSocket == -1) {
            //perror("accept error");
            sleep(1);
        }
        else {
            char sndbuf[1];
            send(serverSocket, (char*)sndbuf, sizeof(sndbuf), 0);
            pthread_t threadGetRequests;
            pthread_t threadSetAnswers;
            pthread_mutex_init(&mutexList, NULL);
            pthread_create(&threadGetRequests, NULL, funcGetRequests, NULL);
            pthread_create(&threadSetAnswers, NULL, funcSetAnswers, NULL);
            threadJoin(threadGetRequests);
            threadJoin(threadSetAnswers);
            pthread_mutex_destroy(&mutexList);
        }
    }

    pthread_exit(0);
}

void sigHandler(int signo) {
    flagSetAnswers = 1;
    flagText = 1;
    flagLink = 1;
}

void daemonStart() {
    pid_t pid;

    pid = fork();

    if (pid < 0)
        exit(EXIT_FAILURE);
    if (pid > 0)
        exit(EXIT_SUCCESS);
    if (setsid() < 0)
        exit(EXIT_FAILURE);
    signal(SIGTERM, sigHandler);
    signal(SIGHUP, sigHandler);

    pid = fork();
    if (pid < 0)
        exit(EXIT_FAILURE);
    if (pid > 0)
        exit(EXIT_SUCCESS);

    umask(0);

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    openlog("ServDaemon", LOG_PID, LOG_DAEMON);

}

int main() {

    daemonStart();

    openlog("ServDaemon", LOG_PID, LOG_DAEMON);

    while (flagLink == 0)
    {
        syslog(LOG_NOTICE, "ServDaemon started.");

        pthread_t threadWaitLink;

        listenSocket = socket(AF_INET, SOCK_STREAM, 0);
        fcntl(listenSocket, F_SETFL, O_NONBLOCK);

        struct sockaddr_in listenSockAddr;
        listenSockAddr.sin_family = AF_INET;
        listenSockAddr.sin_port = htons(7000);
        listenSockAddr.sin_addr.s_addr = htonl(INADDR_ANY);

        bind(listenSocket, (struct sockaddr*)&listenSockAddr, sizeof(listenSockAddr));

        int optval = 1;
        setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

        listen(listenSocket, 1);
        pthread_create(&threadWaitLink, NULL, funcWaitLink, NULL);

        threadJoin(threadWaitLink);
        shutdown(serverSocket, 2);
        close(serverSocket);
        shutdown(listenSocket, 2);
        close(listenSocket);

        sleep(5);
    }

    syslog(LOG_NOTICE, "ServDaemon terminated.");
    closelog();
    return EXIT_SUCCESS;

}