#include <atomic>
#include <functional>
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <future>
#include <sstream>

#include "Logger.h"

using namespace std;

void counter(int id, int numIterations);

class Counter
{
public:
	Counter(int id, int numIterations) : mId(id), mNumIterations(numIterations) {};
	void operator()() const {
		for (int i = 0; i < mNumIterations; ++i) {
			//lock_guard lock(sMutex); // default
			unique_lock lock(sMutex, 200ms); // timed lock
			if (lock) {
				cout << "Counter " << mId << " has value " << i << endl;
			}
			else {
				cout << "fail to lock : " << mId << endl;
			}
		}
	}

private:
	int mId;
	int mNumIterations;
	//static mutex sMutex;
	static timed_mutex sMutex;
};

timed_mutex Counter::sMutex;

int k;
thread_local int n; // 각 스레드마다 thread_local 변수를 복제해서 스레드가 없어질 때까지 유지한다.
// 이 변수는 각 스레드에서 한번만 초기화된다.
// 만약 함수 스코프 안에서 선언하면 모든 스레드가 복제본을 따로 갖고있고 
// 함수를 많이 호출해도 스레드마다 한번만 초기화된다는 점을 제외하면 static과 동일하다.

void doSomeWork();
void threadFunc(exception_ptr& err);
void doWorkInThread();

void increment(atomic<int>& counter);

once_flag gOnceFlag;

void initializeSharedResouces()
{
	cout << "Shared resources initialized." << endl;
}

atomic<bool> gInitialized(false);
mutex gMutex;

void processingFunction()
{
	// call_once(gOnceFlag, initializeSharedResouces);
	// Anti-Pattern. 이중 검사 락 패턴
	// call_once가 속도 더 빠르고, 상호배제 객체 사용으로 더 안전하다.
	if (!gInitialized) { 
		unique_lock lock(gMutex);
		if (!gInitialized) {
			initializeSharedResouces();
			gInitialized = true;
		}
	}
	cout << "Processing..." << endl;
}

void doWork(promise<int> thePromise) {
	thePromise.set_value(42);
}

int calculateSum(int a, int b)
{
	return a + b;
}

int throwRuntimeException()
{
	throw runtime_error("Exception thrown from throwRuntimeException()");
}

void logSomeMessages(int id, Logger& logger)
{
	for (int i = 0; i < 10; ++i) {
		stringstream ss;
		ss << "Log entry " << i << " from thread " << id;
		logger.log(ss.str());
	}
}

