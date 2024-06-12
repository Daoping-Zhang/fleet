#include "raftnode.h"
#include <chrono>
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <vector>
#include "confdeal.h"
#include <functional>
#include"threadpool.h"
#include <chrono>
#include <random>
#include <string>
#include <sstream>
#include <unordered_set>
#include"tools.h"
using json = nlohmann::json;


ThreadPool pool(THREAD_NUM);//线程池，用于work(回复收到的message)

std::default_random_engine generator1;//计时器，follower
std::uniform_int_distribution<int> distribution1(1000, 1001);//变化比较小，稳定
int delay1;

std::random_device rd;//真随机，竞争者的周期变化大，可以错开，避免所有node一直同时进入竞争者状态不给别人投票选不出leader
std::default_random_engine generator2(rd());
std::uniform_int_distribution<int> distribution2(500, 2000);
int delay2;

std::default_random_engine generator3;//计时器，leader
std::uniform_int_distribution<int> distribution3(520, 521);//变化比较小，稳定
int delay3;


//节点初始化，包括初始自己id和地址，监听接口，监听线程以及其他节点地址
Node::Node(string config_path){
    // 解析配置文件并初始化节点信息
    std::vector<std::vector<std::string>> node_info = parse_config(config_path);//解析配置文件
    if (node_info.empty()) {
        std::cerr << "100:Error: failed to parse config file" << std::endl;
        return;
    }

    //初始化log的日志文件（用于本地查看调试）
    log.file_name_=node_info[0][1]+".txt";
    std::ofstream outfile(log.file_name_, std::ios::trunc);// 以 trunc 模式打开文件，清空文件内容（初始化）
    outfile.close();  // 立即关闭文件

    // 初始化节点（自己）地址
    num = node_info.size();
    id=stoi(node_info[0][1])%10;//取port的尾数为id，这里要求集群的port是连号的

    cout<<id<<endl;
    cout<<id<<"start: follower"<<endl;
    send_fd.resize(num-1,-1);//初始化用于send 信息给其他节点的socket
   
    //初始化监听fd
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, node_info[0][0].c_str(), &servaddr.sin_addr);
    servaddr.sin_port = htons(std::stoi(node_info[0][1])); // 将第一个节点的端口号作为监听端口号
     /* Enable address reuse */
    int on = 1;
    // 打开 socket 端口复用, 防止测试的时候出现 Address already in use
    int result = setsockopt( listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on) );
    if (-1 == result) {
        perror ("Set socket");
        return ;
    }
    result = bind(listenfd, (const struct sockaddr *)&servaddr, sizeof (servaddr));
    if (-1 == result) {
        perror("Bind port");
        return ;
    }
    result = listen(listenfd, 5);
    if (-1 == result) {
        perror("Start listen");
        return ;
    }
    accept_thread = std::thread(std::bind(&Node::accept_connections, this));//开一个线程来进行监听，recv
    deal_log_thread=std::thread(std::bind(&Node::do_log, this));//开一个线程处理日志

    // 初始化其他节点信息（地址）
    for (int i = 1; i < num; ++i) {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        //cout<<node_info[i][0].c_str()<<" "<<node_info[i][1].c_str()<<endl;
        inet_pton(AF_INET, node_info[i][0].c_str(), &addr.sin_addr);
        addr.sin_port = htons(std::stoi(node_info[i][1]));
        others_addr.push_back(addr);
    }

    for (int i = 0; i< num; i++)
    {
        id=stoi(node_info[0][1])%10;
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        //cout<<node_info[i][0].c_str()<<" "<<node_info[i][1].c_str()<<endl;
        inet_pton(AF_INET, node_info[i][0].c_str(), &addr.sin_addr);
        addr.sin_port = htons(std::stoi(node_info[i][1]));
        m_node_manage.addMapping(stoi(node_info[i][1])%10, addr);
        m_active_id_table.setActive(stoi(node_info[i][1])%10, true);
    }

    m_ids = m_node_manage.getIdsBySockaddr(servaddr);
    //m_ids.push_back(id);
    m_node_manage.printAllMappings();
    m_init = true;
    m_leader_commit = 0;

    handle_for_sigpipe();// 设置SIGPIPE信号处理方式为忽略，这样当send_fd中的fd失效时，node send不会导致程序崩溃
}




//节点运行
void Node::Run() {
    while (true) {
        if (state == FOLLOWER) {

            FollowerLoop();
        } 
        else if (state== CANDIDATE) {

            CandidateLoop();
        } 
        else if (state== LEADER) {
            cout<<"inLeader"<<endl;

            LeaderLoop();
        }
    }
}



//follower，周期检验
void Node::FollowerLoop() {
    while (state==FOLLOWER) {
        //cout<<"now:follower"<<endl;
        //倒计时
        delay1 = distribution1(generator1);
        //cout<<"delay1"<<delay1<<endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(2*delay1));
        //检验
        if(recv_heartbeat==false)//如果一个周期结束了还没有收到心跳，那么转换为竞选者
        {
            state=CANDIDATE;
            current_term++;
            leader_id=0;//只有一个节点后，暂时不发送
            //if(current_term==1000000)current_term=0;//未考虑到超范围还

            cout<<id<<" become candidate"<<endl;
            cout<<"---------------"<<endl;
        }
        else{//收到心跳了
            recv_heartbeat=false;//重置心跳标志
        }
    }
}




//candidate，周期操作+检验
void Node::CandidateLoop() {
    while (state==CANDIDATE) {
        //cout<<"now:candidate"<<endl;
        //发起投票
        cout<<"inCandia"<<endl;

        num_votes=m_ids.size();//首先投自己一票
        voted=true;//已经投票，不给别人投了
        for(int i=0;i<num-1;i++)//向其他节点发送投票请求
        {
            RequestVote need_vote{current_term,turn_id(id,i),log.latest_index(),log.latest_term()};
            //竞选者当前term，id（转换过的），日志最新条目索引，日志最新条目索引的term
            Message message=toMessage(requestvote,&need_vote,sizeof(need_vote));//转换为通用信息格式
		    sendmsg(send_fd[i],others_addr[i],message);//发送给其他所有节点
            cout<<"sendmsg:"<<i<<endl;

        }
        //倒计时
        delay2 = distribution2(generator2);
        cout<<"delay2"<<delay1<<endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(delay2));
        //检验
        if(state==CANDIDATE)//如果还没成为leader，那么变回follower
        {
            state=FOLLOWER;
            cout<<id<<" become follower"<<endl;
            cout<<"---------------"<<endl;
            leader_commit_index=0;//重置，因为后续根据leader的心跳信息可更正
        }
        voted=false;//不是竞选者后，可为其他节点投票
    }
}


