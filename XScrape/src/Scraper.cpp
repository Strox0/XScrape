#include "Scraper.h"
#include <Windows.h>
#include <wincrypt.h>
#include <fstream>
#include <sstream>
#include <istream>
#include <streambuf>
#include <queue>
#include "Random.h"
#include "UserAgents.h"
#include <regex>
#include <iostream>
#include "boost/iostreams/filtering_stream.hpp"
#include "boost/iostreams/filter/gzip.hpp"
#include "boost/iostreams/filter/zlib.hpp"
#include "boost/iostreams/copy.hpp"
#include "boost/asio.hpp"

#include "InstStatus.h"

#pragma comment (lib, "crypt32.lib")
#pragma comment (lib, "cryptui.lib")

namespace http = boost::beast::http;
namespace ssl = boost::asio::ssl;

struct incr_compr
{
	constexpr auto operator()(const _Data::Incr& left,const _Data::Incr& right) const noexcept
	{
		return left.pos < right.pos;
	}
};

Scraper::Scraper(_Data& data) : m_data(data), m_outfolder("./output/")
{
	Random::Number<int> num(0, UserAgents::size - 1);
	m_user_agent = UserAgents::agents[num.Gen()];

	if (data.increments.size() > 1)
		std::sort(data.increments.begin(), data.increments.end(), incr_compr());
}

Scraper::Scraper(_Data& data, std::string ofolder_name) : m_data(data), m_outfolder(ofolder_name)
{
	m_outfolder.push_back('/');
	m_outfolder.insert(0, "./");

	Random::Number<int> num(0, UserAgents::size - 1);
	m_user_agent = UserAgents::agents[num.Gen()];

	if (data.increments.size() > 1)
		std::sort(data.increments.begin(), data.increments.end(), incr_compr());
}

