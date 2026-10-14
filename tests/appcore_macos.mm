#import <Cocoa/Cocoa.h>

// Drive an actual native resize; the portable SDK has no programmatic Window::Resize().
bool ResizeAppCoreTestWindow(const char *title, double width, double height) {
    @autoreleasepool {
        NSString *expected = [NSString stringWithUTF8String:title];
        for (NSWindow *window in [NSApp windows]) {
            if ([window.title isEqualToString:expected]) {
                [window setContentSize:NSMakeSize(width, height)];
                return true;
            }
        }
    }
    return false;
}
