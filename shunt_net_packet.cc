/**
* @file shunt_net_packet.cc
* @brief shunt net packet by connections
* @author ly
* @version 0.1.0
* @date 2013-11-28
*/
#include "shunt_net_packet.h"
#include <map>
#include "constants.h"
#include "utils.h"
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <csignal>

using namespace log4cxx;

LoggerPtr ShuntNetPacket::logger_(Logger::getLogger("shunt_net_pack"));

/** the following used for inner thread params*/
pthread_key_t pthread_key_shunt;
pthread_once_t pthread_once_shunt = PTHREAD_ONCE_INIT;

//int count_pack = 0;
//struct itimerval tick;
//
//void PrintCountInfo(int signo)
//{
//  switch(signo)
//  {
//      case SIGALRM:
//          cout<<"combine pack count:"<<count_pack<<endl;
//          count_pack = 0;
//          break;
//      default:
//          break;
//  }
//  return ;
//}
//

/**
* @brief create thread key
*/
void CreateThreadKey()
{
	pthread_key_create(&pthread_key_shunt, NULL);
}

/**
* @brief init ShuntNetPacket thread:mainly init zmq
*/
void ShuntNetPacket::Init()
{
	sock_ = new zmq::socket_t (*context_, this->zmqitems_[0].zmqpattern);
	//sock_->setsockopt(ZMQ_RCVHWM, &ZMQ_RCVHWM_SIZE, sizeof(ZMQ_RCVHWM_SIZE));
	if("bind" == this->zmqitems_[0].zmqsocketaction)
	{
		sock_->bind(this->zmqitems_[0].zmqsocketaddr.c_str());
	}
	else if("connect" == this->zmqitems_[0].zmqsocketaction)
	{
		sock_->connect(this->zmqitems_[0].zmqsocketaddr.c_str());
	}
	IncreasePool(kPoolSize);
	//cout<<"complete the initialization!"<<endl;
	LOG4CXX_INFO(logger_, "complete the initialization of shunting part!");
}

/**
* @brief init ShuntNetPacket zmq property from config file
*
* @param index
*/
void ShuntNetPacket::InitZMQEx(int index)
{
	deque<XML_ZMQ> *zmq_deque = listening_item_.get_shunt_net_packet()->get_zmqdeque();
	deque<XML_ZMQ>::iterator iter=zmq_deque->begin();
	for(++iter;iter!=zmq_deque->end();iter++)
	{
		ZMQItem zmq_item;
		zmq_item.zmqpattern = (*iter).get_zmqpattern();
		zmq_item.zmqsocketaction = (*iter).get_zmqsocketaction();
		char buf[16];
		memset(buf,0,sizeof(buf));
		sprintf(buf,"%d",index);
		zmq_item.zmqsocketaddr = (*iter).get_zmqsocketaddr() + buf;
		AddZMQItemEx(zmq_item);
		break;
	}
}

/**
* @brief init zmq and push back zmq deque 
*
* @param index
*/
void ShuntNetPacket::AddToZMQDequeEx(int index)
{
	zmq::socket_t *sock = new zmq::socket_t (*context_,this->zmqitems_ex_[index].zmqpattern);
	if("bind" == this->zmqitems_ex_[index].zmqsocketaction)
	{
		sock->bind(this->zmqitems_ex_[index].zmqsocketaddr.c_str());
	}
	else if("connect" == this->zmqitems_ex_[index].zmqsocketaction)
	{
		sock->connect(this->zmqitems_ex_[index].zmqsocketaddr.c_str());
	}
	sock_ex_deque_.push_back(sock);
}

