/*********************************************

    Copyright (c) Jackson Nestelroad 2020
    jackson.nestelroad.com

*********************************************/

#pragma once

#include <optional>
#include <boost/asio.hpp>

#include <aether/proxy/connection/client_connection.hpp>
#include <aether/proxy/connection/server_connection.hpp>
#include <aether/proxy/error/exceptions.hpp>

namespace proxy::connection {
    /*
        A thin wrapper for a connection pair (client and server).
        Will always have a client connection, but server connection can be started later.
        Client and server connections should never be instantiated separately to assure they are kept together.
    */
    class connection_flow
        : public std::enable_shared_from_this<connection_flow>,
        private boost::noncopyable {
    private:
        boost::asio::io_service &ios;

        // Shared pointers to the two connections to let them use shared_from_this()
        // Private to prevent dangling references

        base_connection::ptr client_ptr;
        base_connection::ptr server_ptr;

        std::string target_host;
        port_t target_port;

    public:
        // "Interfaces" to the shared pointers

        client_connection &client;
        server_connection &server;

        connection_flow(boost::asio::io_service &ios);

        /*
            Sets the server to connect to later.
        */
        void set_server(const std::string &host, port_t port);

        /*
            Connects to a server.
            Set server details using set_server.
        */
        void connect_server_async(const err_callback &handler);

        /*
            Establishes a TLS connection with the server.
            Set server details using set_server.
        */
        void establish_tls_with_server_async(const tcp::tls::openssl::ssl_context_args &args, const err_callback &handler);
        
        /*
            Disconnects both the client and server connections if applicable.
        */
        void disconnect();

        boost::asio::io_service &io_service() const;
    };
}