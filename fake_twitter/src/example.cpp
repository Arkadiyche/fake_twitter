#include <algorithm>

#include <pistache/http.h>
#include <pistache/router.h>
#include <pistache/endpoint.h>

#include "fake_twitter/model/User.h"
#include <rapidjson/rapidjson.h>
#include "fake_twitter/serializer/json.h"

#include <sqlpp11/custom_query.h>
#include <sqlpp11/sqlite3/sqlite3.h>
#include <sqlpp11/sqlpp11.h>
#include <sqlite3.h>

#include "fake_twitter/sqlpp_models/UsersTab.h"

using namespace Pistache;
namespace sql = sqlpp::sqlite3;

class StatsEndpoint {
public:
    explicit StatsEndpoint(Address addr, sql::connection_config config) {
        httpEndpoint = std::make_shared<Http::Endpoint>(addr);
        db = std::make_unique<sql::connection>(config);
    }

    void init(size_t thr = 2) {
        auto opts = Http::Endpoint::options()
                .threads(static_cast<int>(thr));
        httpEndpoint->init(opts);
        setupRoutes();
    }

    void start() {
        httpEndpoint->setHandler(router.handler());
        httpEndpoint->serve();
    }

    void setupRoutes() {
        using namespace Rest;
        Routes::Get(router, "/0.0/users/show.json", Routes::bind(&StatsEndpoint::handleReady, this));
    }

private:
    void handleReady(const Rest::Request &, Http::ResponseWriter response) {
        using namespace rapidjson;
        using namespace fake_twitter;
        using fake_twitter::sqlpp_models::TabUsers;

        TabUsers tab;
        auto result = (*db)(select(all_of(tab)).from(tab)
                                    .where(tab.id == 1 ));
        if (result.empty()) {
            response.send(Http::Code::No_Content);
            return;
        }

        auto& first = result.front();
        model::User user = {first.id.value(), first.name.value(), first.username.value(), first.password_hash.value()};

        response.headers().add<Http::Header::ContentType>(MIME(Application, Json));
        response.send(Http::Code::Ok, serialization::to_json(user));

    }

    std::shared_ptr<Http::Endpoint> httpEndpoint;
    Rest::Router router;

    std::unique_ptr<sql::connection> db;

};

int main(int argc, char *argv[]) {
    Port port(9081);

    int thr = 2;

    if (argc >= 2) {
        port = static_cast<uint16_t>(std::stol(argv[1]));

        if (argc == 3)
            thr = std::stoi(argv[2]);
    }

    Address addr(Ipv4::any(), port);

//    cout << "Cores = " << hardware_concurrency() << endl;
//    cout << "Using " << thr << " threads" << endl;

    std::cout << "Start server" << std::endl;

    sql::connection_config config;
//    config.path_to_database = ":memory:";
    config.path_to_database = "/tmp/db.sqlite";
    config.flags = SQLITE_OPEN_READWRITE;
    config.debug = true;

    StatsEndpoint stats(addr, config);
    stats.init(thr);
    stats.start();
}