/**
* @brief init lua routine thread 
*
* @param index
*/
void ShuntNetPacket::InitLuaRoutineThread(int index)
{
	lua_routine_ = new LuaRoutine(context_,listening_item_);
	deque<XML_ZMQ>* lua_routine_zmq_deque = listening_item_.get_lua_routine()->get_zmqdeque();
	for(deque<XML_ZMQ>::iterator iter = lua_routine_zmq_deque->begin();iter!=lua_routine_zmq_deque->end();iter++)
	{
		ZMQItem lua_routine_zmq_item;
		lua_routine_zmq_item.zmqpattern = iter->get_zmqpattern();
		lua_routine_zmq_item.zmqsocketaction = iter->get_zmqsocketaction();
		/** Todo:config it*/
		if("inproc://business_error" == iter->get_zmqsocketaddr())
		{
			lua_routine_zmq_item.zmqsocketaddr = iter->get_zmqsocketaddr();
		}
		else
		{
			char buf[16];
			memset(buf,0,sizeof(buf));
			sprintf(buf,"%d",index);
			lua_routine_zmq_item.zmqsocketaddr = iter->get_zmqsocketaddr() + buf;
		}
		lua_routine_->AddZMQItem(lua_routine_zmq_item);
	}
	lua_routine_->Init();
	lua_routine_deque_.push_back(lua_routine_);
}

/**
* @brief start luaroutine thread
*/
void ShuntNetPacket::RunLuaRoutineThread()
{
	//lua_routine_->Start();
	deque<LuaRoutine*>::iterator it;
	for(it=lua_routine_deque_.begin();it!=lua_routine_deque_.end();++it)
	{
		(*it)->Start();	
	}
}

/**
* @brief init CombineDCPacket thread
*
* @param index
*/
void ShuntNetPacket::InitCombineDCPacketThread(int index)
{
	combine_dc_packet_ = new CombineDCPacket(context_);
	deque<XML_ZMQ>* combine_dc_zmq_deque = listening_item_.get_combine_dc_packet()->get_zmqdeque();
	for(deque<XML_ZMQ>::iterator iter = combine_dc_zmq_deque->begin();iter!=combine_dc_zmq_deque->end();iter++)
	{
		ZMQItem combine_dc_zmq_item;
		combine_dc_zmq_item.zmqpattern = (*iter).get_zmqpattern();
		combine_dc_zmq_item.zmqsocketaction = (*iter).get_zmqsocketaction();
		char buf[16];
		memset(buf,0,sizeof(buf));
		sprintf(buf,"%d",index);
		combine_dc_zmq_item.zmqsocketaddr = (*iter).get_zmqsocketaddr() + buf;
		combine_dc_packet_->AddZMQItem(combine_dc_zmq_item);
	}
	combine_dc_packet_->Init();
	combine_dc_deque_.push_back(combine_dc_packet_);
}

/**
* @brief start CombineDCPacket thread
*/
void ShuntNetPacket::RunCombineDCPacketThread()
{
	deque<CombineDCPacket*>::iterator it;
	for(it=combine_dc_deque_.begin();it!=combine_dc_deque_.end();++it)
	{
		(*it)->Start();	
	}
}

/**
* @brief init UncompressDCPacket thread
*
* @param index
*/
void ShuntNetPacket::InitUncompressDCPacketThread(int index)
{
	uncompress_dc_packet_ = new UncompressDCPacket(context_ , listening_item_);
	deque<XML_ZMQ>* uncompress_dc_zmq_deque = listening_item_.get_uncompress_dc_packet()->get_zmqdeque();
	for(deque<XML_ZMQ>::iterator iter = uncompress_dc_zmq_deque->begin();iter!=uncompress_dc_zmq_deque->end();iter++)
	{
		ZMQItem uncompress_dc_zmq_item;
		uncompress_dc_zmq_item.zmqpattern =(*iter).get_zmqpattern();
		uncompress_dc_zmq_item.zmqsocketaction = (*iter).get_zmqsocketaction();
		if("inproc://log" == (*iter).get_zmqsocketaddr())
		{
			uncompress_dc_zmq_item.zmqsocketaddr = (*iter).get_zmqsocketaddr();
		}
		else
		{
			char buf[16];
			memset(buf,0,sizeof(buf));
			sprintf(buf,"%d",index);
			uncompress_dc_zmq_item.zmqsocketaddr = (*iter).get_zmqsocketaddr() + buf;
		}
		uncompress_dc_packet_->AddZMQItem(uncompress_dc_zmq_item);
	}
	uncompress_dc_packet_->Init();
	uncompress_dc_deque_.push_back(uncompress_dc_packet_);
}

