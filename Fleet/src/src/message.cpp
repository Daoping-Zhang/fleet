#include"message.h"
#include <netinet/tcp.h>
#include <fcntl.h>
#include <signal.h>
#include <nlohmann/json.hpp>

// 使用简化的命名空间
using json = nlohmann::json;

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


int sendMessage(int sockfd, json message) {
    handle_for_sigpipe(); // 设置SIGPIPE信号处理方式为忽略
    string serialized_message = message.dump();  // 序列化 JSON 对象为字符串
    int ret = send(sockfd, serialized_message.c_str(), serialized_message.size(), 0); 
    
    if (ret < 0) {
        if (errno == EPIPE || errno == ECONNRESET) {
            // 对方已经关闭了连接
            //std::cerr << "Connection closed by peer: " << strerror(errno) << std::endl;
            return -1;
        } else {
            // 发生了其他错误
            perror("send");
            //std::cerr << "Send error: " << strerror(errno) << std::endl;
            return -2;
        }
    }
    //std::cout << "sendMessage: Message sent successfully" << std::endl;
    return 1;
}

union Data {
    Message message;
    char stringData[sizeof(Message)];
};

int recvMessage(int fd, json& message_recv) {
    // 设置接收缓冲区
    std::vector<char> buffer(1024); // 大小为 1024 字节的接收缓冲区

    // 从文件描述符 fd 读取数据
    int bytes_received = read(fd, buffer.data(), buffer.size());
    if (bytes_received < 0) {
        std::cerr << "Failed to read data from socket." << std::endl;
        return -1;  // 读取失败
    } else if (bytes_received == 0) {
        std::cerr << "Connection closed." << std::endl;
        return 0;  // 连接已关闭
    }

    // 将接收到的数据转换为 std::string，然后尝试解析为 JSON
    try {
        std::string recv_data(buffer.begin(), buffer.begin() + bytes_received);
        message_recv = json::parse(recv_data);
    } catch (json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        return -2;  // JSON 解析错误
    }

    return bytes_received;  // 返回接收到的字节数
}

