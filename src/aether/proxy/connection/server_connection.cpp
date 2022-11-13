/*********************************************

  Copyright (c) Jackson Nestelroad 2020
  jackson.nestelroad.com

*********************************************/

#include "server_connection.hpp"

#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <string>
#include <string_view>

#include "aether/proxy/tls/x509/certificate.hpp"
#include "aether/proxy/types.hpp"

namespace proxy::connection {
server_connection::server_connection(boost::asio::io_context& ioc, server_components& components)
    : base_connection(ioc, components), resolver_(ioc), connected_(false), port_() {}

void server_connection::connect_async(std::string host, port_t port, const err_callback_t& handler) {
  // Already have an open connection.
  if (is_connected_to(host, port) && !has_been_closed()) {
    boost::asio::post(ioc_, boost::bind(handler, boost::system::errc::make_error_code(boost::system::errc::success)));
    return;
  } else if (connected_) {
    // Need a new connection
    close();
    connected_ = false;
  }

  host_ = std::move(host);
  port_ = std::move(port);

  set_timeout();
  boost::asio::ip::tcp::resolver::query query(host_, boost::lexical_cast<std::string>(port_));
  resolver_.async_resolve(
      query, boost::asio::bind_executor(
                 strand_, boost::bind(&server_connection::on_resolve, this, boost::asio::placeholders::error,
                                      boost::asio::placeholders::iterator, handler)));
}

void server_connection::on_resolve(const boost::system::error_code& err,
                                   boost::asio::ip::tcp::resolver::iterator endpoint_iterator,
                                   const err_callback_t& handler) {
  timeout_.cancel_timeout();
  if (err != boost::system::errc::success) {
    boost::asio::post(ioc_, boost::bind(handler, err));
  } else if (endpoint_iterator == boost::asio::ip::tcp::resolver::iterator()) {
    // No endpoints found
    boost::asio::post(
        ioc_, boost::bind(handler, boost::system::errc::make_error_code(boost::system::errc::host_unreachable)));
  } else {
    set_timeout();
    endpoint_ = endpoint_iterator->endpoint();
    auto& curr = *endpoint_iterator;
    socket_.async_connect(curr, boost::asio::bind_executor(strand_, boost::bind(&server_connection::on_connect, this,
                                                                                boost::asio::placeholders::error,
                                                                                ++endpoint_iterator, handler)));
  }
}

void server_connection::on_connect(const boost::system::error_code& err,
                                   boost::asio::ip::tcp::resolver::iterator endpoint_iterator,
                                   const err_callback_t& handler) {
  timeout_.cancel_timeout();
  if (err == boost::system::errc::success) {
    connected_ = true;
    boost::asio::post(ioc_, boost::bind(handler, err));
  }
  // Didn't connect, but other endpoints to try.
  else if (endpoint_iterator != boost::asio::ip::tcp::resolver::iterator()) {
    set_timeout();
    endpoint_ = endpoint_iterator->endpoint();
    auto& curr = *endpoint_iterator;
    socket_.async_connect(curr, boost::asio::bind_executor(strand_, boost::bind(&server_connection::on_connect, this,
                                                                                boost::asio::placeholders::error,
                                                                                ++endpoint_iterator, handler)));
  } else {
    // Failed to connect.
    boost::asio::post(ioc_, boost::bind(handler, err));
  }
}

void server_connection::establish_tls_async(tls::openssl::ssl_context_args& args, const err_callback_t& handler) {
  ssl_context_ = tls::openssl::create_ssl_context(args);
  secure_socket_ = std::make_unique<std::remove_reference_t<decltype(*secure_socket_)>>(socket_, *ssl_context_);

  SSL_set_connect_state(secure_socket_->native_handle());

  if (!SSL_set_tlsext_host_name(secure_socket_->native_handle(), host_.c_str())) {
    throw error::tls::ssl_context_error_exception{"Failed to set SNI extension"};
  }

  tls::openssl::enable_hostname_verification(*ssl_context_, host_);

  secure_socket_->async_handshake(
      boost::asio::ssl::stream_base::handshake_type::client,
      boost::asio::bind_executor(
          strand_, boost::bind(&server_connection::on_handshake, this, boost::asio::placeholders::error, handler)));
}

void server_connection::on_handshake(const boost::system::error_code& err, const err_callback_t& handler) {
  if (err == boost::system::errc::success) {
    // Get server's certificate.
    cert_ = secure_socket_->native_handle();

    // Get certificate chain.
    STACK_OF(X509)* chain = SSL_get_peer_cert_chain(secure_socket_->native_handle());

    // Copy certificate chain.
    int len = sk_X509_num(chain);
    for (int i = 0; i < len; ++i) {
      cert_chain_.emplace_back(sk_X509_value(chain, i));
    }

    const unsigned char* proto = nullptr;
    unsigned int length = 0;
    SSL_get0_alpn_selected(secure_socket_->native_handle(), &proto, &length);
    if (proto) {
      alpn_ = std::string(reinterpret_cast<const char*>(proto), length);
    }

    tls_established_ = true;
  }

  boost::asio::post(ioc_, boost::bind(handler, err));
}

void server_connection::disconnect() {
  connected_ = false;
  close();
}

}  // namespace proxy::connection
