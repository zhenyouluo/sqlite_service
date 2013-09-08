#if !defined(SQLITE_SERVICE_SERVICE_HPP_)
#define SQLITE_SERVICE_SERVICE_HPP_

#include <string>
#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/bind/protect.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/make_shared.hpp>
#include <sqlite3.h>

#include "sqlite_service/detail/error.hpp"

namespace services { namespace sqlite {

class database
{
public:
	database(boost::asio::io_service & io_service)
		: io_service_(io_service)
		, processing_work_(processing_service_)
		, processing_thread_(boost::bind(&database::run_wrapper, this))
	{
	}
	~database()
	{
		processing_service_.stop();
		processing_thread_.join();
	}
	void run_wrapper()
	{
		processing_service_.run();
	}
	/**
	 * Open database connection asynchronous.
	 * @param url URL parameter.
	 * @param handler Callback which will be fired after open is done.
	 */
	template <typename OpenHandler>
	void async_open(const ::std::string & url, OpenHandler handler)
	{
		processing_service_.post(boost::bind(&database::__async_open<boost::_bi::protected_bind_t<OpenHandler> >, this,
			url,
			boost::make_shared<boost::asio::io_service::work>(boost::ref(io_service_)),
			boost::protect(handler)));
	}
private:
	/**
	 * Open database connection in blocking mode.
	 */
	template <typename HandlerT>
	void __async_open(std::string url, boost::shared_ptr<boost::asio::io_service::work> work, HandlerT handler)
	{
		boost::system::error_code ec;
		int result;
		struct sqlite3 * conn = NULL;
		if ((result = sqlite3_open(url.c_str(), &conn)) != SQLITE_OK)
		{
			ec.assign(result, get_error_category());
		}
		// Connection pointer could be NULL after open.
		if (!conn)
		{
			ec.assign(SQLITE_NOMEM, get_error_category());
		}
		conn_.reset(conn, &sqlite3_close);
		io_service_.post(boost::bind(handler, ec));
	}
	/** Results of blocking methods are posted here */
	boost::asio::io_service & io_service_;
	/** All blocking methods gets posted here */
	boost::asio::io_service processing_service_;
	/** Keep processing thread always busy */
	boost::asio::io_service::work processing_work_;
	/** This thread runs processing queue */
	boost::thread processing_thread_;
	/** Shared instance of sqlite3 connection */
	boost::shared_ptr<struct sqlite3> conn_;
};

} }

#endif