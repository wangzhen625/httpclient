#ifndef _HTTP_CLEINT_H_
#define _HTTP_CLEINT_H_

#include <string>
#include <map>
#include "tcpconnection.h"

using namespace std;
#define HTTP_CLIENT_DEFAULT_TIMEOUT 15000

enum HttpResponseCode
{
	HTTP_OK = 0,			///Success
	HTTP_PROCESSING,		///Processing
	HTTP_PARSE,				///URL parse error
	HTTP_DNS,				///Can not resolve name
	HTTP_PRTCL,				///Protocol error
	HTTP_NOTFOUND,			///Http 404 error
	HTTP_REFUSED,			///Http 403 error
	HTTP_ERROR,				///Http xxx error
	HTTP_TIMEOUT,			///Connection timeout
	HTTP_CONN,				///Connection error
	HTTP_CLOSE,				///Connection close
	HTTP_DECOMPRESS			///decompress data error
};

class HttpResponse;
class HttpRequest;
//class TcpConnection;

class HttpClient
{
public:
	HttpClient();
	~HttpClient();
	///Execute a GET request on the url
	int get(const string url, HttpRequest request, HttpResponse& response, int timeout = HTTP_CLIENT_DEFAULT_TIMEOUT);
	int post(const string url, HttpRequest request, HttpResponse& response,  int timeout = HTTP_CLIENT_DEFAULT_TIMEOUT);
	///Get last request's http response code
	inline int getHttpResponseCode(){return response_code_;}
	///Set the maximum number of automated redirections
	void setMaxRedirections(int i = 1);
	/// Get the redirect location url
	inline string getRedirectLocation(){return location_url_;}
private:
	enum HTTP_METHOD
	{
		HTTP_GET,
		HTTP_POST,
		HTTP_PUT,
        HTTP_DELETE
	};
	int connect(string url, HTTP_METHOD method, HttpRequest request, HttpResponse& response, int timeout = HTTP_CLIENT_DEFAULT_TIMEOUT);
	int parseUrl(string url, string& host, int& port, string& path);
	int send(const string send_data, int len);
	int recv(string& receive_data, int len);
	int decompress(const string compress_data, string& decompress_data);
	TcpConnection *connection_;
	string location_url_;
	int response_code_;
	int max_redirect_count_;
	int timeout_;
};


class HttpRequest
{
public:
	HttpRequest();
	~HttpRequest();
	//set request header 
	inline void addHeader(string name, string value)
	{
		headers_.insert(pair<string,string>(name, value));
	}
	
	//get all response headers
	inline map<string, string> getHeaders()
	{
		return headers_;
	}

	//set request data is chunked or not
	inline void setIsChunked(bool chunk)
	{
		ischunked_ = chunk;
	}

	//get request data is chunked or not
	inline bool isChunked()
	{
		return ischunked_;
	}

	//set request data type
	inline void setDataType(string data_type)
	{
		request_data_type_ = data_type;
	}

	//get request data type
	inline void getDataType(string data_type)
	{
		request_data_type_ = data_type;
	}

	inline int getDataLength()
	{
		return request_data_.size();
	}
	//read from the request to send
	int read(string& data, int len = 0);
	//write data to request
	int write(string data, int len = 0);
private:
	string request_data_;
	map<string, string> headers_;
	string request_data_type_;
	bool ischunked_;
	size_t read_pos_;
};



class HttpResponse
{
public:
	HttpResponse();
	~HttpResponse();
	inline void setIsChunked(bool chunked)
	{
		ischunked_ = chunked;
	}

	//get data is chunken or not 
	inline bool isChunked()
	{
		return ischunked_;
	}

	//reponse data Content-Type
	inline void setDataType(string type)
	{
		response_data_type_ = type;
	}

	inline string getDataType()
	{
		return response_data_type_;
	} 

	//response all headers
	inline void addHeader(string name, string value)
	{
		headers_.insert(pair<string,string>(name, value));
	}

	//get a designated header
	//@param[in] head_name
	//@param[out] head_value, if head_name not exist, it's can be null
	inline void getHeader(string head_name, string& head_value)
	{	
		map<string, string>::iterator it = headers_.find(head_name);
		if (it != headers_.end()) {
			head_value = it->second;
		}
		head_value = " ";
	}

	//get all response headers
	inline map<string, string> getHeaders()
	{
		return headers_;
	}

	//set response body len
	inline void setDataLength(int len)
	{
		response_data_size_ = len;
	}

	inline int getDataLength()
	{
		return response_data_size_;
	}

	inline string getResponseData()
	{
		return response_data_;
	}
	int write(string data, int size = 0);
	int read(string & data, int size = 0);

private:
	string response_data_;
	map<string, string> headers_;
	string response_data_type_;
	bool ischunked_;
	size_t read_pos_;
	int response_data_size_;
};



#endif