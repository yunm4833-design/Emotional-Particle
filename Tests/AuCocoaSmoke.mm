#import <AudioToolbox/AudioToolbox.h>
#import <AudioUnit/AUCocoaUIView.h>
#import <Cocoa/Cocoa.h>

#include <iostream>

namespace {
void pumpEvents()
{
    [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.12]];
}

id findControl(id node, NSString* title, int depth=0)
{
    if (!node || depth>20)
        return nil;

    if ([node respondsToSelector:@selector(accessibilityTitle)]
        && [[node accessibilityTitle] isEqualToString:title])
        return node;

    if ([node respondsToSelector:@selector(accessibilityChildren)])
        for (id child in [node accessibilityChildren])
            if (id match=findControl(child,title,depth+1))
                return match;

    if ([node isKindOfClass:NSView.class])
        for (NSView* child in [node subviews])
            if (id match=findControl(child,title,depth+1))
                return match;

    return nil;
}

NSPoint controlCentre(NSWindow* window,id control)
{
    const NSRect rect=[window convertRectFromScreen:[control accessibilityFrame]];
    return NSMakePoint(NSMidX(rect),NSMidY(rect));
}

bool sendMouse(NSWindow* window,NSView* root,NSPoint point,NSEventType type)
{
    const NSPoint inHost=[root.superview convertPoint:point fromView:nil];
    if (!NSPointInRect(inHost,root.superview.bounds))
        return false;

    NSView* target=[root hitTest:inHost];
    if (!target)
        return false;

    NSEvent* event=[NSEvent mouseEventWithType:type
                                     location:point
                                modifierFlags:0
                                    timestamp:NSProcessInfo.processInfo.systemUptime
                                 windowNumber:window.windowNumber
                                      context:nil
                                  eventNumber:0
                                   clickCount:1
                                     pressure:type==NSEventTypeLeftMouseDown?1.0:0.0];
    if (type==NSEventTypeLeftMouseDown)
        [target mouseDown:event];
    else
        [target mouseUp:event];
    pumpEvents();
    return true;
}

bool fail(const char* message)
{
    std::cerr<<message<<'\n';
    return false;
}

bool checkEditor(AudioUnit unit)
{
    AudioUnitCocoaViewInfo info{};
    UInt32 size=sizeof(info);
    if (AudioUnitGetProperty(unit,kAudioUnitProperty_CocoaUI,kAudioUnitScope_Global,0,&info,&size)!=noErr)
        return fail("Could not obtain the installed AU's Cocoa UI factory");

    NSURL* bundleURL=CFBridgingRelease(info.mCocoaAUViewBundleLocation);
    NSString* className=CFBridgingRelease(info.mCocoaAUViewClass[0]);
    Class viewClass=[[NSBundle bundleWithURL:bundleURL] classNamed:className];
    if (![viewClass conformsToProtocol:@protocol(AUCocoaUIBase)])
        return fail("Installed AU did not provide a Cocoa UI factory");

    id<AUCocoaUIBase> factory=[[viewClass alloc] init];
    NSView* editor=[factory uiViewForAudioUnit:unit withSize:NSMakeSize(1080,608)];
    if (!editor||editor.frame.size.width<100||editor.frame.size.height<100)
        return fail("Could not create the AU editor");

    const NSSize initialSize=editor.frame.size;
    auto* window=[[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,initialSize.width,initialSize.height)
                                             styleMask:NSWindowStyleMaskTitled
                                               backing:NSBackingStoreBuffered
                                                 defer:NO];
    window.releasedWhenClosed=NO;
    NSView* host=window.contentView;
    [host addSubview:editor];
    [window center];
    [window makeKeyAndOrderFront:nil];
    pumpEvents();

    id header=findControl(editor,@"Sound Details");
    id footer=findControl(editor,@"Sound Details shortcut");
    const auto panelIsOpen=[&] { return findControl(editor,@"RESET DETAILS")!=nil; };
    if (!header||!footer||panelIsOpen())
        return fail("Expected two Sound Details buttons and a closed panel in the installed AU");

    const NSPoint headerPoint=controlCentre(window,header);
    const NSPoint footerPoint=controlCentre(window,footer);

    // Observe the actual AU after mouse-down, before delivering mouse-up.
    if (!sendMouse(window,editor,headerPoint,NSEventTypeLeftMouseDown)||!panelIsOpen())
        return fail("Cocoa mouse-down on the upper button did not reveal Reset Details");
    if (!sendMouse(window,editor,headerPoint,NSEventTypeLeftMouseUp)||!panelIsOpen())
        return fail("Releasing the upper button toggled the panel twice");
    if (!sendMouse(window,editor,footerPoint,NSEventTypeLeftMouseDown)||panelIsOpen()
        ||!sendMouse(window,editor,footerPoint,NSEventTypeLeftMouseUp)||panelIsOpen())
        return fail("Cocoa click on the footer did not close the panel");

    // Keep the editor's size, but clip 40 points off the host's bottom edge.
    constexpr CGFloat crop=40;
    [window setContentSize:NSMakeSize(initialSize.width,initialSize.height-crop)];
    [editor setFrameOrigin:NSMakePoint(0,-crop)];
    pumpEvents();

    const NSPoint clippedFooter=controlCentre(window,footer);
    const NSPoint visibleHeader=controlCentre(window,header);
    const NSRect viewport=host.bounds;
    if (NSPointInRect([host convertPoint:clippedFooter fromView:nil],viewport))
        return fail("The clipped viewport still exposes the footer");
    if (!sendMouse(window,editor,visibleHeader,NSEventTypeLeftMouseDown)||!panelIsOpen()
        ||!sendMouse(window,editor,visibleHeader,NSEventTypeLeftMouseUp)||!panelIsOpen()
        ||!NSEqualRects(viewport,host.bounds))
        return fail("Upper button did not open Sound Details in the clipped viewport");

    [editor removeFromSuperview];
    [window close];
    std::cout<<"PASS: Cocoa mouse events opened and closed the installed AU panel, including with the footer clipped\n";
    return true;
}
}

int main()
{
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [NSApp finishLaunching];
        [NSApp activateIgnoringOtherApps:YES];

        AudioComponentDescription description{kAudioUnitType_Effect,'Empr','Spor',0,0};
        AudioComponent component=AudioComponentFindNext(nullptr,&description);
        UInt32 version=0;
        if (!component||AudioComponentGetVersion(component,&version)!=noErr) {
            std::cerr<<"Emotional Particle AU not found\n";
            return 1;
        }
        std::cout<<"Loaded AU version "<<(version>>16)<<'.'<<((version>>8)&255)<<'.'<<(version&255)<<'\n';

        AudioUnit unit=nullptr;
        if (AudioComponentInstanceNew(component,&unit)!=noErr||!unit) {
            std::cerr<<"Could not instantiate the installed AU\n";
            return 2;
        }
        const bool passed=checkEditor(unit);
        AudioComponentInstanceDispose(unit);
        return passed?0:3;
    }
}
