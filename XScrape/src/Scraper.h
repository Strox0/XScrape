#pragma once
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "CfgParser.h"

//namespace http = boost::beast::http;
//namespace ssl = boost::asio::ssl;

class Scraper
{
	using tcp = boost::asio::ip::tcp;
public:

	Scraper(_Data& data);
	Scraper(_Data& data,std::string ofolder_name);
	void Start();

	void PolyRunMode(bool state,HANDLE comspipe = INVALID_HANDLE_VALUE);

private:
	void LoadRootCerts(boost::asio::ssl::context& ctx);
	void SaveImage(void* data, size_t data_size,std::string extension);
	bool Close(boost::asio::ssl::stream<tcp::socket>& stream,bool force = false);

	bool Connect(std::string domain, boost::asio::ssl::stream<tcp::socket>** outStream,boost::asio::ssl::context** outSslContext);
	bool Connect(std::string domain, std::unique_ptr<boost::asio::ssl::stream<tcp::socket>>& outStream,std::unique_ptr<boost::asio::ssl::context>& outSslContext);

	void DecodeBody(boost::beast::http::response<boost::beast::http::dynamic_body>& response, std::string& outBody);
	void DecodeBody(boost::beast::http::response_parser<boost::beast::http::dynamic_body>& response, std::string& outBody);

	void CreateRequest(boost::asio::ssl::stream<tcp::socket>& stream, boost::beast::http::response_parser<boost::beast::http::dynamic_body>& response, boost::beast::flat_buffer& buffer, bool show_progress = false,int max_it = 10,bool only_header = false);
	void CreateRequest(boost::asio::ssl::stream<tcp::socket>& stream, boost::beast::http::response_parser<boost::beast::http::buffer_body>& response, boost::beast::flat_buffer& buffer, bool show_progress = false, int max_it = 10,bool only_header = false);

	struct task
	{
		std::string webpage;
		int operationIndex;
	};

private:
	boost::asio::io_context m_ioc;
	boost::beast::http::request<boost::beast::http::string_body> m_request;

	boost::asio::ssl::stream<tcp::socket>* m_stream = nullptr;
	boost::asio::ssl::context* m_ssl_ctx = nullptr;

	_Data m_data;
	std::string m_mainbody;
	std::string_view m_user_agent;
	std::string m_outfolder;

	bool m_polyrun = false;
	HANDLE m_coms_pipe = INVALID_HANDLE_VALUE;
};