/**
* @brief start UncompressDCPacket thread
*/
void ShuntNetPacket::RunUncompressDCPacketThread()
{
	deque<UncompressDCPacket*>::iterator it;
	for(it=uncompress_dc_deque_.begin();it!=uncompress_dc_deque_.end();++it)
	{
		(*it)->Start();	
	}
}

/**
* @brief set inner thread params
*
* @param value
*/
void set_inner_thread_params(const void * value)
{
	pthread_once(&pthread_once_shunt, CreateThreadKey);
	pthread_setspecific(pthread_key_shunt, value);
}

/**
* @brief get innet thread params
*
* @return 
*/
void * get_inner_thread_params()
{
	return pthread_getspecific(pthread_key_shunt);
}

/**
* @brief relarge thread pool:a simple pool has not management function 
*
* @param pool_size
*
* @return 
*/
bool ShuntNetPacket::IncreasePool(int pool_size)
{
	for(int i=curent_pool_size_;i<curent_pool_size_ + pool_size;i++)
	{
		InitZMQEx(i);
		AddToZMQDequeEx(i);
#ifndef __linux
		Sleep(100);
#else
		Utils::SleepUsec(10000);
#endif
		//RunLuaRoutineThread(i);
		InitLuaRoutineThread(i);
		//RunParseThread(i);
		InitCombineDCPacketThread(i);
		//RunCombineDCPacketThread(i);
#ifndef __linux
		Sleep(100);
#else
		Utils::SleepUsec(10000);
#endif
		//RunUncompressDCPacketThread(i);
		InitUncompressDCPacketThread(i);
	}
	RunLuaRoutineThread();
	RunCombineDCPacketThread();
	RunUncompressDCPacketThread();
	curent_pool_size_ += pool_size;
	return true;
}

/**
* @brief create did configure file content 
*
* @param did_structs
* @param out_str
*/
void ShuntNetPacket::CreateDidConfContent(vector<DidStruct> & did_structs, char * out_str)
{
	assert(NULL != out_str);
	char did_content[256] = {0};
	strcat(out_str, "<?xml version=\"1.0\" encoding=\"gb2312\" ?>\n <DidStruct>\n");
	for(vector<DidStruct>::iterator iter = did_structs.begin();iter != did_structs.end();iter++)
	{
		memset(did_content, 0,sizeof(did_content));
		sprintf(did_content, "\t <did id=\"%d\" file=\"%s\" whole=\"%u\" compress=\"%u\" /> \n", iter->id ,iter->file_path.c_str(),iter->whole_tag,iter->compress_tag);
		strcat(out_str, did_content);
	}
	strcat(out_str, "</DidStruct>\n");
}

/**
* @brief write did configure file
*
* @param file_name
* @param did_structs
*/
void ShuntNetPacket::WriteDidConfFile(const char * file_name, vector<DidStruct> &did_structs)
{
	char file_content[2048] = {0};
	CreateDidConfContent(did_structs, file_content);
	Utils::WriteIntoFile(file_name, "wb", file_content, strlen(file_content));
}

/**
* @brief if has a new connection through tcp
*
* @param flags
* @param tcpconntag
* @param tcpconnstatus
*
* @return 
*/
bool ShuntNetPacket::IsTcpConnection(unsigned char flags, int &tcpconntag, int &tcpconnstatus)
{
	if(cons::SYN == flags)
	{
		tcpconntag = 1;
		tcpconnstatus = 0;
	}
	if(1 == tcpconntag)
	{
		if(cons::SYN == flags)
		{
			tcpconnstatus |= 0x1;
		}
		else if(cons::SYNACK == flags)
		{
			tcpconnstatus |= 0x2;
		}
		else if(cons::ACK == flags)
		{
			tcpconnstatus |= 0x4;
			tcpconntag = 0;
			if(7 == tcpconnstatus)
			{
				tcpconnstatus = 0;
				return true;
			}
			else
			{
				tcpconnstatus = 0;
			}
        }
		else
		{
			tcpconntag = 0;
			tcpconnstatus = 0;
		}
	}
	return false;
}

