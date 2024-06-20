#pragma once
#ifndef SERVER_CONNECT_H
#define SERVER_CONNECT_H

class Server_connect_winsock
{
private:
    SOCKET connectSocket;

    Server_connect_winsock();
    ~Server_connect_winsock();

    Server_connect_winsock(const Server_connect_winsock&) = delete;
    Server_connect_winsock& operator=(const Server_connect_winsock&) = delete;

public:
    static Server_connect_winsock& getInstance()
    {
        static Server_connect_winsock instance;
        return instance;
    }

    bool ConnectToServer(const std::string& host, const std::string& port);
    bool SendMessageToServer(const std::string& message);
    std::string ReceiveMessage();
    void CloseConnection() const;
    bool IsConnected() const;
    bool CheckForMessage();
};

#endif // SERVER_CONNECT_H