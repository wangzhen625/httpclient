#include "httpclient.h"
#include <zlib.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sstream>

#if (defined(DEBUG) && !defined(TARGET_LPC11U24))
#define DEBUG(x, ...)  printf("[DBG %s %3d] " x "\r\n", DEBUG, __LINE__, ##__VA_ARGS__);
#define WARN(x, ...)  printf("[WRN %s %3d] " x "\r\n", DEBUG, __LINE__, ##__VA_ARGS__);
#define ERROR(x, ...)   printf("[ERR %s %3d] " x "\r\n", DEBUG, __LINE__, ##__VA_ARGS__);
#define INFO(x, ...)  printf("[INF %s %3d] " x "\r\n", DEBUG, __LINE__, ##__VA_ARGS__);
#else
#define DEBUG(x, ...)
#define WARN(x, ...)
#define ERROR(x, ...)
#define INFO(x, ...)
#endif

#define CHUNK_SIZE 1024

#define CHECK_CONN_ERR(ret) \
  do{ \
    if(ret) { \
      connection_->close(); \
      ERROR("Connection error (%d)", ret); \
      return HTTP_CONN; \
    } \
  } while(0)

#define PRTCL_ERR() \
  do{ \
    connection_->close(); \
    ERROR("Protocol error"); \
    return HTTP_PRTCL; \
  } while(0)

HttpClient::HttpClient()
	:location_url_(""),response_code_(0),max_redirect_count_(1)
{
	connection_ = new TcpConnection();
}

HttpClient::~HttpClient()
{
	if(connection_) {
		delete connection_;
		connection_ = NULL;
	}
}

int HttpClient::get(const std::string url, HttpRequest request, HttpResponse& response, int timeout)
{
	return connect(url, HTTP_GET, request, response, timeout);
}

int HttpClient::post(const std::string url, HttpRequest request, HttpResponse& response, int timeout )
{
	return connect(url, HTTP_POST, request, response, timeout);
}

