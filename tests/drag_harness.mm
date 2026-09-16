// Pushes a fake macOS drag (file URL on a pasteboard) through the real JUCE NSView.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#import <AppKit/AppKit.h>
#include <cstdio>

@interface VellumFakeDrag : NSObject
@property (retain) NSPasteboard* pb;
@property NSPoint loc;
@end
@implementation VellumFakeDrag
- (NSPasteboard*) draggingPasteboard { return self.pb; }
- (NSPoint) draggingLocation { return self.loc; }
- (NSDragOperation) draggingSourceOperationMask { return NSDragOperationCopy; }
- (id) draggingSource { return nil; }
- (NSInteger) draggingSequenceNumber { return 1; }
- (NSWindow*) draggingDestinationWindow { return nil; }
@end

static void pump (double ms) { const double end = juce::Time::getMillisecondCounterHiRes() + ms; while (juce::Time::getMillisecondCounterHiRes() < end) CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0.01, false); }

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    vellum::VellumProcessor proc; proc.prepareToPlay (48000.0, 512);
    std::unique_ptr<vellum::VellumEditor> ed (dynamic_cast<vellum::VellumEditor*> (proc.createEditor()));
    ed->setSize (1000, 640);
    ed->addToDesktop (juce::ComponentPeer::windowHasTitleBar);
    ed->setVisible (true);
    pump (300);
    auto* peer = ed->getPeer();
    NSView* view = (__bridge NSView*) peer->getNativeHandle();
    std::printf ("registered drag types: %s\n", [[[view registeredDraggedTypes] description] UTF8String]);

    NSMutableArray* urls = [NSMutableArray array];
    for (int i = 1; i < argc; ++i) [urls addObject: [NSURL fileURLWithPath: [NSString stringWithUTF8String: argv[i]]]];
    NSPasteboard* pb = [NSPasteboard pasteboardWithUniqueName];
    [pb clearContents];
    if (std::getenv ("LEGACY"))
    {
        NSMutableArray* paths = [NSMutableArray array];
        for (NSURL* u in urls) [paths addObject: [u path]];
        [pb declareTypes: @[@"NSFilenamesPboardType"] owner: nil];
        [pb setPropertyList: paths forType: @"NSFilenamesPboardType"];
        std::printf ("using legacy NSFilenamesPboardType only\n");
    }
    else [pb writeObjects: urls];

    VellumFakeDrag* drag = [[VellumFakeDrag alloc] init];
    drag.pb = pb;
    drag.loc = [view convertPoint: NSMakePoint (300, [view bounds].size.height - 150) toView: nil];   // over pad 4 (flipped)

    const NSDragOperation entered = [view draggingEntered: (id<NSDraggingInfo>) drag];
    const NSDragOperation updated = [view draggingUpdated: (id<NSDraggingInfo>) drag];
    const BOOL performed = [view performDragOperation: (id<NSDraggingInfo>) drag];
    std::printf ("draggingEntered -> %lu, draggingUpdated -> %lu, performDragOperation -> %d\n", (unsigned long) entered, (unsigned long) updated, (int) performed);
    pump (2500);
    int loaded = 0;
    for (int i = 0; i < vellum::kNumPads; ++i)
        if (proc.pad (i).model) { std::printf ("pad %d: %s\n", i + 1, proc.pad (i).name.toRawUTF8()); ++loaded; }
    std::printf (loaded > 0 ? "DRAG PATH OK\n" : "DRAG PATH FAILED\n");
    ed->removeFromDesktop();
    return loaded > 0 ? 0 : 1;
}
