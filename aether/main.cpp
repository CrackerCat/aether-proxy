/*********************************************

    Copyright (c) Jackson Nestelroad 2020
    jackson.nestelroad.com

*********************************************/

#include <conio.h>

#include <aether/program/functions.hpp>
#include <aether/proxy/server.hpp>
#include <aether/util/console.hpp>
#include <aether/interceptors/attach.hpp>

/*
    Program entry-point.
    Start the proxy server and wait for the user to stop it.
*/
int main(int argc, char *argv[]) {
    program::options options = program::parse_cmdline_options(argc, argv);
    proxy::server server(options);

    interceptors::attach_options(server, options);

    try {
        server.start();

        out::console::stream("Started running at ", server.endpoint_string(), out::manip::endl);

        // TODO: Input command system to view server status, view logs, and to stop
        out::console::log("Press any key to stop.");
        _getch();
        server.stop();
    }
    // Proxy error
    catch (const proxy::error::base_exception &ex) {
        out::error::log(ex.what());
        return 1;
    }
    // Unexpected error
    catch (const std::exception &ex) {
        out::error::stream("Unexpected error: ", ex.what());
        return 1;
    }

    return 0;
}