int HttpClient::connect(std::string url, HTTP_METHOD method, HttpRequest request, HttpResponse& response, int timeout)
{
	string host("");
	string path("");
	int port = -1;
	string response_body("");
	bool is_compress = false;
	while (max_redirect_count_ --) {
		bool take_redirect = false;
		int res = parseUrl(url, host, port, path);
		if (res != HTTP_OK) {
			ERROR("parseUrl returned %d", res);
			return res;
		}
		INFO("host:%s", host.c_str());
		INFO("port:%d", port);
		INFO("path:%d", path.c_str());
		//connect to server
		int status = connection_->connect(host, port,timeout);
		if (status < 0) {
			connection_->close();
			ERROR("can not connect to host %s:%d", host.c_str(), port);
			return HTTP_CONN;
		}

		string data;
		string meth = (method==HTTP_GET)?"GET":(method==HTTP_POST)?"POST":(method==HTTP_PUT)?"PUT":(method==HTTP_DELETE)?"DELETE":"";
		// request line
		data.append(meth).append(" ").append(path); 
		if (method == HTTP_GET) {
			string getdata;
			request.read(getdata);
			if (!getdata.empty()) {
				data.append("?"+ getdata);
			}
		}
		data.append(" HTTP/1.1\r\n");

		//request header,not include "Content-Length", "Transfer-Encoding" and "Content-Type"
		data.append("Host: ").append(host + "\r\n");

		map<string, string> headers = request.getHeaders();
		map<string, string>::iterator it;
		for (it = headers.begin(); it != headers.end(); ++it) {
			data.append(it->first).append(": ").append(it->second + "\r\n");
		}

		//default header, "Content-Length", "Transfer-Encoding" and "Content-Type"

		if (request.isChunked()) {
			data.append("Transfer-Encoding: chunked\r\n");
		} else {
			std::stringstream ss;
			string str_data_len;
			int data_len = request.getDataLength();
			ss << data_len;
			ss >> str_data_len;
			data.append("Content-Length: ").append(str_data_len + "\r\n");
		}
		string data_type;
		request.getDataType(data_type);
		data.append("Content-Type: ").append(data_type + "\r\n");
		data.append("\r\n");

		//send data(post method)
		if (method == HTTP_POST) {
			string msg;
			int written_len = 0;
			int read_size = 0;
			if (request.isChunked()) {
				while(true) {
					read_size = request.read(msg, CHUNK_SIZE);
					//chunk header
					char chunk_header[16];
                    snprintf(chunk_header, sizeof(chunk_header), "%X\r\n", read_size); //In hex encoding
                    data.append(chunk_header, 16);
                    if (read_size == 0) {
                    	break;
                    } else {
                    	// add chunk data
                    	data.append(msg + "\r\n");
                    	written_len += read_size;
                    	if (written_len > request.getDataLength()) {
                    		break;
                    	}
                    }
				}
			} else {
				read_size = request.read(msg);
				if (read_size != request.getDataLength()) {
					ERROR("not read all data from the request, something error");
					return HTTP_ERROR;	
				}
				data.append(msg);
			}	
		}

		send(data, data.size());

		//receive response
		string receive_data;
		int receive_data_len = 0;
		size_t response_body_begin_pos;
		int recevie_once_len = 0;
		receive_data_len = recv(receive_data, CHUNK_SIZE);
		while(true) {
			//find the rules between response body and response headers
			response_body_begin_pos = receive_data.find("\r\n\r\n");
			//http package head incompelte, continue receive
			if (response_body_begin_pos == string::npos) {  	
				recevie_once_len = recv(receive_data, CHUNK_SIZE);
				receive_data_len += recevie_once_len;
			} else {
				break;
			}
		}

		//parse Http response
		if( sscanf(receive_data.c_str(), "HTTP/%*d.%*d %d %*[^\r\n]", &response_code_) != 1 ) {
            //Cannot match string, error
            ERROR("Not a correct HTTP answer : {%s}\n", receive_data);
            PRTCL_ERR();
        }

        if ( (response_code_ < 200) || (response_code_ > 400)) {
        	WARN("Response code:%d", response_code_);
        	PRTCL_ERR();
        }

         //store the body
        response_body = receive_data.substr(response_body_begin_pos + 4);
        //get headers
        size_t response_head_line_end_pos = receive_data.find("\r\n");
        //remove the response line
        receive_data = receive_data.substr(response_head_line_end_pos + 2);
        string response_headers = receive_data.substr(0, response_body_begin_pos - response_head_line_end_pos);
       

        size_t response_header_pos_previous = 0;
        string response_header("");
        while(true) {
        	//response_headers ="Accept: */*\r\nTransfer-Encoding: chunked\r\nContent-Length: 5\r\nContent-Type: text/json\r\nContent-Encoding:gzip\r\n";
        	response_header_pos_previous = response_headers.find("\r\n");
        	if (response_header_pos_previous == string::npos) {
        		break;
        	}
        	//a header line
        	response_header = response_headers.substr(0, response_header_pos_previous);
        	//the pos of ':'
        	size_t commas_pos = response_header.find(':');
        	string key = response_header.substr(0, commas_pos);
        	string value = response_header.substr(commas_pos + 2);
        	response.addHeader(key, value);
        	if (key == "Content-Length") {
        		response.setDataLength(atoi(value.c_str()));
        	} else if (key == "Transfer-Encoding") {
        		if (value == "chunked" || value == "Chunked")
        		{
        			response.setIsChunked(true);
        		}
        	} else if(key == "Content-Type") {
        		response.setDataType(value);
        	} else if (key == "Location") {
        		location_url_ = value;
        		url = value;
        		connection_->close();
        		take_redirect = true;
        		break;
        	} else if (key == "Content-Encoding" and value == "gzip") {
        		is_compress = true;
        	}
        	//remained headers
        	response_headers = response_headers.substr(response_header_pos_previous + 2);
        } //end while (get headers)
        if (take_redirect) {
        	break;
        }

        if (response.isChunked()) {
        	string chunk_data;
			string chunk_piece_header;
			string chunk_piece_body;
			size_t chunk_piece_header_end_pos;
			int data_len = 0;
			while(true) {
				while(true) {
					chunk_piece_header_end_pos = response_body.find("\r\n");
					//data incomplete
					if (chunk_piece_header_end_pos == string::npos) {
						recevie_once_len = recv(response_body, CHUNK_SIZE);
					} else {
						break;
					}
				}
				//chunk piece header
				chunk_piece_header = response_body.substr(0, chunk_piece_header_end_pos);
				size_t piece_datalen = strtol(chunk_piece_header.c_str(),NULL, 16);
				if (piece_datalen == 0) { //end of chunk data
					break;
				}
				//jump chunk piece header
				response_body = response_body.substr(chunk_piece_header_end_pos + 2);
				size_t chunk_piece_body_end_pos =  response_body.find("\r\n");
				//chunk piece body
				chunk_piece_body = response_body.substr(0, chunk_piece_body_end_pos);
				if (chunk_piece_body.length() != piece_datalen) {
					ERROR("get chunk data error");
					return HTTP_PRTCL;
				}
				chunk_data.append(chunk_piece_body);
				response_body = response_body.substr(chunk_piece_body_end_pos + 2);
				data_len += piece_datalen;
			}
			response_body.clear();
			response_body = chunk_data;
        } else {
        	 if((int)response_body.size() != response.getDataLength()) {
        	 	int left_size = response.getDataLength() - response_body.size();
        		recevie_once_len = recv(response_body, left_size);
        		if (recevie_once_len != left_size) {
        			ERROR("data receive incomplete");
        			return HTTP_PRTCL;
        		}
        	}
        }
	}//end while (max_redirect_count_)

	string decompress_data;
	if (is_compress) {
		int ret = decompress(response_body, decompress_data);
	} else {
		decompress_data = response_body;
	}

	response.write(decompress_data);
	return HTTP_OK;
}

