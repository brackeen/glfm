#include "glfm.h"

#ifdef GLFM_PLATFORM_IOS

#import <UIKit/UIKit.h>
#import <GLKit/GLKit.h>
#include <asl.h>

#define GLFM_ASSETS_USE_STDIO
#include "glfm_platform.h"

#pragma mark - ViewController

@interface GLFMViewController : GLKViewController
{
    NSMutableDictionary *activeTouches;
}

@property (strong, nonatomic) EAGLContext *context;
@property (nonatomic) GLFMDisplay *glfmDisplay;
@property (nonatomic) CGSize displaySize;
@property (nonatomic) BOOL multipleTouchEnabled;
@property (nonatomic) BOOL glkViewCreated;

@end

@implementation GLFMViewController

- (id)init
{
    if ((self = [super init])) {
        activeTouches = [[NSMutableDictionary alloc] init];
        _glfmDisplay = calloc(1, sizeof(GLFMDisplay));
        _glfmDisplay->platformData = (__bridge void *)self;
        glfm_main(_glfmDisplay);
    }
    return self;
}

- (BOOL)prefersStatusBarHidden
{
    return _glfmDisplay->uiChrome != GLFMUserInterfaceChromeNavigationAndStatusBar;
}

- (CGSize)calcDisplaySize
{
    BOOL isPortrait = YES;
    GLKView *view = (GLKView*)self.view;
    if (view.drawableWidth != 0 && view.drawableHeight != 0) {
        return CGSizeMake(view.drawableWidth, view.drawableHeight);
    }
    isPortrait = UIInterfaceOrientationIsPortrait(self.interfaceOrientation);
    
    CGFloat scale = [UIScreen mainScreen].scale;
    CGSize size = [UIScreen mainScreen].bounds.size;
    if (isPortrait) {
        return CGSizeMake(size.width * scale, size.height * scale);
    }
    else {
        return CGSizeMake(size.height * scale, size.width * scale);
    }
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    
    GLKView *view = (GLKView *)self.view;
    
    view.multipleTouchEnabled = self.multipleTouchEnabled;
    self.glkViewCreated = YES;
    self.displaySize = [self calcDisplaySize];
    self.context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    
    if (!self.context) {
        reportSurfaceError(_glfmDisplay, "Failed to create ES context");
        return;
    }
    
    view.context = self.context;
    
    switch (_glfmDisplay->colorFormat) {
        case GLFMColorFormatRGB565:
            view.drawableColorFormat = GLKViewDrawableColorFormatRGB565;
            break;
        case GLFMColorFormatRGBA8888: default:
            view.drawableColorFormat = GLKViewDrawableColorFormatRGBA8888;
            break;
    }
    
    switch (_glfmDisplay->depthFormat) {
        case GLFMDepthFormatNone: default:
            view.drawableDepthFormat = GLKViewDrawableDepthFormatNone;
            break;
        case GLFMDepthFormat16:
            view.drawableDepthFormat = GLKViewDrawableDepthFormat16;
            break;
        case GLFMDepthFormat24:
            view.drawableDepthFormat = GLKViewDrawableDepthFormat24;
            break;
    }
    
    switch (_glfmDisplay->stencilFormat) {
        case GLFMStencilFormatNone: default:
            view.drawableStencilFormat = GLKViewDrawableStencilFormatNone;
            break;
        case GLFMStencilFormat8:
            view.drawableStencilFormat = GLKViewDrawableStencilFormat8;
            break;
    }
    
    self.preferredFramesPerSecond = 60;
    [self setNeedsStatusBarAppearanceUpdate];
    
    [view bindDrawable];

    if (_glfmDisplay->surfaceCreatedFunc != NULL) {
        _glfmDisplay->surfaceCreatedFunc(_glfmDisplay, self.displaySize.width, self.displaySize.height);
    }
}