//leader，周期操作+检验
void Node::LeaderLoop() {
    while (state==LEADER) {
        //cout<<"now:leader"<<endl;
        //发送心跳信息
        if(m_init)
        {
            m_node_manage.createGroupsFromIds(m_node_manage.m_ids); //进行分组
            m_node_manage.printAllMappings();
            generateGroupsMap(); //找到含有与本地绑定的ID对应的组得到m_group_ids可以通过m_group_ids配合m_node_manage中的group信息找到组
             Debug::log("开始分组");

            m_leader_commit = 1;
            m_node_manage.commit_num = m_leader_commit;
            Message message;
            message.type = fleetControl;
            std::vector<char> serializedData = m_node_manage.serialize(true);
    
            // 确保不超出 data 数组的大小
            size_t dataSize = std::min(serializedData.size(), sizeof(message.data));

            // 使用 memcpy 将数据从 vector 复制到数组
            std::memcpy(message.data, serializedData.data(), dataSize);
            
            // 如果 serializedData 大小小于 message.data，可以考虑清零剩余部分
            if (dataSize < sizeof(message.data)) {
                std::memset(message.data + dataSize, 0, sizeof(message.data) - dataSize);
            }

            for(int i = 0; i<num-1;i++)
            {
               sendmsg(send_fd[i],others_addr[i],message);
            }

            delay3 = distribution3(generator3);
            std::this_thread::sleep_for(std::chrono::milliseconds(2*delay3));
            
           
            m_init = false;
            

                    for(int i=0;i<num-1;i++)
                    {
                        AppendEntries heartbeat;
                        heartbeat.leader_id=id;//leader的id，用于follower返回
                        heartbeat.term=current_term;//leader的term，用于同步所有节点的term
                        heartbeat.leader_commit=log.committed_index();//leader最新提交的目录索引
                        heartbeat.leader_commit = m_leader_commit;
                        if(m_active_id_table.getAllActiveIDs().size()>m_active_id_table.getAllInactiveIDs().size())
                        {
                            heartbeat.identify = true;
                        }
                        else
                        {
                            heartbeat.identify = false;
                        }
                        mtx_match.lock();
                        heartbeat.prev_log_index=match_index[i];//leader最近一条复制到follower（i)的条目的索引
                        heartbeat.prev_log_term=log.term_at(match_index[i]);//leader最近一条复制到follower（i)条目的周期
                        mtx_match.unlock();
                        for(int j=1;j<=3;j++){
                            if(match_index[i]+j<=log.latest_index()){//同步的entry
                                strncpy(heartbeat.entries[j-1],log.entry_at(match_index[i]+j).c_str(), sizeof(heartbeat.entries[j-1]));//要复制给follower（i）的条目的index
                                //heartbeat.entries[j][sizeof(heartbeat.entries[j]) - 1] = '\0';
                                heartbeat.entries_term[j-1]=log.term_at(match_index[i]+j);//要复制给follower（i）的条目的term
                            }
                            else{//如果复制给follower（i）的条目已经是leader的最新条目了，则用F标识（如果三条entry都是F，则是纯粹的心跳信息）
                                heartbeat.entries[j-1][0]='F';
                                heartbeat.entries_term[j-1]=0;
                            }
                        }
                        Message message=toMessage(appendentries,&heartbeat,sizeof(heartbeat));
                                    

                        sendmsg(send_fd[i],others_addr[i],message);
                    }                    


        }

        else
        {

        

                if(m_active_id_table.getAllActiveIDs().size()>m_active_id_table.getAllInactiveIDs().size())
            {   
                
                

                if(m_recovery)
                {
                    m_recovery = false;

                    m_node_manage.recovery(m_recovery_ids);
                    Debug::log("节点恢复");
                    m_node_manage.printAllMappings();

                    
                    
                    m_leader_commit++;
                    m_node_manage.commit_num = m_leader_commit;

                    checkAndUpdateIds();

                    Message message;
                    message.type = fleetControl;
                    std::vector<char> serializedData = m_node_manage.serialize(true);
            
                    // 确保不超出 data 数组的大小
                    size_t dataSize = std::min(serializedData.size(), sizeof(message.data));

                    // 使用 memcpy 将数据从 vector 复制到数组
                    std::memcpy(message.data, serializedData.data(), dataSize);
                    
                    // 如果 serializedData 大小小于 message.data，可以考虑清零剩余部分
                    if (dataSize < sizeof(message.data)) {
                        std::memset(message.data + dataSize, 0, sizeof(message.data) - dataSize);
                    }

                    for(int i = 0; i<num-1;i++)
                    {
                        sendmsg(send_fd[i],others_addr[i],message);
                    }

                    m_recovery_ids.clear();
                    delay3 = distribution3(generator3);
                    std::this_thread::sleep_for(std::chrono::milliseconds(2*delay3));


                }


                    if(!m_active_id_table.getAllInactiveIDs().empty())

                {
                    m_active_id_table.printActiveStates();
                    rebindInactiveIds(m_node_manage,m_active_id_table);

                    Debug::log("逻辑节点转移到新的物理节点");
                    m_node_manage.printAllMappings();
                
                    m_leader_commit++;
                    m_node_manage.commit_num = m_leader_commit;
                    
                    checkAndUpdateIds();

                    Message message;
                    message.type = fleetControl;
                    std::vector<char> serializedData = m_node_manage.serialize(true);
            
                    // 确保不超出 data 数组的大小
                    size_t dataSize = std::min(serializedData.size(), sizeof(message.data));

                    // 使用 memcpy 将数据从 vector 复制到数组
                    std::memcpy(message.data, serializedData.data(), dataSize);
                    
                    // 如果 serializedData 大小小于 message.data，可以考虑清零剩余部分
                    if (dataSize < sizeof(message.data)) {
                        std::memset(message.data + dataSize, 0, sizeof(message.data) - dataSize);
                    }

                    for(int i = 0; i<num-1;i++)
                    {
                        sendmsg(send_fd[i],others_addr[i],message);
                    }

                    delay3 = distribution3(generator3);
                    std::this_thread::sleep_for(std::chrono::milliseconds(2*delay3));

                }
                   
                    m_active_id_table.setAllInactive();                   
                    for (int id : m_ids)
                    {
                        m_active_id_table.setActive(id,true);
                    }

                    for(int i=0;i<num-1;i++)
                    {
                        AppendEntries heartbeat;
                        heartbeat.leader_id=id;//leader的id，用于follower返回
                        heartbeat.term=current_term;//leader的term，用于同步所有节点的term
                        heartbeat.leader_commit=log.committed_index();//leader最新提交的目录索引
                        heartbeat.leader_commit = m_node_manage.commit_num;
                        if(m_active_id_table.getAllActiveIDs().size()>m_active_id_table.getAllInactiveIDs().size())
                        {
                            heartbeat.identify = true;
                        }
                        else
                        {
                            heartbeat.identify = false;
                        }
                        mtx_match.lock();
                        heartbeat.prev_log_index=match_index[i];//leader最近一条复制到follower（i)的条目的索引
                        heartbeat.prev_log_term=log.term_at(match_index[i]);//leader最近一条复制到follower（i)条目的周期
                        mtx_match.unlock();
                        for(int j=1;j<=3;j++){
                            if(match_index[i]+j<=log.latest_index()){//同步的entry
                                strncpy(heartbeat.entries[j-1],log.entry_at(match_index[i]+j).c_str(), sizeof(heartbeat.entries[j-1]));//要复制给follower（i）的条目的index
                                //heartbeat.entries[j][sizeof(heartbeat.entries[j]) - 1] = '\0';
                                heartbeat.entries_term[j-1]=log.term_at(match_index[i]+j);//要复制给follower（i）的条目的term
                            }
                            else{//如果复制给follower（i）的条目已经是leader的最新条目了，则用F标识（如果三条entry都是F，则是纯粹的心跳信息）
                                heartbeat.entries[j-1][0]='F';
                                heartbeat.entries_term[j-1]=0;
                            }
                        }
                        Message message=toMessage(appendentries,&heartbeat,sizeof(heartbeat));
                                    

                        sendmsg(send_fd[i],others_addr[i],message);
                }                    

                sendGroupLog();

                
            }else
            {
                printf("Active num: %lu, Inactive num: %lu",m_active_id_table.getAllActiveIDs().size(), m_active_id_table.getAllInactiveIDs().size());
            }


        }

      
       
        






        //倒计时
        delay3 = distribution3(generator3);
        std::this_thread::sleep_for(std::chrono::milliseconds(2*delay3));
        //检验
        /*
        if(live==0)//如果五次没有follower响应，leader变成follower
        {
            state=FOLLOWER;
            cout<<id<<" become follower"<<endl;
            cout<<"---------------"<<endl;
            //leader_commit_index=log.latest_index();
        }
        else{
            live--;//活跃标志倒计（一共5*dealy3）
        }
        */
    }
}



