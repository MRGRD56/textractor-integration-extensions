#include "Extension.h"
#include <fstream>
#include <cpprest/http_client.h>
#include <nlohmann/json.hpp>
#include <codecvt>
#include <string>
#include <process.h>
#include <mutex>
#include "ShellAPI.h"
#include <ctime>

using json = nlohmann::json;

std::string wstring_to_utf8(const std::wstring& str) {
	std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
	return convert.to_bytes(str);
}

std::wstring utf8_to_wstring(const std::string& str)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
	return convert.from_bytes(str);
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH:
		break;
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

void send_http_request_async(std::wstring& sentence, SentenceInfo sentenceInfo, time_t timestamp) {
	const std::string sentence_string = wstring_to_utf8(sentence);

	const std::string request_url = "http://localhost:18952/sentence";
	const std::string request_type = "POST";

	const std::wstring content_type = L"application/json; charset=UTF-8";

	json request_body_json = {
			{"text", sentence_string},
			{"meta", {
						{"isCurrentSelect", static_cast<bool>(sentenceInfo["current select"])},
						{"processId", sentenceInfo["process id"]},
						{"threadNumber", sentenceInfo["text number"]},
						{"threadName", sentenceInfo["text name"]},
						{"timestamp", timestamp}
			}}
	};

	std::string request_body = request_body_json.dump();

	try {
		web::http::client::http_client client(utf8_to_wstring(request_url));
		web::http::http_request request(web::http::methods::POST);
		request.headers().add(L"Connection", L"Keep-Alive");
		request.headers().add(L"Content-Type", content_type);
		request.set_body(request_body);

		client.request(request);
	}
	catch (...) {}
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
	if (&sentence != nullptr && sentenceInfo["current select"]) {
		send_http_request_async(sentence, sentenceInfo, std::time(0));
	}

	return false;
}
