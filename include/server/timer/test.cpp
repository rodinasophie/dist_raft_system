#include <iostream>
using namespace std;
#include "Timer.hpp"

int main() {
	Timer timer(std::chrono::milliseconds(100));
	timer.Run();
	while (!timer.TimedOut()) {
		std::cout << "+\n";
		timer.Run();
	}
}
