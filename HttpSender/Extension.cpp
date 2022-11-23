#include "Extension.h"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

BOOL WINAPI DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
	//switch (ul_reason_for_call)
	//{
	//case DLL_PROCESS_ATTACH:
	//	MessageBoxW(NULL, L"Extension Loaded", L"Example", MB_OK);
	//	break;
	//case DLL_PROCESS_DETACH:
	//	MessageBoxW(NULL, L"Extension Removed", L"Example", MB_OK);
	//	break;
	//}
	return TRUE;
}

void SendHttpRequestAsync(std::wstring& sentence) {
	//json requestBodyJson = {
	//	{"text", *sentence.c_str()},
	//	{"sentenceInfo", {
	//		
	//	}}
	//};


	std::string sentenceString;
	std::transform(sentence.begin(), sentence.end(), std::back_inserter(sentenceString), [](wchar_t c) {
		return (char)c;
		});

	cpr::AsyncResponse response = cpr::PostAsync(
		cpr::Url{ "http://localhost:1234/sentence" },
		cpr::Body{ sentenceString },
		cpr::Header{ {"Content-Type", "text/plain"} });
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
	if (sentenceInfo["current select"]) {
		SendHttpRequestAsync(sentence);
	}

	return false;
}