/**
* @brief if has a disconnection through tcp
*
* @param flags
*
* @return 
*/
bool ShuntNetPacket::IsTcpDisConnection(unsigned char flags)
{
	if(cons::FINACK == flags)
	{
		return true;
	}
	return false;
}

/**
* @brief dispatch data to other threads
*
* @param sock
* @param data
* @param size
*/
void ShuntNetPacket::DispatchData(zmq::socket_t * sock, void * data, int size)
{
	assert(NULL != sock && NULL != data);
	try
	{
		zmq::message_t msg(size);
		memcpy((void*)(msg.data()), data, size);
		//sock->send(msg,ZMQ_NOBLOCK);
		sock->send(msg);
	}
	catch(zmq::error_t error)
	{
		//cout<<"cap: zmq send error! error content:"<<error.what()<<endl;
		LOG4CXX_ERROR(logger_, "cap: zmq send error! error content:" << error.what());
		assert(0);
	}
}

/**
* @brief if has a abnormal disconnection(like not receiving data), then send reset tag to other threads 
*/
void ShuntNetPacket::PreHandleADisconnection()
{
	if(sock_delta_time_map_.empty())
	{
		return ;
	}
	else
	{
		map<zmq::socket_t *, unsigned long>::iterator it;
		for(it=sock_delta_time_map_.begin();it!=sock_delta_time_map_.end();++it)
		{
			struct timeval tv;
			gettimeofday(&tv,NULL);
			if(0 == it->second)
			{
				it->second = tv.tv_sec*1000000 + tv.tv_usec;	
			}
			else
			{
				if(tv.tv_sec*1000000+tv.tv_usec-it->second > 30000000) //30s
				{
					/** if has a abnormal disconnection , then send reset tag to other threads*/
					PacketItem item;
                    item.thread_tag = cons::RESET;
                    item.port_tag = 0;
                    item.data = NULL;
                    DispatchData(it->first, &item, sizeof(item));
					sock_ex_deque_.push_back(it->first);
					map<std::string, zmq::socket_t *>::iterator iter_map;
					for(iter_map=sock_ex_map_.begin();iter_map!=sock_ex_map_.end();)
					{
						if(iter_map->second == it->first) sock_ex_map_.erase(iter_map++);
					}
					//cout<<"handle a abnormal disconnection!"<<endl;
					LOG4CXX_INFO(logger_, "handled a abnormal disconnection!");
				}	
			}
		}
	}
}

