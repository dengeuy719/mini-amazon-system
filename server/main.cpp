#include "server.h"

int main() {
    Server & s = Server::getInstance();
    s.run();
    return 0;
}