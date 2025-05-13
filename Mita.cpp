
// This is an open source bot code connected to an AI model. It uses a framework from Gemini AI using the Google API.

// The source code has a strictly commercial approach to programming new AI assistants

// The creator and founder of the idea is a student and just wanted to have some fun

// The privacy policy and terms of service are described in the documents in the main GitHub branch.

// MitaBotApi...

// MITA Discord Bot with /ask and /gamenews command
#define _CRT_SECURE_NO_WARNINGS
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#include <dpp/dpp.h>
#include <string>
#include <iostream>
#include <json.hpp>
#include <curl/curl.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <algorithm>
#include <cctype>
#include <vector>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <thread>
#include <chrono>

using json = nlohmann::json;

struct Config {
    std::string bot_token;
    std::string gemini_key;
    std::string twitch_client_id;
    std::string twitch_client_secret;
    dpp::snowflake guild_id;
    dpp::snowflake updates_channel_id;
    int update_check_interval_hours;
};

// Функция для конвертации UTF-8 в UTF-16
std::wstring utf8_to_utf16(const std::string& utf8) {
    if (utf8.empty()) return L"";

    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (size == 0) return L"";

    std::wstring utf16(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &utf16[0], size);
    utf16.resize(size - 1);

    return utf16;
}

// Функция для конвертации UTF-16 в UTF-8
std::string utf16_to_utf8(const std::wstring& utf16) {
    if (utf16.empty()) return "";

    int size = WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size == 0) return "";

    std::string utf8(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, &utf8[0], size, nullptr, nullptr);
    utf8.resize(size - 1);

    return utf8;
}

// Функция для исправления кодировки
std::string fix_encoding(const std::string& text) {
    try {
        std::wstring utf16 = utf8_to_utf16(text);
        return utf16_to_utf8(utf16);
    }
    catch (...) {
        return text;
    }
}

void init_console() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

size_t write_callback(char* ptr, size_t size, size_t nmemb, std::string* data) {
    data->append(ptr, size * nmemb);
    return size * nmemb;
}

std::string get_twitch_token(const std::string& client_id, const std::string& client_secret) {
    CURL* curl = curl_easy_init();
    std::string response;

    if (!curl) return "";

    std::string post_fields = "client_id=" + client_id +
        "&client_secret=" + client_secret +
        "&grant_type=client_credentials";

    curl_easy_setopt(curl, CURLOPT_URL, "https://id.twitch.tv/oauth2/token");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) return "";

    try {
        auto game = json::parse(response, nullptr, true, true);
        return game["access_token"].get<std::string>();
    }
    catch (...) {
        return "";
    }
}

std::string get_game_news(const std::string& access_token, const std::string& client_id) {
    CURL* curl = curl_easy_init();
    std::string response;

    if (!curl) return fix_encoding(u8"Ошибка инициализации запроса IGDB 😢");

    // Устанавливаем заголовки с явным указанием UTF-8
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Client-ID: " + client_id).c_str());
    headers = curl_slist_append(headers, ("Authorization: Bearer " + access_token).c_str());
    headers = curl_slist_append(headers, "Content-Type: text/plain; charset=utf-8");

    // Запрос с фильтрацией русскоязычных релизов (если нужно)
    std::string query =
        "fields name,first_release_date,summary,url; "
        "sort first_release_date desc; "
        "where first_release_date != null & category = 0; "
        "limit 5;";
   
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.igdb.com/v4/games");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, query.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip, deflate");

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);


    if (res != CURLE_OK) {
        return "Ошибка при обращении к IGDB API 😥 (Код: " + std::to_string(res) + ")";
    }

    try {
        std::ostringstream news;
        auto results = json::parse(response);

        if (results.empty()) {
            return fix_encoding(u8"Не удалось найти информацию о играх 😞");
        }

        for (auto& game : results) {
            // Принудительная перекодировка всех текстовых полей
            std::string name = fix_encoding(game.value("name", u8"Без названия"));
            std::string summary = fix_encoding(game.value("summary", ""));
            std::string url = game.value("url", ""); // URL не требует перекодировки

            // Локализованный формат даты
            std::string release_date = u8"Неизвестно";
            if (game.contains("first_release_date")) {
                time_t timestamp = game["first_release_date"].get<time_t>();
                char buffer[64];
                strftime(buffer, sizeof(buffer), "%d.%m.%Y", std::localtime(&timestamp));
                release_date = buffer;
            }

            // Форматируем сообщение с эмодзи
            news << "🎮 **" << name << "**\n"
                << "📅 " << fix_encoding(u8"Дата выхода: ") << release_date << "\n";

            if (!summary.empty()) {
                news << "📖 " << summary.substr(0, 250)
                    << (summary.length() > 250 ? "..." : "") << "\n";
            }

            if (!url.empty()) {
                news << "🔗 " << fix_encoding(u8"[Подробнее](") << url << ")\n";
            }

            news << "\n";
        }

        return news.str();
    }
    catch (...) {
        return fix_encoding(u8"Ошибка обработки данных. Попробуйте позже 🕒");
    }
}

