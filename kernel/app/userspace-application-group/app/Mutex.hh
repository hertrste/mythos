#pragma once

class SpinMutex {
public:
	void lock() {
		while (flag.test_and_set()) { mythos::hwthread_pause(10); }
	}

	void unlock() {
		flag.clear();
	}

	template<typename FUNCTOR>
	void operator<<(FUNCTOR fun) {
		lock();
		fun();
		unlock();
	}

private:
	std::atomic_flag flag;
};