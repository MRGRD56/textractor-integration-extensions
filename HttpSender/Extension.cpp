#include "Extension.h"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <codecvt>
#include <string>
#include <process.h>
#include "ShellAPI.h"
using json = nlohmann::json;

const std::string config_file_name = "HttpSenderConfig.json";

const char* default_config = R"""({
  "$info": "Learn more: https://github.com/MRGRD56/MgTextractorExtensions",
  "sentence": {
    "enabled": false,
    "url": "http://localhost:8080",
    "requestType": "JSON_TEXT",
    "selectedThreadOnly": true
  }
})""";

json config = nullptr;

bool is_enabled() {
	if (config != nullptr && config.is_object()) {
		json sentence = config["sentence"];
		json sentenceEnabled = sentence["enabled"];
		bool isSentenceEnabled = sentenceEnabled.get<bool>();
		return isSentenceEnabled;
	}

	return false;
}

std::string wstring_to_utf8(const std::wstring& str) {
	std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
	return convert.to_bytes(str);
}

std::wstring utf8_to_wstring(const std::string& str)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
	return convert.from_bytes(str);
}

inline bool file_exists(const std::string& name) {
	struct stat buffer;
	return stat(name.c_str(), &buffer) == 0;
}

bool has_config() {
	return file_exists(config_file_name);
}

json get_config() {
	std::ifstream config_file(config_file_name);
	if (!config_file.good()) {
		return nullptr;
	}

	json config = json::parse(config_file);
	return config;
}

bool check_config(const bool initialize) {
	if (has_config()) {
		return true;
	}
	
	if (initialize) {
		std::ofstream config_file(config_file_name);
		config_file << default_config << std::endl;
		config_file.close();

		ShellExecute(0, 0, utf8_to_wstring(config_file_name).c_str(), 0, 0 , SW_SHOW);
	}
	
	return false;
}

void update_config(const bool initialize = false) {
	if (check_config(initialize)) {
		config = get_config();
	} else {
		config = nullptr;
	}
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH:
		update_config(true);
		// MessageBoxW(NULL, L"Extension Loaded", L"Example", MB_OK);
		break;
	case DLL_PROCESS_DETACH:
		// MessageBoxW(NULL, L"Extension Removed", L"Example", MB_OK);
		break;
	}
	return TRUE;
}

void send_http_request_async(std::wstring& sentence, SentenceInfo sentenceInfo) {
	const std::string sentence_string = wstring_to_utf8(sentence);
	
	std::string request_body;
	std::string content_type;

	const json sentence_config = config["sentence"];
	const std::string request_type = sentence_config["requestType"].get<std::string>();

	json request_body_json;
	if (request_type == "JSON_TEXT") {
		request_body_json = {
			{"text", sentence_string}
		};
		request_body = request_body_json.dump();
		content_type = "application/json";
	} else if (request_type == "JSON_TEXT_WITH_META") {
		request_body_json = {
			{"text", sentence_string},
			{"meta", {
						{"isCurrentSelect", static_cast<bool>(sentenceInfo["current select"])},
						{"processId", sentenceInfo["process id"]},
						{"threadNumber", sentenceInfo["text number"]},
						{"threadName", sentenceInfo["text name"]}
			}}
		};
		request_body = request_body_json.dump();
		content_type = "application/json";
	} else {
		request_body = sentence_string;
		content_type = "text/plain";
	}

	cpr::AsyncResponse response = PostAsync(
		cpr::Url{ sentence_config["url"]},
		cpr::Body{ request_body },
		cpr::Header{
			{"Content-Type", content_type},
			{"Connection", "Keep-Alive"}
		});
}

/*
	Param sentence: sentence received by Textractor (UTF-16). Can be modified, Textractor will receive this modification only if true is returned.
	Param sentenceInfo: contains miscellaneous info about the sentence (see README).
	Return value: whether the sentence was modified.
	Textractor will display the sentence after all extensions have had a chance to process and/or modify it.
	The sentence will be destroyed if it is empty or if you call Skip().
	This function may be run concurrently with itself: please make sure it's thread safe.
	It will not be run concurrently with DllMain.
*/
bool ProcessSentence(std::wstring& sentence, SentenceInfo sentenceInfo) {
	update_config();
	
	if (!is_enabled()) {
		return false;
	}

	const json sentence_config = config["sentence"];
	
	if (sentence_config["selectedThreadOnly"] == false || sentenceInfo["current select"]) {
		send_http_request_async(sentence, sentenceInfo);
	}

	return false;
}
