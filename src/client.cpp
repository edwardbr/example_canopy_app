/*
 *   Greeting client
 *   Connects to the greeting server over TCP, makes a few RPC calls, then exits.
 *
 *   Build: cmake --preset Coroutine && cmake --build build_coroutine --target client
 *   Run:   ./build_coroutine/output/client [--host 127.0.0.1] [--port 8080]
 */

#include <rpc/rpc.h>
#include <streaming/tcp/stream.h>
#include <transports/streaming/transport.h>
#include <canopy/network_config/network_args.h>
#include <greeting/greeting.h>

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
        const bool has_connect = has_cli_option(argc, argv, "--connect");

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
                {"--va-name=client",
                    "--va-type=ipv4",
                    "--va-prefix=127.0.0.1",
                    "--va-subnet-bits=32",
                    "--va-object-id-bits=32",
                    "--va-subnet=100"});
        }

        if (!has_connect)
            append({"--connect=client:127.0.0.1:8080"});

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

CORO_TASK(rpc::shared_ptr<greeting_app::i_greeter>)
connect_to_server(
    std::shared_ptr<coro::scheduler> scheduler,
    const canopy::network_config::network_config& cfg,
    std::shared_ptr<rpc::event> on_shutdown_event)
{
    const auto& listen_ep = cfg.connect_endpoints.front();
    std::string host = listen_ep.to_string();
    uint16_t port = listen_ep.port;

    rpc::zone_address client_zone_addr;
    auto allocator = canopy::network_config::make_allocator(cfg);
    allocator.allocate_zone(client_zone_addr);

    auto client_service = std::make_shared<rpc::root_service>("greeting_client", rpc::zone{client_zone_addr}, scheduler);
    client_service->set_shutdown_event(on_shutdown_event);

    RPC_INFO("Client: Connecting to {}:{}...", host, port);

    const auto client_domain = listen_ep.family == canopy::network_config::ip_address_family::ipv6
                                   ? coro::net::domain_t::ipv6
                                   : coro::net::domain_t::ipv4;

    coro::net::tcp::client tcp_client(
        scheduler, coro::net::socket_address{coro::net::ip_address::from_string(host, client_domain), port});

    auto connection_status = CO_AWAIT tcp_client.connect(std::chrono::milliseconds(5000));
    if (connection_status != coro::net::connect_status::connected)
    {
        RPC_ERROR("Client: Failed to connect (status: {})", static_cast<int>(connection_status));
        CO_RETURN nullptr;
    }

    RPC_INFO("Client: TCP connection established");

    auto tcp_stm = std::make_shared<streaming::tcp::stream>(std::move(tcp_client), scheduler);
    auto transport = rpc::stream_transport::make_client("client_transport", client_service, std::move(tcp_stm));

    auto connect_result = CO_AWAIT client_service->connect_to_zone<greeting_app::i_greeter, greeting_app::i_greeter>(
        "greeting_server", transport, rpc::shared_ptr<greeting_app::i_greeter>());

    if (connect_result.error_code != rpc::error::OK())
    {
        RPC_ERROR("Client: Zone connection failed: {}", static_cast<int>(connect_result.error_code));
        CO_RETURN nullptr;
    }

    co_return connect_result.output_interface;
}

CORO_TASK(bool)
run_client(
    std::shared_ptr<coro::scheduler> scheduler,
    const canopy::network_config::network_config& cfg)
{
    auto on_shutdown_event = std::make_shared<rpc::event>();
    auto remote = co_await connect_to_server(scheduler, cfg, on_shutdown_event);

    RPC_INFO("Client: RPC connection established");

    // --- greet ---
    std::string greeting;
    auto err = CO_AWAIT remote->greet("World", greeting);
    if (err == rpc::error::OK())
        std::cout << "greet(\"World\") -> \"" << greeting << "\"\n";
    else
        RPC_ERROR("Client: greet() failed: {}", static_cast<int>(err));

    // --- add ---
    int sum = 0;
    err = CO_AWAIT remote->add(40, 2, sum);
    if (err == rpc::error::OK())
        std::cout << "add(40, 2) -> " << sum << "\n";
    else
        RPC_ERROR("Client: add() failed: {}", static_cast<int>(err));

    // --- get_server_info ---
    std::string info;
    err = CO_AWAIT remote->get_server_info(info);
    if (err == rpc::error::OK())
        std::cout << "get_server_info() -> \"" << info << "\"\n";
    else
        RPC_ERROR("Client: get_server_info() failed: {}", static_cast<int>(err));

    remote.reset();

    co_await on_shutdown_event->wait();
    RPC_INFO("Client: Done.");
    CO_RETURN true;
}

int main(
    int argc,
    char* argv[])
{
    canopy::network_config::network_config cfg;

    {
        args::ArgumentParser parser("Greeting client: calls RPC methods on the greeting server over TCP.");
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

    coro::sync_wait(coro::when_all(run_client(scheduler, cfg)));

    scheduler->shutdown();
    return 0;
}
