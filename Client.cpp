#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <time.h> 
#include <cstring>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 8192

FILE* fd;
int clientSocket;
int flagSet = 0;
int flagConnect = 0;
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

void* funcSetRequests(void* arg) {

    char sndbuf[BUFFER_SIZE];
    flagSet = 0;
    int bytesRead = 0;
    int sentcount = 0;
    while (!feof(fd)) {
        memset(sndbuf, 0, BUFFER_SIZE);
        bytesRead = fread(&sndbuf, 1, BUFFER_SIZE, fd);
        sentcount = send(clientSocket, sndbuf, bytesRead, 0);
        if (sentcount == -1)
        {
            perror("Отправка не удалась. Повторное подключение...");
            sleep(1);
        }
        else {
            //OK
        }
    }
    std::cout << "Файл отправлен на сервер" << std::endl;
    flagConnect = 1;
    flagLink = 1;
    fclose(fd);
    pthread_exit(0);
}

void* funcGetAnswer(void* arg) {

    char rcvbuf[1];
    int reccount = 0;
    while (flagConnect == 0)
    {
        memset(rcvbuf, 0, 1);

        reccount = recv(clientSocket, rcvbuf, 1, 0);
        if (reccount == -1) {
            //Ожидание ответа
            sleep(1);
        }
        else if (reccount == 0) {
            printf("разъединение\n");
            flagSet = 1;
            flagConnect = 1;
            pthread_exit(0);
        }
        else {
            //ok connect
        }
    }
    pthread_exit(0);
}

int main(int argc, char* argv[]) {

    if (argc != 2) {
        printf("Не задан параметр\n");
        return 0;
    }
    fd = fopen(argv[1], "rb");
    if (fd == NULL) {
        perror("Ошибка открытия файла\n");
        exit(0);
    }

    while (flagLink == 0)
    {
        clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        fcntl(clientSocket, F_SETFL, O_NONBLOCK);
        struct sockaddr_in clientSockAddr;
        clientSockAddr.sin_family = AF_INET;
        clientSockAddr.sin_port = htons(7000);
        clientSockAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

        flagConnect = 0;
        while (flagConnect == 0) {
            int result = connect(clientSocket, (struct
                sockaddr*)&clientSockAddr, sizeof(clientSockAddr));
            if (result == -1) {
                printf("Ожидание подключения к серверу\n");
                sleep(1);
            }
            else {
                char rcvbuf[256];
                recv(clientSocket, (char*)rcvbuf, sizeof(rcvbuf), 0);
                std::cout << "Соединение с сервером установлено\n";
                pthread_t threadSetRequests;
                pthread_t threadGetAnswer;
                pthread_create(&threadSetRequests, NULL, funcSetRequests, NULL);
                pthread_create(&threadGetAnswer, NULL, funcGetAnswer, NULL);
                threadJoin(threadSetRequests);
                threadJoin(threadGetAnswer);
            }
        }

        shutdown(clientSocket, 2);
        close(clientSocket);
        printf("Соединение остановлено\n");
    }
    return 0;
}