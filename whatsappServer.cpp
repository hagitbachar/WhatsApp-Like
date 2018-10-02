//
// Created by hagitba on 6/13/18.
//

#include <arpa/inet.h>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <netdb.h>
#include <limits.h>
#include <vector>
#include <set>
#include "whatsappio.h"

#define MIN_PORT 0
#define MAX_PORT 65535
#define MAX_CLIENTS  30
#define MAX_MSG_LENGTH 256
#define SUCCESSED "yes"
#define NOT_SUCCESSED "no"

/**
 * the client struct
 */
typedef struct Client
{
    std::string name;
    int socket;
    bool operator == (struct Client other) const
    {
        return socket == other.socket;
    }
}Client;

/**
 * the group struct
 */
typedef struct Group
{
    std::string name;
    std::set<std::string> members;
    bool operator == (struct Group other) const
    {
        return name == other.name;
    }
}Group;

/**
 * vector of all the connected clients
 */
std::vector<Client> clients;
/**
 * vector of all the groups that clients created
 */
std::vector<Group> groups;

/**
 * this function create the main socket that will listen to clients that want to connect him
 * @param port - port num
 * @return server_fd on success, -1 on failure
 */
int establish(int port)
{
    char myname[HOST_NAME_MAX + 1];
    int server_fd;
    struct sockaddr_in sa;
    struct hostent *ip;

    //hostnet initialization
    gethostname(myname, HOST_NAME_MAX);
    ip = gethostbyname(myname); //returns a pointer to the filled struct hostent, or NULL on error.
    if (ip == NULL)
    {
        print_fail_connection();
        exit(1);
    }
    //sa.sin_family = AF_INET;
    memset(&sa, 0, sizeof(struct sockaddr_in));
    sa.sin_family = ip->h_addrtype;
    memcpy(&sa.sin_addr, ip->h_addr, ip->h_length);
    sa.sin_port= htons(port);

    // create socket, return val - On success, a file descriptor for the new socket is returned.
    // On error, -1 is returned, and errno is set appropriately.
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        print_error("socket", errno);
        return -1;

    }
    //assigns the address to the socket
    if (bind(server_fd, (struct sockaddr*)&sa, sizeof(struct sockaddr_in)) < 0)
    {
        print_error("bind", errno);
        close(server_fd);
        return -1;
    }

    if(listen(server_fd, 10) < 0)
    {
        print_error("listen", errno);
        close(server_fd);
        return -1;
    }
    return server_fd;
}


/**
 * this function connect new client
 * @param server_fd - the main socket
 * @param clientsfds - the fd_set that we need to add to her the new socket of the new client
 * @return 0 iff connected successfully
 */
int connectNewClient(int server_fd, fd_set& clientsfds)
{
    if(clients.size() >= MAX_CLIENTS)
    {
        print_fail_connection();
        return -1;
    }
    int new_socket;
    if ((new_socket = accept(server_fd,  NULL, NULL)) < 0)
    {
        close(server_fd);
        return -1;
    }
    char buffer[WA_MAX_NAME + 1] = {0};
    ssize_t num_byte_read = (ssize_t)read(new_socket, buffer, WA_MAX_NAME + 1);
    if(num_byte_read < 0)
    {
        print_error("read", errno);
    }
    std::string name(buffer, num_byte_read-1);
    std::string connectedSuccessfully =SUCCESSED;
    for (Client client: clients)
    {
        if(client.name == name)
        {
            connectedSuccessfully = NOT_SUCCESSED;
            if(send(new_socket, connectedSuccessfully.c_str(), strlen(connectedSuccessfully.c_str()) + 1, 0) < 0)
            {
                print_error("send", errno);
            }
            close(new_socket);
            return -1;
        }
    }
    for (Group group: groups)
    {
        if(group.name == name)
        {
            connectedSuccessfully = NOT_SUCCESSED;
            if(send(new_socket, connectedSuccessfully.c_str(), strlen(connectedSuccessfully.c_str()) + 1, 0) < 0)
            {
                print_error("send", errno);
            }

            close(new_socket);
            return -1;
        }
    }
    Client newClient;
    newClient.name = name;
    newClient.socket = new_socket;
    clients.push_back(newClient);
    FD_SET(new_socket, &clientsfds);
    print_connection_server(name);
    if(send(new_socket, connectedSuccessfully.c_str(), strlen(connectedSuccessfully.c_str()) + 1, 0) < 0)
    {
        print_error("send", errno);
    }
    return 0;
}

