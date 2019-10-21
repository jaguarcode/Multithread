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

	// ��׶��� ������ ���� ����
	/*
		mExit ���� true�� �����ϱ� ���� notify_all()�� ȣ���ϱ� ����
		���� mMutex�� ���� �Ǵ�.
		�̴� processEntries()���� ������ �����̳� ������� �߻����� �ʰ� �ϱ� ���ؼ���.
		processEntries()�� while���� ���� �κп� �� �� �ִ�.
		�̶� mExit�� �˻��� ���Ŀ� wait()�� ȣ���ϱ� ������ �д�.
		�̶� ���� �����尡 Logger�� �Ҹ��ڸ� ȣ���ߴµ� �Ҹ��ڰ� mMutex�� ���� �� �� ���ٸ�
		�Ҹ��ڴ� processEntries()�� mExit�� Ȯ���� �� ���� ������ ��ٸ��� ���� 
		mExit�� true�� �����ϰ� notify_all()�� ȣ���ϰ� �ȴ�.
		���� processEntries()�� mExit�� �����Ǵ� ���ο� ���� �� �� ���Եǰ� �˸��� ��ģ��.
		�׷��� ���ø����̼ǿ� ������� �߻��Ѵ�.
		�Ҹ��ڴ� join()�� ȣ��� ������ ��ٸ���, ��׶��� ������� ���� ������ �˸���
		���� ��ٸ��� �����̴�.
		�̶� �Ҹ��ڴ� �ݵ�� join()�� �ϱ� ���� ���� mMutex�� ���� ���� �����ؾ� �Ѵ�.
		�׷��� �߰�ȣ ���̿� �ڵ尡 �ۼ��Ǿ���.

		caution. �Ϲ������� ��ٸ��� ������ ������ ���� ���Ǻ����� ���� ���ؽ��� ���� �ɾ�� �Ѵ�.
	*/
	virtual ~Logger() 
	{
		{
			std::unique_lock lock(mMutex);
			// flag�� �����ϰ� �����忡 �˸��� ������ �����带 ���� ���� ��Ų��
			mExit = true;
			mCondVar.notify_all();
		}

		// �����尡 ����� ������ ��ٸ���. �� �κ��� �տ� ���� ��� �ۿ� �־� �Ѵ�.
		// join()�� ȣ���ϱ� ���� �ݵ�� ���� �����ؾ� �ϱ� ����.
		mThread.join();
	}

	// ���� �����ڿ� ���� �����ڸ� �����Ѵ�.
	Logger(const Logger& src) = delete;
	Logger& operator=(const Logger& rhs) = delete;

	// �α� �׸��� ť�� �����ϴ� �Լ�
	void log(std::string_view entry)
	{
		std::unique_lock lock(mMutex);
		mQueue.push(std::string(entry));
		mCondVar.notify_all();
	}

private:
	// ��׶��� �����忡�� ������ �Լ�
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

	bool mExit = false; // ��׶��� �������� ���� ����
};