//根据收到的信息进行对应的处理
void Node::work(int fd)
{
    Message message_recv{};//收到的message
    //cout<<"size: "<<recv_fd.size()<<endl;
    int ret=recvMessage(fd, message_recv);
    
    MessageType type=message_recv.type;//节点根据message的type来进行对应的处理

    if(ret!=-1){
        if(type==requestvote){//请求投票类型，candidtae->follwer
            if(state==FOLLOWER)
            {
                if(voted==false&&recv_heartbeat==false)//还没有投票的话,而且当前集群无leader
                {       
                    RequestVote recv_info;
                    recv_info=getRequestVote(message_recv);
                    if(recv_info.term>current_term)current_term=recv_info.term;//更新为candidate的term（更大的）
                    if((recv_info.term>=current_term)&&(recv_info.last_log_index>=log.latest_index())&&(recv_info.last_log_term>=log.latest_term()))
                    {//投票条件：candidate的term大于等于当前term，最新的log更新（或一样），最新log的term更大（或相等）
                        VoteResponse give_vote{current_term,true};
                        Message message=toMessage(voteresponse,&give_vote,sizeof(give_vote));
                        sendmsg(send_fd[recv_info.candidate_id],others_addr[recv_info.candidate_id],message);
                        voted=true;//表示已投票
                    }
                    recv_heartbeat=true;//重置倒计时
                }
            }
        }
         else if(type==fleetControl)
         {
            
            
            std::vector<char> data(message_recv.data, message_recv.data+500);
            
            m_node_manage = m_node_manage.deserialize(data);
            if(m_init)
            {
                Debug::log("初始化");
                //m_node_manage.createGroupsFromIds(m_node_manage.m_ids);
                int id = m_ids[0];
                m_ids.clear();
                m_ids[0] = id;
                
    
                if(m_ids[0]!=m_node_manage.groups[m_ids[0]].leaderId)
                {
                    
                }

                

                initializeNewId(m_ids[0], true);



                m_init = false;
            }
            Debug::log("同步node Manage");

            checkAndUpdateIds();

            generateGroupsMap(); //必须先更新m_ids;
            m_node_manage.printAllMappings();

            recv_heartbeat = true;
          

         }
        else if(type==voteresponse){//投票类型，follower->candidate
            if(state==CANDIDATE)
            {
                VoteResponse recv_info;
                if(recv_info.term>current_term)current_term=recv_info.term;//更新为leader的term（如果更大）
                recv_info=getVoteResponse(message_recv);
                if(recv_info.vote_granted==true)//这里还没考虑term！
                {
                    num_votes++;
                    cout<<"num_votes"<<num_votes<<endl;
                }
                if(num_votes>num/2){ 
                    cout<<id<<" become a leader"<<endl;
                    cout<<"---------------"<<endl;
                    match_index.resize(num-1,log.latest_index());
                    match_term.resize(num-1,log.term_at(log.latest_index()));
                    live=5;
                    state=LEADER;
                }//一旦票数大于一半，就变成leader
            }
        }
        else if(type==appendentries)//心跳（条目数为0）|日志同步信息（更新条目），leader->follower
        {
            AppendEntries recv_info;
            recv_info=getAppendEntries(message_recv);
            if(state==FOLLOWER)
            {
                AppendResponse res;
                res.follow_ids= m_ids;//告诉leader自己的id
                if(m_node_manage.commit_num < recv_info.leader_commit)
                {
                    res.identify = true;
                    m_init = true;
                }
                else
                {
                    res.identify = false;
                }

                if(true)
                {
                    sendGroupLog();     
                }
                recv_heartbeat=true;//设置心跳标志
                if(recv_info.term>current_term)current_term=recv_info.term;//如果比leader的term小，那么更新为leader的term
                res.term=current_term;//返回自己的term（可能比leader的大）
                res.follower_commit=log.committed_index();
                if(recv_info.leader_commit>log.committed_index())
                    leader_id=recv_info.leader_id;//更新leader的id
                leader_commit_index=recv_info.leader_commit;//设置leader的最后一条提交的日志（赞同数超过一半）索引（follower提交不能超过这个数）
                if(((log.latest_index()==recv_info.prev_log_index)&&(log.term_at(log.latest_index())==recv_info.prev_log_term)))//||(recv_info.prev_log_index==0)
                {//follower节点的最新条目的索引和term与leader发过来的验证信息匹配，或者leader发现follower日志为空（或者全都不匹配）
                    int i=0;
                    while((i<3)&&(recv_info.entries[i][0]!='F'))//根据leader发送的条目进行更新(有效条目)
                    {
                        cout<<"follower "<<id<<" 响应leader "<<leader_id<<"同步请求: "<<endl;
                        cout<<id<<" 最新的条目索引 "<<log.latest_index()<<" leader确认索引 "<<recv_info.prev_log_index<<endl;
                        cout<<recv_info.prev_log_index<<"+ "<<i+1<<": "<<recv_info.entries[i]<<endl;
                        string str(recv_info.entries[i]);
                        mtx_append.lock();
                        log.append(str,recv_info.entries_term[i]);//把leader的条目索引和周期加入自己的日志
                        mtx_append.unlock();
                        i++;
                    }
                    res.log_index=recv_info.prev_log_index+i;//follower成功复制到自己的日志的条目的索引
                    res.success=true;//同步成功       
                }
                else//如果未匹配
                {
                    if(log.latest_index()>recv_info.prev_log_index)//follwer的index比leader确认的大，（leader还没收到response)
                    {
                        if(recv_info.leader_commit>log.committed_index()){
                            //该node的最新条目数没有leader多，同步，删除自己不匹配的那些条目
                            //如果自己的日志比leader的多，那么删了多的，一般用在leader转换成follower后收到新的leader的同步信息
                            cout<<id<<": 不匹配，删掉了 "<<recv_info.prev_log_index+1<<" 到 "<<log.latest_index()<<"的日志"<<endl;
                            log.erase(recv_info.prev_log_index+1,log.latest_index());
                        }
                        res.log_index=-1;
                    }
                    else if(log.latest_index()<recv_info.prev_log_index){//follwer的index比leader确认的小，（follower需要leader回滚)
                        res.log_index=recv_info.prev_log_index-1;//回滚？
                        cout<<id<<": 请求leader回滚,因为："<<endl;
                        cout<<"我的最新的条目索引 "<<log.latest_index()<<"leader确认索引 "<<recv_info.prev_log_index<<endl;
                        cout<<"我的最新的条目周期 "<<log.term_at(log.latest_index())<<"leader确认周期 "<<recv_info.prev_log_term<<endl;
                        cout<<"不相等"<<endl;
                    }  
                    res.success=false;//同步失败
                }     
                Message message;
                message.type = appendresponse;

                std::vector<char> serializedData = res.serialize();
    
                // 确保不超出 data 数组的大小
                size_t dataSize = std::min(serializedData.size(), sizeof(message.data));

                // 使用 memcpy 将数据从 vector 复制到数组
                std::memcpy(message.data, serializedData.data(), dataSize);
                
                // 如果 serializedData 大小小于 message.data，可以考虑清零剩余部分
                if (dataSize < sizeof(message.data)) {
                    std::memset(message.data + dataSize, 0, sizeof(message.data) - dataSize);
                }


                //cout<<"leader_id"<<leader_id<<endl;
                sendmsg(send_fd[leader_id],m_node_manage.getSockaddrById(leader_id),message);
            }
            else if(state==LEADER)//1,如果一个leader长时间未发送心跳给follower，那么其他节点会选出新的leader，这种情况下会收到心跳信息
            {
                if(recv_info.leader_commit>log.committed_index())//选择提交数目更多的
                {//自己已经不是最新的 Leader，会立即放弃自己的 Leader 身份，并更新自己的任期为接收到的 AppendEntries 消息中的任期
                    state=FOLLOWER;
                    current_term=recv_info.term;
                    cout<<id<<"become a follower"<<endl;
                    cout<<"---------------"<<endl;
                    leader_commit_index=0;
                }
            }
        }
        else if (type == initializeRequestData)
        {
            InitializeData recv_info;
            std::string receivedString(message_recv.data);
            recv_info=recv_info.deserialize(receivedString);

            int group_id = recv_info.group_id;



            

            uint64_t commitIndex = m_log.getCommittedIndex(recv_info.group_id);
            uint64_t latestIndex = m_log.getLatestIndex(recv_info.group_id);

            std::string serializeGroupData = m_kv.serializeGroup(group_id);

            InitializeData send_data(m_ids[0], group_id, latestIndex, commitIndex, serializeGroupData);
            
            Message message;
            message.type = initializeBackData;

            std::string serializedSend_data = send_data.serialize();

            std::memset(message.data, 0, sizeof(message.data));

                                // 复制字符串到 char 数组中
                                // 使用 std::min 确保不会超出字符串长度和数组大小的较小值
                                
            size_t numBytesToCopy = std::min(serializedSend_data.size(), sizeof(message.data) - 1);

            std::memcpy(message.data, serializedSend_data.c_str(), numBytesToCopy);

            int send_id = recv_info.m_id;

            sendmsg(send_fd[send_id],m_node_manage.getSockaddrById(send_id),message);

            Debug::log("发送快照");


        }
        else if (type == initializeBackData)
        {

            InitializeData recv_info;
            std::string receivedString(message_recv.data);
            recv_info=recv_info.deserialize(receivedString);

            int group_id = recv_info.group_id;

            m_kv.deserializeGroup(group_id, recv_info.serializedData);

            m_log.latest_index[group_id] = recv_info.latestIndex;
            m_log.committed_index[group_id] = recv_info.commitIndex;

            Debug::log("恢复一组数据");

        }
        else if (type == fleetGroupSendData)
        {
            
            FleetGroupSendData recv_info;
            std::string receivedString(message_recv.data);
            recv_info=recv_info.deserialize(receivedString);
            uint64_t commitIndex = m_log.getCommittedIndex(recv_info.group_id);
            uint64_t latestIndex = m_log.getLatestIndex(recv_info.group_id);
            


            if(commitIndex < recv_info.commitIndex)
            {
                Debug::log("同步提交");

                executeEntries(recv_info.group_id, commitIndex, recv_info.commitIndex, m_kv, m_log);
            }

            if(latestIndex <= recv_info.latestIndex)
            {
                FleetGroupReceiveData send_data;
                send_data.group_id = recv_info.group_id;    

                std::vector<int> group = m_node_manage.getMembersByGroupId(recv_info.group_id);

                Debug::log("收到小组长提交的日志");
                std::vector<LogEntry> uncommittedEntries = m_log.deserializeLogEntries(recv_info.entriesSerialized);
                for(LogEntry entry : uncommittedEntries)
                {
                    m_log.append(recv_info.group_id, entry);
                    Debug::log("添加日志");
                    std::cout<<recv_info.group_id<<std::endl;
                }

                send_data.latestIndex = recv_info.latestIndex;

                int vote_num = 0;

                for (int id : m_ids)
                {

                    
                    if (std::find(group.begin(), group.end(), id) != group.end())
                    {
                        vote_num++;
                        send_data.ids.push_back(id);
                    }
                    
                }


                        
                send_data.vote_num = vote_num;
                send_data.success = true;


                Message message;
                message.type = fleetGroupReceiveData;

                std::string serializedSend_data = send_data.serialize();

                std::memset(message.data, 0, sizeof(message.data));

                                // 复制字符串到 char 数组中
                                // 使用 std::min 确保不会超出字符串长度和数组大小的较小值
                                
                size_t numBytesToCopy = std::min(serializedSend_data.size(), sizeof(message.data) - 1);

                std::memcpy(message.data, serializedSend_data.c_str(), numBytesToCopy);

                int send_id = m_node_manage.getLeaderIdByGroup(recv_info.group_id);

                sendmsg(send_fd[send_id],m_node_manage.getSockaddrById(send_id),message);

                      
            }

            

        }

        else if (type == fleetGroupReceiveData)
        {
            
            FleetGroupReceiveData recv_info;
            std::string receivedString(message_recv.data);

            recv_info = recv_info.deserialize(receivedString);

            int group_id = recv_info.group_id;
            Debug::log("收到投票");

            std::cout<<"groupId: "<< group_id<<endl;

            uint64_t latestIndex = recv_info.latestIndex;
            uint64_t now_commitIndex = m_log.getCommittedIndex(group_id);

            recv_info=recv_info.deserialize(receivedString);
            if(recv_info.success)
            {
                for(int id : recv_info.ids)
                
                {
                    mtx_append.lock();
                    m_log.addVote(group_id,latestIndex,id);
                    mtx_append.unlock();
                    Debug::log("投票+1");
                    
                    std::cout<<"group_id"<<group_id<<std::endl;
                    
                }

                 // 检查是否有一半以上的节点投票
                if (m_log.vote_count[group_id][latestIndex] > static_cast<int> (m_node_manage.getMembersByGroupId(group_id).size() / 2)
                   && now_commitIndex < latestIndex) {
                    Debug::log("开始");

                    mtx_append.lock();
                    executeEntries(group_id, now_commitIndex, latestIndex , m_kv, m_log);
                    mtx_append.unlock();

                    

                    Debug::log("成功提交");
                    
                    m_log.commit(group_id, latestIndex);
                }
                
            }            
        }
        else if(type==appendresponse)//回应心跳|同步信息 follower->leader
        {
            if(state==LEADER)
            {

                AppendResponse recv_info;
                recv_info=recv_info.deserialize(message_recv.data,1);
                if(recv_info.identify)
                {
                    m_recovery_ids.push_back(recv_info.follow_ids[0]);
                    m_recovery = true;
                    m_active_id_table.setActive(recv_info.follow_ids[0],true);
                    Debug::log("need recovery");
                }else
                {
                    for (int id : recv_info.follow_ids)
                        {
                            m_active_id_table.setActive(id,true);
                        }
                }
                int follower_id = recv_info.follow_ids[0];
                if(recv_info.term>current_term)current_term=recv_info.term;//如果follower的term比leader大。那么更新为大的term
                if(recv_info.success==true)
                {
                    for(int i=match_index[fd_id(id,follower_id)]+1;i<=recv_info.log_index;i++)//刚刚成功复制给follower的条目
                    {
                        log.add_num(i-1,follower_id);
                        //cout<<"条目"<<i<<"获得了"<<recv_info.follower_id<<"的响应"<<endl;
                    }  
                }    

                if(recv_info.follower_commit>log.committed_index()){
                    state=FOLLOWER;
                    cout<<id<<"不是提交条目数最多的leader"<<endl;
                    cout<<id<<"become a follower"<<endl;
                    cout<<"---------------"<<endl;
                    leader_commit_index=0;
                    return ;
                }
                mtx_match.lock();
                if(recv_info.log_index!=-1)//代表follwer把多余的删了，此时match不用变，等待回滚匹配，以免造成死循环 0  3 2 1|回滚| 3 0 3 2 1|回滚|  3 0 ...
                {  
                    match_index[fd_id(id,follower_id)]=recv_info.log_index;//匹配follower最新更新到的条目索引
                    if(match_index[fd_id(id,follower_id)]!=0)//如果follower返回的index不为0，则更新相应node的match index为返回值
                        match_term[fd_id(id,follower_id)]=log.term_at(match_index[fd_id(id,follower_id)]);//match index相应的term
                    else{
                        match_term[fd_id(id,follower_id)]=0;//index为0，说明follower中没有匹配的或者为空，此时baterm设置为0来匹配follower
                    }     
                }
               
                
                mtx_match.unlock();
                live=5;//翻转leader沙漏
            }
        }
        else if(type==clientresponse)//leader对follower（作为中转）传过来的客户请求的回应，leader->follower
        {
            ClientResponse recv_info;
            recv_info=getClientResponse(message_recv);
            int ret = send(recv_info.client_fd, &recv_info.message, sizeof(recv_info.message), 0);
            if(ret!=-1)cout<<id<<" 成功响应客户端(follower)"<<": "<<recv_info.message<<endl;
            //cout<<recv_info.fd<<endl;
            //cout<<recv_info.message<<endl;
        }
        else if(type==clientrequest)//follower（作为中转）传给leader的客户请求，follower->leader
        {
            //cout<<"get a req from follower"<<endl;
            while(state==CANDIDATE){//如果是竞争者，阻塞
                
            }
            if(state==FOLLOWER){//这种情况是leader在接收到消息时变成了follower
                while(leader_id==0){//还不知道leader的id，不能乱发，否则会越过数组范围，阻塞
                    if(state==LEADER){
                        goto leader_work1;//如果发现自己又变成leader了，执行leader的操作
                    }
                }
                sendmsg(send_fd[leader_id],others_addr[leader_id],message_recv);//知道leader id（有效的）了，可以再转发给新的leader
                //这里有一个巧妙的地方，新的leader会把消息返回给连接客户端的那个follwer，而不是第二或者更多次转发的follower，因为传递的message中的id号没变
            }
            else if(state==LEADER){

            leader_work1:
                while(log.committed_index()<log.latest_index())
                {
                    //如果还没提交过去的日志，那么阻塞等待，比如set后get，不等待set执行完后再get,会出错   
                }
                ClientRequest recv_info;
                recv_info=getClientRequest(message_recv);
                int NodeId=recv_info.node_id;//连接客户端的node的id
                int ClientFd=recv_info.client_fd;//node连接客户端的套接字
                string str(recv_info.message);
                mtx_append.lock();
                log.append(str,current_term);//提交日志
                mtx_append.unlock();
                std::vector<std::string> result = parse_string(str);//解析请求
                //构造返回报文（->node connect to client)
                ClientResponse res;
                memset(res.message,0,sizeof(res.message));
                res.client_fd=ClientFd;
                res.node_id=NodeId;
                string msg;
                if(result[0]=="GET")
                {
                    cout<<"get a get ask(from follwer)"<<endl;
                    msg=to_redis_string(kv.get(result[1]));
                    if(kv.get(result[1])=="")msg="1\r\n$3\r\nnil\r\n";//找不到key对应的value，返回nil
                    std::strcpy(res.message, msg.c_str());
                }
                else if(result[0]=="DEL")
                {
                    int count=0;
                    for(int k=1;k<=static_cast<int>(result.size())-1;k++){//返回有效的删除数
                        if(kv.get(result[k])!="")
                            count++;
                    }
                    cout<<"get a del ask(leader)"<<endl;
                    msg=":"+to_string(count)+"\r\n";
                    std::strcpy(res.message, msg.c_str());
                }
                else if(result[0]=="SET")
                {
                    cout<<"get a set ask(from follwer)"<<endl;//返回ok就是
                    msg="+OK\r\n";
                    std::strcpy(res.message, msg.c_str());
                }
                //while(log.committed_index()<log.latest_index()){//这段其实应该是必要的，为了快点响应就先注释了
                    //确认提交了该条目才继续
                //}
                Message res_msg=toMessage(clientresponse,&res,sizeof(res));
                sendmsg(send_fd[NodeId],others_addr[NodeId],res_msg);
            }
        }
        else if(type==info){//客户端client的请求，client->leader（直接回应）|follower（中转）|candidate(fail)
            string s = getString(message_recv);
            Debug::log("收到client请求");
            json receiveJson = json::parse(s);

            std::string key = receiveJson["key"];
            std::string value = receiveJson.value("value", ""); // 如果没有 value，则默认为空字符串
            std::string method = receiveJson["method"];
            uint64_t hashKey = receiveJson["hashKey"];
            int groupId = receiveJson["groupId"];

                // 打印 JSON 对象中的字段
            std::cout << "Key: " << key << std::endl;
            std::cout << "Value: " << value << std::endl;
            std::cout << "Method: " << method << std::endl;
            std::cout << "HashKey: " << hashKey << std::endl;
            std::cout << "GroupId: " << groupId << std::endl;
            

            LogEntry entry(key, value, method, hashKey, m_node_manage.commit_num);
            uint64_t commitIndex = m_log.getCommittedIndex(groupId);
            uint64_t latestIndex = m_log.getLatestIndex(groupId);  
            std::cout<<commitIndex<<std::endl;
            std::cout<<latestIndex<<std::endl;        
            mtx_append.lock();
            m_log.append(groupId, entry);
            mtx_append.unlock();
            commitIndex = m_log.getCommittedIndex(groupId);
            latestIndex = m_log.getLatestIndex(groupId);  
            std::cout<<commitIndex<<std::endl;
            std::cout<<latestIndex<<std::endl;   
            while(false)
            {

            }
            if(state==LEADER){//client->leader

            
                cout<<"get a client ask(leader)"<<endl;
                while(log.committed_index()<log.latest_index()){
                
                }
                mtx_append.lock();
                log.append(s,current_term);//提交日志
                mtx_append.unlock();
                std::vector<std::string> result = parse_string(s);
                //cout<<result[0]<<endl;
                string msg;
                if(result[0]=="GET")
                {
                    cout<<"get a get ask(leader)"<<endl;
                    msg=to_redis_string(kv.get(result[1]));
                    if(kv.get(result[1])=="")msg="1\r\n$3\r\nnil\r\n";
                }
                else if(result[0]=="DEL")
                {
                    int count=0;
                    for(int k=1;k<=static_cast<int>(result.size())-1;k++){
                        if(kv.get(result[k])!="")
                            count++;
                    }
                    cout<<"get a del ask(leader)"<<endl;
                    msg=":"+to_string(count)+"\r\n";
                }
                else if(result[0]=="SET")
                {
                    cout<<"get a set ask(leader)"<<endl;
                    msg="+OK\r\n";
                }
                //while(log.committed_index()<log.latest_index()){
                    //请求被提交了才返回信息给客户端
                //}
                int ret = send(fd, msg.c_str(), msg.size(), 0);
                if(ret!=-1)cout<<id<<" 成功响应客户端(leader)"<<": "<<msg<<endl;
            }
        }
    }
}



