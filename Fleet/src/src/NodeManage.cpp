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

std::vector<char> NodeManage::serialize(bool includeGroups) const {
        std::vector<char> data;
        // Serialize commit_num
        data.insert(data.end(), reinterpret_cast<const char*>(&commit_num), reinterpret_cast<const char*>(&commit_num + 1));
        // Serialize id_to_sockaddr
        size_t map_size = id_to_sockaddr.size();
        data.insert(data.end(), reinterpret_cast<const char*>(&map_size), reinterpret_cast<const char*>(&map_size + 1));
        for (const auto& pair : id_to_sockaddr) {
            int id = pair.first;
            const sockaddr_in& addr = pair.second;
            data.insert(data.end(), reinterpret_cast<const char*>(&id), reinterpret_cast<const char*>(&id + 1));
            data.insert(data.end(), reinterpret_cast<const char*>(&addr), reinterpret_cast<const char*>(&addr + 1));
        }

        // Serialize sockaddr_to_ids
        map_size = sockaddr_to_ids.size();
        data.insert(data.end(), reinterpret_cast<const char*>(&map_size), reinterpret_cast<const char*>(&map_size + 1));
        for (const auto& pair : sockaddr_to_ids) {
            const sockaddr_in& addr = pair.first;
            data.insert(data.end(), reinterpret_cast<const char*>(&addr), reinterpret_cast<const char*>(&addr + 1));

            const std::vector<int>& ids = pair.second;
            size_t vector_size = ids.size();
            data.insert(data.end(), reinterpret_cast<const char*>(&vector_size), reinterpret_cast<const char*>(&vector_size + 1));
            for (int id : ids) {
                data.insert(data.end(), reinterpret_cast<const char*>(&id), reinterpret_cast<const char*>(&id + 1));
            }
        }

        // Conditionally serialize groups
        if (includeGroups) {
            map_size = groups.size();
            data.insert(data.end(), reinterpret_cast<const char*>(&map_size), reinterpret_cast<const char*>(&map_size + 1));
            for (const auto& pair : groups) {
                int groupId = pair.first;
                const GroupInfo& info = pair.second;
                data.insert(data.end(), reinterpret_cast<const char*>(&groupId), reinterpret_cast<const char*>(&groupId + 1));

                int leaderId = info.leaderId;
                data.insert(data.end(), reinterpret_cast<const char*>(&leaderId), reinterpret_cast<const char*>(&leaderId + 1));

                const std::vector<int>& memberIds = info.memberIds;
                size_t  vector_size = memberIds.size();
                data.insert(data.end(), reinterpret_cast<const char*>(&vector_size), reinterpret_cast<const char*>(&vector_size + 1));
                for (int memberId : memberIds) {
                    data.insert(data.end(), reinterpret_cast<const char*>(&memberId), reinterpret_cast<const char*>(&memberId + 1));
                }
            }
        }

        return data;
}


NodeManage NodeManage::deserialize(const std::vector<char>& data) {
    NodeManage nodeManage;
        size_t i = 0;
        // 反序列化 commit_num
        std::memcpy(&nodeManage.commit_num, &data[i], sizeof(nodeManage.commit_num));
        i += sizeof(nodeManage.commit_num);
        // Deserialize id_to_sockaddr
        size_t map_size;
        if (data.size() >= i + sizeof(size_t)) {
            std::memcpy(&map_size, &data[i], sizeof(map_size));
            i += sizeof(map_size);
        }

        for (size_t j = 0; j < map_size && i < data.size(); ++j) {
            int id;
            sockaddr_in addr;
            if (data.size() >= i + sizeof(int)) {
                std::memcpy(&id, &data[i], sizeof(id));
                i += sizeof(id);
            }
            if (data.size() >= i + sizeof(sockaddr_in)) {
                std::memcpy(&addr, &data[i], sizeof(addr));
                i += sizeof(addr);
            }
            nodeManage.id_to_sockaddr[id] = addr;
        }

        // Deserialize sockaddr_to_ids
        if (data.size() >= i + sizeof(size_t)) {
            std::memcpy(&map_size, &data[i], sizeof(map_size));
            i += sizeof(map_size);
        }

        for (size_t j = 0; j < map_size && i < data.size(); ++j) {
            sockaddr_in addr;
            if (data.size() >= i + sizeof(sockaddr_in)) {
                std::memcpy(&addr, &data[i], sizeof(addr));
                i += sizeof(sockaddr_in);
            }

            size_t vector_size;
            if (data.size() >= i + sizeof(size_t)) {
                std::memcpy(&vector_size, &data[i], sizeof(vector_size));
                i += sizeof(vector_size);
            }

            std::vector<int> ids;
            ids.reserve(vector_size);
            for (size_t k = 0; k < vector_size && i < data.size(); ++k) {
                int id;
                if (data.size() >= i + sizeof(int)) {
                    std::memcpy(&id, &data[i], sizeof(id));
                    i += sizeof(int);
                }
                ids.push_back(id);
            }
            nodeManage.sockaddr_to_ids[addr] = ids;
        }

        // Optional: Deserialize groups if data is still available
        if (i + sizeof(size_t) <= data.size()) {
            std::memcpy(&map_size, &data[i], sizeof(map_size));
            i += sizeof(map_size);

            for (size_t j = 0; j < map_size && i < data.size(); ++j) {
                int groupId;
                if (data.size() >= i + sizeof(int)) {
                    std::memcpy(&groupId, &data[i], sizeof(groupId));
                    i += sizeof(int);
                }

                int leaderId;
                if (data.size() >= i + sizeof(int)) {
                    std::memcpy(&leaderId, &data[i], sizeof(leaderId));
                    i += sizeof(int);
                }

                size_t member_size;
                if (data.size() >= i + sizeof(size_t)) {
                    std::memcpy(&member_size, &data[i], sizeof(member_size));
                    i += sizeof(member_size);
                }

                std::vector<int> members;
                members.reserve(member_size);
                for (size_t k = 0; k < member_size && i < data.size(); ++k) {
                    int memberId;
                    if (data.size() >= i + sizeof(int)) {
                        std::memcpy(&memberId, &data[i], sizeof(memberId));
                        i += sizeof(int);
                    }
                    members.push_back(memberId);
                }
                GroupInfo info{leaderId, members};
                nodeManage.groups[groupId] = info;
            }
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
