// NodeManage.h
#ifndef NODE_MANAGE_H
#define NODE_MANAGE_H

#include <iostream>
#include <unordered_map>
#include <vector>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <limits>  // 添加这一行以包含 <limits> 头文件

/**
 * @file NodeManage.h
 * @brief NodeManage 类的头文件。
 *
 * NodeManage 类用于管理网络节点的映射，每个节点由一个整数ID标识，并与一个网络地址（sockaddr_in）相关联。
 * 一个ID对应一个地址，但一个地址可以与多个ID绑定。
 * 
 * 此外，它还支持将这些ID分组，并为每个组指定一个组长。
 */

// 完全定义用于哈希的结构
struct SockaddrInHash {
    size_t operator()(const sockaddr_in& addr) const {
        return std::hash<uint32_t>()(addr.sin_addr.s_addr) ^ std::hash<uint16_t>()(addr.sin_port);
    }
};

// 完全定义用于等值比较的结构
struct SockaddrInEqual {
    bool operator()(const sockaddr_in& a, const sockaddr_in& b) const {
        return a.sin_port == b.sin_port && a.sin_addr.s_addr == b.sin_addr.s_addr;
    }
};



// 定义存储组信息的结构体
struct GroupInfo {
    int leaderId;  // 组长ID
    std::vector<int> memberIds;  // 组员ID列表
};


// 重载 == 运算符
inline bool operator==(const sockaddr_in& a, const sockaddr_in& b) {
    return a.sin_port == b.sin_port && a.sin_addr.s_addr == b.sin_addr.s_addr;
}


class NodeManage {
    
public:
    /**
     * 将ID与网络地址关联。
     * @param id 节点的唯一标识符。
     * @param sockaddr 与ID关联的网络地址（sockaddr_in结构体）。
     */
    void addMapping(int id, const sockaddr_in& sockaddr);

    /**
     * 根据给定的ID检索其关联的sockaddr_in结构体。
     * @param id 需要检索网络地址的ID。
     * @return 与该ID关联的sockaddr_in结构体。
     */
    sockaddr_in getSockaddrById(int id);

    /**
     * 检索与特定网络地址关联的所有ID。
     * @param sockaddr 需要检索其关联ID的网络地址。
     * @return 与指定sockaddr_in关联的ID列表。
     */
    std::vector<int> getIdsBySockaddr(const sockaddr_in& sockaddr);

    /**
     * 创建一个具有指定组长的组。
     * @param groupId 组的标识符。
     * @param leaderId 组长的ID。
     */
    void createGroup(int groupId, int leaderId);

    /**
     * 将ID添加到指定的组中。
     * @param groupId 要添加到的组的标识符。
     * @param id 要添加到组中的ID。
     */
    void addToGroup(int groupId, int id);

    /**
     * 获取指定组中所有ID的列表。
     * @param groupId 需要检索ID的组的标识符。
     * @return 该组中所有ID的列表。
     */
    std::vector<int> getIdsByGroup(int groupId);

    std::vector<int> findGroupIdsByLeaderId(int leaderId);

    /**
     * 获取指定组的组长ID。
     * @param groupId 需要检索组长的组的标识符。
     * @return 该组的组长ID。
     */
    int getLeaderIdByGroup(int groupId);

    /**
     * 根据成员ID获取其所在的所有组的索引列表。
     * @param memberId 需要查询的成员ID。
     * @return 包含该成员的所有组的索引列表。
     */
    std::vector<int> getGroupIndicesWithMemberId(int memberId);

    void createGroupsFromIds(const std::vector<int>& m_ids);
    
    sockaddr_in findAddressWithFirstId(int id) const;
    
    void recovery(const std::vector<int>& recovery_ids);
    
    void unbinding(int id, const sockaddr_in& addr);

    sockaddr_in findLeastUsedAddress() const;


    std::vector<int> m_ids;

        // 序列化 NodeManage 数据
    std::vector<char> serialize(bool includeGroups) const;

    // 反序列化 NodeManage 数据
    static NodeManage deserialize(const std::vector<char>& data);
    std::unordered_map<int, sockaddr_in> id_to_sockaddr;
    std::unordered_map<sockaddr_in, std::vector<int>, SockaddrInHash, SockaddrInEqual> sockaddr_to_ids;
    std::unordered_map<int, GroupInfo> groups;
    int commit_num; //记录改动次数

    void printAllMappings() const {
        printIdToSockaddr();
        printSockaddrToIds();
        printGroups();
    }

    sockaddr_in findLeastUsedAddressForIds(const std::vector<int>& ids) {
    std::unordered_map<sockaddr_in, int, SockaddrInHash, SockaddrInEqual> addrCount;

    // 遍历输入的 ID，记录每个地址的使用频率
    for (int id : ids) {
        sockaddr_in addr = getSockaddrById(id);
        addrCount[addr]++;
    }

    // 找到绑定 ID 数最少的地址
    sockaddr_in leastUsedAddr;
    int minUsage = std::numeric_limits<int>::max(); // 初始化为最大整数值，用于比较

    for (const auto& pair : addrCount) {
        if (pair.second < minUsage) {
            minUsage = pair.second;
            leastUsedAddr = pair.first;
        }
    }

    // 检查是否找到有效地址
    if (minUsage == std::numeric_limits<int>::max()) {
        std::cerr << "No valid addresses found for given IDs." << std::endl;
        // 可以选择返回一个特定的错误值或异常
        leastUsedAddr.sin_addr.s_addr = INADDR_NONE; // 设置为无效地址
    }

    return leastUsedAddr;
}

private:
    void printIdToSockaddr() const {
        std::cout << "ID to sockaddr mappings:\n";
        for (const auto& pair : id_to_sockaddr) {
            std::cout << "ID " << pair.first << " -> IP: " << inet_ntoa(pair.second.sin_addr)
                      << ", Port: " << ntohs(pair.second.sin_port) << std::endl;
        }
    }

    void printSockaddrToIds() const {
        std::cout << "Sockaddr to IDs mappings:\n";
        for (const auto& pair : sockaddr_to_ids) {
            std::cout << "IP: " << inet_ntoa(pair.first.sin_addr) << ", Port: " << ntohs(pair.first.sin_port) << " -> IDs: ";
            for (int id : pair.second) {
                std::cout << id << " ";
            }
            std::cout << std::endl;
        }
    }

void printGroups() const {
        std::cout << "Groups mapping:\n";
        for (const auto& group : groups) {
            std::cout << "Group ID " << group.first << " with Leader ID " << group.second.leaderId << " has members: ";
            for (int id : group.second.memberIds) {
                std::cout << id << " ";
            }
            std::cout << std::endl;
        }
    }

    
};

#endif // NODE_MANAGE_H
