#include"message.h"
#include <netinet/tcp.h>
#include <fcntl.h>
#include <signal.h>
Message toMessage(MessageType type, void* data, size_t size) {
    Message message;
    message.type = type;
    std::memcpy(message.data, data, size);
    return message;
}

//还原：
// 从 Message 中还原 RequestVote 结构体
RequestVote getRequestVote(const Message& message) {
    RequestVote requestVote;
    std::memcpy(&requestVote, message.data, sizeof(requestVote));
    return requestVote;
}

// 从 Message 中还原 VoteResponse 结构体
VoteResponse getVoteResponse(const Message& message) {
    VoteResponse voteResponse;
    std::memcpy(&voteResponse, message.data, sizeof(voteResponse));
    return voteResponse;
}

// 从 Message 中还原 AppendEntries 结构体
AppendEntries getAppendEntries(const Message& message) {
    AppendEntries appendEntries;
    appendEntries = *reinterpret_cast<const AppendEntries*>(message.data);
    return appendEntries;
}

// 从 Message 中还原 AppendResponse 结构体
AppendResponse getAppendResponse(const Message& message) {
    AppendResponse appendResponse;
    std::memcpy(&appendResponse, message.data, sizeof(appendResponse));
    return appendResponse;
}

// 从 Message 中还原 ClientResponse 结构体
ClientResponse getClientResponse(const Message& message) {
    ClientResponse clientResponse;
    clientResponse = *reinterpret_cast<const ClientResponse*>(message.data);
    return clientResponse;
}

ClientRequest getClientRequest(const Message& message) {
    ClientRequest clientRequest;
    clientRequest = *reinterpret_cast<const ClientRequest*>(message.data);
    return clientRequest;
}



string getString(const Message& message) {
    string result=message.data;
    return result;
}

void handle_for_sigpipe(){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    if(sigaction(SIGPIPE, &sa, NULL))
        return;
}


int sendMessage(int sockfd, Message message) {
    handle_for_sigpipe(); // 设置SIGPIPE信号处理方式为忽略
    int ret = send(sockfd, &message, sizeof(message), 0);
    if (ret < 0) {
        if (errno == EPIPE || errno == ECONNRESET) {
            // 对方已经关闭了连接
            return -1;
        }
        else {
            // 发生了其他错误
            perror("send");
            return -2;
        }
    }
    return 1;
}


union Data {
    Message message;
    char stringData[sizeof(Message)];
};

int recvMessage(int sockfd, Message& message) {
    if (sockfd == -1) {
        return -1;
    }
    Data data;
    int bytes = recv(sockfd, data.stringData, sizeof(data.stringData), 0);
    if (bytes < 0) {
        return -1;
    } else if (bytes == 0) {
        return -1;
    }
    if (bytes == sizeof(Message)) { // message received
        message = data.message;
    } else { // string received
        message.type = info;
        std::string str(data.stringData);
        message.data[0] = '\0'; // clear the data field
        str.copy(message.data, bytes); // copy at most 99 characters
    }
    memset(data.stringData, 0, sizeof(data.stringData));
    return 1;
}