int main()
{
	/*
		THREAD

		-기본
		현재 시스템에서 thread객체가 실행 가능한 상태에 있을 때 joinable하다고 표현한다.
		스레드는 생생되면 unjoinable(default)하게 생성된다. 
		조인 가능한 thread객체를 제거하려면 먼저 그 객체의 join()이나 detach()를 호출해야 한다.		

		join()을 호출하면 joinable하게 변경되고 해당 스레드가 작업을 마칠때까지 블록된다.
		detach()를 호출하면 thread객체를 OS 내부의 thread와 분리한다. OS thread는 독립적으로 실행된다.

		두 메서드(join, detach) 모두 스레드를 조인 불가능한 상태로 전환시킨다.

		조인 가능한 상태에서 thread객체를 제거하면 그 객체의 소멸자는 std::terminate()를 호출해서 
		모든 스레드뿐만 아니라 어플리케이션까지 종료시킨다.
	*/

	//thread t1(counter, 1, 1000); // 1. 일반 함수 포인터 초기화
	//thread t2(counter, 2, 500);

	thread t1(Counter{ 1, 10 }); // 2. 유니폼 초기화
	
	Counter c(2, 5); // 3. 일반 변수처럼 네임드 인스턴스 초기화
	thread t2(c);

	thread t3(Counter(3, 15)); // 4. 임시객체 초기화
	//thread t3(Counter()); // Counter 생성자가 인자를 받지 않을 경우, 이렇게 사용하면 컴파일 에러 발생 (해당 줄을 Counter함수의 선언으로 해석하기 때문)
	//thread t3(Counter{}); // 이렇게 사용하면 정상 처리된다.

	Counter c2(4, 6);
	thread t4(ref(c2)); // 5. 함수 객체의 인스턴스를 복제하지 않고 레퍼런스만 전달할 경우

	int id = 5;
	int numIterations = 5;
	thread t5([&id, &numIterations]() { // 6. 람다식 초기화
		for (int i = 0; i < numIterations; ++i) {
			cout << "Counter " << id << " has value " << i << endl;
		}
	});

	Counter c3(6, 4);
	// 7. 멤버 함수로 초기화 (특정한 객체에 있는 멤버 함수를 스레드로 분리해서 실행할 수 있다.)
	// 똑같은 객체에 여러 스레드가 접근하여 데이터 경쟁이 발생하지 않도록 스레드 세이프하게 작성해야 한다. (상호배제(mutex)사용 등)
	thread t6(&Counter::operator(), &c3); 

	t1.join(); // main()함수가 join()을 호출해서 메인 스레드를 블록시키고 모든 스레드가 종료될 때까지 기다린다. 
	t2.join(); // 이렇게 메인 스레드를 블록시키면 안된다. (예제니까 이렇게 함)
	t3.join(); // GUI 어플리케이션은 특히 메인 스레드를 블록시키면 UI가 반응하지 않는다.
	t4.join(); // 이럴때는 thread끼리 메시지로 통신하는 기법을 사용한다.
	t5.join();
	t6.join();

	/*
		-종료
		다른 스레드에서 스레드를 중단시키는 메커니즘은 제공하지 않지만, 
		공유 변수(아토믹/조건변수)를 활용하여 주기적으로 확인하여 종료할 수 있다.

		-결과얻기
		1. 결과를 담은 변수에 대한 포인터나 레퍼런스를 스레드로 전달하여 스레드마다 결과를 저장하게 하는 것
		2. 함수 객체의 클래스 멤버 변수에 처리 결과를 저장했다가 종료시 그 값을 가지고 오는 것
			이때는 반드시 std::ref()를 이용해서 함수 객체의 레퍼런스를 thread 생성자에 전달 해야 한다.
		3. promise(입력)/future(출력) 사용하기 (가장 간단)

		- 익셉션
		스레드에서 던진 익셉션은 해당 스레드 안에서 처리해야 한다.
		던진 익셉션을 처리하지 못하면 C++ 런타임은 std::terminate()를 호출해서 어플리케이션 전체를 종료시킨다.
		1. exception_ptr current_exception() noexcept;
			catch블록에서 처리하며 현재 처리할 익셉션을 가리키는 exception_ptr 객체나 그 복사본을 리턴한다.
			현재 처리할 exception이 없으면 Null exception_ptr 객체를 리턴한다.
		2. [[noreturn]] void rethrow_exception(exception_ptr p);
			exception_ptr 매개변수가 참조하는 익셉션을 다시 던진다. 참조한 익셉션을 반드시 처음 발생한 스레드안에서만 다시 던져야 한다는 법은 없다.
			여러 스래드에서 발생한 익셉션을 처리하는 용도로 알맞다.
		3. template<class E> exception_ptr make_exception_ptr(E e) noexcept;
			주어진 익셉션 객체의 복사본을 참조하는 exception_ptr 객체를 생성한다.
			아래 코드의 축약이다.
			try {
				throw e;
			} catch(...) {
				return current_exception();
			}
	*/

	try {
		doWorkInThread();
	}
	catch (exception& e) {
		cout << "Main function caught: '" << e.what() << "'" << endl;
	}

	/*
		아토믹 타입을 사용하면 동기화 기법을 사용하지 않고도 읽기와 쓰기를 동시에 처리하는 아토믹 접근(atomic access)이 가능하다.
		컴파일러는 먼저 메모리에서 이 값을 읽고 레지스터로 불러와서 값을 증가시키고 다시 결과를 메모리에 저장하는데,
		이때 같은 메모리 영역을 다른 스레드가 건들면 데이터 경쟁이 발생한다.

		atomic<int> counter(0);
		++counter; // thread-safe하다.

		atomic_bool, atomic_char, atomic_uchar, atomic_int, atomic_uint, 
		atomic_long, atomic_ulong, atomic_llong, atomic_ullong, atomic_wchar_t
	*/

	class Foo { private: int mArray[123]; };
	class Bar { private: int mInt; };

	atomic<Foo> f;
	atomic<Bar> b;

	cout << is_trivially_constructible<Foo>() << " " << f.is_lock_free() << endl; // 결과: 1 0
	cout << is_trivially_constructible<Bar>() << " " << b.is_lock_free() << endl; // 결과: 1 1

	atomic<int> counter(0); // atomic<int> counter = 0;
	vector<thread> threads;

	for (int i = 0; i < 10; ++i) {
		threads.push_back(thread{ increment, ref(counter) });
	}

	for (auto& t : threads) {
		t.join();
	}

	cout << "result: " << counter << endl; // atomic 선언하지 않으면 데이터 경쟁 발생. 결과 1000 아님.

	atomic<int> value(10);
	cout << "value = " << value << endl; // 10
	//정수형 아토믹 연산: fetch_add(), fetch_sub(), fetch_and(), fetch_or(), fetch_xor(), ++, --, +=, -=, &=, ^=, |= 
	//포인터 타입: fetch_add(), fetch_sub(), ++, --, +=, -=
	int fetched = value.fetch_add(4); 
	cout << "fetched = " << fetched << endl; // 10
	cout << "value = " << value << endl; // 14

	/*
		상호배제
		표준라이브러리는 mutext와 lock을 통해서 상호 배제 매커니즘을 제공한다.

		mutex (상호배제, mutual exclusion)
			- 다른 스레드와 공유하는 (읽기/쓰기용) 메모리를 사용하려면 먼저 mutex 객체에 락을 걸어야(잠금 요청)한다.
				다른 스레드가 먼저 락을 걸어뒀다면 그 락이 해제되거나 타임아웃으로 지정된 시간이 경과해야 쓸 수 있다.
			- 스레드가 락을 걸었다면 공유 메모리를 마음껏 쓸 수 있다. 물론 공유 데이터를 사용하려는 스레드마다
				뮤텍스에 대한 락을 걸고 해제하는 동작을 정확히 구현해야 한다.
			- 공유 메모리에 대한 읽기/쓰기 작업이 끝나면 다른 스레드가 공유 메모리에 대한 락을 걸 수 있도록 락을 해제한다.
				두 개 이상의 스레드가 락을 기다리고 있다면 어느 스레드가 먼저 락을 걸어 작업을 진행할지는 알 수 없다.
			- C++표준은 시간 제약이 없는 non-timed mutex(std::mutex<헤더:mutex>, recursive_mutex<헤더:mutex>, shared_mutex<헤더:shared_mutex(C++17이상)>)와
				시간 제약이 있는 timed mutex(std::timed_mutex<헤더:mutex>, recursive_timed_mutex<헤더:mutex>, shared_timed_mutex<헤더:shared_mutex(C++17이상)>)를 제공한다.

	*/
	vector<thread> threads2(3);
	for (auto& t : threads2) {
		t = thread{ processingFunction };
	}
	
	for (auto& t : threads2) {
		t.join();
	}

	/*
		Promise와 future
	*/

	// 스레드로 promise 생성 후 전달
	promise<int> promise1;
	auto future1 = promise1.get_future();
	thread theThread1{ doWork, move(promise1) };

	// 여기서.. 원하는 작업을 수행한다.

	// 최종 결과를 가져온다. get()을 호출하면 최종결과가 나올때까지 블록된다.
	// 성능이 크게 떨어지기때문에 future에 최종결과가 나왔는지 주기적으로 검사(wait_for())하도록 구현하거나
	// 조건 변수와 같은 동기화 기법을 사용하도록 구현한다.
	int result = future1.get();
	cout << "future result: " << result << endl;

	// 스레드를 join한다.
	theThread1.join();

	// packaged_task (promise를 명시적으로 사용하지 않아도 future 사용)
	packaged_task<int(int, int)> task(calculateSum);
	auto future2 = task.get_future();
	thread theThread2{ move(task), 1,2 };
	// 여기서..다른 작업을 수행한다.
	int result2 = future2.get();
	cout << "future result from packaged_task: " << result2 << endl;
	theThread2.join();

	/*
		async
		스레드로 계산하는 작업을 C++런타임으로 좀 더 제어하고 싶다면 async 사용
		async() 구동법
			- 함수를 스레드로 만들어 비동기식으로 구동한다.
			- 스레드를 따로 만들지 않고, 리턴된 future에 대해 get()을 호출할때 동기식으로 함수를 실행한다.

		launch::async : 주어진 함수를 다른 스레드에서 실행
		launch::deferred: get()을 호출할 때 주어진 함수를 현재 스레드와 동기식으로 실행
		launch::async | launch::deferred: C++런타임이 결정 (default동작)

		async()를 호출해서 리턴된 future는 실제 결과가 담길 때까지 소멸자에서 블록된다.
		리턴된 future를 캡쳐하지 않으면 임시 future 객체가 생성된다.
		해당 호출문이 끝나기 전에 소멸자가 호출되면서 결과가 나올 때까지 블록된다.
	*/

	//auto future3 = async(launch::async, calculateSum, 1, 2);
	//auto future3 = async(launch::deferred, calculateSum, 1, 2);
	auto future3 = async(calculateSum, 1, 2);

	// 여기서.. 다른 작업을 수행한다.

	int result3 = future3.get();
	cout << "async result: " << result3 << endl;

	// future의 가장 큰 장점, 스레드끼리 익셉션을 주고받는 데 활용할 수 있다는 것
	auto future4 = async(launch::async, throwRuntimeException);
	try {
		int result4 = future4.get();
		cout << result4 << endl;
	}
	catch (const exception& e) {
		cout << "Caught exception: " << e.what() << endl;
	}

	/*
		std::shared_future
		
		std::future<T>의 인수 T는 이동 생성될 수 있어야 한다. future<T>에 대해 get()을 호출하면
		future로부터 결과가 이동돼 리턴된다. 그러므로, future<T>에 대해 get()을 한번만 호출 할 수 있다.

		get()을 여러번 호출하려면 shared_future를 사용한다. 이때 T는 복제될 수 있어야 한다.
	*/

	promise<void> thread1Started, thread2Started;
	
	promise<int> signalPromise;
	auto signalFuture = signalPromise.get_future().share();
	//shared_future<int> signalFuture(signalPromise.get_future());

	auto function1 = [&thread1Started, signalFuture]() {
		thread1Started.set_value();
		// 매개변수가 설정될 때까지 기다린다.
		int parameter = signalFuture.get();
		// ...
		cout << "thread1 started with " << parameter << endl;
	};

	auto function2 = [&thread2Started, signalFuture]() {
		thread2Started.set_value();
		// 매개변수가 설정될 때까지 기다린다.
		int parameter = signalFuture.get();
		// ...
		cout << "thread2 started with " << parameter << endl;
	};

	// 두 람다 표현식을 비동기식으로 구동한다.
	auto result_thread1 = async(launch::async, function1);
	auto result_thread2 = async(launch::async, function2);

	// 두 스레드 모두 구동될 때까지 기다린다.
	thread1Started.get_future().wait();
	thread2Started.get_future().wait();

	// 두 스레드 모두 매개변수가 설정되기를 기다린다.
	// 두 스레드를 깨우는 매개변수를 설정한다.
	signalPromise.set_value(42);

	// Logger class
	Logger logger;
	vector<thread> loggerThreads;
	for (int i = 0; i < 10; ++i) {
		loggerThreads.emplace_back(logSomeMessages, i, ref(logger));
	}

	for (auto& t : loggerThreads) {
		t.join();
	}

	return 0;
}

