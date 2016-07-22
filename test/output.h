#ifndef OTUPUT_H
#define OUTPUT_H

#include <iostream>
#include <mutex>

std::mutex lck;
bool in_use = false;


class Output {
	public:
		Output() {
			lck.lock();
		}

		~Output(){
			lck.unlock();
		}

		template <class T>
			Output &operator<<(const T &x) {
				std::cout << x;
				return *this;
			}
};

#define vaout Output()

#endif