/**
 * send command to client
 * @param nameFrom - the sender name
 * @param nameTo - the name of the receiver
 * @param msgToSend - the nessage to send
 * @param clientSocket - the socket of the sender
 */
void sendCommand(std::string nameFrom, std::string nameTo, std::string msgToSend, int clientSocket)
{
    bool success = false, isInGroup;
    std::string nameAndMsg = nameFrom + ": " + msgToSend;
    for(Client client: clients)
    {
        if(client.name == nameTo && nameFrom != nameTo)
        {
            success = true;
            if(send(client.socket, nameAndMsg.c_str(), strlen(nameAndMsg.c_str()) + 1, 0) < 0)
            {
                print_error("send", errno);
            }
        }
    }
    for(Group g: groups)
    {
        if(g.name == nameTo)
        {
            success = true;
            isInGroup = g.members.find(nameFrom) != g.members.end();
            if(!isInGroup)
            {
                success = false;
                break;
            }
            for(std::string nameInGroup: g.members)
            {
                for(Client clientInGroup: clients)
                {
                    if(clientInGroup.name == nameInGroup && clientInGroup.name != nameFrom)
                    {
                        send(clientInGroup.socket, nameAndMsg.c_str(), strlen(nameAndMsg.c_str()) + 1, 0);
                    }
                }
            }
        }
    }
    print_send(true, success, nameFrom , nameTo, msgToSend);
    std::string sentSuccessfully = success ? SUCCESSED  : NOT_SUCCESSED;
    if(send(clientSocket, sentSuccessfully.c_str(), strlen(sentSuccessfully.c_str()) + 1, 0) < 0)
    {
        print_error("send", errno);
    }
}
/**
 * create group command
 * @param clientName - the client name who write the command
 * @param groupName - the group name the client want to create
 * @param groupMembers - the members that need to be insert to the new group
 * @param clientSocket -  the client socket who write the command
 *
 */
void createGroupCommand(std::string clientName, std::string groupName, std::vector<std::string> groupMembers,
                                                                                             int clientSocket)
{
    bool success, isAClient;
    Group groupToCreate;
    std::string createGroupSuccessfully;

    success = true;
    if(groupMembers.size() < 1 || (groupMembers.size() == 1 && groupMembers.front() == clientName))
    {
        success = false;
    }
    for(Group group: groups)
    {
        if(groupName == group.name)
        {
            success = false;
        }
    }
    for(Client client: clients)
    {
        if(groupName == client.name)
        {
            success = false;
        }
    }
    for(std::string nameToAddTheGroup: groupMembers)
    {
        isAClient = false;
        for(Client client: clients)
        {
            if (client.name == nameToAddTheGroup)
            {
                isAClient = true;
            }
        }
        if(!isAClient)
        {
            success = false;
            break;
        }
    }
    if(success)
    {
        groupToCreate.name = groupName;
        groupToCreate.members.insert(clientName);
        for(std::string nameToGroup: groupMembers)
        {
            groupToCreate.members.insert(nameToGroup);
        }
        groups.push_back(groupToCreate);
    }
    print_create_group(true, success, clientName, groupName);
    createGroupSuccessfully =
            success ? ("Group "+groupName+" was created successfully.") : ("ERROR: failed to create group "+groupName);

    if(send(clientSocket, createGroupSuccessfully.c_str(), strlen(createGroupSuccessfully.c_str()) + 1, 0) < 0)
    {
        print_error("send", errno);
    }
}

/**
 * exit command
 * @param clientName - the client name who write the command
 * @param clientSocket -  the client socket who write the command
 * @param readfds - the fd_set of sockets
 */
void exitCommand(std::string clientName, int clientSocket, fd_set* readfds)
{
    std::set<std::string>::iterator it;
    print_exit(true, clientName);

    for(unsigned int i = 0; i < clients.size(); i++)
    {
        if(clients.at(i).name == clientName)
        {
            clients.erase(clients.begin() + i);
            break;
        }
    }
    for(unsigned int i = 0; i < groups.size(); i++)
    {
        it = groups.at(i).members.find(clientName);
        if(it != groups.at(i).members.end())
        {
            groups.at(i).members.erase(it);
        }
        if(groups.at(i).members.size() == 1)
        {
            groups.erase(groups.begin() + i);
        }

    }
    std::string exit= "exit";

    if(send(clientSocket, exit.c_str(), strlen(exit.c_str()) + 1, 0) < 0)
    {
        print_error("send", errno);
    }
    FD_CLR(clientSocket, readfds);
}

