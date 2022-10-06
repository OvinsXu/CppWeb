#ifndef _CELLTimestamp_
#define _CELLTimestamp_

#include<chrono>

using namespace std::chrono;

class CELLTimestamp
{
public:
	CELLTimestamp()
	{
		update();
	}

	~CELLTimestamp()
	{
	}
	void update() {
		_begin = high_resolution_clock::now();
	}
	double getElapsedTimeInSec() {
		return this->getElapsedTimeInMicroSec() * 0.000001;
	}
	double getElapsedTimeInMilliSec() {
		return this->getElapsedTimeInMicroSec() * 0.001;
	}
	long long getElapsedTimeInMicroSec() {
		return duration_cast<microseconds>(high_resolution_clock::now() - _begin).count();
	}

private:
	time_point<high_resolution_clock> _begin;
};

#endif // !_CELLTimestamp_