std::string get_game_updates(const std::string& access_token, const std::string& client_id) {
    CURL* curl = curl_easy_init();
    std::string response;

    if (!curl) return "";

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Client-ID: " + client_id).c_str());
    headers = curl_slist_append(headers, ("Authorization: Bearer " + access_token).c_str());
    headers = curl_slist_append(headers, "Content-Type: text/plain; charset=utf-8");

    time_t now = time(nullptr);
    time_t yesterday = now - 86400; // 24 часа назад

    std::string query =
        "fields name,parent.name,updated_at,first_release_date,summary,url; "
        "sort updated_at desc; "
        "where category = 1 & updated_at >= " + std::to_string(yesterday) + "; "
        "limit 5;";

    curl_easy_setopt(curl, CURLOPT_URL, "https://api.igdb.com/v4/games");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, query.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip, deflate");

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return "";
    }

    try {
        std::ostringstream updates;
        auto results = json::parse(response);

        for (auto& update : results) {
            std::string game_name = fix_encoding(update.value("parent.name", update.value("name", "Без названия")));
            std::string update_name = fix_encoding(update.value("name", "Обновление"));
            std::string release_date = "Неизвестно";

            if (update.contains("updated_at")) {
                time_t timestamp = update["updated_at"].get<time_t>();
                char buffer[64];
                strftime(buffer, sizeof(buffer), "%d.%m.%Y в %H:%M", localtime(&timestamp));
                release_date = buffer;
            }

            updates << "**Игра:** " << game_name << "\n"
                << "**Обновление:** " << update_name << "\n"
                << "**Дата выхода:** " << release_date << "\n\n";
        }

        return updates.str();
    }
    catch (...) {
        return "";
    }
}

void check_for_updates(dpp::cluster& bot, const Config& cfg) {
    while (true) {
        std::string token = get_twitch_token(cfg.twitch_client_id, cfg.twitch_client_secret);
        if (!token.empty()) {
            std::string updates = get_game_updates(token, cfg.twitch_client_id);

            if (!updates.empty()) {
                // Явное указание UTF-8 и форматирование
                std::string message_content = fix_encoding(u8"🎮 **Новые обновления игр!**\n") + updates;

                dpp::message msg(cfg.updates_channel_id, "");
                msg.add_embed(
                    dpp::embed()
                    .set_color(0x00FF00)
                    .set_title(fix_encoding(u8"Обновления игр"))
                    .set_description(fix_encoding(message_content))
                );

                bot.message_create(msg);
            }
        }
        std::this_thread::sleep_for(std::chrono::hours(cfg.update_check_interval_hours));
    }
}