//单独一个线程来处理日志上面的内容
void Node::do_log()
{
    while(1)
    {
        mtx_append.lock();
        int start=log.committed_index()+1;
        mtx_append.unlock();
        if(start>log.latest_index())//条件一：日志中有未提交的条目
        {
            continue;//不提交（跳过）
        }
        if(state==FOLLOWER){//条件二：当node是follower时，提交的日志不能比leader提交的日志条目的index高
            if(start>leader_commit_index){
                continue;
            }
            else{
                //cout<<id<<" 提交第"<<start<<"条目(follower)"<<endl;
            }
        }
        if(state==LEADER){//条件三：当node是leader时，可以提交的条目的需要满足已经成功复制给超过半数节点
            if(log.get_num(start)<(num-1)/2)
            {
                continue;
            }
            else{
                //cout<<id<<" 提交第"<<start<<"条目(leader)"<<endl;
            }
        }
        cout<<id<<"开始处理log"<<log.entry_at(start)<<endl;
        std::vector<std::string> result = parse_string(log.entry_at(start));
        if(result[0]=="GET")
        {
            //kv.get(result[1]);
            log.commit(start);
        }
        else if(result[0]=="SET")
        {
            string value=result[2];
            for(int i=3;i<static_cast<int>(result.size());i++)
            {
                value+=" "+result[i];
            }
            kv.set(result[1],value);
            log.commit(start);
        }
        else if(result[0]=="DEL"){
            for(int i=1;i<static_cast<int>(result.size());i++)
            {
                kv.del(result[i]);
            }
            log.commit(start);
        }
        start++;
    
    }
}




