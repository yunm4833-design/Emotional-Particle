// EP Cocoa snapshot compatibility: avoid CGWindowListCreateImage, removed in SDK 15.
static Image createNSWindowSnapshot (NSWindow* window)
{
    JUCE_AUTORELEASEPOOL
    {
        NSView* view = [window contentView];
        if (view == nil || NSIsEmptyRect ([view bounds]))
            return {};

        const NSRect bounds = [view bounds];
        NSBitmapImageRep* bitmap = [view bitmapImageRepForCachingDisplayInRect: bounds];
        if (bitmap == nil || [bitmap pixelsWide] <= 0 || [bitmap pixelsHigh] <= 0)
            return {};

        [view cacheDisplayInRect: bounds toBitmapImageRep: bitmap];
        const int width = (int) [bitmap pixelsWide], height = (int) [bitmap pixelsHigh];
        Image result (Image::ARGB, width, height, true);
        selectImageForDrawing (result);
        [bitmap drawInRect: NSMakeRect (0, 0, width, height)];
        releaseImageAfterDrawing();
        return result;
    }
}
