// NodeManage.cc
#include "NodeManage.h"
#include <cstring>
#include <set>
#include <algorithm> 


void NodeManage::addMapping(int id, const sockaddr_in& sockaddr) {
    // 检查 m_ids 中是否已经存在该 ID
    if (std::find(m_ids.begin(), m_ids.end(), id) == m_ids.end()) {
        m_ids.push_back(id);
    }

    // 检查 sockaddr_to_ids 中的该地址是否已经存在该 ID
    if (std::find(sockaddr_to_ids[sockaddr].begin(), sockaddr_to_ids[sockaddr].end(), id) == sockaddr_to_ids[sockaddr].end()) {
        sockaddr_to_ids[sockaddr].push_back(id);
    }

    id_to_sockaddr[id] = sockaddr;
   
}

sockaddr_in NodeManage::getSockaddrById(int id) {
    return id_to_sockaddr[id];
}

std::vector<int> NodeManage::getIdsBySockaddr(const sockaddr_in& sockaddr) {
    return sockaddr_to_ids[sockaddr];
}


void NodeManage::createGroup(int groupId, int leaderId) {
    groups[groupId].leaderId = leaderId;
    groups[groupId].memberIds.push_back(leaderId);
}

void NodeManage::addToGroup(int groupId, int id) {
    groups[groupId].memberIds.push_back(id);
}

std::vector<int> NodeManage::getIdsByGroup(int groupId) {
    return groups[groupId].memberIds;
}

int NodeManage::getLeaderIdByGroup(int groupId) {
    return groups[groupId].leaderId;
}

std::vector<int> NodeManage::getGroupIndicesWithMemberId(int memberId) {
    std::vector<int> indices;
    for (const auto& group : groups) {
        if (std::find(group.second.memberIds.begin(), group.second.memberIds.end(), memberId) != group.second.memberIds.end()) {
            indices.push_back(group.first);
        }
    }
    return indices;
}

std::string NodeManage::serialize() const {
        json response;
        json nodeArray = json::array();  // Nodes 数组
        json groupArray = json::array(); // Groups 数组

        // 添加 commit_num
        response["commit_num"] = commit_num;

        // 构造 nodes 部分
        for (const auto& pair : sockaddr_to_ids) {
            for (int id : pair.second) {
                json nodeInfo;
                nodeInfo["id"] = id;
                std::stringstream ipStream;
                ipStream << inet_ntoa(pair.first.sin_addr) << ":" << ntohs(pair.first.sin_port);
                nodeInfo["ip"] = ipStream.str();
                nodeArray.push_back(nodeInfo);
            }
        }

        // 构造 groups 部分
        for (const auto& group : groups) {
            json groupInfo;
            groupInfo["id"] = group.first;
            groupInfo["leader"] = group.second.leaderId;
            groupInfo["nodes"] = group.second.memberIds;
            groupArray.push_back(groupInfo);
        }

        response["nodes"] = nodeArray;
        response["groups"] = groupArray;

        return response.dump();
}

NodeManage NodeManage::deserialize(const std::string& s) {

        json data = json::parse(s);
        NodeManage nodeManage;
        nodeManage.commit_num = data["commit_num"].get<int>();

        // 解析 nodes
        for (const auto& nodeInfo : data["nodes"]) {
            int id = nodeInfo["id"].get<int>();
            std::string ipPort = nodeInfo["ip"].get<std::string>();
            size_t colonPos = ipPort.find(':');
            std::string ip = ipPort.substr(0, colonPos);
            int port = std::stoi(ipPort.substr(colonPos + 1));

            sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(static_cast<uint16_t>(port));
            inet_aton(ip.c_str(), &addr.sin_addr);

            nodeManage.sockaddr_to_ids[addr].push_back(id); // 假设每个地址可能有多个ID
        }

        // 解析 groups
        for (const auto& groupInfo : data["groups"]) {
            int groupId = groupInfo["id"].get<int>();
            int leaderId = groupInfo["leader"].get<int>();
            std::vector<int> memberIds = groupInfo["nodes"].get<std::vector<int>>();

            GroupInfo info;
            info.leaderId = leaderId;
            info.memberIds = memberIds;
            nodeManage.groups[groupId] = info;
        }

        return nodeManage;
}

void NodeManage::createGroupsFromIds(const std::vector<int>& m_ids) {
    if (m_ids.empty()) return;  // 如果列表为空，直接返回

    int num_ids = m_ids.size();
    for (int i = 0; i < num_ids; ++i) {
        int currentId = m_ids[i];
        int groupId = currentId;  // 使用当前 ID 作为组 ID
        createGroup(groupId, currentId);

        // 添加前一个 ID，循环到最后一个如果是第一个
        int prevId = (i == 0) ? m_ids[num_ids - 1] : m_ids[i - 1];
        addToGroup(groupId, prevId);

        // 添加后一个 ID，循环到第一个如果是最后一个
        int nextId = (i == num_ids - 1) ? m_ids[0] : m_ids[i + 1];
        addToGroup(groupId, nextId);
    }
}

