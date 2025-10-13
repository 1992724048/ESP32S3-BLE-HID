#include "./Event.hpp"

Event::Event() {
    touch();
}

Event::~Event() {
    touch();
}

void Event::registrator() {
    register_ble();
}
