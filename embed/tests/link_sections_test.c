/* An accidental full-machine dependency must fail the embedded link. This
 * fixture is intentionally linked without section garbage collection. */
extern void unavailable_device_owner(void);

void unused_device_function(void) {
    unavailable_device_owner();
}

int main(void) {
    return 0;
}