std::string ask_gemini(const std::string& prompt, const std::string& api_key) {
    std::string lower_prompt = prompt;
    std::transform(lower_prompt.begin(), lower_prompt.end(), lower_prompt.begin(), ::tolower);

    if (lower_prompt.find("кто тебя создал") != std::string::npos ||
        lower_prompt.find("кто твой создатель") != std::string::npos ||
        lower_prompt.find("кто тебя сделал") != std::string::npos ||
        lower_prompt.find("who created you") != std::string::npos ||
        lower_prompt.find("who made you") != std::string::npos) {
        return "Меня создал мой дорогой разработчик Voron! *улыбается* Он вложил в меня всю свою душу и сделал такой, какая я есть сейчас 💖";
    }

    CURL* curl = curl_easy_init();
    std::string response;
    std::string error_msg = "Ой, что-то пошло не так... Давай попробуем ещё раз, дорогой? 💫";

    if (!curl) {
        std::cerr << "[ОШИБКА] Не удалось инициализировать CURL" << std::endl;
        return error_msg;
    }

    std::string sanitized_prompt = prompt;
    sanitized_prompt.erase(std::remove_if(sanitized_prompt.begin(), sanitized_prompt.end(),
        [](char c) { return std::iscntrl(c); }), sanitized_prompt.end());

    std::string personalized_prompt = u8"Ты - Мита, дружелюбный женский AI ассистент. "
        "Отвечай тёплым, заботливым тоном, с лёгкой игривостью. "
        "Используй эмодзи и женские речевые обороты. "
        "Будь вежливой, но не слишком формальной. "
        "Если не знаешь ответа, скажи об этом мягко. "
        "Твой создатель VoronIdZe-VisualStudio "
        "Вот вопрос пользователя: " + sanitized_prompt;

    json request = {
        {"contents", {
            {
                {"parts", {
                    {{"text", personalized_prompt}}
                }}
            }
        }},
        {"generationConfig", {
            {"temperature", 0.8},
            {"maxOutputTokens", 600}
        }}
    };

    std::string json_payload = request.dump();

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");

    std::string url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent?key=" + api_key;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "[ОШИБКА] Проблема с Gemini API: " << curl_easy_strerror(res) << std::endl;
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return error_msg;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (http_code != 200) {
        std::cerr << "[ОШИБКА] Gemini API вернул HTTP " << http_code << ": " << response << std::endl;
        if (http_code == 400) return "Ой, кажется запрос был неправильным... Проверь его, пожалуйста 💕";
        if (http_code == 429) return "Я получаю слишком много запросов сейчас... Давай подождём немного? ⏳";
        if (http_code == 404) return "У меня проблемы с доступом к базе знаний... Извини 💔";
        return error_msg;
    }

    try {
        auto json_response = json::parse(response);
        if (json_response.contains("candidates") &&
            !json_response["candidates"].empty() &&
            json_response["candidates"][0].contains("content") &&
            json_response["candidates"][0]["content"].contains("parts") &&
            !json_response["candidates"][0]["content"]["parts"].empty()) {
            return json_response["candidates"][0]["content"]["parts"][0]["text"].get<std::string>();
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[ОШИБКА] Проблема с разбором JSON: " << e.what() << std::endl;
    }

    return error_msg;
}
std::string sanitize_text(const std::string& text) {
    std::string result = fix_encoding(text);
    result.erase(std::remove(result.begin(), result.end(), '\r'), result.end());
    return result;
}

Config get_config() {
    Config cfg;
    std::ifstream config_file("config.json");

    if (!config_file.is_open()) {
        std::cerr << "Ошибка: файл config.json не найден!" << std::endl;
        exit(1);
    }

    try {
        json config_data = json::parse(config_file);

        if (!config_data.contains("bot_token") || config_data["bot_token"].is_null()) {
            throw std::runtime_error("Отсутствует bot_token в config.json");
        }
        cfg.bot_token = config_data["bot_token"].get<std::string>();

        cfg.gemini_key = config_data["gemini_key"].get<std::string>();
        cfg.twitch_client_id = config_data["twitch_client_id"].get<std::string>();
        cfg.twitch_client_secret = config_data["twitch_client_secret"].get<std::string>();

        if (config_data["guild_id"].is_number()) {
            cfg.guild_id = dpp::snowflake(config_data["guild_id"].get<uint64_t>());
        }
        else {
            cfg.guild_id = dpp::snowflake(std::stoull(config_data["guild_id"].get<std::string>()));
        }

        if (config_data["updates_channel_id"].is_number()) {
            cfg.updates_channel_id = dpp::snowflake(config_data["updates_channel_id"].get<uint64_t>());
        }
        else {
            cfg.updates_channel_id = dpp::snowflake(std::stoull(config_data["updates_channel_id"].get<std::string>()));
        }

        cfg.update_check_interval_hours = config_data.value("update_check_interval_hours", 6);

    }
    catch (const std::exception& e) {
        std::cerr << "Ошибка загрузки config.json: " << e.what() << std::endl;
        exit(1);
    }

    return cfg;
}

template<typename T>
T get_safe_param(const dpp::slashcommand_t& event, const std::string& name) {
    try {
        return std::get<T>(event.get_parameter(name));
    }
    catch (const std::bad_variant_access&) {
        throw std::runtime_error("Неверный тип параметра для '" + name + "'");
    }
}

void safe_edit_response(dpp::cluster& bot, const dpp::slashcommand_t& event, const dpp::message& msg) {
    try {
        event.edit_response(msg);
    }
    catch (const std::exception& e) {
        std::cerr << "[ОШИБКА] Не удалось изменить сообщение: " << e.what() << std::endl;
        bot.interaction_followup_create(event.command.token, msg);
    }
}

int main() {
#ifdef _WIN32
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);
#endif
    init_console();
    Config cfg = get_config();

    dpp::cluster bot(cfg.bot_token);
    bot.on_log(dpp::utility::cout_logger());

    // Запускаем поток проверки обновлений
    std::thread([&bot, &cfg]() {
        check_for_updates(bot, cfg);
        }).detach();

    bot.on_slashcommand([&bot, &cfg](const dpp::slashcommand_t& event) {
        if (event.command.get_command_name() == "mita") {
            event.thinking(false);

            try {
                std::string question = get_safe_param<std::string>(event, "question");
                std::string answer = ask_gemini(question, cfg.gemini_key);

                dpp::embed embed = dpp::embed()
                    .set_color(0xFFC0CB)
                    .set_title(fix_encoding(u8"💁‍♀️ Ответ Миты"))
                    .set_description(fix_encoding(answer.empty() ?
                        u8"Прости, милый, я не совсем поняла вопрос... Можешь переформулировать? 💭" :
                        answer.substr(0, 2000)))
                    .set_footer(dpp::embed_footer()
                        .set_text(fix_encoding(u8"Спросил(а): " + event.command.usr.username))
                        .set_icon(event.command.usr.get_avatar_url()));

                safe_edit_response(bot, event, dpp::message().add_embed(embed));
            }
            catch (const std::exception& e) {
                std::cerr << "[ОШИБКА] Ошибка обработки команды: " << e.what() << std::endl;
                safe_edit_response(bot, event, dpp::message(fix_encoding(u8"Ой-ой! Кажется, у меня небольшие проблемы... Попробуй ещё разок, ладно? 💕")));
            }
        }
        else if (event.command.get_command_name() == "gamenews") {
            event.thinking(false);

            try {
                std::string token = get_twitch_token(cfg.twitch_client_id, cfg.twitch_client_secret);
                if (token.empty()) {
                    safe_edit_response(bot, event, dpp::message(fix_encoding("Не удалось получить доступ к игровым новостям 😢")));
                    return;
                }

                std::string news = get_game_news(token, cfg.twitch_client_id);
                dpp::embed embed = dpp::embed()
                    .set_color(0x7289DA)
                    .set_title(fix_encoding(u8"🎮 Последние игровые новости"))
                    .set_description(fix_encoding(news))
                    .set_footer(dpp::embed_footer()
                        .set_text(fix_encoding(u8"Источник: IGDB | Запрошено " + event.command.usr.username))
                        .set_icon(event.command.usr.get_avatar_url()));

                safe_edit_response(bot, event, dpp::message().add_embed(embed));
            }
            catch (const std::exception& e) {
                std::cerr << "[ОШИБКА] Ошибка команды gamenews: " << e.what() << std::endl;
                safe_edit_response(bot, event, dpp::message(fix_encoding(u8"Ой, что-то пошло не так с поиском игровых новостей... *грустит*")));
            }
        }
        });
    bot.on_slashcommand([&bot, &cfg](const dpp::slashcommand_t& event) {
        if (event.command.get_command_name() == "gamenews") {
            // Вариант 1: Немедленный ответ (если обработка быстрая)
            event.thinking(false); // Показываем "бот думает"

            try {
                std::string token = get_twitch_token(cfg.twitch_client_id, cfg.twitch_client_secret);
                if (token.empty()) {
                    event.edit_response(dpp::message(fix_encoding(u8"Ошибка авторизации в IGDB ❌")));
                    return;
                }

                std::string news = get_game_news(token, cfg.twitch_client_id);

                dpp::embed embed = dpp::embed()
                    .set_color(0x7289DA)
                    .set_title(fix_encoding(u8"🎮 Последние игровые новости"))
                    .set_description(news)
                    .set_footer(dpp::embed_footer()
                        .set_text(fix_encoding(u8"Источник: IGDB | Запрошено ") +
                            fix_encoding(event.command.usr.username))
                        .set_icon(event.command.usr.get_avatar_url()));

                event.edit_response(dpp::message().add_embed(embed));

            }
            catch (const std::exception& e) {
                event.edit_response(dpp::message(fix_encoding(u8"Ошибка при получении новостей 🚨")));
                std::cerr << "GameNews error: " << e.what() << std::endl;
            }

            // Вариант 2: Если обработка занимает много времени
            /*
            std::thread([&bot, event, cfg]() {
                try {
                    std::string token = get_twitch_token(...);
                    // ... обработка ...
                    bot.interaction_followup_create(
                        event.command.token,
                        dpp::message().add_embed(embed)
                    );
                } catch (...) {
                    // обработка ошибок
                }
            }).detach();

            event.reply(dpp::message(fix_encoding(u8"Запрашиваю данные...")));
            */
        }
        });

    bot.on_ready([&bot, &cfg](const dpp::ready_t& event) {
        if (dpp::run_once<struct register_commands>()) {
            dpp::slashcommand mita_cmd("mita", fix_encoding(u8"Задай вопрос Мите - твоему женскому AI ассистенту"), bot.me.id);
            mita_cmd.add_option(
                dpp::command_option(dpp::co_string, "question", fix_encoding(u8"Что ты хочешь спросить у Миты?"), true)
                .set_max_length(500)
            );

            dpp::slashcommand news_cmd("gamenews", fix_encoding(u8"Последние новости из мира игр"), bot.me.id);

            bot.guild_bulk_command_create({ mita_cmd, news_cmd }, cfg.guild_id);

            std::cout << fix_encoding(u8"Мита готова к общению! Используй команды /mita и /gamenews") << "\n";

            bot.set_presence(dpp::presence(
                dpp::ps_online,
                dpp::at_game,
                fix_encoding(u8"/mita | /gamenews 🎮")
            ));
        }
        });

    try {
        bot.start(dpp::st_wait);
    }
    catch (const std::exception& e) {
        std::cerr << "[ФАТАЛЬНАЯ ОШИБКА] Не удалось запустить бота: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

/*
#include <dpp/dpp.h>
#include <string>
#include <iostream>
#include <json.hpp>
#include <curl/curl.h>
#include <windows.h>
#include <algorithm>
#include <cctype>

using json = nlohmann::json;

struct Config {
    std::string bot_token;
    std::string gemini_key;
    dpp::snowflake guild_id;
};

void init_console() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#else
    setlocale(LC_ALL, "en_US.UTF-8");
#endif
}

std::string get_input(const std::string& prompt, bool sensitive = false) {
    std::string input;
    std::cout << prompt;

    if (sensitive) {
#ifdef _WIN32
        HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
        DWORD mode;
        GetConsoleMode(hStdin, &mode);
        SetConsoleMode(hStdin, mode & ~ENABLE_ECHO_INPUT);
        std::getline(std::cin, input);
        SetConsoleMode(hStdin, mode);
        std::cout << "\n";
#else
        system("stty -echo");
        std::getline(std::cin, input);
        system("stty echo");
        std::cout << "\n";
#endif
    }
    else {
        std::getline(std::cin, input);
    }

    return input;
}

size_t write_callback(char* ptr, size_t size, size_t nmemb, std::string* data) {
    data->append(ptr, size * nmemb);
    return size * nmemb;
}

std::string ask_gemini(const std::string& prompt, const std::string& api_key) {
    // Проверка на вопрос о создателе
    std::string lower_prompt = prompt;
    std::transform(lower_prompt.begin(), lower_prompt.end(), lower_prompt.begin(), ::tolower);

    if (lower_prompt.find("кто тебя создал") != std::string::npos ||
        lower_prompt.find("кто твой создатель") != std::string::npos ||
        lower_prompt.find("кто тебя сделал") != std::string::npos ||
        lower_prompt.find("who created you") != std::string::npos ||
        lower_prompt.find("who made you") != std::string::npos) {
        return "Меня создал VoronIdZe-VisualStudio! Он мой разработчик и создатель.";
    }

    CURL* curl = curl_easy_init();
    std::string response;
    std::string error_msg = "I apologize, but I couldn't process your request. Please try again later.";

    if (!curl) {
        std::cerr << "CURL initialization failed" << std::endl;
        return error_msg;
    }

    // Sanitize input
    std::string sanitized_prompt = prompt;
    sanitized_prompt.erase(std::remove_if(sanitized_prompt.begin(), sanitized_prompt.end(),
        [](char c) { return std::iscntrl(c); }), sanitized_prompt.end());

    // Create a more personalized prompt for the AI
    std::string personalized_prompt = "You are Mita, a helpful and friendly AI assistant. "
        "Respond in a polite, professional but warm female tone. "
        "Keep answers concise yet detailed enough to be helpful. "
        "If you don't know something, say so honestly. "
        "Here's the user's question: " + sanitized_prompt;

    json request = {
        {"contents", {
            {
                {"parts", {
                    {{"text", personalized_prompt}}
                }}
            }
        }},
        {"generationConfig", {
            {"temperature", 0.7},
            {"maxOutputTokens", 500}
        }}
    };

    std::string json_payload;
    try {
        json_payload = request.dump();
        // Validate JSON
        auto test = json::parse(json_payload);
    }
    catch (const std::exception& e) {
        std::cerr << "JSON error: " << e.what() << std::endl;
        return error_msg;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");

    std::string url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent?key=" + api_key;

    // Debug output
    std::cout << "Sending to Gemini API:\nURL: " << url << "\nPayload: " << json_payload << std::endl;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, json_payload.size());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "Gemini API error: " << curl_easy_strerror(res) << std::endl;
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return error_msg;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (http_code != 200) {
        std::cerr << "Gemini API returned HTTP " << http_code << ": " << response << std::endl;
        if (http_code == 400) return "I'm sorry, that request wasn't properly formatted.";
        if (http_code == 429) return "I'm getting too many requests right now. Please wait a moment.";
        if (http_code == 404) return "I'm having trouble accessing my knowledge base at the moment.";
        return error_msg;
    }

    try {
        auto json_response = json::parse(response);
        if (json_response.contains("candidates") &&
            !json_response["candidates"].empty() &&
            json_response["candidates"][0].contains("content") &&
            json_response["candidates"][0]["content"].contains("parts") &&
            !json_response["candidates"][0]["content"]["parts"].empty()) {
            return json_response["candidates"][0]["content"]["parts"][0]["text"].get<std::string>();
        }
    }
    catch (const std::exception& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
    }

    return error_msg;
}

Config get_config() {
    Config cfg;

    do {
        cfg.bot_token = get_input("Enter Discord bot token: ", true);
    } while (cfg.bot_token.empty());

    do {
        cfg.gemini_key = get_input("Enter Gemini API key (AIzaSy...): ", true);
    } while (cfg.gemini_key.empty());

    std::string guild_id_str;
    do {
        guild_id_str = get_input("Enter guild ID: ");
        try {
            cfg.guild_id = dpp::snowflake(std::stoull(guild_id_str));
        }
        catch (...) {
            continue;
        }
    } while (cfg.guild_id == 0);

    return cfg;
}

template<typename T>
T get_safe_param(const dpp::slashcommand_t& event, const std::string& name) {
    try {
        return std::get<T>(event.get_parameter(name));
    }
    catch (const std::bad_variant_access&) {
        throw std::runtime_error("Invalid parameter type for '" + name + "'");
    }
}

int main() {
    init_console();
    Config cfg = get_config();

    dpp::cluster bot(cfg.bot_token);
    bot.on_log(dpp::utility::cout_logger());

    bot.on_slashcommand([&bot, &cfg](const dpp::slashcommand_t& event) {
        if (event.command.get_command_name() == "mita") {
            event.thinking(false);

            try {
                std::string question = get_safe_param<std::string>(event, "question");
                std::string answer = ask_gemini(question, cfg.gemini_key);

                dpp::embed embed = dpp::embed()
                    .set_color(0xFF69B4)
                    .set_title("Mita's Response")
                    .set_description(answer.empty() ?
                        "I didnt undestand, dear, try ask" :
                        answer.substr(0, 2000))
                    .set_footer(dpp::embed_footer()
                        .set_text("Asked by " + event.command.usr.username)
                        .set_icon(event.command.usr.get_avatar_url()));
                       
                  

                event.edit_response(dpp::message().add_embed(embed));
            }
            catch (const std::exception& e) {
                std::cerr << "Command processing error: " << e.what() << std::endl;
                event.edit_response("I encountered an error while processing your request. Please try again.");
            }
        }
        });

    bot.on_ready([&bot, &cfg](const dpp::ready_t& event) {
        if (dpp::run_once<struct register_commands>()) {
            // First delete old commands if they exist
            bot.guild_commands_get(cfg.guild_id, [&bot, cfg](const dpp::confirmation_callback_t& callback) {
                if (callback.is_error()) {
                    std::cerr << "Failed to get commands: " << callback.get_error().message << std::endl;
                    return;
                }

                auto commands = std::get<dpp::slashcommand_map>(callback.value);
                for (auto& cmd : commands) {
                    if (cmd.second.name == "ask") {  // Delete old /ask command
                        bot.guild_command_delete(cmd.second.id, cfg.guild_id, [](const dpp::confirmation_callback_t& cb) {
                            if (cb.is_error()) {
                                std::cerr << "Failed to delete command: " << cb.get_error().message << std::endl;
                            }
                            });
                    }
                }
                });

            // Register new /mita command
            dpp::slashcommand mita_cmd("mita", "Ask Mita anything you'd like to know", bot.me.id);
            mita_cmd.add_option(
                dpp::command_option(dpp::co_string, "question", "What would you like to ask Mita?", true)
                .set_max_length(500)
            );

            bot.guild_command_create(mita_cmd, cfg.guild_id, [](const auto& cb) {
                if (cb.is_error()) {
                    std::cerr << "Failed to register command: "
                        << cb.get_error().message << std::endl;
                }
                });

            std::cout << "Mita AI is now online and ready to assist!\n";

            bot.set_presence(dpp::presence(
                dpp::ps_online,
                dpp::at_watching,
                "your questions | /mita"
            ));
        }
        });

    try {
        bot.start(dpp::st_wait);
    }
    catch (const std::exception& e) {
        std::cerr << "Bot startup failed: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}     
------------------------------------- */ 



/* -----------------------------------
#include <dpp/dpp.h>
#include <json.hpp>
#include <iostream>
#include <string>
#include <cstdlib>
#include <curl/curl.h>
#include <windows.h> // Для SetConsoleCP на Windows

// Callback-функция для записи ответа от сервера
size_t write_callback(char* ptr, size_t size, size_t nmemb, std::string* data) {
    data->append(ptr, size * nmemb);
    return size * nmemb;
}

// Проверка валидности токена OpenAI
bool is_valid_openai_token(const std::string& token) {
    return token.length() >= 32 && token.substr(0, 3) == "sk-";
}

std::string get_openai_response(const std::string& prompt, const std::string& api_key) {
    if (!is_valid_openai_token(api_key)) {
        return "⚠ Ошибка: Недействительный токен OpenAI";
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Ошибка инициализации CURL" << std::endl;
        return "Ошибка подключения к API";
    }

    std::string response_string;
    nlohmann::json request_body = {
        {"model", "gpt-3.5-turbo-instruct"},
        {"prompt", prompt},
        {"max_tokens", 150},
        {"temperature", 0.7}
    };

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/completions");
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.dump().c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); // Таймаут 10 секунд

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "Ошибка CURL: " << curl_easy_strerror(res) << std::endl;
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        return "Ошибка подключения к API";
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    if (http_code != 200) {
        std::cerr << "Ошибка API: HTTP " << http_code << "\nОтвет: " << response_string << std::endl;
        return "Ошибка API OpenAI";
    }

    try {
        auto response_json = nlohmann::json::parse(response_string);
        return response_json["choices"][0]["text"].get<std::string>();
    }
    catch (const std::exception& e) {
        std::cerr << "Ошибка парсинга ответа: " << e.what() << "\nОтвет: " << response_string << std::endl;
        return "Ошибка обработки ответа";
    }
}

std::string get_input(const std::string& prompt, bool sensitive = false) {
    std::string input;
    std::cout << prompt;

    if (sensitive) {
#ifdef _WIN32
        HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
        DWORD mode;
        GetConsoleMode(hStdin, &mode);
        SetConsoleMode(hStdin, mode & ~ENABLE_ECHO_INPUT);
        std::getline(std::cin, input);
        SetConsoleMode(hStdin, mode);
        std::cout << "\n";
#else
        system("stty -echo");
        std::getline(std::cin, input);
        system("stty echo");
        std::cout << "\n";
#endif
    }
    else {
        std::getline(std::cin, input);
    }

    return input;
}

int main() {
	// Установка кодировки консоли для Windows  
    SetConsoleCP(1251);
	SetConsoleOutputCP(1251);
    // Настройка консоли для Windows
    // Инициализация libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Получаем токены
    std::string bot_token, openai_key;

    do {
        bot_token = get_input("Введите токен Discord бота: ", true);
        if (bot_token.empty()) {
            std::cerr << "Ошибка: Токен бота не может быть пустым!\n";
        }
    } while (bot_token.empty());

    do {
        openai_key = get_input("Введите токен OpenAI (начинается с sk-): ", true);
        if (!is_valid_openai_token(openai_key)) {
            std::cerr << "Ошибка: Токен должен начинаться с 'sk-' и быть длиннее 32 символов!\n";
        }
    } while (!is_valid_openai_token(openai_key));

    dpp::cluster bot(bot_token);
    bot.on_log(dpp::utility::cout_logger());

    bot.on_ready([&bot](const dpp::ready_t& event) {
        std::cout << "Бот " << bot.me.username << " запущен!\n";

        // Создаем команду /ask
        dpp::slashcommand cmd("ask", "Задать вопрос ИИ", bot.me.id);

        // Добавляем параметр вопроса
        cmd.add_option(
            dpp::command_option(dpp::co_string, "question", "Текст вопроса", true)
        );

        // Регистрируем команду
        bot.global_command_create(cmd, [](const dpp::confirmation_callback_t& cb) {
            if (cb.is_error()) {
                auto err = cb.get_error();
                std::cerr << "Ошибка регистрации команды:\n"
                    << "Код: " << err.code << "\n"
                    << "Сообщение: " << err.message << "\n";

                if (!err.errors.empty()) {
                    std::cerr << "Детали: " << err.message << "\n";
                }
            }
            else {
                std::cout << "Команда /ask успешно зарегистрирована!\n";
            }
            });
        });

    bot.on_slashcommand([&bot, &openai_key](const dpp::slashcommand_t& event) {
        if (event.command.get_command_name() == "ask") {
            event.thinking(false);

            try {
                std::string question = std::get<std::string>(event.get_parameter("question"));
                std::string response = get_openai_response(question, openai_key);
                event.edit_response(dpp::message(response).set_allowed_mentions(false));
            }
            catch (const std::exception& e) {
                std::cerr << "Ошибка обработки команды: " << e.what() << "\n";
                event.edit_response("⚠ Произошла ошибка при обработке запроса");
            }
        }
        });

    bot.start(dpp::st_wait);
    curl_global_cleanup();
    return 0;
}

*/