//管理连接，监听收到的消息
void Node::accept_connections() {
    // epoll 实例 file describe
    int epfd = 0;
    int result = 0;
    struct epoll_event ev, event[MAX_EVENT];
    // 创建epoll实例
    epfd = epoll_create1(0);
    if (1 == epfd) {
        perror("Create epoll instance");
        return ;
    }
    ev.data.fd = listenfd;
    ev.events = EPOLLIN;
    // 设置epoll的事件
    result = epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);

    if(-1 == result) {
        perror("Set epoll_ctl");
        return ;
    }
    while (1) {
        int wait_count;
        // 等待事件
        wait_count = epoll_wait(epfd, event, MAX_EVENT, -1);
        for (int i = 0 ; i < wait_count; i++) {
            uint32_t events = event[i].events;
            int __result;
            if ( events & EPOLLERR || events & EPOLLHUP || ((!events) & EPOLLIN)) {
                printf("Epoll has error\n");
                close (event[i].data.fd);
                continue;
            } else if (listenfd == event[i].data.fd) {
                // listen的 file describe 事件触发， accpet事件
                struct sockaddr_in client_addr;
                socklen_t addr_size = sizeof(client_addr);
                int accp_fd = accept(listenfd, (struct sockaddr *)&client_addr, &addr_size);
                if (-1 == accp_fd) {
                    perror("Accept");
                    continue;
                }
                else{
                    //cout<<"find a node!"<<endl;
                }
                //recv_fd.push_back(accp_fd);
                ev.data.fd = accp_fd;
                ev.events = EPOLLIN | EPOLLET;
                // 为新accept的 file describe 设置epoll事件
                __result = epoll_ctl(epfd, EPOLL_CTL_ADD, accp_fd, &ev);
                if (-1 == __result) {
                    perror("epoll_ctl");
                    return ;
                }
            } else {//收到消息，使用work来对应处理收到的信息
                int fd=event[i].data.fd;
                pool.enqueue([this,fd] {
                    work(fd);
                });
            }
        }
    }
}




