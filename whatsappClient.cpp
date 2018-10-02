//
// Created by hagitba on 6/13/18.
//

#include <netdb.h>
#include <cstring>
#include <unistd.h>
#include <limits.h>
#include <arpa/inet.h>
#include <iostream>
#include <algorithm>
#include "whatsappio.h"

#define MAX_CLIENTS  30
#define SUCCESSED "yes"
#define NOT_SUCCESSED "no"

/**
 * check the name validity
 * @param name to check
 */
int checkValidName(std::string name)
{
    for (char c : name)
    {
        if(!std::isalnum(c))
        {
            print_invalid_input();
            return -1;
        }
    }
    return 0;
}
/**
 * handle the user command
 * @param fd - the client socket
 * @param command - to handle
 * @param clientName - the client that write the command
 */
void handleUserCommand(int fd, std::string &command, const std::string &clientName)
{

    if(std::count(command.begin(), command.end(), ' ') == 1)
    {
        print_invalid_input();
        return;
    }

    command_type commandT;
    std::vector<std::string> clientsForParse;
    std::string name;
    std::string msg;
    parse_command(command, commandT, name, msg, clientsForParse);
    switch (commandT){
        case INVALID:
            print_invalid_input();
            break;
        case SEND:
            if(checkValidName(name) == -1)
            {
                return;
            }

            if(send(fd, command.c_str(), strlen(command.c_str()) + 1, 0) < 0)
            {
                print_error("send", errno);
                exit(1);
            }
            break;
        case WHO:
            if(send(fd, command.c_str(), strlen(command.c_str()) + 1, 0) < 0)
            {
                print_error("send", errno);
                exit(1);
            }
            break;
        case CREATE_GROUP:
            if(checkValidName(name) == -1)
            {
                return;
            }
            if(clientsForParse.size() == 1 && clientName == clientsForParse.front())
            {
                print_send(false, false, "", "", "");
            }
            else
            {
                if(send(fd, command.c_str(), strlen(command.c_str()) + 1, 0) < 0)
                {
                    print_error("send", errno);
                    exit(1);
                }
            }
            break;

        case EXIT:
            if(send(fd, command.c_str(), strlen(command.c_str()) + 1, 0) < 0)
            {
                print_error("send", errno);
                exit(1);
            }
    }

}

/**
 * the main function
 * @param argc - the number of arguments that the program receive
 * @param argv - the arguments that the program receive
 * @return o on success, -1 on failure
 */
int main(int argc, char **argv)
{

    if (argc != 4)
    {
        print_client_usage();
        return -1;
    }
    std::string clientName = argv[1];
    long int port = strtol(argv[3], NULL, 10);

    for (char c : clientName)
    {
        if(!std::isalnum(c))
        {
            print_invalid_input();
            return -1;
        }
    }
    if(port == 0L) {
        print_invalid_input();
        return -1;
    }
    struct sockaddr_in sa;
    struct hostent *hp;
    int serverSocketfd;

    char hostname[HOST_NAME_MAX + 1];
    gethostname(hostname, HOST_NAME_MAX);

    if ((hp = gethostbyname(hostname)) == NULL)
    {
        print_error("gethostbyname", errno);
        exit(1);
    }

    memset(&sa, 0, sizeof(sa));
    memcpy((char *) &sa.sin_addr, hp->h_addr, hp->h_length);
    sa.sin_family = hp->h_addrtype;
    sa.sin_port = htons((u_short) port);
    if ((serverSocketfd = socket(hp->h_addrtype, SOCK_STREAM,0)) < 0) {
        print_error("socket", errno);
        exit(1);
    }

    if (connect(serverSocketfd, (struct sockaddr*)&sa , sizeof(sa)) < 0)
    {
        close(serverSocketfd);
        print_error("connect", errno);
        exit(1);
    }

    if(send(serverSocketfd, clientName.c_str(), strlen(clientName.c_str()) + 1, 0) < 0)
    {
        print_error("send", errno);
        exit(1);
    }

    char buffer[WA_MAX_MESSAGE + 1] = {0};
    ssize_t num_byte_read = (ssize_t)read(serverSocketfd, buffer, WA_MAX_MESSAGE + 1);
    if(num_byte_read < 0)
    {
        print_error("read", errno);
        exit(1);
    }
    std::string connectedSuccessfully(buffer, num_byte_read-1);
    if(connectedSuccessfully ==NOT_SUCCESSED)
    {
        print_dup_connection();
        close(serverSocketfd);
        exit(1);
    }
    print_connection();

    fd_set clientsfds;
    fd_set readfds;
    FD_ZERO(&clientsfds);
    FD_SET(serverSocketfd, &clientsfds);
    FD_SET(STDIN_FILENO, &clientsfds);
    while (true)
    {
        readfds = clientsfds;
        if (select(MAX_CLIENTS+3, &readfds, NULL, NULL, NULL) < 0)
        {
            print_error("select", errno);
            exit(1);
        }
        if (FD_ISSET(serverSocketfd, &readfds))
        {
        //will also add the client to the clientsfds
            buffer[WA_MAX_MESSAGE + 1] = {0};
            ssize_t num_byte_read = (ssize_t)read(serverSocketfd, buffer, WA_MAX_MESSAGE + 1);
            if(num_byte_read < 0)
            {
                print_error("read", errno);
                exit(1);
            }
            std::string msgFromServer(buffer, num_byte_read-1);
            msgFromServer = (std::string)msgFromServer;
            if(msgFromServer == "exit")
            {
                print_exit(false, "");
                close(serverSocketfd);
                exit(0);
            }
            else if (msgFromServer == NOT_SUCCESSED)
            {
                print_send(false, false, "", "", "");
            }
            else if (msgFromServer == SUCCESSED)
            {
                print_send(false, true, "", "", "");
            }
            else{

                std::cout<<msgFromServer<<std::endl;
            }
        }
        if (FD_ISSET(STDIN_FILENO, &readfds))
        {
            buffer[WA_MAX_INPUT + 1] = {0};

            ssize_t num_byte_read = (ssize_t)read(STDIN_FILENO, buffer, WA_MAX_INPUT + 1);
            if(num_byte_read < 0)
            {
                print_error("read", errno);
                exit(1);
            }
            std::string userCommand(buffer, num_byte_read-1);
            handleUserCommand(serverSocketfd, userCommand, clientName);
        }
    }
}



