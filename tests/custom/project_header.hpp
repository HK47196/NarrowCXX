#pragma once

inline int* header_allocation() {
    return new int(2); // expect: narrow-no-direct-new-delete
}
