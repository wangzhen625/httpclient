#include "httpclient.h"
#include <map>
#include <string>
#include <iostream>
using namespace std;

int main()
{
	HttpClient *httpclient = new HttpClient();
	HttpRequest request;
	HttpResponse response;
	request.addHeader("User-Agent", " Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9) Gecko/2008061015 Firefox/3.0");
	request.addHeader("Accept", " text/html,application/xhtml+xml,application/xml");
	request.addHeader("Accept-Language", "en-us,en;q=0.5");
	request.addHeader("Accept-Encoding", "gzip,deflate");
	request.write("hello");
	httpclient->get("127.0.0.1:8889/wangzhen/test/20160802",request, response);
	map<string, string> headers = response.getHeaders();
	map<string, string>::iterator it;
	for (it = headers.begin(); it != headers.end(); it++) {
		cout << it->first<<": " <<it->second<<endl;
	}
	cout << response.getResponseData()<<endl;
	return 0;
}