void Scraper::Start()
{
	int img_num = 0;
	if (Connect(m_data.domain, &m_stream, &m_ssl_ctx))
		return;

	m_request.method(http::verb::get);
	m_request.version(11);
	m_request.target(m_data.target);
	m_request.set(http::field::host, m_data.domain);
	m_request.set(http::field::accept_language, "en-US,en-GB;q=0.9,en;q=0.8");
	m_request.set(http::field::accept_encoding, "gzip, deflate");
	m_request.set(http::field::accept, "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7");		
	m_request.set(http::field::user_agent, m_user_agent);

	for (size_t mi = 0; mi < m_data.limit; mi++)
	{
		{
			int it_count = 0;
			while (it_count < 5)
			{
				it_count++;
				try
				{
					boost::beast::flat_buffer buffer;
					http::response_parser<http::dynamic_body> response;
					CreateRequest(*m_stream, response, buffer);

					DecodeBody(response, m_mainbody);
					break;
				}
				catch (boost::system::system_error const& e)
				{					
					if (e.code() == boost::asio::error::eof)
					{
						Close(*m_stream);
						delete m_stream;
						delete m_ssl_ctx;
						if (Connect(m_data.domain, &m_stream, &m_ssl_ctx))
							return;
						break;
					}
					else
						throw e;
				}
				Sleep(1000);
			}
		}

		for (auto& bounds : m_data.bounds)
		{
			std::queue<task> tasks;
			tasks.push({ m_mainbody,0 });

			while (!tasks.empty())
			{
				task curr_task = tasks.front();
				tasks.pop();

				std::regex start_reg(bounds[curr_task.operationIndex].expr);

				std::sregex_iterator reg_it(curr_task.webpage.cbegin(), curr_task.webpage.cend(), start_reg);
				std::sregex_iterator reg_it_end;

				if (reg_it == reg_it_end)
				{
					std::cout << "Warning no regex matches found\n";
				}

				if (curr_task.operationIndex + 1 == bounds.size())
				{
					while (reg_it != reg_it_end)
					{
						img_num++;

						if (m_polyrun)
						{
							inst_status stat(m_data.domain.c_str(), "Running", img_num);
							DWORD written = 0;

							WriteFile(m_coms_pipe, &stat, sizeof(inst_status), &written, NULL);

							if (written != sizeof(inst_status))
							{
								MessageBoxA(NULL, "Failed to write to pipe", "Error", MB_ICONERROR);
							}
						}

						std::string result = reg_it->str(std::stoull(bounds[curr_task.operationIndex].group_num));
						std::unique_ptr<boost::asio::ssl::stream<tcp::socket>> curr_stream = nullptr;
						std::unique_ptr<ssl::context> ssl_ctx = nullptr;
						bool new_website = false;
						std::string domain = m_data.domain;

						std::cout << "Downloading image from : " << result << std::endl;

						if (result.find("https://") != std::string::npos)
						{
							new_website = true;
							result.erase(0, 8);
							size_t target_i = result.find('/');
							if (target_i != std::string::npos)
							{
								domain = result.substr(0, target_i);

								while (Connect(domain, curr_stream, ssl_ctx))
								{
									Sleep(10);
								}

								result.erase(0, target_i);
							}
						}
						else
							curr_stream.reset(m_stream);

						//create head request
						m_request.target(result);
						m_request.method(http::verb::get);
						m_request.set(http::field::host, domain);
						m_request.set(http::field::connection, "keep-alive");

						size_t image_size = 0;
						std::string extension;

						boost::beast::flat_buffer buffer;
						http::response_parser<http::dynamic_body> res;
						res.body_limit(boost::none);
						CreateRequest(*curr_stream, res, buffer,true);
						std::cout << "\tDone\n";

						if (res.get().result() == http::status::ok)
						{
							//determain image size, extension
							image_size = std::stoull(res.get()[http::field::content_length]);
							std::string content_type = res.get()[http::field::content_type];

							if (content_type == "image/jpeg")
								extension = ".jpg";
							else if (content_type == "image/png")
								extension = ".png";
							else if (content_type == "image/gif")
								extension = ".gif";
							else if (content_type == "image/webp")
								extension = ".webp";
							else if (content_type == "image/bmp")
								extension = ".bmp";
							else if (content_type == "image/svg+xml")
								extension = ".svg";
							else if (content_type == "video/mp4")
								extension = ".mp4";
							else if (content_type == "video/webm")
								extension = ".webm";
							else if (content_type == "video/x-msvideo")
								extension = ".avi";
							else if (content_type == "video/quicktime")
								extension = ".mov";
							else if (content_type == "video/mpeg")
								extension = ".mpeg";
						}

						std::vector<unsigned char> img_buffer(boost::asio::buffers_begin(res.get().body().data()), boost::asio::buffers_end(res.get().body().data()));

						if (res.get().find(http::field::content_encoding) != res.get().end())
						{
							std::cout << "\tDecoding...\n";
							boost::iostreams::filtering_istream decoder;
							std::string encoding = res.get()[http::field::content_encoding];
							std::stringstream bufferStream;

							if (encoding == "gzip")
								decoder.push(boost::iostreams::gzip_decompressor());
							else if (encoding == "deflate")
								decoder.push(boost::iostreams::zlib_decompressor());

							bufferStream.write(reinterpret_cast<const char*>(img_buffer.data()), img_buffer.size());

							decoder.push(bufferStream);

							std::vector<unsigned char> decoded_img_buffer;
							boost::iostreams::copy(decoder, std::back_inserter(decoded_img_buffer));
							std::cout << "\tDone\n";
							SaveImage(decoded_img_buffer.data(), decoded_img_buffer.size(), extension);
						}
						else
							SaveImage(img_buffer.data(), img_buffer.size(), extension);

						if (new_website)
						{
							Close(*curr_stream);
						}
						Sleep(100);
						reg_it++;
					}
				}

				if (curr_task.operationIndex < bounds.size() - 1)
				{
					while (reg_it != reg_it_end)
					{
						std::string result = reg_it->str(std::stoull(bounds[curr_task.operationIndex].group_num));

						m_request.target(result);
						m_request.method(http::verb::get);

						boost::beast::flat_buffer buffer;
						http::response_parser<http::dynamic_body> response;
						CreateRequest(*m_stream, response, buffer);

						std::string body;
						DecodeBody(response, body);
						tasks.push({ body,curr_task.operationIndex + 1 });

						Sleep(100);
						reg_it++;
					}
				}
			}
		}

		size_t length_override = 0;
		for (auto& i : m_data.increments)
		{
			std::string mod_nums = m_data.target.substr(i.pos+length_override, i.length);

			size_t num = std::stoull(mod_nums);

			num += i.ammount;
			mod_nums = std::to_string(num);

			m_data.target.erase(i.pos+length_override, i.length);
			m_data.target.insert(i.pos+length_override, mod_nums);

			m_request.target(m_data.target);
			m_request.set(http::field::host, m_data.domain);

			if (mi+1 < m_data.limit)
				std::cout << "New target : " << m_data.target << std::endl;

			size_t old_len = i.length;
			i.length = mod_nums.length();
			if (old_len < i.length)
			{
				length_override += i.length - old_len;
			}
		}
	}

	std::cout << "Donwloaded : " << img_num << " images\n";
	Close(*m_stream);
}