//用于节点来send信息的函数（包括创建连接,重连,这样可以通过发送信息（心跳，投票等)来快速和新加入的节点连接）
//当连接断开后，也不会中断（sendMessage(fd,msg)防止中断）
void Node::sendmsg(int &fd,struct sockaddr_in addr,Message msg){
    lock_guard<std::mutex> lock(send_mutex_);  // 加锁
    int ret;
    if(fd!=-1)
    {
        //cout<<"send"<<endl;
        ret = sendMessage(fd,msg);
        //cout<<"end"<<endl;
    }
    if (fd==-1||ret <0) { // 未建立连接或者发送消息失败
        // 关闭旧的套接字
        if(ret<0)close(fd);
        // 创建新的套接字
        fd = socket(AF_INET, SOCK_STREAM, 0);
        // 连接服务器
        ret = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
        if (ret <0) { // 重新连接失败
            // 处理连接失败的情况
            //cout << "reconnect failed" << endl;
            fd=-1;
        } 
        else { // 重新连接成功
            // 发送消息
            //cout<<"连接node成功"<<endl;
            ret = sendMessage(fd,msg);
            if (ret ==-1) { // 发送消息失败
                // 处理发送消息失败的情况
                cout << "close connect" << endl;
            }
            else
            {
                cout<<"no idea"<<endl;
            }
        }
    } 
}