/**
* @brief thread running function
*/
void ShuntNetPacket::RunThreadFunc()
{
	struct pcap_pkthdr *header = NULL;
	unsigned char * pkt_data = NULL;
	ip_head *ih;
	tcp_head *tcph;
	int iph_len = 0;
	int tcph_len = 0;
	int head_len = 0;
	unsigned long tcp_current_seq = 0;
	unsigned long tcp_data_len = 0;
	char key_ip_src[32];
	char key_ip_dst[32];
	XML_ListeningItem *listening_item = &(this->listening_item_);
	int port = listening_item->get_port();
	deque<zmq::socket_t *> *sock_deque = &(this->sock_ex_deque_);
	map<std::string,zmq::socket_t*>* sock_ex_map_ = &(this->sock_ex_map_);

	int tcpconnstatus = 0;
	int tcpconntag = 0;

	zmq::message_t msg_rcv(sizeof(CapNetPac ketItem));
	while(true)
	{
		//PreHandleADisconnection();

		msg_rcv.rebuild();
		sock_->recv(&msg_rcv);
		CapNetPacketItem *msg_item = (CapNetPacketItem*)(msg_rcv.data());
		header = &(msg_item->header);
		pkt_data = msg_item->data;

		ih = (ip_head *)(pkt_data + 14);
		iph_len = (ih->ver_ihl & 0xf) * 4;
		tcph = (tcp_head *)((char *)ih + iph_len);
		tcph_len = 4*((tcph->dataoffset)>>4&0x0f);
		head_len = iph_len + tcph_len;

		//pack_seq = ntohl(tcph->seq);

		memset(key_ip_src,0,sizeof(key_ip_src));
		memset(key_ip_dst,0,sizeof(key_ip_dst));
		sprintf(key_ip_src,"%d.%d.%d.%d:%d",ih->saddr.byte1,ih->saddr.byte2,ih->saddr.byte3,ih->saddr.byte4,ntohs(tcph->source));
		sprintf(key_ip_dst,"%d.%d.%d.%d:%d",ih->daddr.byte1,ih->daddr.byte2,ih->daddr.byte3,ih->daddr.byte4,ntohs(tcph->dest));

		if(cons::TCP == ih->protocol)
		{
			if(IsTcpConnection(tcph->flags, tcpconntag, tcpconnstatus))
			{
				//cout<<"A new connection was been built! The ip and port is:"<<key_ip_src<<endl;
				LOG4CXX_INFO(logger_, "A new connection was been built! The ip and port is:" << key_ip_src);
				tcpconnstatus = 0;
				if(sock_ex_map_->end() == sock_ex_map_->find(key_ip_src))
				{
					if(!sock_deque->empty())
					{
						zmq::socket_t * sock = sock_deque->front();
						sock_deque->pop_front();
						sock_ex_map_->insert(pair<std::string,zmq::socket_t *>(key_ip_src,sock));
						sock_delta_time_map_.insert(make_pair(sock,0));
						//cout<<"connection:key_ip:"<<key_ip_src<<endl;
						LOG4CXX_INFO(logger_, "connection:key_ip:" << key_ip_src);
					}
					else
					{
						if(this->IncreasePool(kPoolSize))
						{
							zmq::socket_t *sock = sock_deque->front();
							sock_deque->pop_front();
							sock_ex_map_->insert(pair<std::string,zmq::socket_t *>(key_ip_src,sock));
							sock_delta_time_map_.insert(make_pair(sock,0));
							//cout<<"connection:key_ip:"<<key_ip_src<<endl;
							LOG4CXX_INFO(logger_, "connection:key_ip:" << key_ip_src);
						}
						else
						{
							return ;
						}
					}
				}
			}
			if(IsTcpDisConnection(tcph->flags))
			{
				map<std::string,zmq::socket_t*>::iterator iter_map;
				if (port == ntohs(tcph->source))
				{
					if((iter_map=sock_ex_map_->find(key_ip_dst)) != sock_ex_map_->end())
                    {
                        zmq::socket_t * sock =iter_map->second;
                        PacketItem item;
                        item.thread_tag = cons::RESET;
                        item.port_tag = 0;
						item.market_id = 0;
                        item.data = NULL;
                        DispatchData(sock, &item, sizeof(item));
                        sock_deque->push_back(sock);
						map<zmq::socket_t *, unsigned long>::iterator it_time = sock_delta_time_map_.find(sock);
						if(it_time != sock_delta_time_map_.end()) sock_delta_time_map_.erase(it_time);
                        sock_ex_map_->erase(iter_map);
                        //cout<<key_ip_dst<<" was disconnected!dst"<<endl;
						LOG4CXX_INFO(logger_, key_ip_dst << " was disconnected!dst");
                    }
				}
				else if (port == ntohs(tcph->dest))
				{
					if((iter_map=sock_ex_map_->find(key_ip_src)) != sock_ex_map_->end())
					{
						zmq::socket_t * sock =iter_map->second;
						PacketItem item;
						item.thread_tag = cons::RESET;
						item.port_tag = 0;
						item.market_id = 0;
						item.data = NULL;
						DispatchData(sock, &item, sizeof(item));
						sock_deque->push_back(sock);
						map<zmq::socket_t *, unsigned long>::iterator it_time = sock_delta_time_map_.find(sock);
						if(it_time != sock_delta_time_map_.end()) sock_delta_time_map_.erase(it_time);
						sock_ex_map_->erase(iter_map);
						//cout<<key_ip_src<<" was disconnected!src"<<endl;
						LOG4CXX_INFO(logger_, key_ip_src << " was disconnected!src");
					}
				}
				else
				{
					assert(0);
				}

			}

			map<std::string,zmq::socket_t*>::iterator iter;
			tcp_data_len = ntohs(ih->tlen) - head_len;//must use ih->tlen, because sometime it will have supplement package.
			tcp_current_seq = ntohl(tcph->seq);
			//cout<<"cap:current_seq:"<<tcp_current_seq<<" data_len:"<<tcp_data_len<<endl;
			/** parsing two direction data*/

			//caishu  --> zhongzhuan
			if((iter=sock_ex_map_->find(key_ip_dst)) != sock_ex_map_->end())
			{
				//count_pack += 1;
				/*cout<<"key_ip_dst:"<<key_ip_dst<<endl;*/
				PacketItem item;
				item.thread_tag = cons::NORMAL;
				item.port_tag = port;
				item.market_id = market_id_;
				item.header = *header;
				//unsigned char * data_buf = (unsigned char*)malloc(sizeof(CAP_PACK_BUF_SIZE));
				unsigned char *data_buf = new unsigned char[FLAGS_CAP_PACK_BUF_SIZE];
				assert(NULL != data_buf);
				memset(data_buf,0,FLAGS_CAP_PACK_BUF_SIZE);
				memcpy(data_buf, pkt_data, header->caplen);
				item.data = data_buf;
				zmq::socket_t * psock = iter->second;
				map<zmq::socket_t *, unsigned long>::iterator it_time = sock_delta_time_map_.find(psock); 
				if(it_time != sock_delta_time_map_.end()) 
				{
					it_time->second = 0;
				}
				DispatchData(psock, &item, sizeof(item));
			}
			else //zhongzhuan  --> caishu
			{
				DC_HEAD * pdch = (DC_HEAD *)(pkt_data + 14 + head_len);
				//LOG4CXX_INFO(logger_, "zz->cs:" << (int)(pdch->m_cType));
				if(DC_TAG == pdch->m_cTag && DCT_DSDID == pdch->m_cType)
				{
					LOG4CXX_INFO(logger_, "ly-dsdid");
					DC_DSDID *pdsdid =  (DC_DSDID *)(pdch + 1);
					int port = listening_item->get_port();
					vector<DidStruct> did_structs;
					LOG4CXX_INFO(logger_, "actually the total num of did templates is " \
								<< pdch->m_nLen/sizeof(DC_DSDID));
					
					map<int, std::string> & did_filepath_map = listening_item->get_did_filepath_map();
					for(unsigned int i=0;i<pdch->m_nLen/sizeof(DC_DSDID);i++)
					{
						DidStruct did_struct;
						map<int, std::string>::iterator iter = did_filepath_map.find(pdsdid->m_dwDid);
						if(iter != did_filepath_map.end())
						{
							did_struct.id = pdsdid->m_dwDid;
							did_struct.whole_tag = pdsdid->m_bFull;
							did_struct.compress_tag = pdsdid->m_bNoCompress ? 0 : 1;
							did_struct.file_path = iter->second;
							did_structs.push_back(did_struct);
						}
						pdsdid += 1;
					}

					char did_conf_file[64] = {0};
					sprintf(did_conf_file, "%d_did_config.xml", port);
					WriteDidConfFile(did_conf_file, did_structs);
				}
				else if(DC_TAG == pdch->m_cTag && DCT_LOGIN	== pdch->m_cTag)
				{
					LOG4CXX_INFO(logger_, "dc login");
					DC_LOGIN *plogin = (DC_LOGIN *)(pdch + 1);
					market_id_ = plogin->m_wMarket;	
				}
			}
		}
		if(NULL != pkt_data)
		{
			delete[] pkt_data;
			pkt_data = NULL;
		}

	}
}
//
//void ShuntNetPacket::PacketHandler(unsigned char *param, const struct pcap_pkthdr *header, const unsigned char *pkt_data)
//{
//	pcap_dump(param,header,pkt_data);
//}