void Scraper::PolyRunMode(bool state, HANDLE comspipe)
{
	m_coms_pipe = comspipe;
	m_polyrun = true;
}

bool Scraper::Close(ssl::stream<boost::asio::ip::tcp::socket>& stream,bool force)
{
	boost::system::error_code ec;
	if (force)
		stream.next_layer().close(ec);
	else
		stream.shutdown(ec);
	if (ec == boost::asio::error::eof || ec.value() == 1)
	{
		ec = {};
	}
	if (ec)
	{
		std::cerr << "Error shutting down SSL stream: " << ec.value() << '\n';
		return true;
	}
	else
		return false;
}

void Scraper::CreateRequest(boost::asio::ssl::stream<tcp::socket>& stream, http::response_parser<http::dynamic_body>& response, boost::beast::flat_buffer& buffer, bool show_progress,int max_it,bool only_header)
{
	int iteration = 0;
	while (iteration < max_it)
	{
		iteration++;
		http::write(stream, m_request);

		if (!show_progress)
		{
			if (only_header)
				http::read_header(stream, buffer, response);
			else
				http::read(stream, buffer, response);
		}
		else
		{
			size_t read = 0;

			if (only_header)
			{
				while (!response.is_header_done())
				{
					read += http::read_some(stream, buffer, response);
					std::cout << "\rReciving : " << (double)read / 1000.0 << " kb";
				}
			}
			else
			{
				while (!response.is_done())
				{
					read += http::read_some(stream, buffer, response);
					std::cout << "\rReciving : " << (double)read / 1000.0 << " kb";
				}
			}
		}

		if (response.get().result() == http::status::ok)
			break;

		std::cout << m_request.at(http::field::host) << m_request.target() << " Not OK, status Code : " << response.get().result_int() << std::endl;

		if (response.get().result() == http::status::service_unavailable || response.get().result() == http::status::too_many_requests)
		{
			if (response.get().find(http::field::retry_after) != response.get().end())
			{
				std::string sleep_time = response.get().at(http::field::retry_after);

				if (sleep_time[0] < '0' || sleep_time[0] > '9')
					sleep_time = "1";

				size_t wait = std::stoull(sleep_time);
				wait *= 1000;

				Sleep(wait);
			}
			else
				Sleep(5000);
		}
		else
			break;
	}
}

void Scraper::CreateRequest(boost::asio::ssl::stream<tcp::socket>& stream, http::response_parser<http::buffer_body>& response, boost::beast::flat_buffer& buffer, bool show_progress, int max_it, bool only_header)
{
	int iteration = 0;
	while (iteration < max_it)
	{
		iteration++;
		http::write(stream, m_request);

		if (!show_progress)
		{
			if (only_header)
				http::read_header(stream, buffer, response);
			else
				http::read(stream, buffer, response);
		}
		else
		{
			size_t read = 0;

			if (only_header)
			{
				while (!response.is_header_done())
				{
					read += http::read_some(stream, buffer, response);
					std::cout << "\rReciving : " << (double)read / 1000.0 << " kb";
				}
			}
			else
			{
				while (!response.is_done())
				{
					read += http::read_some(stream, buffer, response);
					std::cout << "\rReciving : " << (double)read/1000.0 << " kb";
				}
			}
		}
		
		if (response.get().result() == http::status::ok)
			break;

		std::cout << m_request.at(http::field::host) << m_request.target() << " Not OK, status Code : " << response.get().result_int() << std::endl;

		if (response.get().result() == http::status::service_unavailable || response.get().result() == http::status::too_many_requests)
		{
			if (response.get().find(http::field::retry_after) != response.get().end())
			{
				std::string sleep_time = response.get().at(http::field::retry_after);

				if (sleep_time[0] < '0' || sleep_time[0] > '9')
					sleep_time = "1";

				size_t wait = std::stoull(sleep_time);
				wait *= 1000;

				Sleep(wait);
			}
			else
				Sleep(5000);
		}
		else
			break;
	}
}