- (NSUInteger)supportedInterfaceOrientations
{
    GLFMUserInterfaceIdiom uiIdiom;
    if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPhone) {
        uiIdiom = GLFMUserInterfaceIdiomPhone;
    }
    else {
        uiIdiom = GLFMUserInterfaceIdiomTablet;
    }
    GLFMUserInterfaceOrientation uiOrientations = _glfmDisplay->allowedOrientations;
    if (uiOrientations == GLFMUserInterfaceOrientationAny) {
        if (uiIdiom == GLFMUserInterfaceIdiomTablet) {
            return UIInterfaceOrientationMaskAll;
        }
        else {
            return UIInterfaceOrientationMaskAllButUpsideDown;
        }
    }
    else if (uiOrientations == GLFMUserInterfaceOrientationPortrait) {
        if (uiIdiom == GLFMUserInterfaceIdiomTablet) {
            return UIInterfaceOrientationMaskPortrait | UIInterfaceOrientationMaskPortraitUpsideDown;
        }
        else {
            return UIInterfaceOrientationMaskPortrait;
        }
    }
    else {
        return UIInterfaceOrientationMaskLandscape;
    }
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    if (_glfmDisplay->lowMemoryFunc != NULL) {
        _glfmDisplay->lowMemoryFunc(_glfmDisplay);
    }
}

- (void)dealloc
{
    if ([EAGLContext currentContext] == self.context) {
        [EAGLContext setCurrentContext:nil];
    }
}

- (void)glkView:(GLKView*)view drawInRect:(CGRect)rect
{
    CGSize newDisplaySize = [self calcDisplaySize];
    if (!CGSizeEqualToSize(newDisplaySize, self.displaySize)) {
        self.displaySize = newDisplaySize;
        if (_glfmDisplay->surfaceResizedFunc != NULL) {
            _glfmDisplay->surfaceResizedFunc(_glfmDisplay, self.displaySize.width, self.displaySize.height);
        }
        
    }
    if (_glfmDisplay->mainLoopFunc != NULL) {
        _glfmDisplay->mainLoopFunc(_glfmDisplay, self.timeSinceFirstResume);
    }
}

#pragma mark - UIResponder

