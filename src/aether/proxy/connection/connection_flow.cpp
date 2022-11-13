/*********************************************

  Copyright (c) Jackson Nestelroad 2020
  jackson.nestelroad.com

*********************************************/

#include "connection_flow.hpp"

#include <boost/asio.hpp>
#include <memory>
#include <string_view>
#include <utility>

#include "aether/proxy/connection/client_connection.hpp"
#include "aether/proxy/connection/server_connection.hpp"
#include "aether/proxy/error/error_state.hpp"
#include "aether/proxy/error/exceptions.hpp"
#include "aether/proxy/types.hpp"
#include "aether/util/identifiable.hpp"

namespace proxy::connection {
connection_flow::connection_flow(boost::asio::io_context& ioc, server_components& components)
    : ioc_(ioc),
      client_ptr_(std::make_unique<client_connection>(ioc_, components)),
      server_ptr_(std::make_unique<server_connection>(ioc, components)),
      target_port_(),
      intercept_tls_(false),
      intercept_websocket_(false),
      client(static_cast<client_connection&>(*client_ptr_)),
      server(static_cast<server_connection&>(*server_ptr_)) {}

void connection_flow::set_server(std::string host, port_t port) {
  if (server.connected()) {
    server.disconnect();
  }
  target_host_ = std::move(host);
  target_port_ = port;
}

void connection_flow::connect_server_async(const err_callback_t& handler) {
  server.connect_async(target_host_, target_port_, handler);
}

void connection_flow::establish_tls_with_client_async(tls::openssl::ssl_server_context_args& args,
                                                      const err_callback_t& handler) {
  client.establish_tls_async(args, handler);
}

void connection_flow::establish_tls_with_server_async(tls::openssl::ssl_context_args& args,
                                                      const err_callback_t& handler) {
  server.establish_tls_async(args, handler);
}

void connection_flow::disconnect() {
  client.close();
  server.disconnect();
}

}  // namespace proxy::connection
