#pragma once

#include <string>
#include <string_view>
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>
#include <iostream>
#include <fstream>

class Logger
{
public:
	Logger()
	{
		mThread = std::thread{ &Logger::processEntries, this };
	}

	// 백그라운드 스레드 정상 종료
	/*
		mExit 값을 true로 설정하기 전과 notify_all()을 호출하기 전에
		먼저 mMutex에 락을 건다.
		이는 processEntries()에서 데이터 경쟁이나 데드락이 발생하지 않게 하기 위해서다.
		processEntries()를 while문의 시작 부분에 둘 수 있다.
		이때 mExit를 검사한 직후와 wait()를 호출하기 직전에 둔다.
		이때 메인 스레드가 Logger의 소멸자를 호출했는데 소멸자가 mMutex에 락을 걸 수 없다면
		소멸자는 processEntries()가 mExit를 확인한 후 조건 변수를 기다리기 전에 
		mExit를 true로 설정하고 notify_all()을 호출하게 된다.
		따라서 processEntries()는 mExit에 설정되는 새로운 값을 볼 수 없게되고 알림을 놓친다.
		그러면 어플리케이션에 데드락이 발생한다.
		소멸자는 join()이 호출될 때까지 기다리고, 백그라운드 스레드는 조건 변수에 알림이
		오길 기다리기 때문이다.
		이때 소멸자는 반드시 join()을 하기 전에 먼저 mMutex에 대한 락을 해제해야 한다.
		그래서 중괄호 사이에 코드가 작성되었다.

		caution. 일반적으로 기다리는 조건을 설정할 때는 조건변수에 대한 뮤텍스에 락을 걸어야 한다.
	*/
	virtual ~Logger() 
	{
		{
			std::unique_lock lock(mMutex);
			// flag를 설정하고 스레드에 알림을 보내서 스레드를 정상 종료 시킨다
			mExit = true;
			mCondVar.notify_all();
		}

		// 스레드가 종료될 때까지 기다린다. 이 부분은 앞에 나온 블록 밖에 둬야 한다.
		// join()을 호출하기 전에 반드시 락을 해제해야 하기 때문.
		mThread.join();
	}

	// 복제 생성자와 대입 연산자를 삭제한다.
	Logger(const Logger& src) = delete;
	Logger& operator=(const Logger& rhs) = delete;

	// 로그 항목을 큐에 저장하는 함수
	void log(std::string_view entry)
	{
		std::unique_lock lock(mMutex);
		mQueue.push(std::string(entry));
		mCondVar.notify_all();
	}

private:
	// 백그라운드 스레드에서 실행할 함수
	void processEntries()
	{
		std::ofstream logFile("log.txt");
		if (logFile.fail()) {
			std::cerr << "Failed to open logfile." << std::endl;
			return;
		}

		std::unique_lock lock(mMutex);
		while (true) {
			mCondVar.wait(lock);

			lock.unlock();
			while (true) {
				lock.lock();
				if (mQueue.empty()) {
					break;
				}
				else {
					logFile << mQueue.front() << std::endl;
					mQueue.pop();
				}
				lock.unlock();
			}

			if (mExit) {
				break;
			}
		}
	}

	std::mutex mMutex;
	std::condition_variable mCondVar;
	std::queue<std::string> mQueue;
	std::thread mThread;

	bool mExit = false; // 백그라운드 스레드의 종료 여부
};