int HttpClient::decompress(const std::string compress_data, std::string& decompress_data)
{
	z_stream decompress_stream;
	decompress_stream.zalloc = NULL;
	decompress_stream.zfree = NULL;
	decompress_stream.opaque = NULL;

	int decompress_len = (compress_data.size()/1024 + 1) * 1024 * 20;
	string tmp_str("");
	tmp_str.resize(decompress_len);
	decompress_stream.avail_in = compress_data.size();
    decompress_stream.avail_out = decompress_len;
    decompress_stream.next_in = (Bytef *)(compress_data.c_str());
    decompress_stream.next_out = (Bytef *)(tmp_str.c_str());

    int err = inflateInit2(&decompress_stream, MAX_WBITS+16);
    if (err != Z_OK)
    {
        inflateEnd(&decompress_stream);
        return HTTP_DECOMPRESS;
    }
    err = inflate(&decompress_stream, Z_FINISH);
    inflateEnd(&decompress_stream);
    if (err != Z_STREAM_END)
        return HTTP_DECOMPRESS;
    decompress_data = tmp_str;
   // return decompress_stream.total_out;
    return HTTP_OK;
}
// https://developer.mbed.org/users/WiredHome/code/Socket/
int HttpClient::parseUrl(std::string url, std::string& host, int& port, std::string& path)
{
	if (url.empty()) {
		WARN("url is null");
		return HTTP_PARSE;
	}
	size_t host_begin_pos = url.find("://");
	if (host_begin_pos != string::npos) {
		url = url.substr(host_begin_pos + 3);
	}
	
	size_t host_end_pos = url.find('/');
	if (host_end_pos != string::npos) {
		host = url.substr(0, host_end_pos);
	}else {
		host = url;
	}
	
	if ( host.empty()) {
		ERROR("can't find host");
		return HTTP_PARSE;
	}

	size_t port_begin_pos = host.find(':');
	size_t path_begin_pos = url.find('/'); //host_end_pos == path_pos
	if (port_begin_pos == string::npos) {
		port = 80;
	} else {
		string strport = host.substr(port_begin_pos + 1, path_begin_pos - port_begin_pos);
		host = host.substr(0,port_begin_pos);
		port = atoi(strport.c_str()); 
	}
	path = url.substr(path_begin_pos);
	return HTTP_OK;
}


int HttpClient::send(const std::string send_data, int len)
{
	size_t written_len = 0;
	if (len == 0) {
		len = send_data.size();
	}
	INFO("send(%s,%d)", send_data.c_str(), len);

	if (!connection_->isConnected()) {
		INFO("Connection is close by the server");
		return HTTP_CLOSE;
	}

	connection_->setNoBlocking(false, timeout_);
	int ret = connection_->send(send_data.c_str(), len);
	if (ret > 0) {
		written_len += ret;
	} else if (ret == 0) {
		WARN("Connection was closed by server");
        return HTTP_CLOSE; //Connection was closed by server
	} else if( ret < 0) {
		ERROR("Connection error (send returned %d)", ret);
        return HTTP_CONN;
	}

	INFO("Written %d bytes", writtenLen);
    return HTTP_OK;
}

int HttpClient::recv(string& receive_data, int size)
{
	if (!connection_->isConnected()) {
		INFO("Connection is close by the server");
		return HTTP_CLOSE;
	}
	int received_len = 0;
	int ret = connection_->receive(receive_data, size);
	if (ret > 0) {
		received_len += ret;
	} else if (ret == 0) {
		WARN("Connection was closed by server");
        return HTTP_CLOSE; //Connection was closed by server
	} else if( ret < 0) {
		ERROR("Connection error (send returned %d)", ret);
        return HTTP_CONN;
	}
	return HTTP_OK;
}







HttpRequest::HttpRequest()
	:ischunked_(false),read_pos_(0)
{

}
HttpRequest::~HttpRequest()
{

}

int HttpRequest::read(std::string& data, int size) 
{	
	int read_size;
	if (read_pos_ < request_data_.size()) {
		if (size == 0) {
			data = request_data_.substr(read_pos_);
			read_pos_ = request_data_.size();
			read_size = request_data_.size();
		} else {
			read_size = (read_pos_ + size <= request_data_.size()?size:(request_data_.size() - read_pos_ + size));
			data = request_data_.substr(read_pos_, read_size);
			read_pos_ += read_size;
		}
	}
	return read_size;
}

int HttpRequest::write(std::string data, int size)
{
	int write_size = (size == 0? data.size():size);
	request_data_.append(data);
	return write_size;
}



HttpResponse::HttpResponse()
	:ischunked_(false),read_pos_(0),response_data_size_(0)
{

}
HttpResponse::~HttpResponse()
{

}

int HttpResponse::read(std::string& data, int size) 
{	
	int read_size;
	if (read_pos_ < response_data_.size()) {
		if (size == 0) {
			data = response_data_.substr(read_pos_);
			read_pos_ = response_data_.size();
			read_size = response_data_.size();
		} else {
			read_size = (read_pos_ + size <= response_data_.size()?size:(response_data_.size() - read_pos_ + size));
			data = response_data_.substr(read_pos_, read_size);
			read_pos_ += read_size;
		}
	}
	return read_size;
}

int HttpResponse::write(std::string data, int size)
{
	int write_size = (size == 0? data.size():size);
	response_data_.append(data);
	return write_size;
}