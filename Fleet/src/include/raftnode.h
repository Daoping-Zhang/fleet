#ifndef RAFTNODE_H
#define RAFTNODE_H
#include <vector>
#include <mutex>
#include <iostream>
#include <thread>
#include <condition_variable>
#include <cstring>
#include <unordered_map>
#include<string>
#include "confdeal.h"
#include"kvstore.h"
#include"message.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include"threadpool.h"
#include "log.h"
#include <atomic>
#include "NodeManage.h"
#include "Debug.h"
#include "ActiveIDTable.h"
#include "AppendResponse.h"
#include "FleetLog.h"
#include "FleetKVstore.h"
#include "FleetGroupSendData.h"
#include "FleetGroupReceiveData.h"
#include "Initialize.h"

#define MAXREQ 256
#define MAX_EVENT 20
#define THREAD_NUM 4

enum NodeState {
    FOLLOWER,
    CANDIDATE,
    LEADER
};//节点的状态，包括 FOLLOWER、CANDIDATE 和 LEADER 三种状态。


class Node {

    // 哈希函数定义
struct sockaddr_in_hash {
    std::size_t operator()(const sockaddr_in& addr) const {
        std::size_t h1 = std::hash<std::string>()(std::string(reinterpret_cast<const char*>(&addr.sin_addr), sizeof(addr.sin_addr)));
        std::size_t h2 = std::hash<int>()(addr.sin_port);
        return h1 ^ (h2 << 1);
    }
};

// 等于运算符定义
struct sockaddr_in_equal {
    bool operator()(const sockaddr_in& lhs, const sockaddr_in& rhs) const {
        return lhs.sin_addr.s_addr == rhs.sin_addr.s_addr && lhs.sin_port == rhs.sin_port;
    }
};


public:
    Node(string config_path);
    void Run();//节点运行
    ~Node()
    {
        if (accept_thread.joinable()) {
            accept_thread.join();
        }
        for(int i=0;i<num-1;i++)
        {
            close(send_fd[i]);
        }
    }
    std::vector<int> m_ids;
    std::unordered_map<int, std::vector<int>> m_groups;
    
    ActiveIDTable m_active_id_table;
    int m_leader_commit;
    std::vector<int> m_recovery_ids;
    bool m_recovery = false;

    std::unordered_map<sockaddr_in, int, sockaddr_in_hash, sockaddr_in_equal> m_addr_fd_map;


    void checkAndUpdateIds(); // 添加的用于检查和更新ID的方法
    bool is_fd_active(int fd, sockaddr_in addr);



private:
    void accept_connections();//管理连接，epoll机制
    void work(int fd);//接收信息
    void do_log();//处理日志
    void rebindInactiveIds(NodeManage& nodeManage, ActiveIDTable& activeTable);
    void generateGroupsMap();
    void sendGroupLog();
    void sendHeartBeat();
    void printAddressInfo(const sockaddr_in& addr);

    void updateIds(const std::vector<int>& newIds); // 添加的用于更新ID的方法
    void initializeNewId(int newId, bool init); // 初始化新ID的方法
    void removeOldIdData(int oldId); // 删除旧ID相关数据的方法
    

    void FollowerLoop();//跟随者状态循环，包括等待一段时间并转换为候选者状态
    void CandidateLoop();//候选者状态循环，
    //向其他节点发送 RequestVote 消息并等待投票结果，如果获得多数选票，则成为 Leader
    void LeaderLoop();//领导者状态循环，包括向其他节点发送 AppendEntries 消息并等待响应，如果收到多数节点的确认，则提交日志条目
    void sendmsg(int &fd,struct sockaddr_in addr,Message msg);//发送信息

    void executeEntries(int groupId, uint64_t now_commitIndex, uint64_t latestIndex, FleetKVStore& kv, FleetLog& log);
    
    bool m_init = true; //判断是否是刚开始


private:

    //all 通用：
    int id;//节点id
    int num;//整个集群节点数量
    //集群中的leader的id
    atomic<int> leader_id{0};
    int listenfd;//监听套接字
    thread accept_thread;//用于管理连接的线程
    thread deal_log_thread;//用于处理日志的线程
    struct sockaddr_in servaddr;//自己的地址
    vector<int> send_fd;//用于send信息给其他节点的socket
    atomic<NodeState> state{FOLLOWER};//当前状态
    atomic<int> current_term {0};//当前任期
    vector<sockaddr_in> others_addr;//其他节点的地址
    Log log;//该节点维护的日志
    FleetLog m_log;
    mutex mtx_append;
    kvstore kv;//该节点维护的数据库
    FleetKVStore m_kv;
    mutex send_mutex_;//发送锁，one by one，不能冲突

    NodeManage m_node_manage;
    

    //leader专属：
    atomic<int> live{0};//leader沙漏，刚开始成为leader时置为5，如果五个周期收不到任何回应，会丢失leader身份
    mutex mtx_match;
    vector<int> match_index;//match_index_[i]表示leader节点已经复制给节点i的最高日志条目的索引号
    vector<int> match_term;//match_index_[i]表示leader节点已经复制给节点i的最高日志条目的term

    //candidate专属：
    int num_votes = 0;//已获得的票数
    atomic<bool> voted{false};//是否已经在该term投票


    //follower专属：
    mutex mtx_del;
    atomic<bool> recv_heartbeat{false};//接收到心跳|同步信息的标志
    atomic<int> leader_commit_index{0};//当前集群中leader提交的条目index，自己不能超过这个
   
};
#endif