void Scraper::DecodeBody(http::response_parser<http::dynamic_body>& response, std::string& outBody)
{
	if (response.get().find(http::field::content_encoding) != response.get().end())
	{
		std::cout << "Decoding...\n";
		boost::iostreams::filtering_istream decoder;
		std::string encoding = response.get()[http::field::content_encoding];
		std::stringstream bufferStream;

		if (encoding == "gzip")
			decoder.push(boost::iostreams::gzip_decompressor());
		else if (encoding == "deflate")
			decoder.push(boost::iostreams::zlib_decompressor());

		std::vector<unsigned char> tmp_bodybuf(boost::asio::buffers_begin(response.get().body().data()), boost::asio::buffers_end(response.get().body().data()));
		bufferStream.write(reinterpret_cast<const char*>(tmp_bodybuf.data()), tmp_bodybuf.size());

		decoder.push(bufferStream);

		std::stringstream stringBodyStream;
		boost::iostreams::copy(decoder, stringBodyStream);
		std::cout << "Done...\n";
		outBody = stringBodyStream.str();
	}
	else
		outBody = boost::beast::buffers_to_string(response.get().body().data());
}

void Scraper::DecodeBody(http::response<http::dynamic_body>& response, std::string& outBody)
{
	if (response.find(http::field::content_encoding) != response.end())
	{
		std::cout << "Decoding...\n";
		boost::iostreams::filtering_istream decoder;
		std::string encoding = response[http::field::content_encoding];
		std::stringstream bufferStream;

		if (encoding == "gzip")
			decoder.push(boost::iostreams::gzip_decompressor());
		else if (encoding == "deflate")
			decoder.push(boost::iostreams::zlib_decompressor());

		std::vector<unsigned char> tmp_bodybuf(boost::asio::buffers_begin(response.body().data()), boost::asio::buffers_end(response.body().data()));
		bufferStream.write(reinterpret_cast<const char*>(tmp_bodybuf.data()), tmp_bodybuf.size());

		decoder.push(bufferStream);

		std::stringstream stringBodyStream;
		boost::iostreams::copy(decoder, stringBodyStream);
		std::cout << "Done...\n";
		outBody = stringBodyStream.str();
	}
	else
		outBody = boost::beast::buffers_to_string(response.body().data());
}

bool Scraper::Connect(std::string domain, boost::asio::ssl::stream<tcp::socket>** outStream, boost::asio::ssl::context** outSslContext)
{
	std::string port = "443";
	size_t port_pos = domain.rfind(':');
	if (port_pos != std::string::npos)
	{
		port = domain.substr(port_pos + 1);
		domain.erase(domain.begin()+port_pos,domain.end());
	}

	*outSslContext = new boost::asio::ssl::context(ssl::context_base::tlsv12_client);
	LoadRootCerts(*(*(outSslContext)));
	(*(outSslContext))->set_verify_mode(ssl::context_base::verify_peer);
	*outStream = new boost::asio::ssl::stream<tcp::socket>(m_ioc, *(*(outSslContext)));

	if (!SSL_set_tlsext_host_name((*(outStream))->native_handle(), domain.data()))
	{
		delete *outStream;
		delete *outSslContext;
		return true;
	}

	boost::system::error_code ec;
	tcp::resolver reslover(m_ioc);
	boost::asio::connect((*(outStream))->next_layer(), reslover.resolve(domain,port),ec);

	if (ec)
	{
		delete* outStream;
		delete* outSslContext;
		return true;
	}

	(*(outStream))->handshake(ssl::stream_base::client,ec);

	if (ec)
	{
		(*(outStream))->next_layer().close();
		delete* outStream;
		delete* outSslContext;
		return true;
	}

	return false;
}

