/* Shared PUAE source units also contain non-68000 functions. Those functions
 * must remain separately discardable without supplying fake device owners. */
extern void unavailable_device_owner(void);

void unused_device_function(void) {
    unavailable_device_owner();
}

int main(void) {
    return 0;
}
