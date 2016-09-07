#include "tcpconnection.h"
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>         
#include <sys/socket.h>
#include <arpa/inet.h>
using namespace std;

TcpConnection::TcpConnection():socketfd_(-1),is_connected_(false),timeout_(0)
{

}

TcpConnection::~TcpConnection()
{
	
}

bool TcpConnection::connect(const string ip, int port, int timeout)
{
	close();
	socketfd_ = socket(AF_INET, SOCK_STREAM, 0);
	if (socketfd_ < 0) {
		return false;
	}
	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = inet_addr(ip.c_str());
	// if (server_addr.sin_addr.s_addr == INADDR_NONE) {
	// 	hostent* myhost = gethostbyname(ip.c_str());
	// 	if (myhost != NULL) {
	// 		char** pp = myhost->h_addr_list;
	// 		while(*pp != NULL) {
	// 			server_addr.sin_addr.s_addr = *((unsigned int *)*pp); //get first ip
	// 			pp++;
	// 			break;
	// 		}
	// 	}
	// }

	int ret = ::connect(socketfd_, (struct sockaddr*)&server_addr, sizeof(struct sockaddr));
	if (ret < 0) {
		close();
		return false;
	} 
	timeout_ = timeout;
	struct timeval tv = {timeout_, 0};
	if ((setsockopt(socketfd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1) || (setsockopt(socketfd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == -1)) {
		close();
		return false;
	}
	is_connected_ = true;
	return true;
}

void TcpConnection::setNoBlocking(bool block, int timeout)
{

}

int TcpConnection::send(const char* data, int length)
{
	if (socketfd_ == -1 || is_connected_ == false) {
		return -1;
	}
	int ret = -1;
	int send_len = 0;
	time_t t_start = time(NULL);
	while(true) {
		ret = ::send(socketfd_, data + send_len, length - send_len, 0);
		if (ret == 0) {
			close();
			return 0;
		} else if(ret < 0) {
			if(errno == EAGAIN || errno ==EWOULDBLOCK || errno == EINTR) {
                if(time(NULL) - t_start > timeout_) {
                   close();
                   return 0;
                }else {
                   continue;//继续发送
                }
            } else {
                close();
                return ret;//发送出错
            }
		}
		send_len += ret;
		if (send_len == length) break; 
	}
	return send_len;
}

int TcpConnection::receive(std::string& data, int length)
{
	if (socketfd_ == -1 || is_connected_ == false) {
		return -1;
	}
	int ret = -1;
	char buf[4096];
	time_t t_start = time(NULL);
	while(true) {
		bzero(buf,4096);
		ret = ::recv(socketfd_, buf, length, 0);
		if (ret == 0) {
			close();
			return 0;
		} else if(ret < 0) {
			if(errno == EAGAIN || errno ==EWOULDBLOCK || errno == EINTR) {
                if(time(NULL) - t_start > timeout_) {
                   close();
                   return 0;
                }else {
                   continue;//继续
                }
            } else {
                close();
                return ret;//接受出错
            }
		}
		data.append(buf, ret);
		break;
	}
	return ret;
}

void TcpConnection::close()
{
	::close(socketfd_);
}
