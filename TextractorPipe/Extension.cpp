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
#include <queue>
#include <functional>
#include <thread>
#include <condition_variable>
#include <atomic>

using json = nlohmann::json;

const LPCWSTR PIPE_NAME = L"\\\\.\\pipe\\MRGRD56_TextractorPipe_f30799d5-c7eb-48e2-b723-bd6314a03ba2";


using Task = std::function<void()>;

class BlockingQueue {
public:
	void push(Task task) {
		{
			std::lock_guard<std::mutex> lock(mutex_);
			queue_.push(task);
		}
		condition_.notify_one();
	}

	Task pop() {
		std::unique_lock<std::mutex> lock(mutex_);
		condition_.wait(lock, [this] { return !queue_.empty(); });
		Task task = queue_.front();
		queue_.pop();
		return task;
	}

	bool empty() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return queue_.empty();
	}

private:
	mutable std::mutex mutex_;
	std::queue<Task> queue_;
	std::condition_variable condition_;
};

class TaskExecutor {
public:
	TaskExecutor() : done(false) {
		worker = std::thread([this]() { this->run(); });
	}

	~TaskExecutor() {
		this->shotdown();
	}

	void addTask(Task task) {
		tasks.push(task);
	}

	void shotdown() {
		done = true;
		tasks.push([]() {}); // Push an empty task to unblock pop if waiting.
		if (worker.joinable()) {
			worker.join();
		}
	}

private:
	std::atomic<bool> done;
	BlockingQueue tasks;
	std::thread worker;

	void run() {
		while (!done) {
			Task task = tasks.pop();
			if (done) break; // Check if we are done before executing.
			task();
		}
	}
};

TaskExecutor* executor = nullptr;

HANDLE pipe = nullptr;


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
			executor = new TaskExecutor();
		}
		break;
	case DLL_PROCESS_DETACH:
		try {
			if (executor != nullptr) {
				delete executor;
			}
		} catch (...) {}
		try {
			CloseHandle(&pipe);
		} catch (...) {}
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
		try {
			std::string data = serialize_sentence(sentence, sentenceInfo, std::time(0));
			executor->addTask([data]() {
				send_data_to_pipe(data);
			});
		} catch (...) {}
	}

	return false;
}