// initialization
void counter(int id, int numIterations)
{
	for (int i = 0; i < numIterations; ++i) {
		cout << "Counter " << id << " has value " << i << endl;
	}
}

// exception
void doSomeWork() // throw exception
{
	for (int i = 0; i < 5; ++i) {
		cout << i << endl;
	}
	cout << "Thread throwing a runtime_error exception..." << endl;
	throw runtime_error("Exception from thread");
}

void threadFunc(exception_ptr& err)
{
	try {
		doSomeWork();
	}
	catch (...) {
		cout << "Thread caught exception, returning exception..." << endl;
		err = current_exception();
	}
}

void doWorkInThread()
{
	exception_ptr error;
	thread t{ threadFunc, ref(error) };
	t.join();
	if (error) {
		cout << "Main Thread received exception, rethrowing it..." << endl;
		rethrow_exception(error);
	}
	else {
		cout << "Main Thread did not receive any exception." << endl;
	}
}

// atomic
void increment(atomic<int>& counter)
{
	// counter를 직접 증가하면 동기화를 위해서 load, increment, save 작업을 
	// 하나의 atomic 트랜잭션으로 처리해서 중간에 다른 스레드가 개입을 하지 못함.
	// 성능 문제가 발생한다.
	// 로컬 변수를 활용해서 작업을 하고 counter에 반영하도록 하는 것이 바람직하다.
	int result = 0;
	for (int i = 0; i < 100; ++i) {
		++result;
		this_thread::sleep_for(1ms);
	}
	counter += result;
}