/*********************************************

    Copyright (c) Jackson Nestelroad 2020
    jackson.nestelroad.com

*********************************************/

#include "server.hpp"

namespace proxy {
    server::server()
        : io_contexts(program::options::instance().thread_pool_size),
        is_running(false),
        needs_cleanup(false),
        interceptors(),
        connection_manager(interceptors)
    {
        // Using SSL, set up the server's certificate store
        if (!program::options::instance().tunnel_all_connect_requests) {
            tcp::tls::tls_service::create_cert_store();
        }
    }

    server::~server() {
        stop();
    }

    void server::run_service(boost::asio::io_context &ioc) {
        while (true) {
            try {
                ioc.run();
                break;
            }
            catch (const std::exception &ex) {
                out::safe_error::log("Unexpected error", ex.what());
            }
        }
    }

    void server::start() {
        is_running = true;
        needs_cleanup = true;
        signals.reset(new util::signal_handler(io_contexts.get_io_context()));

        signals->wait(boost::bind(&server::signal_stop, this));

        acc.reset(new acceptor(io_contexts, connection_manager));
        acc->start();
        io_contexts.run(run_service);
    }

    // When server is stopped with signals, it stops running but is not claned up here
    // Since the signal handler is running on the same threads as the server, we 
    // cannot clean up here without a resource dead lock
    // Thus, the needs_cleanup flag signals another thread to clean things up
    void server::signal_stop() {
        is_running = false;
        blocker.unblock();
    }

    // Stops the server and calls cleanup methods
    void server::stop() {
        signal_stop();
        cleanup();
    }

    // Cleans up the server, if necessary
    void server::cleanup() {
        if (needs_cleanup) {
            if (acc) {
                acc->stop();
            }
            io_contexts.stop();
            signals.reset();
            blocker.unblock();
            needs_cleanup = false;
        }
    }

    void server::await_stop() {
        if (is_running) {
            blocker.block();
        }
        cleanup();
    }

    void server::pause_signals() {
        if (!signals) {
            throw error::invalid_operation_exception { "Cannot pause signals when server is not running." };
        }
        signals->pause();
    }

    void server::unpause_signals() {
        if (!signals) {
            throw error::invalid_operation_exception { "Cannot unpause signals when server is not running." };
        }
        signals->unpause();
    }

    bool server::running() const {
        return is_running;
    }

    std::string server::endpoint_string() const {
        if (!acc) {
            throw error::invalid_operation_exception { "Cannot access port before server has started. Call server.start(options) first." };
        }
        std::stringstream str;
        str << acc->get_endpoint();
        return str.str();
    }

    boost::asio::io_context &server::get_io_context() {
        return io_contexts.get_io_context();
    }
}