#ifndef _TCP_CONNECTION_H_
#define _TCP_CONNECTION_H_
#include <string>


class TcpConnection
{
public:
	TcpConnection();
	~TcpConnection();
	inline bool isConnected() 
	{
		return is_connected_;
	}

	bool connect(const std::string ip, int port, int timeout);
	void setNoBlocking(bool block, int timeout);
	int send(const char* data, int length);
	int receive(std::string& data, int length);
	void close();
private:
	int socketfd_;
	bool is_connected_;
	int server_port_;
	int timeout_;
	std::string server_ip_;
};




#endif