bool Scraper::Connect(std::string domain, std::unique_ptr<boost::asio::ssl::stream<tcp::socket>>& outStream, std::unique_ptr<boost::asio::ssl::context>& outSslContext)
{
	std::string port = "443";
	size_t port_pos = domain.rfind(':');
	if (port_pos != std::string::npos)
	{
		port = domain.substr(port_pos + 1);
		domain.erase(domain.begin() + port_pos, domain.end());
	}

	outSslContext.reset(new boost::asio::ssl::context(ssl::context_base::tlsv12_client));
	LoadRootCerts(*(outSslContext));
	outSslContext->set_verify_mode(ssl::context_base::verify_peer);
	outStream.reset(new boost::asio::ssl::stream<tcp::socket>(m_ioc, *outSslContext));

	if (!SSL_set_tlsext_host_name(outStream->native_handle(), domain.data()))
	{
		outStream.release();
		outSslContext.release();
		return true;
	}

	boost::system::error_code ec;
	tcp::resolver reslover(m_ioc);
	boost::asio::connect(outStream->next_layer(), reslover.resolve(domain, port), ec);

	if (ec)
	{
		outStream.release();
		outSslContext.release();
		return true;
	}

	outStream->handshake(ssl::stream_base::client, ec);

	if (ec)
	{
		outStream->next_layer().close();
		outStream.release();
		outSslContext.release();
		return true;
	}

	return false;
}

void Scraper::LoadRootCerts(boost::asio::ssl::context& ctx)
{
	HCERTSTORE hStore = CertOpenSystemStoreA(NULL, "ROOT");
	if (hStore == NULL)
	{
		// Handle error
		return;
	}

	X509_STORE* store = X509_STORE_new();
	PCCERT_CONTEXT pContext = NULL;
	while ((pContext = CertFindCertificateInStore(hStore, X509_ASN_ENCODING, 0, CERT_FIND_ANY, NULL, pContext)) != NULL)
	{
		const unsigned char* pData = pContext->pbCertEncoded;
		X509* x509 = d2i_X509(NULL, &pData, pContext->cbCertEncoded); // Notice the use of &pData
		if (x509 != NULL)
		{
			X509_STORE_add_cert(store, x509);
			X509_free(x509);
		}
	}

	if (pContext != NULL) CertFreeCertificateContext(pContext);
	CertCloseStore(hStore, CERT_CLOSE_STORE_FORCE_FLAG);

	SSL_CTX_set_cert_store(ctx.native_handle(), store);
}

void Scraper::SaveImage(void* data, size_t data_size,std::string extension)
{
	static bool dir_exists_run = false;
	if (!dir_exists_run)
	{
		if (!std::filesystem::exists(m_outfolder))
			std::filesystem::create_directories(m_outfolder);
		else
		{
			MessageBeep(MB_ICONEXCLAMATION);
			std::cout << "Output directory already exists, do you want it to be renamed? (Y/N) ";
			char yn = 0;
			std::cin >> yn;

			if (yn == 'y' || yn == 'Y')
			{
				int number = 0;
				//std::string dirn = "./output-0/";
				std::string dirn = m_outfolder;
				dirn.insert(dirn.size() - 1, "-0");
				while (std::filesystem::exists(dirn))
				{
					number++;
					dirn.erase(dirn.end() - 1, dirn.end());
					dirn.append(std::to_string(number) + '/');
				}
				std::filesystem::rename(m_outfolder, dirn);
				std::filesystem::create_directories(m_outfolder);
			}
			else if (yn != 'n' && yn != 'N')
			{
				std::cout << "Incorrect input\n";
				throw std::runtime_error("error");
			}
		}
		dir_exists_run = true;
	}

	std::string filename;
	if (!m_data.conseq_filenames)
	{
		do
		{
			filename = m_outfolder + Random::String(20, Random::CharSetFlags::Small | Random::CharSetFlags::Numbers) + extension;
		} while (std::filesystem::exists(filename));
	}
	else
	{
		static int num = 0;
		static bool run = false;

		if (!run)
		{
			for (auto& i : std::filesystem::directory_iterator(m_outfolder))
			{
				std::string fn = i.path().stem().string();

				bool isAllDigits = std::all_of(fn.begin(), fn.end(), ::isdigit);

				if (!isAllDigits)
					continue;

				int number = std::stoi(fn);
				if (number >= num)
					num = number + 1;
			}
			run = true;
		}
		else
			++num;

		filename = m_outfolder + std::to_string(num) + extension;
	}

	std::ofstream file(filename, std::ios::binary);
	file.write((char*)data, data_size);
	file.close();

	std::cout << "Output : " << filename << '\n';
}
