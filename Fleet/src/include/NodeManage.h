// NodeManage.h
#ifndef NODE_MANAGE_H
#define NODE_MANAGE_H

#include <unordered_map>
#include <vector>
#include <netinet/in.h>

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

    std::vector<int> m_ids;

        // 序列化 NodeManage 数据
    std::vector<char> serialize(bool includeGroups) const;

    // 反序列化 NodeManage 数据
    static NodeManage deserialize(const std::vector<char>& data);


private:

    std::unordered_map<int, sockaddr_in> id_to_sockaddr;
    std::unordered_map<sockaddr_in, std::vector<int>, SockaddrInHash, SockaddrInEqual> sockaddr_to_ids;
    std::unordered_map<int, GroupInfo> groups;
};

#endif // NODE_MANAGE_H
