#include "pch.h"
#include "Server_connect_winsock.h"

Server_connect_winsock::Server_connect_winsock() : connectSocket(INVALID_SOCKET) {
    // Winsock 라이브러리 초기화
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        //std::cerr << "WSAStartup failed: " << iResult << std::endl;
    }
    // 논블로킹 설정
    u_long mode = 1;
    ioctlsocket(connectSocket, FIONBIO, &mode);
}

Server_connect_winsock::~Server_connect_winsock() {
    CloseConnection();
}

bool Server_connect_winsock::ConnectToServer(const std::string& host, const std::string& port) {
    // Winsock 초기화
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        //std::cerr << "WSAStartup failed: " << iResult << std::endl;
    }

    struct addrinfo* result = nullptr, * ptr = nullptr, hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // 주소 정보 가져오기
    iResult = getaddrinfo(host.c_str(), port.c_str(), &hints, &result);
    if (iResult != 0) {
        //std::cerr << "getaddrinfo failed: " << iResult << std::endl;
        WSACleanup();
        return false;
    }

    // 첫 번째 유효한 주소로 소켓 생성 및 연결 시도
    for (ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        connectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (connectSocket == INVALID_SOCKET) {
            //std::cerr << "Error at socket(): " << WSAGetLastError() << std::endl;
            freeaddrinfo(result);
            WSACleanup();
            return false;
        }

        // 서버에 연결 시도
        iResult = connect(connectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(connectSocket);
            connectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (connectSocket == INVALID_SOCKET) {
        //std::cerr << "Unable to connect to server!" << std::endl;
        WSACleanup();
        return false;
    }

    return true;
}

bool Server_connect_winsock::SendMessageToServer(const std::string& message) {
    int iResult = send(connectSocket, message.c_str(), static_cast<int>(message.length()), 0);
    if (iResult == SOCKET_ERROR) {
        return false;
    }
    return true;
}


//std::string Server_connect_winsock::ReceiveMessage() {
//    char buffer[4096] = { 0 };
//    int recvResult = recv(connectSocket, buffer, sizeof(buffer), 0);
//    if (recvResult > 0) {
//        std::string receivedMessage(buffer, recvResult);
//        return receivedMessage;
//    }
//    else if (recvResult == 0) {
//        // Connection closed by peer
//        closesocket(connectSocket);
//        return "";
//    }
//    else {
//        int error = WSAGetLastError();
//        if (error == WSAEWOULDBLOCK) {
//            // No data available right now, try again later
//            return "";
//        }
//        else {
//            // Other errors
//            closesocket(connectSocket);
//            return "";
//        }
//    }
//}
//std::string Server_connect_winsock::ReceiveMessage() {
//    // 4바이트 버퍼: 이미지 데이터의 길이를 수신하기 위함
//    char buffer[4] = { 0 };
//    int recvResult = recv(connectSocket, buffer, sizeof(buffer), 0);
//
//    if (recvResult == 4) {  // 4바이트가 제대로 수신되었는지 확인
//        // 4바이트 데이터(이미지의 길이)를 빅 엔디안에서 리틀 엔디안으로 변환
//        int dataLen = ntohl(*reinterpret_cast<int*>(buffer));
//
//        // 이미지 데이터 수신을 위한 버퍼
//        std::vector<char> data(dataLen);
//        int totalReceived = 0;
//
//        // 이미지 데이터 수신
//        while (totalReceived < dataLen) {
//            recvResult = recv(connectSocket, data.data() + totalReceived, dataLen - totalReceived, 0);
//            if (recvResult <= 0) {
//                std::cerr << "recv failed: " << WSAGetLastError() << std::endl;
//                return "";
//            }
//            totalReceived += recvResult;
//        }
//
//        // 수신한 데이터를 string으로 반환
//        return std::string(data.begin(), data.end());
//    }
//    else {
//        std::cerr << "recv failed or incomplete data length: " << WSAGetLastError() << std::endl;
//        return "";
//    }
//}
std::string Server_connect_winsock::ReceiveMessage() {
    // 1. 초기 상태 확인
    char buffer[10] = { 0 };
    int recvResult = recv(connectSocket, buffer, sizeof(buffer), 0);
    if (recvResult <= 0) {
        std::cerr << "recv failed: " << WSAGetLastError() << std::endl;
        return "";
    }
    // 2. 서버 응답 확인
    if (strncmp(buffer, "Error", 5) == 0) {
        return "Error";
    }
    else if (strncmp(buffer, "Move_OK", 5) == 0) {
        return "Move_OK";
    }
    else if (strncmp(buffer, "Cam_OK", 5) == 0) {
        return "Cam_OK";
    }
    else if (strncmp(buffer, "Film_OK", 5) == 0) {
        return "Film_OK";
    }
    else if (strncmp(buffer, "OK", 2) == 0) {
        // 3. 이미지 사이즈 수신
        char sizeBuffer[4] = { 0 };
        recvResult = recv(connectSocket, sizeBuffer, sizeof(sizeBuffer), 0);
        if (recvResult != sizeof(sizeBuffer)) {
            std::cerr << "recv failed: " << WSAGetLastError() << std::endl;
            return "";
        }
        int dataLen = ntohl(*reinterpret_cast<int*>(sizeBuffer));

        // 4. 이미지 데이터 수신
        std::vector<char> data(dataLen);
        int totalReceived = 0;
        while (totalReceived < dataLen) {
            recvResult = recv(connectSocket, data.data() + totalReceived, dataLen - totalReceived, 0);
            if (recvResult <= 0) {
                std::cerr << "recv failed: " << WSAGetLastError() << std::endl;
                return "";
            }
            totalReceived += recvResult;
        }

        // 5. 이미지 데이터 반환
        return std::string(data.begin(), data.end());
    }
    else {
        std::cerr << "Unexpected server response: " << buffer << std::endl;
        return "";
    }
}

void Server_connect_winsock::CloseConnection() const {
    closesocket(connectSocket);
    WSACleanup();
}

//bool Server_connect_winsock::IsConnected() const {
//     //소켓 에러 여부 확인
//    char buffer[4096];
//    int recvResult = recv(connectSocket, buffer, sizeof(buffer), MSG_PEEK); // MSG_PEEK를 사용하여 데이터를 가져오지 않고 검사
//    if (recvResult == SOCKET_ERROR) {
//        //std::cerr << "recv failed: " << WSAGetLastError() << std::endl;
//        CloseConnection();
//        return false;
//    }
//    else if (recvResult == 0) {
//        //std::cerr << "Connection closed by peer." << std::endl;
//        CloseConnection();
//        return false;
//    }
//
//    return true;
//}
bool Server_connect_winsock::IsConnected() const {
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(connectSocket, &read_set);

    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    int result = select(0, &read_set, NULL, NULL, &timeout);
    if (result > 0) {
        // 소켓이 읽기 가능 상태이면 recv로 확인
        char buffer[1];
        int recvResult = recv(connectSocket, buffer, sizeof(buffer), MSG_PEEK);
        if (recvResult == 0) {
            // 소켓이 정상적으로 종료됨
            return false;
        }
        else if (recvResult == SOCKET_ERROR) {
            // 소켓 오류 발생
            return false;
        }
    }
    else if (result == 0) {
        // 타임아웃, 소켓 상태 유지
        return true;
    }
    else {
        // select에서 오류 발생
        return false;
    }

    return true;
}


bool Server_connect_winsock::CheckForMessage()
{
    // 소켓 상태 확인
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(connectSocket, &readfds);

    // 소켓 상태 체크
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    int ready = select(connectSocket + 1, &readfds, NULL, NULL, &timeout);

    // 메시지 도착 여부 확인
    if (ready > 0 && FD_ISSET(connectSocket, &readfds)) {
        // 메시지가 도착했음
        return true;
    }
    else {
        // 메시지가 도착하지 않음
        return false;
    }
}