void Node::generateGroupsMap()
{
       
       for (int id : m_ids) {
            std::vector<int> groupIndices = m_node_manage.getGroupIndicesWithMemberId(id);
            m_groups[id] = groupIndices;
            
        }
   
}

void Node::rebindInactiveIds(NodeManage& nodeManage, ActiveIDTable& activeTable) {
    std::vector<int> inactiveIds = activeTable.getAllInactiveIDs(); // 获取所有不活跃的 ID
    std::vector<int> activeIds = activeTable.getAllActiveIDs(); // 获取所有活跃的 ID

    // 将所有不活跃的 ID 重新绑定到最少使用的地址
    for (int inactiveId : inactiveIds) {
        nodeManage.unbinding(inactiveId, nodeManage.getSockaddrById(inactiveId));//接触现有绑定
        sockaddr_in leastUsedAddr = nodeManage.findLeastUsedAddress(activeIds);
        nodeManage.addMapping(inactiveId, leastUsedAddr); // 重新绑定地址
        std::cout << "Rebound inactive ID " << inactiveId << " to least used active address "
                  << inet_ntoa(leastUsedAddr.sin_addr) << ":" << ntohs(leastUsedAddr.sin_port) << std::endl;
    }


}

void Node::executeEntries(int groupId, uint64_t now_commitIndex, uint64_t latestIndex, FleetKVStore& kv, FleetLog& log)
{
     // 确保我们不会试图处理不存在的日志条目
    if (now_commitIndex > latestIndex) {
        std::cout << "没有新的提交需要处理。" << std::endl;
        return;
    }

    // 遍历所有未提交但已确认的日志条目
    for (uint64_t index = now_commitIndex ; index < latestIndex; ++index) {
        const auto& entry = log.logs[groupId][index];

        // 创建DataItem实例
        DataItem item(entry.key, entry.value, entry.method, entry.hashKey, groupId);

        if (entry.method == "SET") {
            // 如果是SET操作，添加或更新键值对
            if (kv.setDataItem(item)) {
                std::cout << "Key: " << entry.key << " set with value: " << entry.value << std::endl;
            } else {
                std::cout << "Failed to set key: " << entry.key << std::endl;
            }
        } else if (entry.method == "DEL") {
            // 如果是DEL操作，删除键值对
            if (kv.deleteDataItem(entry.key, entry.hashKey, groupId)) {
                std::cout << "Key: " << entry.key << " deleted." << std::endl;
            } else {
                std::cout << "Failed to delete key: " << entry.key << std::endl;
            }
        }
    }

    // 更新提交索引
    log.commit(groupId, latestIndex);
    std::cout << "Entries from index " << now_commitIndex + 1 << " to " << latestIndex << " have been committed." << std::endl;
}

