// This is an open source bot code connected to an AI model. It uses a framework from Gemini AI using the Google API. 
// The source code has a strictly commercial approach to programming new AI assistants
// The creator and founder of the idea is a student and just wanted to have some fun
// The privacy policy and terms of service are described in the documents in the main GitHub branch.
// MitaBotApi...



#include <dpp/dpp.h>
#include <string>
#include <iostream>
#include <json.hpp>
#include <curl/curl.h>
#include <windows.h>
#include <algorithm>

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
        if (event.command.get_command_name() == "ask") {
            event.thinking(false);

            try {
                std::string question = get_safe_param<std::string>(event, "question");
                std::string answer = ask_gemini(question, cfg.gemini_key);

                dpp::embed embed = dpp::embed()
                    .set_color(0xFF69B4)
                    .set_title("Mita's Response")
                    .set_description(answer.empty() ?
                        "I didn't quite understand that. Could you rephrase your question?" :
                        answer.substr(0, 2000))
                    .set_footer(dpp::embed_footer()
                        .set_text("Asked by " + event.command.usr.username)
                        .set_icon(event.command.usr.get_avatar_url()))
                    .set_thumbnail("https://i.imgur.com/JJxSJ5s.png");

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
            dpp::slashcommand ask_cmd("ask", "Ask Mita a question", bot.me.id);
            ask_cmd.add_option(
                dpp::command_option(dpp::co_string, "question", "What would you like to ask Mita?", true)
                .set_max_length(500)
            );

            bot.guild_command_create(ask_cmd, cfg.guild_id, [](const auto& cb) {
                if (cb.is_error()) {
                    std::cerr << "Failed to register command: "
                        << cb.get_error().message << std::endl;
                }
                });

            std::cout << "Mita AI is now online and ready to assist!\n";

            bot.set_presence(dpp::presence(
                dpp::ps_online,
                dpp::at_listening,
                "your questions | /ask"
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
/*
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


