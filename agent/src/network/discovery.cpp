#include "network/discovery.hpp"

#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/error.h>
#include <avahi-common/simple-watch.h>

#include <iostream>
#include <utility>

namespace remote::discovery {

namespace {
constexpr const char* kServiceType = "_remotecontrol._tcp";

struct Context {
    AvahiSimplePoll* poll = nullptr;
    AvahiClient* client = nullptr;
    AvahiEntryGroup* group = nullptr;
    std::string instance_name;
    std::uint16_t port = 0;
};

void entry_group_callback(AvahiEntryGroup*, AvahiEntryGroupState state, void* userdata) {
    auto* context = static_cast<Context*>(userdata);

    if (state == AVAHI_ENTRY_GROUP_ESTABLISHED) {
        std::cout << "mDNS: advertising " << context->instance_name
                  << " on " << kServiceType << ":" << context->port << '\n';
    } else if (state == AVAHI_ENTRY_GROUP_COLLISION) {
        std::cerr << "mDNS: service name collision\n";
    } else if (state == AVAHI_ENTRY_GROUP_FAILURE) {
        std::cerr << "mDNS: entry group failure: "
                  << avahi_strerror(avahi_client_errno(context->client)) << '\n';
        avahi_simple_poll_quit(context->poll);
    }
}

void client_callback(AvahiClient* client, AvahiClientState state, void* userdata) {
    auto* context = static_cast<Context*>(userdata);
    context->client = client;

    if (state == AVAHI_CLIENT_FAILURE) {
        std::cerr << "mDNS: client failure: "
                  << avahi_strerror(avahi_client_errno(client)) << '\n';
        avahi_simple_poll_quit(context->poll);
        return;
    }

    if (state != AVAHI_CLIENT_S_RUNNING || context->group) return;

    context->group = avahi_entry_group_new(client, entry_group_callback, context);
    if (!context->group) {
        std::cerr << "mDNS: unable to create entry group\n";
        avahi_simple_poll_quit(context->poll);
        return;
    }

    const int result = avahi_entry_group_add_service(
        context->group,
        AVAHI_IF_UNSPEC,
        AVAHI_PROTO_UNSPEC,
        static_cast<AvahiPublishFlags>(0),
        context->instance_name.c_str(),
        kServiceType,
        nullptr,
        nullptr,
        context->port,
        "version=1",
        "protocol=1",
        nullptr
    );

    if (result < 0) {
        std::cerr << "mDNS: unable to add service: " << avahi_strerror(result) << '\n';
        avahi_simple_poll_quit(context->poll);
        return;
    }

    if (avahi_entry_group_commit(context->group) < 0) {
        std::cerr << "mDNS: unable to publish service: "
                  << avahi_strerror(avahi_client_errno(client)) << '\n';
        avahi_simple_poll_quit(context->poll);
    }
}

} // namespace

Service::Service(std::string instance_name, std::uint16_t port)
    : instance_name_(std::move(instance_name)), port_(port) {}

Service::~Service() {
    stop();
}

bool Service::start(std::string& error) {
    if (running_) return true;

    auto stop_requested = std::make_shared<std::atomic_bool>(false);
    stop_requested_ = stop_requested;

    auto ready = std::make_shared<std::atomic_bool>(false);
    auto failed = std::make_shared<std::string>();

    thread_ = std::thread([
        instance_name = instance_name_,
        port = port_,
        stop_requested,
        ready,
        failed
    ]() {
        Context context;
        context.instance_name = instance_name;
        context.port = port;
        context.poll = avahi_simple_poll_new();

        if (!context.poll) {
            *failed = "Unable to create Avahi simple poll";
            ready->store(true);
            return;
        }

        int client_error = 0;
        context.client = avahi_client_new(
            avahi_simple_poll_get(context.poll),
            static_cast<AvahiClientFlags>(0),
            client_callback,
            &context,
            &client_error
        );

        if (!context.client) {
            *failed = std::string("Unable to create Avahi client: ") + avahi_strerror(client_error);
            avahi_simple_poll_free(context.poll);
            ready->store(true);
            return;
        }

        ready->store(true);

        while (!stop_requested->load()) {
            if (avahi_simple_poll_iterate(context.poll, 100) < 0) break;
        }

        if (context.group) avahi_entry_group_free(context.group);
        avahi_client_free(context.client);
        avahi_simple_poll_free(context.poll);
    });

    while (!ready->load()) std::this_thread::yield();

    if (!failed->empty()) {
        error = *failed;
        stop_requested->store(true);
        if (thread_.joinable()) thread_.join();
        stop_requested_.reset();
        return false;
    }

    running_ = true;
    return true;
}

void Service::stop() {
    if (stop_requested_) stop_requested_->store(true);
    if (thread_.joinable()) thread_.join();
    stop_requested_.reset();
    running_ = false;
}

} // namespace remote::discovery
