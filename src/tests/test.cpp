#include "tests/TimeTest.hpp"

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cout << "Incorrect input\n";
        return -1;
    }
    TimeTest test;
    //test.SetLatency(20000); // microsecond
    test.Run(LEADER_ELECTION, argv[1]);
}