sockaddr_in NodeManage::findAddressWithFirstId(int id) const {
    for (const auto& pair : sockaddr_to_ids) {
        if (!pair.second.empty() && pair.second.front() == id) {
            return pair.first;
        }
    }

    // 如果未找到，返回一个无效的 sockaddr_in 结构体
    sockaddr_in invalid_addr;
    invalid_addr.sin_family = AF_UNSPEC;
    return invalid_addr;
}

void NodeManage::recovery(const std::vector<int>& recovery_ids) {
    for (int id : recovery_ids) {
        // 检查是否存在与该 ID 绑定的地址
        unbinding(id,getSockaddrById(id));

        // 查找第一个 ID 为该 ID 的地址
        sockaddr_in new_addr = findAddressWithFirstId(id);

        // 检查找到的地址是否有效
        if (new_addr.sin_family != AF_UNSPEC) {
            // 重新绑定到找到的地址
            addMapping(id, new_addr);
        } else {
            std::cerr << "No valid address found for ID " << id << std::endl;
        }
    }
}

void NodeManage::unbinding(int id, const sockaddr_in& addr) {
    auto it = id_to_sockaddr.find(id);
    if (it != id_to_sockaddr.end() && it->second == addr) {
        // 输出已经解绑的信息
        std::cout << "ID " << id << " is already bound to the given address. Unbinding now." << std::endl;

        // 解除 ID 和地址的绑定
        id_to_sockaddr.erase(it);

        auto& ids = sockaddr_to_ids[addr];
        if (!ids.empty() && ids.front() == id) {
            // 如果地址中绑定的 ID 列表的第一个元素是该 ID，则不作处理
            return;
        } else {
            // 否则，从该地址的 ID 列表中删除该 ID
            ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());

            // 如果该地址不再绑定任何 ID，删除该地址
            if (ids.empty()) {
                sockaddr_to_ids.erase(addr);
            }
        }
    } else {
        // 给定的 ID 没有与给定的地址绑定
        std::cout << "ID " << id << " is not bound to the given address. No action taken." << std::endl;
    }
}

sockaddr_in NodeManage::findLeastUsedAddress(const std::vector<int>& activeIds) {
    std::unordered_map<sockaddr_in, int, SockaddrInHash, SockaddrInEqual> activeAddrUsage;
    sockaddr_in leastUsedAddr;
    int minUsage = std::numeric_limits<int>::max(); // 初始化为最大整数值，用于比较

    // 构建 activeAddrs 并计算每个地址的使用情况
    for (int activeId : activeIds) {
        sockaddr_in addr = getSockaddrById(activeId);
        activeAddrUsage[addr]++; // 计算这个地址的使用次数
    }

    // 找到绑定最少ID数量的地址
    for (const auto& addrPair : activeAddrUsage) {
        if (addrPair.second < minUsage) {
            minUsage = addrPair.second;
            leastUsedAddr = addrPair.first;
        }
    }

    // 如果未找到任何有效的地址，返回一个无效的 sockaddr_in 结构体
    if (minUsage == std::numeric_limits<int>::max()) {
        leastUsedAddr.sin_family = AF_UNSPEC;
    }

    return leastUsedAddr;
}



std::vector<int> NodeManage::findGroupIdsByLeaderId(int leaderId) {
        std::vector<int> matchingGroupIds;
        for (const auto& entry : groups) {
            if (entry.second.leaderId == leaderId) {
                matchingGroupIds.push_back(entry.first); // 添加匹配的 groupId
            }
        }
        return matchingGroupIds;
    }

json NodeManage::serializeNetworkInfo(int leader_id) const {
    json response;
    json nodeArray = json::array();  // Nodes 数组
    json groupArray = json::array(); // Groups 数组

    // 构造 nodes 部分
    for (const auto& pair : sockaddr_to_ids) {
        json nodeInfo;
        nodeInfo["id"] = pair.second;
        std::stringstream ipStream;
        ipStream << inet_ntoa(pair.first.sin_addr) << ":" << ntohs(pair.first.sin_port);
        nodeInfo["ip"] = ipStream.str();
        nodeArray.push_back(nodeInfo);
    }

    // 构造 groups 部分
    for (const auto& group : groups) {
        json groupInfo;
        groupInfo["id"] = group.first;
        groupInfo["leader"] = group.second.leaderId;
        groupInfo["nodes"] = group.second.memberIds;
        groupArray.push_back(groupInfo);
    }

    response["nodes"] = nodeArray;
    response["fleetLeader"] = leader_id;
    response["groups"] = groupArray;

    return response;
}

void NodeManage::printNetworkInfo(int leader_id) const {
    json networkInfo = serializeNetworkInfo(leader_id);
    std::cout << networkInfo.dump(4) << std::endl; // Pretty print with 4 spaces indentation
}