void Node::sendGroupLog()
{
    for (int id : m_ids)
    {
        std::vector<int> groupIds = m_node_manage.findGroupIdsByLeaderId(id);
        for (int groupId : groupIds)
        {
            uint64_t commitIndex = m_log.getCommittedIndex(groupId);
            uint64_t latestIndex = m_log.getLatestIndex(groupId);
            FleetGroupSendData send_data(groupId, latestIndex, commitIndex, "");

            if (commitIndex < latestIndex)
            {
                // 获取未提交的日志条目
                Debug::log("开始处理日志提交");
                std::vector<LogEntry> uncommittedEntries = m_log.getUncommittedEntries(groupId);

                // 序列化所有未提交的日志条目
                std::string serializedEntries = m_log.serializeLogEntries(uncommittedEntries);
                send_data.entriesSerialized = serializedEntries;
                std::string serializedSend_data = send_data.serialize();

                Message message;
                message.type = fleetGroupSendData;
                std::memset(message.data, 0, sizeof(message.data));

                // 复制字符串到 char 数组中
                size_t numBytesToCopy = std::min(serializedSend_data.size(), sizeof(message.data) - 1);
                std::memcpy(message.data, serializedSend_data.c_str(), numBytesToCopy);

                std::unordered_set<std::string> sentAddresses;

                for (int id : m_node_manage.getIdsByGroup(groupId))
                {
                    if (std::find(m_ids.begin(), m_ids.end(), id) != m_ids.end() && commitIndex < latestIndex)
                    {
                        // 有需要同步的日志，先给自己投一票
                        m_log.addVote(groupId, latestIndex, id);
                        Debug::log("投票给自己");
                        std::cout << groupId << std::endl;
                    }
                    else
                    {
                        sockaddr_in addr = m_node_manage.getSockaddrById(id);

                        // 将 sockaddr_in 转换为字符串以便存储在 unordered_set 中
                        std::string addrStr = std::string(inet_ntoa(addr.sin_addr)) + ":" + std::to_string(ntohs(addr.sin_port));

                        // 检查该地址是否已经发送过消息
                        if (sentAddresses.find(addrStr) == sentAddresses.end())
                        {
                            // 发送消息
                            sendmsg(send_fd[id], addr, message);

                            // 将该地址标记为已发送
                            sentAddresses.insert(addrStr);
                        }
                    }
                }
            }
        }
    }
}


void Node::updateIds(const std::vector<int>& newIds) {
    std::unordered_set<int> currentIds(m_ids.begin(), m_ids.end());
    std::unordered_set<int> newIdSet(newIds.begin(), newIds.end());

    // 处理新加入的 ID
    for (int newId : newIds) {
        if (currentIds.find(newId) == currentIds.end()) {
            // 新 ID 初始化操作
            initializeNewId(newId, false);
            m_ids.push_back(newId);
        }
    }

    // 处理退出的 ID
    for (int oldId : currentIds) {
        if (newIdSet.find(oldId) == newIdSet.end()) {
            // 删除旧 ID 相关数据
            removeOldIdData(oldId);
        }
    }

    // 更新 m_ids 为新的 ID 列表
    m_ids = newIds;
}

void Node::initializeNewId(int newId, bool init) {
    // 初始化新 ID 的操作
    // 例如：分配资源、初始化数据结构等
    Debug::log("初始化新逻辑节点");
    if(init)
    {
        
    }
    std::vector<int> new_groups_id = m_node_manage.getGroupIndicesWithMemberId(newId);
        // 打印 new_groups_id 中的所有元素
    std::cout << "new_groups_id contains:";
    for (int id : new_groups_id) {
        std::cout << " " << id;
    }
    std::cout << std::endl;

    for( int group_id : new_groups_id)
    {
        if(true)
        {

            InitializeData request(newId,group_id,0,0,"");
            Message message;
            message.type = initializeRequestData;
            std::string serializedSend_data = request.serialize();

            std::memset(message.data, 0, sizeof(message.data));

                                    // 复制字符串到 char 数组中
                                    // 使用 std::min 确保不会超出字符串长度和数组大小的较小值
                                    
            size_t numBytesToCopy = std::min(serializedSend_data.size(), sizeof(message.data) - 1);

            std::memcpy(message.data, serializedSend_data.c_str(), numBytesToCopy);
            
            int leader_id = m_node_manage.getLeaderIdByGroup(group_id);
            

            if(newId != leader_id)
            {
                
                sendmsg(send_fd[leader_id],m_node_manage.getSockaddrById(leader_id),message);

            }

            else
            {
                std::vector<int> ids = m_node_manage.getIdsByGroup(group_id);
                for(int id : ids)
                {
                    if(newId != id)
                    {
                        sendmsg(send_fd[ids[1]],m_node_manage.getSockaddrById(ids[1]),message);
                        break;
                    }
                    

                } 
                
            }

            Debug::log("发送恢复请求");
        }


    }

    std::cout << "Initializing new ID: " << newId << std::endl;
}

void Node::removeOldIdData(int oldId) {
    // 删除旧 ID 相关数据的操作
    // 例如：释放资源、清理数据结构等
    Debug::log("释放旧逻辑节点资源");
    std::cout << "Removing old ID: " << oldId << std::endl;
}

// 调用 updateIds 方法来更新 ID 列表
void Node::checkAndUpdateIds() {
    std::vector<int> newIds = m_node_manage.getIdsBySockaddr(servaddr); // 获取新的 ID 列表
    updateIds(newIds); // 更新 ID 列表
}
