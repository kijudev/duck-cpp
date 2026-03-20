#pragma once

template <typename T, typename Alloc>
class StdVector {};

class NASZALOKATOR {};
class ICHALOKATOR {};

void game() {
    StdVector<int, NASZALOKATOR> a;
    StdVector<int, ICHALOKATOR> b;
}

template <typename T>
class Vec {
    AllocatorI* alloc;
}

std : vector<OwnPointer<Base>>