/**
 * who command
 * @param clientName - the client name who write the command
 * @param clientSocket -  the client socket who write the command
 */
void whoCommand(std::string clientName, int clientSocket)
{
    std::string clientNamesString;
    for (Client client : clients)
    {
        clientNamesString += client.name + ",";
    }
    clientNamesString = clientNamesString.substr(0, strlen(clientNamesString.c_str()) - 1);
    if(send(clientSocket, clientNamesString.c_str(), strlen(clientNamesString.c_str()) + 1, 0) < 0)
    {
        print_error("send", errno);
    }
    print_who_server(clientName);
}
/**
 * handle the client command
 * @param clientSocket the socket of the client that send the request
 * @param readfds the fd_set of all the clients' sockets
 */
void handleClientRequest(int clientSocket, fd_set* readfds)
{
    char buffer[WA_MAX_MESSAGE + 1] = {0};
    ssize_t num_byte_read = (ssize_t)read(clientSocket, buffer, WA_MAX_MESSAGE + 1);
    if(num_byte_read < 0)
    {
        print_error("read", errno);
    }
    std::string command(buffer, num_byte_read-1);

    std::string nameAndMsg, clientName;

    command_type commandT;
    std::vector<std::string> clientsForParse;
    std::string name, msgToSend;
    parse_command(command, commandT, name, msgToSend, clientsForParse);
    //search for the name of the from client
    for (Client client : clients)
    {
        if(client.socket == clientSocket)
        {
            clientName = client.name;
        }
    }

    switch (commandT)
    {
        case INVALID:
            print_invalid_input();
            break;
        case SEND:
            sendCommand(clientName, name, msgToSend, clientSocket);
            break;
        case WHO:
            whoCommand(clientName, clientSocket);
            break;
        case CREATE_GROUP:
            createGroupCommand(clientName, name, clientsForParse, clientSocket);
            break;
        case EXIT:
            exitCommand(clientName, clientSocket, readfds);

            break;
    }
}

/**
 * the main function
 * @param argc the number of arguments that the program receive
 * @param argv the arguments that the program receive
 * @return o on success, -1 on failure
 */
int main(int argc, char **argv) {
    if (argc != 2) {
        print_server_usage();
        exit(1);
    }
    long int port = strtol(argv[1], NULL, 10);
    if (port == 0L || port < MIN_PORT || port > MAX_PORT)
    {
        print_server_usage();
        exit(1);
    }
    int server_fd;

    if ((server_fd = establish((int) port)) < 0) {
        return -1;
    }
    fd_set clientsfds;
    fd_set readfds;
    FD_ZERO(&clientsfds);
    FD_SET(server_fd, &clientsfds);
    FD_SET(STDIN_FILENO, &clientsfds);

    clients = std::vector<Client>();
    groups = std::vector<Group>();
    while (true)
    {
        readfds = clientsfds;

        if (select(MAX_CLIENTS + 3, &readfds, NULL, NULL, NULL) < 0)
        {
            close(server_fd);
            print_error("select", errno);
        }
        if (FD_ISSET(server_fd, &readfds))
        {
            if (connectNewClient(server_fd, clientsfds) < 0)
            {
                print_fail_connection();
            }
        }
        if (FD_ISSET(STDIN_FILENO, &readfds))
        {
            char command[MAX_MSG_LENGTH];
            std::cin.getline(command, MAX_MSG_LENGTH);
            if ( ((std::string)command).compare("EXIT") == 0)
            {
                for (Client client : clients)
                {
                    std::string exit= "exit";
                    if(send(client.socket, exit.c_str(), strlen(exit.c_str()) + 1, 0) < 0)
                    {
                        print_error("send", errno);
                    }
                }
                print_exit();
                close(server_fd);
                exit(0);
            }
        }
        else
        {
            //will check each client if it’s in readfds
            //and then receive a message from him
            for (Client client : clients)
            {
                if (FD_ISSET(client.socket, &readfds))
                {
                    handleClientRequest(client.socket, &readfds);
                }
            }
        }
    }
}