- (void)addTouchEvent:(UITouch*)touch withType:(GLFMTouchPhase)phase
{
    NSValue *key = [NSValue valueWithPointer: (const void*)touch];
    NSNumber *value = [activeTouches objectForKey: key];
    if (value == nil) {
        value = [NSNumber numberWithInteger:activeTouches.count];
        [activeTouches setObject:value forKey:key];
    }
    int touchNumber = [value intValue];
    
    CGFloat scale = [UIScreen mainScreen].scale;
    CGPoint currLocation = [touch locationInView:self.view];
    currLocation.x *= scale;
    currLocation.y *= scale;
    
    if (_glfmDisplay->touchFunc != NULL) {
        _glfmDisplay->touchFunc(_glfmDisplay, touchNumber, phase, currLocation.x, currLocation.y);
    }
    
    if (phase == GLFMTouchPhaseEnded || phase == GLFMTouchPhaseCancelled) {
        [activeTouches removeObjectForKey:key];
    }
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event
{
    for (UITouch *touch in touches) {
        [self addTouchEvent:touch withType:GLFMTouchPhaseBegan];
    }
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event
{
    for (UITouch *touch in touches) {
        [self addTouchEvent:touch withType:GLFMTouchPhaseMoved];
    }
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event
{
    for (UITouch *touch in touches) {
        [self addTouchEvent:touch withType:GLFMTouchPhaseEnded];
    }
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event
{
    for (UITouch *touch in touches) {
        [self addTouchEvent:touch withType:GLFMTouchPhaseCancelled];
    }
}

#pragma mark - Key commands

// Key input only works when8 a key is pressed, and has no repeat events or release events.
// Not ideal, but useful for prototyping.

- (BOOL)canBecomeFirstResponder
{
    return YES;
}

- (NSArray *)keyCommands
{
    if (_glfmDisplay->keyFunc == NULL) {
        return @[];
    }
    
    static NSArray *keyCommands = NULL;
    if (keyCommands == NULL) {
        keyCommands = @[ [UIKeyCommand keyCommandWithInput:UIKeyInputUpArrow
                                             modifierFlags:0
                                                    action:@selector(keyPressed:)],
                         [UIKeyCommand keyCommandWithInput:UIKeyInputDownArrow
                                             modifierFlags:0
                                                    action:@selector(keyPressed:)],
                         [UIKeyCommand keyCommandWithInput:UIKeyInputLeftArrow
                                             modifierFlags:0
                                                    action:@selector(keyPressed:)],
                         [UIKeyCommand keyCommandWithInput:UIKeyInputRightArrow
                                             modifierFlags:0
                                                    action:@selector(keyPressed:)],
                         [UIKeyCommand keyCommandWithInput:UIKeyInputEscape
                                             modifierFlags:0
                                                    action:@selector(keyPressed:)],
                         [UIKeyCommand keyCommandWithInput:@" "
                                             modifierFlags:0
                                                    action:@selector(keyPressed:)],
                         [UIKeyCommand keyCommandWithInput:@"\r"
                                             modifierFlags:0
                                                    action:@selector(keyPressed:)],
                         [UIKeyCommand keyCommandWithInput:@"\t"
                                             modifierFlags:0
                                                    action:@selector(keyPressed:)],
                         [UIKeyCommand keyCommandWithInput:@"\b"
                                             modifierFlags:0
                                                    action:@selector(keyPressed:)]];
    };
    
    return keyCommands;
}

- (void)keyPressed: (UIKeyCommand *)keyCommand
{
    if (_glfmDisplay->keyFunc != NULL) {
        NSString *key = [keyCommand input];
        GLFMKey keyCode = 0;
        if (key == UIKeyInputUpArrow) {
            keyCode = GLFMKeyUp;
        }
        else if (key == UIKeyInputDownArrow) {
            keyCode = GLFMKeyDown;
        }
        else if (key == UIKeyInputLeftArrow) {
            keyCode = GLFMKeyLeft;
        }
        else if (key == UIKeyInputRightArrow) {
            keyCode = GLFMKeyRight;
        }
        else if (key == UIKeyInputEscape) {
            keyCode = GLFMKeyEscape;
        }
        else if ([@" " isEqualToString:key]) {
            keyCode = GLFMKeySpace;
        }
        else if ([@"\t" isEqualToString:key]) {
            keyCode = GLFMKeyTab;
        }
        else if ([@"\b" isEqualToString:key]) {
            keyCode = GLFMKeyBackspace;
        }
        else if ([@"\r" isEqualToString:key]) {
            keyCode = GLFMKeyEnter;
        }
        
        if (keyCode != 0) {
            _glfmDisplay->keyFunc(_glfmDisplay, keyCode, GLFMKeyActionPressed, 0);
        }
    }
}

@end

#pragma mark - Application Delegate

@interface GLFMAppDelegate : NSObject <UIApplicationDelegate>

@property (strong, nonatomic) UIWindow *window;
@property (nonatomic) BOOL active;

@end

@implementation GLFMAppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    _active = YES;
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    self.window.rootViewController = [[GLFMViewController alloc] init];
    [self.window makeKeyAndVisible];
    return YES;
}

- (void)setActive:(BOOL)active
{
    if (_active != active) {
        _active = active;
        
        GLFMViewController *vc = (GLFMViewController *)[self.window rootViewController];
        if (vc.glfmDisplay != NULL) {
            if (_active && vc.glfmDisplay->resumingFunc != NULL) {
                vc.glfmDisplay->resumingFunc(vc.glfmDisplay);
            }
            else if (!_active && vc.glfmDisplay->pausingFunc != NULL) {
                vc.glfmDisplay->pausingFunc(vc.glfmDisplay);
            }
        }
    }
}

- (void)applicationWillResignActive:(UIApplication *)application
{
    self.active = NO;
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
    self.active = NO;
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
    self.active = YES;
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
    self.active = YES;
}

- (void)applicationWillTerminate:(UIApplication *)application
{
    self.active = NO;
}

@end

#pragma mark - Main

int main(int argc, char *argv[])
{
    @autoreleasepool {
        return UIApplicationMain(0, NULL, nil, NSStringFromClass([GLFMAppDelegate class]));
    }
}

#pragma mark - GLFM helper functions

CGSize getDisplaySize(GLFMDisplay *display)
{
    BOOL isPortrait = YES;
    if (display != NULL && display->platformData != NULL) {
        GLFMViewController *vc = (__bridge GLFMViewController*)display->platformData;
        GLKView *view = (GLKView*)vc.view;
        if (view.drawableWidth != 0 && view.drawableHeight != 0) {
            return CGSizeMake(view.drawableWidth, view.drawableHeight);
        }
        isPortrait = UIInterfaceOrientationIsPortrait(vc.interfaceOrientation);
    }
    CGFloat scale = [UIScreen mainScreen].scale;
    CGSize size = [UIScreen mainScreen].bounds.size;
    if (isPortrait) {
        return CGSizeMake(size.width * scale, size.height * scale);
    }
    else {
        return CGSizeMake(size.height * scale, size.width * scale);
    }
}

#pragma mark - GLFM implementation

static const char *glfmGetAssetPath()
{
    static char *path = NULL;
    if (path == NULL) {
        path = strdup([NSBundle mainBundle].bundlePath.UTF8String);
    }
    return path;
}

void glfmSetUserInterfaceOrientation(GLFMDisplay *display, const GLFMUserInterfaceOrientation allowedOrientations)
{
    if (display != NULL) {
        if (display->allowedOrientations != allowedOrientations) {
            display->allowedOrientations = allowedOrientations;
            
            // HACK: Notify that the value of supportedInterfaceOrientations has changed
            GLFMViewController *vc = (__bridge GLFMViewController*)display->platformData;
            UIViewController* dummyVC = [[UIViewController alloc] init];
            dummyVC.view = [[UIView alloc] init];
            [vc presentViewController:dummyVC animated:NO completion:^{
                [vc dismissViewControllerAnimated:NO completion:NULL];
            }];
        }
    }
}

int glfmGetDisplayWidth(GLFMDisplay *display)
{
    if (display != NULL && display->platformData != NULL) {
        GLFMViewController *vc = (__bridge GLFMViewController*)display->platformData;
        return vc.displaySize.width;
    }
    else {
        return 0;
    }
}

int glfmGetDisplayHeight(GLFMDisplay *display)
{
    if (display != NULL && display->platformData != NULL) {
        GLFMViewController *vc = (__bridge GLFMViewController*)display->platformData;
        return vc.displaySize.height;
    }
    else {
        return 0;
    }
}

float glfmGetDisplayScale(GLFMDisplay *display)
{
    return [UIScreen mainScreen].scale;
}

GLFMUserInterfaceIdiom glfmGetUserInterfaceIdiom(GLFMDisplay *display)
{
    if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPhone) {
        return GLFMUserInterfaceIdiomPhone;
    }
    else {
        return GLFMUserInterfaceIdiomTablet;
    }
}

void glfmSetMultitouchEnabled(GLFMDisplay *display, const GLboolean multitouchEnabled)
{
    if (display != NULL) {
        GLFMViewController *vc = (__bridge GLFMViewController*)display->platformData;
        vc.multipleTouchEnabled = multitouchEnabled;
        if (vc.glkViewCreated) {
            GLKView *view = (GLKView *)vc.view;
            view.multipleTouchEnabled = multitouchEnabled;
        }
    }
}

GLboolean glfmGetMultitouchEnabled(GLFMDisplay *display)
{
    if (display != NULL) {
        GLFMViewController *vc = (__bridge GLFMViewController*)display->platformData;
        return vc.multipleTouchEnabled;
    }
    else {
        return 0;
    }
}

void glfmLog(const GLFMLogLevel logLevel, const char *format, ...)
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        // NOTE: This method requires Mac OS X 10.9 or iOS 7.
        // Older platforms would use asl_add_log_file(NULL, STDERR_FILENO) instead.
        // Would like to have milliseconds here too (like NSLog), but it doesn't appear to be possible.
        asl_add_output_file(NULL, STDERR_FILENO,
                            "$((Time)(J)) $(Sender)[$(PID)] $((Level)(str)): $Message",
                            ASL_TIME_FMT_UTC, ASL_FILTER_MASK_UPTO(ASL_LEVEL_DEBUG), ASL_ENCODE_SAFE);
    });
    
    int level;
    switch (logLevel) {
        case GLFMLogLevelDebug:
            level = ASL_LEVEL_DEBUG;
            break;
        case GLFMLogLevelInfo: default:
            level = ASL_LEVEL_INFO;
            break;
        case GLFMLogLevelWarning:
            level = ASL_LEVEL_WARNING;
            break;
        case GLFMLogLevelError:
            level = ASL_LEVEL_ERR;
            break;
        case GLFMLogLevelCritical:
            level = ASL_LEVEL_CRIT;
            break;
    }
    
    va_list args;
    va_start(args, format);
    asl_vlog(NULL, NULL, level, format, args);
    va_end(args);
}

#endif