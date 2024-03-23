#include "Extension.h"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include <codecvt>
#include <string>
#include <process.h>
#include <mutex>
#include <future>
#include <Windows.h>
#include "ShellAPI.h"
#include <ctime>
#include <cstddef> // For size_t
#include <cstdint> // For int32_t
#include <limits>

using json = nlohmann::json;

const LPCWSTR PIPE_NAME = L"\\\\.\\pipe\\MRGRD56_TextractorPipe";

std::string wstring_to_utf8(const std::wstring& str) {
	std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
	return convert.to_bytes(str);
}

std::wstring utf8_to_wstring(const std::string& str) {
	std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
	return convert.from_bytes(str);
}

HANDLE create_pipe() {
	return CreateNamedPipe(
		PIPE_NAME,            // Имя pipe
		PIPE_ACCESS_OUTBOUND, // Режим записи для pipe
		PIPE_TYPE_BYTE |      // Передача данных байтами
		PIPE_READMODE_BYTE |  // Чтение данных байтами
		PIPE_WAIT,            // Блокировка операций до их выполнения
		1,                    // Максимальное количество экземпляров
		1024 * 16,            // Размер выходного буфера в байтах
		1024 * 16,            // Размер входного буфера в байтах
		NMPWAIT_USE_DEFAULT_WAIT, // Тайм-аут клиента
		NULL                  // Атрибуты безопасности по умолчанию
	);
}

HANDLE pipe = nullptr;

BOOL WINAPI DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
	switch (ul_reason_for_call) {
		case DLL_PROCESS_ATTACH:
		{
			HANDLE new_pipe = create_pipe();
			if (new_pipe == INVALID_HANDLE_VALUE) {
				std::cerr << "Ошибка создания pipe." << std::endl;
				return FALSE;
			}
			pipe = new_pipe;
		}
		break;
	case DLL_PROCESS_DETACH:
		CloseHandle(&pipe);
		break;
	}
	return TRUE;
}

std::string serialize_sentence(std::wstring& sentence, SentenceInfo sentenceInfo, time_t timestamp) {
	const std::string sentence_string = wstring_to_utf8(sentence);
	
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
	return request_body;
}

//int32_t size_to_int32(std::size_t value) {
//	if (value > std::numeric_limits<int32_t>::max()) {
//		// Handle the case where the value is too large for a signed int32_t.
//		// For the sake of this example, we'll just print an error and return the max value.
//		std::cerr << "Warning: Value too large, conversion may lose data." << std::endl;
//		return std::numeric_limits<int32_t>::max();
//	}
//	return static_cast<int32_t>(value);
//}

void send_data_to_pipe(const std::string& data) {
	std::cout << "Waiting for a client to connect..." << std::endl;

	DWORD lastError = NULL;

	// Ожидание подключения клиента
	BOOL connected = ConnectNamedPipe(pipe, NULL) ?
		TRUE : ((lastError = GetLastError()) == ERROR_PIPE_CONNECTED);

	if (!connected) {
		std::cerr << "Unable to connect a client." << std::endl;

		if (lastError == ERROR_NO_DATA || lastError == ERROR_BROKEN_PIPE) {
			BOOL disconnected = DisconnectNamedPipe(pipe);
			send_data_to_pipe(data);
		}

		return;
	}

	std::cout << "Client connected. Sending data..." << std::endl;

	DWORD bytesWritten;

	uint32_t length = static_cast<uint32_t>(data.size());

	BOOL sizeWriteResult = WriteFile(pipe, &length, sizeof(length), &bytesWritten, NULL);

	if (!sizeWriteResult) {
		std::cerr << "Error sending size." << std::endl;
	} else {
		std::cout << "Size successfully sent." << std::endl;
	}

	BOOL dataWriteResult = WriteFile(pipe, data.c_str(), data.size(), &bytesWritten, NULL);
	
	if (!dataWriteResult) {
		std::cerr << "Error sending data." << std::endl;
	} else {
		std::cout << "Data successfully sent." << std::endl;
	}
}

// Функция для отправки данных через именованный канал
//void send_data_to_pipe(const std::string& data) {
//	// Создание именованного канала
//	HANDLE pipe = CreateFileW(PIPE_NAME.c_str(),
//		GENERIC_WRITE,
//		0, // No sharing
//		NULL, // Default security attributes
//		OPEN_EXISTING, // Opens existing pipe
//		0, // Default attributes
//		NULL); // No template file
//	if (pipe == INVALID_HANDLE_VALUE) {
//		std::cerr << "Failed to connect to pipe: " << GetLastError() << std::endl;
//		return;
//	}
//
//	// Отправка данных
//	DWORD bytesWritten;
//	BOOL result = WriteFile(pipe, data.c_str(), data.size(), &bytesWritten, NULL);
//	if (!result) {
//		std::cerr << "Failed to send data: " << GetLastError() << std::endl;
//	}
//
//	// Закрытие дескриптора именованного канала
//	CloseHandle(pipe);
//}

// Функция для отправки данных через named pipe асинхронно
//void send_data_to_pipe_async1(const std::wstring& data) {
//	auto sendData = [](const std::wstring data) {
//		HANDLE hPipe;
//
//		// Попытка подключения к named pipe
//		hPipe = CreateFile(PIPE_NAME.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
//		if (hPipe == INVALID_HANDLE_VALUE) {
//			std::cerr << "Failed to connect to pipe: " << GetLastError() << std::endl;
//			return;
//		}
//
//		// Отправка данных
//		DWORD bytesWritten = 0;
//		BOOL result = WriteFile(hPipe, data.c_str(), (data.size() + 1) * sizeof(wchar_t), &bytesWritten, NULL);
//		if (!result) {
//			std::cerr << "Failed to write to pipe: " << GetLastError() << std::endl;
//		}
//
//		CloseHandle(hPipe);
//	};
//
//	// Запуск асинхронной отправки данных
//	std::async(std::launch::async, sendData, data);
//}

//void send_data_to_pipe_async(const std::string& data) {
//	// Запуск асинхронной отправки данных
//	std::async(std::launch::async, [&data](){
//		send_data_to_pipe(data);
//	});
//}

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
		std::string data = serialize_sentence(sentence, sentenceInfo, std::time(0));
		send_data_to_pipe(data);
	}

	return false;
}
