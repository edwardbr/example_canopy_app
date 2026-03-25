/*
 *   Greeting server
 *   Listens on TCP port 8080 (configurable) and serves greet/add/get_server_info calls.
 *
 *   Build: cmake --preset Coroutine && cmake --build build_coroutine --target server
 *   Run:   ./build_coroutine/output/server [--host 127.0.0.1] [--port 8080]
 */

#include <rpc/rpc.h>
#include <streaming/listener.h>
#include <streaming/tcp/acceptor.h>
#include <streaming/tcp/stream.h>
#include <transports/streaming/transport.h>
#include <canopy/network_config/network_args.h>
#include <greeting/greeting.h>

#include <csignal>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    struct augmented_cli
    {
        int argc = 0;
        std::vector<std::string> storage;
        std::vector<char*> argv;
    };

    bool has_cli_option(
        int argc,
        char* argv[],
        std::string_view option)
    {
        const std::string with_equals = std::string(option) + "=";
        for (int i = 1; i < argc; ++i)
        {
            const std::string_view arg = argv[i];
            if (arg == option || arg.rfind(with_equals, 0) == 0)
                return true;
        }

        return false;
    }

    augmented_cli add_default_network_args(
        int argc,
        char* argv[])
    {
        augmented_cli result;
        result.storage.reserve(8);
        result.argv.reserve(argc + 8);

        for (int i = 0; i < argc; ++i)
            result.argv.push_back(argv[i]);

        const bool has_any_va
            = has_cli_option(argc, argv, "--va-name") || has_cli_option(argc, argv, "--va-type")
              || has_cli_option(argc, argv, "--va-prefix") || has_cli_option(argc, argv, "--va-subnet-bits")
              || has_cli_option(argc, argv, "--va-object-id-bits") || has_cli_option(argc, argv, "--va-subnet");
        const bool has_listen = has_cli_option(argc, argv, "--listen");

        auto append = [&result](std::initializer_list<const char*> args)
        {
            for (const char* arg : args)
            {
                result.storage.emplace_back(arg);
                result.argv.push_back(result.storage.back().data());
            }
        };

        if (!has_any_va)
        {
            append(
                {"--va-name=server",
                    "--va-type=ipv4",
                    "--va-prefix=127.0.0.1",
                    "--va-subnet-bits=32",
                    "--va-object-id-bits=32",
                    "--va-subnet=1"});
        }

        if (!has_listen)
            append({"--listen=server:127.0.0.1:8080"});

        result.argc = static_cast<int>(result.argv.size());
        return result;
    }
}

void rpc_log(
    int level,
    const char* str,
    size_t sz)
{
    std::string message(str, sz);
    switch (level)
    {
    case 0:
        printf("[TRACE] %s\n", message.c_str());
        break;
    case 1:
        printf("[DEBUG] %s\n", message.c_str());
        break;
    case 2:
        printf("[INFO]  %s\n", message.c_str());
        break;
    case 3:
        printf("[WARN]  %s\n", message.c_str());
        break;
    case 4:
        printf("[ERROR] %s\n", message.c_str());
        break;
    default:
        printf("[LOG %d] %s\n", level, message.c_str());
        break;
    }
}

namespace greeting_app
{
    class greeter_impl : public rpc::base<greeter_impl, i_greeter>
    {
    public:
        CORO_TASK(int)
        greet(
            const std::string& name,
            std::string& response) override
        {
            response = "Hello, " + name + "! Greetings from the Canopy server.";
            CO_RETURN rpc::error::OK();
        }

        CORO_TASK(int)
        add(int a,
            int b,
            int& result) override
        {
            result = a + b;
            CO_RETURN rpc::error::OK();
        }

        CORO_TASK(int)
        get_server_info(std::string& info) override
        {
            info = "greeting_app server v1.0 (built with Canopy)";
            CO_RETURN rpc::error::OK();
        }
    };
}

static rpc::event g_shutdown;

void handle_signal(int)
{
    g_shutdown.set();
}

CORO_TASK(int)
run_server(
    std::shared_ptr<coro::scheduler> scheduler,
    const canopy::network_config::network_config& cfg)
{
    auto on_shutdown_event = std::make_shared<rpc::event>();
    {
        const auto& listen_ep = cfg.listen_endpoints.front();
        std::string host = listen_ep.to_string();
        uint16_t port = listen_ep.port;

        const auto domain = listen_ep.family == canopy::network_config::ip_address_family::ipv6
                                ? coro::net::domain_t::ipv6
                                : coro::net::domain_t::ipv4;
        const coro::net::socket_address endpoint{coro::net::ip_address::from_string(host, domain), port};

        rpc::zone_address server_zone_addr;
        auto allocator = canopy::network_config::make_allocator(cfg);
        allocator.allocate_zone(server_zone_addr);

        auto service = std::make_shared<rpc::root_service>("greeting_server", rpc::zone{server_zone_addr}, scheduler);
        service->set_shutdown_event(on_shutdown_event);

        RPC_INFO("Server: Listening on {}:{} — press Ctrl+C to stop", host, port);

        auto listener = std::make_shared<streaming::listener>(
            "server_transport",
            std::make_shared<streaming::tcp::acceptor>(endpoint),
            rpc::stream_transport::make_connection_callback<greeting_app::i_greeter, greeting_app::i_greeter>(
                [](const rpc::shared_ptr<greeting_app::i_greeter>&,
                    const std::shared_ptr<rpc::service>&) -> CORO_TASK(rpc::service_connect_result<greeting_app::i_greeter>)
                {
                    RPC_INFO("Server: Client");
                    CO_RETURN rpc::service_connect_result<greeting_app::i_greeter>{
                        rpc::error::OK(), rpc::shared_ptr<greeting_app::i_greeter>(new greeting_app::greeter_impl())};
                }));

        if (!listener->start_listening(service))
        {
            RPC_ERROR("Server: Failed to start listening on {}:{}", host, port);
            CO_RETURN 1;
        }

        service.reset();

        co_await g_shutdown.wait();

        RPC_INFO("Server: Shutting down...");

        co_await listener->stop_listening();
        listener.reset();
    }

    co_await on_shutdown_event->wait();

    RPC_INFO("Server: Done.");
    CO_RETURN 0;
}

int main(
    int argc,
    char* argv[])
{
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    canopy::network_config::network_config cfg;

    {
        args::ArgumentParser parser("Greeting server: serves RPC calls over TCP.");
        args::HelpFlag help(parser, "help", "Display this help message and exit", {'h', "help"});
        auto net = canopy::network_config::add_network_args(parser);
        auto cli = add_default_network_args(argc, argv);

        try
        {
            parser.ParseCLI(cli.argc, cli.argv.data());
        }
        catch (const args::Help&)
        {
            std::cout << parser;
            return 0;
        }
        catch (const args::ParseError& e)
        {
            std::cerr << e.what() << "\n" << parser;
            return 1;
        }

        try
        {
            cfg = net.get_config();
        }
        catch (const std::invalid_argument& e)
        {
            std::cerr << "Configuration error: " << e.what() << "\n";
            return 1;
        }

        cfg.log_values();
    }

    auto scheduler = std::shared_ptr<coro::scheduler>(coro::scheduler::make_unique(
        coro::scheduler::options{.thread_strategy = coro::scheduler::thread_strategy_t::spawn,
            .pool = coro::thread_pool::options{.thread_count = 4},
            .execution_strategy = coro::scheduler::execution_strategy_t::process_tasks_on_thread_pool}));

    int result = coro::sync_wait(run_server(scheduler, cfg));

    scheduler->shutdown();
    return result;
}
