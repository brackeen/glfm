// GLFM
// https://github.com/brackeen/glfm

#if defined(__APPLE__)

#include "glfm.h"

#if !defined(GLFM_INCLUDE_METAL)
#  define GLFM_INCLUDE_METAL 1
#endif

#if TARGET_OS_IOS || TARGET_OS_TV
#  import <UIKit/UIKit.h>
#elif TARGET_OS_OSX
#  import <AppKit/AppKit.h>
#  define UIApplication NSApplication
#  define UIApplicationDelegate NSApplicationDelegate
#  define UIView NSView
#  define UIViewAutoresizingFlexibleWidth NSViewWidthSizable
#  define UIViewAutoresizingFlexibleHeight NSViewHeightSizable
#  define UIViewController NSViewController
#  define UIWindow NSWindow
#endif
#if TARGET_OS_IOS
#  import <CoreHaptics/CoreHaptics.h>
#  import <CoreMotion/CoreMotion.h>
#endif
#if GLFM_INCLUDE_METAL
#  import <MetalKit/MetalKit.h>
#endif

#include <dlfcn.h>
#include "glfm_platform.h"

#define MAX_SIMULTANEOUS_TOUCHES 10

#ifdef NDEBUG
#  define CHECK_GL_ERROR() ((void)0)
#else
#  define CHECK_GL_ERROR() do { GLenum error = glGetError(); if (error != GL_NO_ERROR) \
   NSLog(@"OpenGL error 0x%04x at glfm_platform_apple.m:%i", error, __LINE__); } while(0)
#endif

#if __has_feature(objc_arc)
#  define GLFM_AUTORELEASE(value) value
#  define GLFM_RELEASE(value) ((void)0)
#  define GLFM_WEAK __weak
#else
#  define GLFM_AUTORELEASE(value) [value autorelease]
#  define GLFM_RELEASE(value) [value release]
#  define GLFM_WEAK __unsafe_unretained
#endif

static bool glfm__isCGFloatEqual(CGFloat a, CGFloat b) {
#if CGFLOAT_IS_DOUBLE
    return fabs(a - b) <= DBL_EPSILON;
#else
    return fabsf(a - b) <= FLT_EPSILON;
#endif
}

static void glfm__getDefaultDisplaySize(const GLFMDisplay *display,
                                        double *width, double *height, double *scale);
static void glfm__getDrawableSize(double displayWidth, double displayHeight, double displayScale,
                                  int *width, int *height);

// MARK: - GLFMView protocol

@protocol GLFMView

@property(nonatomic, readonly) GLFMRenderingAPI renderingAPI;
@property(nonatomic, readonly) int drawableWidth;
@property(nonatomic, readonly) int drawableHeight;
@property(nonatomic, readonly) BOOL surfaceCreatedNotified;
@property(nonatomic, assign) BOOL animating;
@property(nonatomic, copy, nullable) void (^preRenderCallback)(void);

- (void)draw;
- (void)swapBuffers;
- (void)requestRefresh;

@end

// MARK: GLFMNullView

@interface GLFMNullView : UIView <GLFMView>

@end

@implementation GLFMNullView

@synthesize preRenderCallback = _preRenderCallback;

- (GLFMRenderingAPI)renderingAPI {
    return GLFMRenderingAPIOpenGLES2;
}

- (int)drawableWidth {
    return 0;
}

- (int)drawableHeight {
    return 0;
}

- (BOOL)animating {
    return NO;
}

- (BOOL)surfaceCreatedNotified {
    return NO;
}

- (void)setAnimating:(BOOL)animating {
    (void)animating;
}

- (void)draw {
    if (_preRenderCallback) {
        _preRenderCallback();
    }
}

- (void)swapBuffers {
    
}

- (void)requestRefresh {
    
}

- (void)dealloc {
    GLFM_RELEASE(_preRenderCallback);
#if !__has_feature(objc_arc)
    [super dealloc];
#endif
}

@end

#if GLFM_INCLUDE_METAL

// MARK: - GLFMMetalView

@interface GLFMMetalView : MTKView <GLFMView, MTKViewDelegate>

@property(nonatomic, assign) GLFMDisplay *glfmDisplay;
@property(nonatomic, assign) int drawableWidth;
@property(nonatomic, assign) int drawableHeight;
@property(nonatomic, assign) BOOL surfaceCreatedNotified;
@property(nonatomic, assign) BOOL refreshRequested;

@end

@implementation GLFMMetalView

@synthesize drawableWidth, drawableHeight, surfaceCreatedNotified, refreshRequested;
@synthesize glfmDisplay = _glfmDisplay, preRenderCallback = _preRenderCallback;
@dynamic renderingAPI, animating;

- (instancetype)initWithFrame:(CGRect)frame contentScaleFactor:(CGFloat)contentScaleFactor
                       device:(id<MTLDevice>)device glfmDisplay:(GLFMDisplay *)glfmDisplay {
    if ((self = [super initWithFrame:frame device:device])) {
        self.contentScaleFactor = contentScaleFactor;
        self.delegate = self;
        self.glfmDisplay = glfmDisplay;
        self.drawableWidth = (int)self.drawableSize.width;
        self.drawableHeight = (int)self.drawableSize.height;
        [self requestRefresh];

        switch (glfmDisplay->colorFormat) {
            case GLFMColorFormatRGB565:
                if (@available(iOS 8, tvOS 8, macOS 11, *)) {
                    self.colorPixelFormat = MTLPixelFormatB5G6R5Unorm;
                } else {
                    self.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
                }
                break;
            case GLFMColorFormatRGBA8888:
            default:
                self.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
                break;
        }
        
        if (glfmDisplay->depthFormat == GLFMDepthFormatNone &&
            glfmDisplay->stencilFormat == GLFMStencilFormatNone) {
            self.depthStencilPixelFormat = MTLPixelFormatInvalid;
        } else if (glfmDisplay->depthFormat == GLFMDepthFormatNone) {
            self.depthStencilPixelFormat = MTLPixelFormatStencil8;
        } else if (glfmDisplay->stencilFormat == GLFMStencilFormatNone) {
            if (@available(iOS 13, tvOS 13, macOS 10.12, *)) {
                if (glfmDisplay->depthFormat == GLFMDepthFormat16) {
                    self.depthStencilPixelFormat = MTLPixelFormatDepth16Unorm;
                } else {
                    self.depthStencilPixelFormat = MTLPixelFormatDepth32Float;
                }
            } else {
                self.depthStencilPixelFormat = MTLPixelFormatDepth32Float;
            }
            
        } else {
            self.depthStencilPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
        }
        
        self.sampleCount = (glfmDisplay->multisample == GLFMMultisampleNone) ? 1 : 4;
    }
    return self;
}

- (GLFMRenderingAPI)renderingAPI {
    return GLFMRenderingAPIMetal;
}

#if TARGET_OS_OSX

- (void)setContentScaleFactor:(CGFloat)contentScaleFactor {
    self.layer.contentsScale = contentScaleFactor;
    CAMetalLayer *metalLayer = (CAMetalLayer *)self.layer;
    CGSize drawableSize = metalLayer.drawableSize;
    drawableSize.width = self.layer.bounds.size.width * self.layer.contentsScale;
    drawableSize.height = self.layer.bounds.size.height * self.layer.contentsScale;
    metalLayer.drawableSize = drawableSize;
}

#endif // TARGET_OS_OSX

- (BOOL)animating {
    return !self.paused;
}

- (void)setAnimating:(BOOL)animating {
    if (self.animating != animating) {
        self.paused = !animating;
        [self requestRefresh];
    }
}

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
    
}

- (void)drawInMTKView:(MTKView *)view {
    int newDrawableWidth = (int)self.drawableSize.width;
    int newDrawableHeight = (int)self.drawableSize.height;
    if (!self.surfaceCreatedNotified) {
        self.surfaceCreatedNotified = YES;
        [self requestRefresh];

        self.drawableWidth = newDrawableWidth;
        self.drawableHeight = newDrawableHeight;
        if (self.glfmDisplay->surfaceCreatedFunc) {
            self.glfmDisplay->surfaceCreatedFunc(self.glfmDisplay,
                                                 self.drawableWidth, self.drawableHeight);
        }
    } else if (newDrawableWidth != self.drawableWidth || newDrawableHeight != self.drawableHeight) {
        [self requestRefresh];
        self.drawableWidth = newDrawableWidth;
        self.drawableHeight = newDrawableHeight;
        if (self.glfmDisplay->surfaceResizedFunc) {
            self.glfmDisplay->surfaceResizedFunc(self.glfmDisplay,
                                                 self.drawableWidth, self.drawableHeight);
        }
    }
    
    if (_preRenderCallback) {
        _preRenderCallback();
    }
    
    if (self.refreshRequested) {
        self.refreshRequested = NO;
        if (self.glfmDisplay->surfaceRefreshFunc) {
            self.glfmDisplay->surfaceRefreshFunc(self.glfmDisplay);
        }
    }
    
    if (self.glfmDisplay->renderFunc) {
        self.glfmDisplay->renderFunc(self.glfmDisplay);
    }
}

- (void)swapBuffers {
    // Do nothing
}

- (void)requestRefresh {
    self.refreshRequested = YES;
}

- (void)layoutSubviews {
    // First render as soon as safeAreaInsets are set
    if (!self.surfaceCreatedNotified) {
        [self requestRefresh];
        [self draw];
    }
}

- (void)dealloc {
    GLFM_RELEASE(_preRenderCallback);
#if !__has_feature(objc_arc)
    [super dealloc];
#endif
}

@end

#endif // GLFM_INCLUDE_METAL

#if TARGET_OS_IOS || TARGET_OS_TV

// MARK: - GLFMOpenGLESView (iOS, tvOS)

@interface GLFMOpenGLESView : UIView <GLFMView>

@property(nonatomic, assign) GLFMDisplay *glfmDisplay;
@property(nonatomic, assign) GLFMRenderingAPI renderingAPI;
@property(nonatomic, strong) CADisplayLink *displayLink;
@property(nonatomic, strong) EAGLContext *context;
@property(nonatomic, strong) NSString *colorFormat;
@property(nonatomic, assign) BOOL preserveBackbuffer;
@property(nonatomic, assign) NSUInteger depthBits;
@property(nonatomic, assign) NSUInteger stencilBits;
@property(nonatomic, assign) BOOL multisampling;
@property(nonatomic, assign) BOOL surfaceCreatedNotified;
@property(nonatomic, assign) BOOL surfaceSizeChanged;
@property(nonatomic, assign) BOOL refreshRequested;

@end

@implementation GLFMOpenGLESView {
    GLint _drawableWidth;
    GLint _drawableHeight;
    GLuint _defaultFramebuffer;
    GLuint _colorRenderbuffer;
    GLuint _attachmentRenderbuffer;
    GLuint _msaaFramebuffer;
    GLuint _msaaRenderbuffer;
}

@synthesize renderingAPI, displayLink, context, colorFormat, preserveBackbuffer;
@synthesize depthBits, stencilBits, multisampling;
@synthesize surfaceCreatedNotified, surfaceSizeChanged, refreshRequested;
@synthesize glfmDisplay = _glfmDisplay, preRenderCallback = _preRenderCallback;
@dynamic drawableWidth, drawableHeight, animating;

+ (Class)layerClass {
    return [CAEAGLLayer class];
}

- (instancetype)initWithFrame:(CGRect)frame contentScaleFactor:(CGFloat)contentScaleFactor
                  glfmDisplay:(GLFMDisplay *)glfmDisplay {
    if ((self = [super initWithFrame:frame])) {
        
        self.contentScaleFactor = contentScaleFactor;
        self.glfmDisplay = glfmDisplay;
        [self requestRefresh];
        
        if (glfmDisplay->preferredAPI >= GLFMRenderingAPIOpenGLES3) {
            self.context = GLFM_AUTORELEASE([[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3]);
            self.renderingAPI = GLFMRenderingAPIOpenGLES3;
        }
        if (!self.context) {
            self.context = GLFM_AUTORELEASE([[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2]);
            self.renderingAPI = GLFMRenderingAPIOpenGLES2;
        }
        
        if (!self.context) {
            glfm__reportSurfaceError(glfmDisplay, "Failed to create ES context");
            GLFM_RELEASE(self);
            return nil;
        }
        
        switch (glfmDisplay->colorFormat) {
            case GLFMColorFormatRGB565:
                self.colorFormat = kEAGLColorFormatRGB565;
                break;
            case GLFMColorFormatRGBA8888:
            default:
                self.colorFormat = kEAGLColorFormatRGBA8;
                break;
        }
        
        switch (glfmDisplay->depthFormat) {
            case GLFMDepthFormatNone:
            default:
                self.depthBits = 0;
                break;
            case GLFMDepthFormat16:
                self.depthBits = 16;
                break;
            case GLFMDepthFormat24:
                self.depthBits = 24;
                break;
        }
        
        switch (glfmDisplay->stencilFormat) {
            case GLFMStencilFormatNone:
            default:
                self.stencilBits = 0;
                break;
            case GLFMStencilFormat8:
                self.stencilBits = 8;
                break;
        }
        
        self.multisampling = glfmDisplay->multisample != GLFMMultisampleNone;
        
        [self createDrawable];
    }
    return self;
}

- (void)dealloc {
    self.animating = NO;
    [self deleteDrawable];
    if ([EAGLContext currentContext] == self.context) {
        [EAGLContext setCurrentContext:nil];
    }
    self.context = nil;
    self.colorFormat = nil;
    GLFM_RELEASE(_preRenderCallback);
#if !__has_feature(objc_arc)
    [super dealloc];
#endif
}

- (int)drawableWidth {
    return (int)_drawableWidth;
}

- (int)drawableHeight {
    return (int)_drawableHeight;
}

- (BOOL)animating {
    return (self.displayLink != nil);
}

- (void)setAnimating:(BOOL)animating {
    if (self.animating != animating) {
        [self requestRefresh];
        if (!animating) {
            [self.displayLink invalidate];
            self.displayLink = nil;
        } else {
            self.displayLink = [CADisplayLink displayLinkWithTarget:self
                                                           selector:@selector(render:)];
            [self.displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
        }
    }
}

- (void)createDrawable {
    if (_defaultFramebuffer != 0 || !self.context) {
        return;
    }

    if (!self.colorFormat) {
        self.colorFormat = kEAGLColorFormatRGBA8;
    }

    [EAGLContext setCurrentContext:self.context];

    CAEAGLLayer *eaglLayer = (CAEAGLLayer *)self.layer;
    eaglLayer.opaque = YES;
    eaglLayer.drawableProperties =
        @{ kEAGLDrawablePropertyRetainedBacking : @(self.preserveBackbuffer),
           kEAGLDrawablePropertyColorFormat : self.colorFormat };

    glGenFramebuffers(1, &_defaultFramebuffer);
    glGenRenderbuffers(1, &_colorRenderbuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, _defaultFramebuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _colorRenderbuffer);

    // iPhone 6 Display Zoom hack - use a modified bounds so that the renderbufferStorage method
    // creates the correct size renderbuffer.
    CGRect oldBounds = eaglLayer.bounds;
    if (glfm__isCGFloatEqual(eaglLayer.contentsScale, (CGFloat)2.343750)) {
        if (glfm__isCGFloatEqual(eaglLayer.bounds.size.width, (CGFloat)320.0) &&
            glfm__isCGFloatEqual(eaglLayer.bounds.size.height, (CGFloat)568.0)) {
            eaglLayer.bounds = CGRectMake(eaglLayer.bounds.origin.x, eaglLayer.bounds.origin.y,
                                          eaglLayer.bounds.size.width,
                                          1334 / eaglLayer.contentsScale);
        } else if (glfm__isCGFloatEqual(eaglLayer.bounds.size.width, (CGFloat)568.0) &&
                   glfm__isCGFloatEqual(eaglLayer.bounds.size.height, (CGFloat)320.0)) {
            eaglLayer.bounds = CGRectMake(eaglLayer.bounds.origin.x, eaglLayer.bounds.origin.y,
                                          1334 / eaglLayer.contentsScale,
                                          eaglLayer.bounds.size.height);
        }
    }

    if (![self.context renderbufferStorage:GL_RENDERBUFFER fromDrawable:eaglLayer]) {
        NSLog(@"Error: Call to renderbufferStorage failed");
    }

    eaglLayer.bounds = oldBounds;

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                              _colorRenderbuffer);

    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &_drawableWidth);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &_drawableHeight);

    if (self.multisampling) {
        glGenFramebuffers(1, &_msaaFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, _msaaFramebuffer);

        glGenRenderbuffers(1, &_msaaRenderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, _msaaRenderbuffer);

        GLenum internalFormat = GL_RGBA8_OES;
        if ([kEAGLColorFormatRGB565 isEqualToString:self.colorFormat]) {
            internalFormat = GL_RGB565;
        }

        glRenderbufferStorageMultisampleAPPLE(GL_RENDERBUFFER, 4, internalFormat,
                                              _drawableWidth, _drawableHeight);

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                  _msaaRenderbuffer);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            NSLog(@"Error: Couldn't create multisample framebuffer: 0x%04x", status);
        }
    }

    if (self.depthBits > 0 || self.stencilBits > 0) {
        glGenRenderbuffers(1, &_attachmentRenderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, _attachmentRenderbuffer);

        GLenum internalFormat;
        if (self.depthBits > 0 && self.stencilBits > 0) {
            internalFormat = GL_DEPTH24_STENCIL8_OES;
        } else if (self.depthBits >= 24) {
            internalFormat = GL_DEPTH_COMPONENT24_OES;
        } else if (self.depthBits > 0) {
            internalFormat = GL_DEPTH_COMPONENT16;
        } else {
            internalFormat = GL_STENCIL_INDEX8;
        }

        if (self.multisampling) {
            glRenderbufferStorageMultisampleAPPLE(GL_RENDERBUFFER, 4, internalFormat,
                                                  _drawableWidth, _drawableHeight);
        } else {
            glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, _drawableWidth, _drawableHeight);
        }

        if (self.depthBits > 0) {
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                      _attachmentRenderbuffer);
        }
        if (self.stencilBits > 0) {
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                      _attachmentRenderbuffer);
        }
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        NSLog(@"Error: Framebuffer incomplete: 0x%04x", status);
    }

    CHECK_GL_ERROR();
}

- (void)deleteDrawable {
    if (_defaultFramebuffer) {
        glDeleteFramebuffers(1, &_defaultFramebuffer);
        _defaultFramebuffer = 0;
    }
    if (_colorRenderbuffer) {
        glDeleteRenderbuffers(1, &_colorRenderbuffer);
        _colorRenderbuffer = 0;
    }
    if (_attachmentRenderbuffer) {
        glDeleteRenderbuffers(1, &_attachmentRenderbuffer);
        _attachmentRenderbuffer = 0;
    }
    if (_msaaRenderbuffer) {
        glDeleteRenderbuffers(1, &_msaaRenderbuffer);
        _msaaRenderbuffer = 0;
    }
    if (_msaaFramebuffer) {
        glDeleteFramebuffers(1, &_msaaFramebuffer);
        _msaaFramebuffer = 0;
    }
}

- (void)prepareRender {
    [EAGLContext setCurrentContext:self.context];
    if (self.multisampling) {
        glBindFramebuffer(GL_FRAMEBUFFER, _msaaFramebuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, _msaaRenderbuffer);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, _defaultFramebuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, _colorRenderbuffer);
    }
    CHECK_GL_ERROR();
}

- (void)finishRender {
    if (self.multisampling) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER_APPLE, _msaaFramebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER_APPLE, _defaultFramebuffer);
        glResolveMultisampleFramebufferAPPLE();
    }

    static bool checked_GL_EXT_discard_framebuffer = false;
    static bool has_GL_EXT_discard_framebuffer = false;
    if (!checked_GL_EXT_discard_framebuffer) {
        checked_GL_EXT_discard_framebuffer = true;
        const char *extensions = (const char *)glGetString(GL_EXTENSIONS);
        if (extensions) {
            has_GL_EXT_discard_framebuffer = strstr(extensions, "GL_EXT_discard_framebuffer") != NULL;
        }
    }
    if (has_GL_EXT_discard_framebuffer) {
        GLenum target = GL_FRAMEBUFFER;
        GLenum attachments[3];
        GLsizei numAttachments = 0;
        if (self.multisampling) {
            target = GL_READ_FRAMEBUFFER_APPLE;
            attachments[numAttachments++] = GL_COLOR_ATTACHMENT0;
        }
        if (self.depthBits > 0) {
            attachments[numAttachments++] = GL_DEPTH_ATTACHMENT;
        }
        if (self.stencilBits > 0) {
            attachments[numAttachments++] = GL_STENCIL_ATTACHMENT;
        }
        if (numAttachments > 0) {
            if (self.multisampling) {
                glBindFramebuffer(GL_FRAMEBUFFER, _msaaFramebuffer);
            } else {
                glBindFramebuffer(GL_FRAMEBUFFER, _defaultFramebuffer);
            }
            glDiscardFramebufferEXT(target, numAttachments, attachments);
        }
    }

    glBindRenderbuffer(GL_RENDERBUFFER, _colorRenderbuffer);
    [self.context presentRenderbuffer:GL_RENDERBUFFER];

    CHECK_GL_ERROR();
}

- (void)render:(CADisplayLink *)displayLink {
    if (!self.surfaceCreatedNotified) {
        self.surfaceCreatedNotified = YES;
        [self requestRefresh];

        if (self.glfmDisplay->surfaceCreatedFunc) {
            self.glfmDisplay->surfaceCreatedFunc(self.glfmDisplay,
                                                 self.drawableWidth, self.drawableHeight);
        }
    }
    
    if (self.surfaceSizeChanged) {
        self.surfaceSizeChanged  = NO;
        [self requestRefresh];
        if (self.glfmDisplay->surfaceResizedFunc) {
            self.glfmDisplay->surfaceResizedFunc(self.glfmDisplay,
                                                 self.drawableWidth, self.drawableHeight);
        }
    }
    
    if (_preRenderCallback) {
        _preRenderCallback();
    }
    
    [self prepareRender];
    if (self.refreshRequested) {
        self.refreshRequested = NO;
        if (self.glfmDisplay->surfaceRefreshFunc) {
            self.glfmDisplay->surfaceRefreshFunc(self.glfmDisplay);
        }
    }
    if (self.glfmDisplay->renderFunc) {
        self.glfmDisplay->renderFunc(self.glfmDisplay);
    }
}

- (void)swapBuffers {
    [self finishRender];
}

- (void)draw {
    if (self.displayLink) {
        [self render:self.displayLink];
    }
}

- (void)requestRefresh {
    self.refreshRequested = YES;
}

- (void)layoutSubviews {
    int newDrawableWidth;
    int newDrawableHeight;
    glfm__getDrawableSize((double)self.bounds.size.width, (double)self.bounds.size.height,
                          (double)self.contentScaleFactor, &newDrawableWidth, &newDrawableHeight);

    if (self.drawableWidth != newDrawableWidth || self.drawableHeight != newDrawableHeight) {
        [self deleteDrawable];
        [self createDrawable];
        self.surfaceSizeChanged = self.surfaceCreatedNotified;
    }
    
    // First render as soon as safeAreaInsets are set
    if (!self.surfaceCreatedNotified) {
        [self requestRefresh];
        [self draw];
    }
}

@end

#endif // TARGET_OS_IOS || TARGET_OS_TV

// MARK: - GLFMAppDelegate interface

@interface GLFMAppDelegate : NSObject <UIApplicationDelegate>

@property(nonatomic, strong) UIWindow *window;
@property(nonatomic, assign) BOOL active;

@end

// MARK: - GLFMViewController

@interface GLFMViewController : UIViewController

@property(nonatomic, assign) GLFMDisplay *glfmDisplay;
#if GLFM_INCLUDE_METAL
@property(nonatomic, strong) id<MTLDevice> metalDevice;
#endif

@end

#if TARGET_OS_IOS || TARGET_OS_TV

@interface GLFMViewController () <UIKeyInput, UITextInputTraits>

@property(nonatomic, assign) BOOL multipleTouchEnabled;
@property(nonatomic, assign) BOOL keyboardRequested;
@property(nonatomic, assign) BOOL keyboardVisible;
#if TARGET_OS_IOS
@property(nonatomic, strong) CMMotionManager *motionManager;
@property(nonatomic, assign) UIInterfaceOrientation orientation;
#endif

@end

#endif // TARGET_OS_IOS || TARGET_OS_TV

@implementation GLFMViewController {
    const void *activeTouches[MAX_SIMULTANEOUS_TOUCHES];
}

@synthesize glfmDisplay;

#if GLFM_INCLUDE_METAL
@synthesize metalDevice = _metalDevice;
#endif
#if TARGET_OS_IOS || TARGET_OS_TV
@synthesize multipleTouchEnabled, keyboardRequested, keyboardVisible;
#endif
#if TARGET_OS_IOS
@synthesize motionManager = _motionManager, orientation;
#endif

- (id)init {
    if ((self = [super init])) {
        [self clearTouches];
        self.glfmDisplay = calloc(1, sizeof(GLFMDisplay));
        self.glfmDisplay->platformData = (__bridge void *)self;
        self.glfmDisplay->supportedOrientations = GLFMInterfaceOrientationAll;
    }
    return self;
}

#if GLFM_INCLUDE_METAL

- (id<MTLDevice>)metalDevice {
    if (!_metalDevice) {
        self.metalDevice = GLFM_AUTORELEASE(MTLCreateSystemDefaultDevice());
    }
    return _metalDevice;
}

#endif

#if TARGET_OS_IOS

- (BOOL)prefersStatusBarHidden {
    return self.glfmDisplay->uiChrome != GLFMUserInterfaceChromeNavigationAndStatusBar;
}

- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures {
    UIRectEdge edges =  UIRectEdgeLeft | UIRectEdgeRight;
    return (self.glfmDisplay->uiChrome == GLFMUserInterfaceChromeFullscreen ?
            (UIRectEdgeBottom | edges) : edges);
}

#endif

- (UIView<GLFMView> *)glfmView {
    return (UIView<GLFMView> *)self.view;
}

- (UIView<GLFMView> *)glfmViewIfLoaded {
    if (self.isViewLoaded) {
        return (UIView<GLFMView> *)self.view;
    } else {
        return nil;
    }
}

- (void)loadView {
    glfmMain(self.glfmDisplay);

    GLFMAppDelegate *delegate = UIApplication.sharedApplication.delegate;
#if TARGET_OS_IOS || TARGET_OS_TV
    CGRect frame = delegate.window.bounds;
    CGFloat scale = [UIScreen mainScreen].nativeScale;
#else
    CGRect frame = [delegate.window contentRectForFrameRect:delegate.window.frame];
    CGFloat scale = delegate.window.backingScaleFactor;
#endif
    
    UIView<GLFMView> *glfmView = nil;
    
#if GLFM_INCLUDE_METAL
    if (self.glfmDisplay->preferredAPI == GLFMRenderingAPIMetal && self.metalDevice) {
        glfmView = GLFM_AUTORELEASE([[GLFMMetalView alloc] initWithFrame:frame
                                                      contentScaleFactor:scale
                                                                  device:self.metalDevice
                                                             glfmDisplay:self.glfmDisplay]);
    }
#endif
#if TARGET_OS_IOS || TARGET_OS_TV
    if (!glfmView) {
        glfmView = GLFM_AUTORELEASE([[GLFMOpenGLESView alloc] initWithFrame:frame
                                                         contentScaleFactor:scale
                                                                glfmDisplay:self.glfmDisplay]);
    }
#endif
    if (!glfmView) {
        assert(glfmView != nil);
        glfmView = GLFM_AUTORELEASE([[GLFMNullView alloc] initWithFrame:frame]);
    }
    GLFM_WEAK __typeof(self) weakSelf = self;
    glfmView.preRenderCallback = ^{
        [weakSelf preRenderCallback];
    };
    self.view = glfmView;
    self.view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
}

- (void)viewDidLoad {
    [super viewDidLoad];

    GLFMAppDelegate *delegate = UIApplication.sharedApplication.delegate;
    self.glfmViewIfLoaded.animating = delegate.active;
    
#if TARGET_OS_IOS
    self.view.multipleTouchEnabled = self.multipleTouchEnabled;
    self.orientation = [[UIApplication sharedApplication] statusBarOrientation];

    [self setNeedsStatusBarAppearanceUpdate];

    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(keyboardFrameChanged:)
                                               name:UIKeyboardWillChangeFrameNotification
                                             object:self.view.window];
    
    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(deviceOrientationChanged:)
                                               name:UIDeviceOrientationDidChangeNotification
                                             object:self.view.window];
#endif
}

#if TARGET_OS_IOS

- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
    GLFMInterfaceOrientation orientations = self.glfmDisplay->supportedOrientations;
    BOOL portraitRequested = (orientations & (GLFMInterfaceOrientationPortrait | GLFMInterfaceOrientationPortraitUpsideDown)) != 0;
    BOOL landscapeRequested = (orientations & GLFMInterfaceOrientationLandscape) != 0;
    BOOL isTablet = [[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPad;
    if (portraitRequested && landscapeRequested) {
        if (isTablet) {
            return UIInterfaceOrientationMaskAll;
        } else {
            return UIInterfaceOrientationMaskAllButUpsideDown;
        }
    } else if (landscapeRequested) {
        return UIInterfaceOrientationMaskLandscape;
    } else {
        if (isTablet) {
            return (UIInterfaceOrientationMask)(UIInterfaceOrientationMaskPortrait |
                    UIInterfaceOrientationMaskPortraitUpsideDown);
        } else {
            return UIInterfaceOrientationMaskPortrait;
        }
    }
}

- (void)deviceOrientationChanged:(NSNotification *)notification {
    UIInterfaceOrientation newOrientation = [[UIApplication sharedApplication] statusBarOrientation];
    if (self.orientation != newOrientation) {
        self.orientation = newOrientation;
        [self.glfmViewIfLoaded requestRefresh];
        if (self.glfmDisplay->orientationChangedFunc) {
            self.glfmDisplay->orientationChangedFunc(self.glfmDisplay,
                                                     glfmGetInterfaceOrientation(self.glfmDisplay));
        }
    }
}

#endif // TARGET_OS_IOS

- (void)preRenderCallback {
#if TARGET_OS_IOS
    [self handleMotionEvents];
#endif
}

#if TARGET_OS_IOS

- (CMMotionManager *)motionManager {
    if (!_motionManager) {
        self.motionManager = GLFM_AUTORELEASE([CMMotionManager new]);
        self.motionManager.deviceMotionUpdateInterval = 0.01;
    }
    return _motionManager;
}

- (BOOL)isMotionManagerLoaded {
    return _motionManager != nil;
}

- (void)handleMotionEvents {
    if (!self.isMotionManagerLoaded || !self.motionManager.isDeviceMotionActive) {
        return;
    }
    CMDeviceMotion *deviceMotion = self.motionManager.deviceMotion;
    if (!deviceMotion) {
        // No readings yet
        return;
    }
    GLFMSensorFunc accelerometerFunc = self.glfmDisplay->sensorFuncs[GLFMSensorAccelerometer];
    if (accelerometerFunc) {
        GLFMSensorEvent event = { 0 };
        event.sensor = GLFMSensorAccelerometer;
        event.timestamp = deviceMotion.timestamp;
        event.vector.x = deviceMotion.userAcceleration.x + deviceMotion.gravity.x;
        event.vector.y = deviceMotion.userAcceleration.y + deviceMotion.gravity.y;
        event.vector.z = deviceMotion.userAcceleration.z + deviceMotion.gravity.z;
        accelerometerFunc(self.glfmDisplay, event);
    }
    
    GLFMSensorFunc magnetometerFunc = self.glfmDisplay->sensorFuncs[GLFMSensorMagnetometer];
    if (magnetometerFunc) {
        GLFMSensorEvent event = { 0 };
        event.sensor = GLFMSensorMagnetometer;
        event.timestamp = deviceMotion.timestamp;
        event.vector.x = deviceMotion.magneticField.field.x;
        event.vector.y = deviceMotion.magneticField.field.y;
        event.vector.z = deviceMotion.magneticField.field.z;
        magnetometerFunc(self.glfmDisplay, event);
    }
    
    GLFMSensorFunc gyroscopeFunc = self.glfmDisplay->sensorFuncs[GLFMSensorGyroscope];
    if (gyroscopeFunc) {
        GLFMSensorEvent event = { 0 };
        event.sensor = GLFMSensorGyroscope;
        event.timestamp = deviceMotion.timestamp;
        event.vector.x = deviceMotion.rotationRate.x;
        event.vector.y = deviceMotion.rotationRate.y;
        event.vector.z = deviceMotion.rotationRate.z;
        gyroscopeFunc(self.glfmDisplay, event);
    }
    
    GLFMSensorFunc rotationFunc = self.glfmDisplay->sensorFuncs[GLFMSensorRotationMatrix];
    if (rotationFunc) {
        GLFMSensorEvent event = { 0 };
        event.sensor = GLFMSensorRotationMatrix;
        event.timestamp = deviceMotion.timestamp;
        CMRotationMatrix matrix = deviceMotion.attitude.rotationMatrix;
        event.matrix.m00 = matrix.m11; event.matrix.m01 = matrix.m12; event.matrix.m02 = matrix.m13;
        event.matrix.m10 = matrix.m21; event.matrix.m11 = matrix.m22; event.matrix.m12 = matrix.m23;
        event.matrix.m20 = matrix.m31; event.matrix.m21 = matrix.m32; event.matrix.m22 = matrix.m33;
        rotationFunc(self.glfmDisplay, event);
    }
}

- (void)updateMotionManagerActiveState {
    BOOL enable = NO;
    GLFMAppDelegate *delegate = UIApplication.sharedApplication.delegate;
    if (delegate.active) {
        for (int i = 0; i < GLFM_NUM_SENSORS; i++) {
            if (self.glfmDisplay->sensorFuncs[i] != NULL) {
                enable = YES;
                break;
            }
        }
    }
    
    if (enable && !self.motionManager.deviceMotionActive) {
        CMAttitudeReferenceFrame referenceFrame;
        CMAttitudeReferenceFrame availableReferenceFrames = [CMMotionManager availableAttitudeReferenceFrames];
        if (availableReferenceFrames & CMAttitudeReferenceFrameXMagneticNorthZVertical) {
            referenceFrame = CMAttitudeReferenceFrameXMagneticNorthZVertical;
        } else if (availableReferenceFrames & CMAttitudeReferenceFrameXArbitraryCorrectedZVertical) {
            referenceFrame = CMAttitudeReferenceFrameXArbitraryCorrectedZVertical;
        } else {
            referenceFrame = CMAttitudeReferenceFrameXArbitraryZVertical;
        }
        [self.motionManager startDeviceMotionUpdatesUsingReferenceFrame:referenceFrame];
    } else if (!enable && self.isMotionManagerLoaded && self.motionManager.deviceMotionActive) {
        [self.motionManager stopDeviceMotionUpdates];
    }
}

#endif // TARGET_OS_IOS

- (void)dealloc {
    if (self.glfmViewIfLoaded.surfaceCreatedNotified && self.glfmDisplay->surfaceDestroyedFunc) {
        self.glfmDisplay->surfaceDestroyedFunc(self.glfmDisplay);
    }
    free(self.glfmDisplay);
    self.glfmViewIfLoaded.preRenderCallback = nil;
#if TARGET_OS_IOS
    self.motionManager = nil;
#endif
#if GLFM_INCLUDE_METAL
    self.metalDevice = nil;
#endif
#if !__has_feature(objc_arc)
    [super dealloc];
#endif
}

- (void)clearTouches {
    for (int i = 0; i < MAX_SIMULTANEOUS_TOUCHES; i++) {
        activeTouches[i] = NULL;
    }
}

#if TARGET_OS_IOS || TARGET_OS_TV

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    if (self.glfmDisplay->lowMemoryFunc) {
        self.glfmDisplay->lowMemoryFunc(self.glfmDisplay);
    }
}

// MARK: UIResponder

- (void)addTouchEvent:(UITouch *)touch withType:(GLFMTouchPhase)phase {
    int firstNullIndex = -1;
    int index = -1;
    for (int i = 0; i < MAX_SIMULTANEOUS_TOUCHES; i++) {
        if (activeTouches[i] == (__bridge const void *)touch) {
            index = i;
            break;
        } else if (firstNullIndex == -1 && activeTouches[i] == NULL) {
            firstNullIndex = i;
        }
    }
    if (index == -1) {
        if (firstNullIndex == -1) {
            // Shouldn't happen
            return;
        }
        index = firstNullIndex;
        activeTouches[index] = (__bridge const void *)touch;
    }

    if (self.glfmDisplay->touchFunc) {
        CGPoint currLocation = [touch locationInView:self.view];
        currLocation.x *= self.view.contentScaleFactor;
        currLocation.y *= self.view.contentScaleFactor;

        self.glfmDisplay->touchFunc(self.glfmDisplay, index, phase,
                                    (double)currLocation.x, (double)currLocation.y);
    }

    if (phase == GLFMTouchPhaseEnded || phase == GLFMTouchPhaseCancelled) {
        activeTouches[index] = NULL;
    }
}

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event {
    for (UITouch *touch in touches) {
        [self addTouchEvent:touch withType:GLFMTouchPhaseBegan];
    }
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event {
    for (UITouch *touch in touches) {
        [self addTouchEvent:touch withType:GLFMTouchPhaseMoved];
    }
}

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)event {
    for (UITouch *touch in touches) {
        [self addTouchEvent:touch withType:GLFMTouchPhaseEnded];
    }
}

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)event {
    for (UITouch *touch in touches) {
        [self addTouchEvent:touch withType:GLFMTouchPhaseCancelled];
    }
}

#if TARGET_OS_TV

- (BOOL)handlePress:(UIPress *)press withAction:(GLFMKeyAction)action {
    if (self.glfmDisplay->keyFunc) {
        GLFMKey key = (GLFMKey)0;
        switch (press.type) {
            case UIPressTypeUpArrow:
                key = GLFMKeyUp;
                break;
            case UIPressTypeDownArrow:
                key = GLFMKeyDown;
                break;
            case UIPressTypeLeftArrow:
                key = GLFMKeyLeft;
                break;
            case UIPressTypeRightArrow:
                key = GLFMKeyRight;
                break;
            case UIPressTypeSelect:
                key = GLFMKeyNavSelect;
                break;
            case UIPressTypeMenu:
                key = GLFMKeyNavMenu;
                break;
            case UIPressTypePlayPause:
                key = GLFMKeyPlayPause;
                break;
            case UIPressTypePageUp:
                key = GLFMKeyPageUp;
                break;
            case UIPressTypePageDown:
                key = GLFMKeyPageDown;
                break;
        }
        if (key != 0) {
            return self.glfmDisplay->keyFunc(self.glfmDisplay, key, action, 0);
        } else {
            return NO;
        }
    } else {
        return NO;
    }
}

- (void)pressesBegan:(NSSet<UIPress *> *)presses withEvent:(UIPressesEvent *)event {
    BOOL handled = YES;
    for (UIPress *press in presses) {
        handled &= [self handlePress:press withAction:GLFMKeyActionPressed];
    }
    if (!handled) {
        [super pressesBegan:presses withEvent:event];
    }
}

- (void)pressesChanged:(NSSet<UIPress *> *)presses withEvent:(UIPressesEvent *)event {
    [super pressesChanged:presses withEvent:event];
}

- (void)pressesEnded:(NSSet<UIPress *> *)presses withEvent:(UIPressesEvent *)event {
    BOOL handled = YES;
    for (UIPress *press in presses) {
        handled &= [self handlePress:press withAction:GLFMKeyActionReleased];
    }
    if (!handled) {
        [super pressesEnded:presses withEvent:event];
    }
}

- (void)pressesCancelled:(NSSet<UIPress *> *)presses withEvent:(UIPressesEvent *)event {
    [super pressesCancelled:presses withEvent:event];
}

#endif // TARGET_OS_TV

// MARK: UIKeyInput

#if TARGET_OS_IOS

- (void)keyboardFrameChanged:(NSNotification *)notification {
    NSObject *value = notification.userInfo[UIKeyboardFrameEndUserInfoKey];
    if ([value isKindOfClass:[NSValue class]]) {
        NSValue *nsValue = (NSValue *)value;
        CGRect keyboardFrame = [nsValue CGRectValue];

        self.keyboardVisible = CGRectIntersectsRect(self.view.window.frame, keyboardFrame);
        if (!self.keyboardVisible) {
            // User hid keyboard (iPad)
            self.keyboardRequested = NO;
        }
        
        [self.glfmViewIfLoaded requestRefresh];

        if (self.glfmDisplay->keyboardVisibilityChangedFunc) {
            // Convert to view coordinates
            keyboardFrame = [self.view convertRect:keyboardFrame fromView:nil];

            // Convert to pixels
            keyboardFrame.origin.x *= self.view.contentScaleFactor;
            keyboardFrame.origin.y *= self.view.contentScaleFactor;
            keyboardFrame.size.width *= self.view.contentScaleFactor;
            keyboardFrame.size.height *= self.view.contentScaleFactor;

            self.glfmDisplay->keyboardVisibilityChangedFunc(self.glfmDisplay, self.keyboardVisible,
                                                            (double)keyboardFrame.origin.x,
                                                            (double)keyboardFrame.origin.y,
                                                            (double)keyboardFrame.size.width,
                                                            (double)keyboardFrame.size.height);
        }
    }
}

#endif // TARGET_OS_IOS

// UITextInputTraits - disable suggestion bar
- (UITextAutocorrectionType)autocorrectionType {
    return UITextAutocorrectionTypeNo;
}

- (BOOL)hasText {
    return YES;
}

- (void)insertText:(NSString *)text {
    if (self.glfmDisplay->charFunc) {
        self.glfmDisplay->charFunc(self.glfmDisplay, text.UTF8String, 0);
    }
}

- (void)deleteBackward {
    if (self.glfmDisplay->keyFunc) {
        self.glfmDisplay->keyFunc(self.glfmDisplay, GLFMKeyBackspace, GLFMKeyActionPressed, 0);
        self.glfmDisplay->keyFunc(self.glfmDisplay, GLFMKeyBackspace, GLFMKeyActionReleased, 0);
    }
}

- (BOOL)canBecomeFirstResponder {
    return self.keyboardRequested;
}

- (NSArray<UIKeyCommand *> *)keyCommands {
    static NSArray<UIKeyCommand *> *keyCommands = NULL;
    if (!keyCommands) {
        NSArray<NSString *> *keyInputs = @[
            UIKeyInputUpArrow, UIKeyInputDownArrow, UIKeyInputLeftArrow, UIKeyInputRightArrow,
            UIKeyInputEscape, UIKeyInputPageUp, UIKeyInputPageDown,
        ];
        if (@available(iOS 13.4, tvOS 13.4, *)) {
            keyInputs = [keyInputs arrayByAddingObjectsFromArray: @[
                UIKeyInputHome, UIKeyInputEnd,
            ]];
                           
        }
        NSMutableArray *mutableKeyCommands = GLFM_AUTORELEASE([NSMutableArray new]);
        [keyInputs enumerateObjectsUsingBlock:^(NSString *keyInput, NSUInteger idx, BOOL *stop) {
            (void)idx;
            (void)stop;
            [mutableKeyCommands addObject:[UIKeyCommand keyCommandWithInput:keyInput
                                                              modifierFlags:(UIKeyModifierFlags)0
                                                                     action:@selector(keyPressed:)]];
        }];
        keyCommands = [mutableKeyCommands copy];
    }

    return keyCommands;
}

- (void)keyPressed:(UIKeyCommand *)keyCommand {
    if (self.glfmDisplay->keyFunc) {
        NSString *key = [keyCommand input];
        GLFMKey keyCode = (GLFMKey)0;
        if (key == UIKeyInputUpArrow) {
            keyCode = GLFMKeyUp;
        } else if (key == UIKeyInputDownArrow) {
            keyCode = GLFMKeyDown;
        } else if (key == UIKeyInputLeftArrow) {
            keyCode = GLFMKeyLeft;
        } else if (key == UIKeyInputRightArrow) {
            keyCode = GLFMKeyRight;
        } else if (key == UIKeyInputEscape) {
            keyCode = GLFMKeyEscape;
        } else if (key == UIKeyInputPageUp) {
            keyCode = GLFMKeyPageUp;
        } else if (key == UIKeyInputPageDown) {
            keyCode = GLFMKeyPageDown;
        }
        
        if (@available(iOS 13.4, tvOS 13.4, *)) {
            if (key == UIKeyInputHome) {
                keyCode = GLFMKeyHome;
            } else if (key == UIKeyInputEnd) {
                keyCode = GLFMKeyEnd;
            }
        }

        if (keyCode != 0) {
            self.glfmDisplay->keyFunc(self.glfmDisplay, keyCode, GLFMKeyActionPressed, 0);
            self.glfmDisplay->keyFunc(self.glfmDisplay, keyCode, GLFMKeyActionReleased, 0);
        }
    }
}

#endif // TARGET_OS_IOS || TARGET_OS_TV

@end

#if TARGET_OS_IOS || TARGET_OS_TV

// MARK: - GLFMAppDelegate (iOS, tvOS)

@implementation GLFMAppDelegate

@synthesize window, active = _active;

- (BOOL)application:(UIApplication *)application
    didFinishLaunchingWithOptions:(NSDictionary<UIApplicationLaunchOptionsKey, id> *)launchOptions {
    _active = YES;
    self.window = GLFM_AUTORELEASE([[UIWindow alloc] init]);
    if (self.window.bounds.size.width <= (CGFloat)0.0 || self.window.bounds.size.height <= (CGFloat)0.0) {
        // Set UIWindow frame for iOS 8.
        // On iOS 9, the UIWindow frame may be different than the UIScreen bounds for iPad's
        // Split View or Slide Over.
        self.window.frame = [[UIScreen mainScreen] bounds];
    }
    self.window.rootViewController = GLFM_AUTORELEASE([[GLFMViewController alloc] init]);
    [self.window makeKeyAndVisible];
    return YES;
}

- (void)setActive:(BOOL)active {
    if (_active != active) {
        _active = active;

        GLFMViewController *vc = (GLFMViewController *)[self.window rootViewController];
        if (vc.glfmDisplay && vc.glfmDisplay->focusFunc) {
            vc.glfmDisplay->focusFunc(vc.glfmDisplay, _active);
        }
        if (vc.isViewLoaded) {
            if (!active) {
                // Draw once when entering the background so that a game can show "paused" state.
                [vc.glfmView requestRefresh];
                [vc.glfmView draw];
            }
            vc.glfmView.animating = active;
        }
#if TARGET_OS_IOS
        if (vc.isMotionManagerLoaded) {
            [vc updateMotionManagerActiveState];
        }
#endif
        [vc clearTouches];
    }
}

- (void)applicationWillResignActive:(UIApplication *)application {
    self.active = NO;
}

- (void)applicationDidEnterBackground:(UIApplication *)application {
    self.active = NO;
}

- (void)applicationWillEnterForeground:(UIApplication *)application {
    self.active = YES;
}

- (void)applicationDidBecomeActive:(UIApplication *)application {
    self.active = YES;
}

- (void)applicationWillTerminate:(UIApplication *)application {
    self.active = NO;
}

- (void)dealloc {
    self.window = nil;
#if !__has_feature(objc_arc)
    [super dealloc];
#endif
}

@end

#endif // TARGET_OS_IOS || TARGET_OS_TV

#if TARGET_OS_OSX

// MARK: - GLFMAppDelegate (macOS)

@interface GLFMAppDelegate () <NSWindowDelegate>

@end

@implementation GLFMAppDelegate

@synthesize window, active = _active;

- (void)createDefaultMenuWithAppName:(NSString *)appName {
    NSMenu *appMenu = GLFM_AUTORELEASE([NSMenu new]);
    NSApp.mainMenu = GLFM_AUTORELEASE([NSMenu new]);
    NSApp.servicesMenu = GLFM_AUTORELEASE([NSMenu new]);
    NSApp.windowsMenu = GLFM_AUTORELEASE([[NSMenu alloc] initWithTitle:NSLocalizedString(@"Window", nil)]);

    // App Menu
    [NSApp.mainMenu addItemWithTitle:@"" action:nil keyEquivalent:@""].submenu = appMenu;
    [appMenu addItemWithTitle:[NSString stringWithFormat:NSLocalizedString(@"About %@", nil), appName]
                       action:@selector(orderFrontStandardAboutPanel:)
                keyEquivalent:@""];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItemWithTitle:NSLocalizedString(@"Services", nil)
                       action:nil
                keyEquivalent:@""].submenu = NSApp.servicesMenu;
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItemWithTitle:[NSString stringWithFormat:NSLocalizedString(@"Hide %@", nil), appName]
                       action:@selector(hide:)
                keyEquivalent:@"h"];
    [appMenu addItemWithTitle:NSLocalizedString(@"Hide Others", nil)
                       action:@selector(hideOtherApplications:)
                keyEquivalent:@"h"].keyEquivalentModifierMask = NSEventModifierFlagOption | NSEventModifierFlagCommand;
    [appMenu addItemWithTitle:NSLocalizedString(@"Show All", nil)
                       action:@selector(unhideAllApplications:)
                keyEquivalent:@""];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItemWithTitle:[NSString stringWithFormat:NSLocalizedString(@"Quit %@", nil), appName]
                       action:@selector(terminate:)
                keyEquivalent:@"q"];

    // Window Menu
    [NSApp.mainMenu addItemWithTitle:@"" action:nil keyEquivalent:@""].submenu = NSApp.windowsMenu;
    [NSApp.windowsMenu addItemWithTitle:NSLocalizedString(@"Minimize", nil)
                                 action:@selector(performMiniaturize:)
                          keyEquivalent:@"m"];
    [NSApp.windowsMenu addItemWithTitle:NSLocalizedString(@"Zoom", nil)
                                 action:@selector(performZoom:)
                          keyEquivalent:@""];
}

- (void)applicationWillFinishLaunching:(NSNotification *)notification {
    _active = YES;
    
    // App name (used for menu bar and window title) is "CFBundleName" or the process name.
    NSDictionary *infoDictionary = [[NSBundle mainBundle] infoDictionary];
    NSString *appName = infoDictionary[@"CFBundleName"];
    if (!appName || appName.length == 0) {
        appName = [[NSProcessInfo processInfo] processName];
    }

    // Window style: Closable, miniaturizable, resizable.
    NSWindowStyleMask windowStyle = (NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                     NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable);

    // Create NSWindow centered on the main screen, One-half screen size.
    double width, height, scale;
    glfm__getDefaultDisplaySize(NULL, &width, &height, &scale);
    CGRect screenFrame = [NSScreen mainScreen].frame;
    CGRect contentFrame;
    contentFrame.origin.x = screenFrame.origin.x + screenFrame.size.width / 2 - width / 2;
    contentFrame.origin.y = screenFrame.origin.y + screenFrame.size.height / 2 - height / 2;
    contentFrame.size.width = width;
    contentFrame.size.height = height;
    self.window = GLFM_AUTORELEASE([[NSWindow alloc] initWithContentRect:contentFrame
                                                               styleMask:windowStyle
                                                                 backing:NSBackingStoreBuffered
                                                                   defer:NO]);
    self.window.title = appName;
    //self.window.titlebarSeparatorStyle = NSTitlebarSeparatorStyleNone; // Make the topmost row of the view visible
    self.window.excludedFromWindowsMenu = YES; // Single-window app
    self.window.tabbingMode = NSWindowTabbingModeDisallowed; // No tabs
    self.window.releasedWhenClosed = NO;
    self.window.delegate = self;
    GLFMViewController *glfmViewController = GLFM_AUTORELEASE([[GLFMViewController alloc] init]);
    
    // Set the contentViewController and call glfmMain() (in loadView).
    self.window.contentViewController = glfmViewController;
    if (!self.window.contentViewController.isViewLoaded) {
        [self.window.contentViewController loadView];
    }
    
    // Check if window size should change (due to orientation setting)
    glfm__getDefaultDisplaySize(glfmViewController.glfmDisplay, &width, &height, &scale);
    if (!glfm__isCGFloatEqual(width, (double)contentFrame.size.width) ||
        glfm__isCGFloatEqual(height, (double)contentFrame.size.height)) {
        contentFrame.origin.x = screenFrame.origin.x + screenFrame.size.width / 2 - width / 2;
        contentFrame.origin.y = screenFrame.origin.y + screenFrame.size.height / 2 - height / 2;
        contentFrame.size.width = width;
        contentFrame.size.height = height;
        [self.window setFrame:[self.window frameRectForContentRect:contentFrame] display:NO];
    }
    
    // Create default menu if one wasn't created in glfmMain()
    if (!NSApp.mainMenu) {
        [self createDefaultMenuWithAppName:appName];
    }
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    // Draw first before showing the window
    GLFMViewController *glfmViewController = (GLFMViewController *)self.window.contentViewController;
    [glfmViewController.glfmViewIfLoaded draw];
    
    if (NSApp.activationPolicy == NSApplicationActivationPolicyRegular) {
        [self.window makeKeyAndOrderFront:nil];
    } else {
        // Executable-only (unbundled) app
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [self.window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
    }
}

- (void)setActive:(BOOL)active {
    if (_active != active) {
        _active = active;
        
        GLFMViewController *vc = (GLFMViewController *)self.window.contentViewController;
        if (vc.glfmDisplay && vc.glfmDisplay->focusFunc) {
            vc.glfmDisplay->focusFunc(vc.glfmDisplay, _active);
        }
        if (vc.isViewLoaded) {
            if (!active) {
                // Draw once when entering the background so that a game can show "paused" state.
                [vc.glfmView requestRefresh];
                [vc.glfmView draw];
            }
            vc.glfmView.animating = active;
        }
#if TARGET_OS_IOS
        if (vc.isMotionManagerLoaded) {
            [vc updateMotionManagerActiveState];
        }
#endif
        [vc clearTouches];
    }
}

- (void)applicationWillResignActive:(NSNotification *)notification {
    self.active = NO;
}

- (void)applicationDidBecomeActive:(NSNotification *)notification {
    self.active = self.window && !self.window.miniaturized;
}

- (void)windowWillMiniaturize:(NSNotification *)notification {
    self.active = NO;
}

- (void)windowDidDeminiaturize:(NSNotification *)notification  {
    self.active = NSApp.active;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
    if (self.window) {
        // Terminate later in windowWillClose:
        [self.window close];
        return NSTerminateCancel;
    } else {
        return NSTerminateNow;
    }
}

- (void)windowWillClose:(NSNotification *)notification {
    if (self.window == notification.object) {
        self.active = NO;
        self.window = nil;
        // Dispatch later, after surfaceDestroyedFunc is called
        dispatch_async(dispatch_get_main_queue(), ^{
            [NSApp terminate:nil];
        });
    }
}

- (void)dealloc {
    self.window = nil;
#if !__has_feature(objc_arc)
    [super dealloc];
#endif
}

@end

#endif // TARGET_OS_OSX

// MARK: - Main

#if TARGET_OS_IOS || TARGET_OS_TV

int main(int argc, char *argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([GLFMAppDelegate class]));
    }
}

#else

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // Create the sharedApplication instance from "NSPrincipalClass"
        NSDictionary *infoDictionary = [[NSBundle mainBundle] infoDictionary];
        NSString *principalClassName = infoDictionary[@"NSPrincipalClass"];
        Class principalClass = NSClassFromString(principalClassName);
        if ([principalClass respondsToSelector:@selector(sharedApplication)]) {
            [principalClass sharedApplication];
        } else {
            [NSApplication sharedApplication];
        }
        
        // Set the delegate and run
        GLFMAppDelegate *delegate = [GLFMAppDelegate new];
        [NSApp setDelegate:delegate];
        [NSApp run];
        GLFM_RELEASE(delegate);
    }
    return 0;
}
 
#endif // TARGET_OS_OSX

// MARK: - GLFM private functions

static void glfm__getDefaultDisplaySize(const GLFMDisplay *display,
                                        double *width, double *height, double *scale) {
#if TARGET_OS_IOS || TARGET_OS_TV
    (void)display;
    CGSize size = UIScreen.mainScreen.bounds.size;
    *width = (double)size.width;
    *height = (double)size.height;
    *scale = UIScreen.mainScreen.nativeScale;
#else
    NSScreen *screen = [NSScreen mainScreen];
    *scale = screen.backingScaleFactor;
    
    // Create a window half the size of the screen.
    // If portrait orientation (but not landscape) is specified, use a portrait-sized default window.
    GLFMInterfaceOrientation supportedOrientations = !display ? GLFMInterfaceOrientationUnknown : display->supportedOrientations;
    if (((supportedOrientations & GLFMInterfaceOrientationPortrait) != 0 ||
         (supportedOrientations & GLFMInterfaceOrientationPortraitUpsideDown) != 0) &&
        (supportedOrientations & GLFMInterfaceOrientationLandscapeLeft) == 0 &&
        (supportedOrientations & GLFMInterfaceOrientationLandscapeRight) == 0) {
        *width = screen.frame.size.height / 2;
        *height = screen.frame.size.width / 2;
    } else {
        *width = screen.frame.size.width / 2;
        *height = screen.frame.size.height / 2;
    }
#endif
}

/// Get drawable size in pixels from display dimensions in points.
static void glfm__getDrawableSize(double displayWidth, double displayHeight, double displayScale,
                                  int *width, int *height) {
    int newDrawableWidth = (int)(displayWidth * displayScale);
    int newDrawableHeight = (int)(displayHeight * displayScale);

#if TARGET_OS_IOS
    // On the iPhone 6 when "Display Zoom" is set, the size will be incorrect.
    if (glfm__isCGFloatEqual(displayScale, (CGFloat)2.343750)) {
        if (newDrawableWidth == 750 && newDrawableHeight == 1331) {
            newDrawableHeight = 1334;
        } else if (newDrawableWidth == 1331 && newDrawableHeight == 750) {
            newDrawableWidth = 1334;
        }
    }
#endif

    *width = newDrawableWidth;
    *height = newDrawableHeight;
}

static void glfm__displayChromeUpdated(GLFMDisplay *display) {
    if (display && display->platformData) {
#if TARGET_OS_IOS
        GLFMViewController *vc = (__bridge GLFMViewController *)display->platformData;
        [vc.glfmViewIfLoaded requestRefresh];
        [vc setNeedsStatusBarAppearanceUpdate];
        if (@available(iOS 11, *)) {
            [vc setNeedsUpdateOfScreenEdgesDeferringSystemGestures];
        }
#endif
    }
}

static void glfm__sensorFuncUpdated(GLFMDisplay *display) {
#if TARGET_OS_IOS
    if (display) {
        GLFMViewController *vc = (__bridge GLFMViewController *)display->platformData;
        [vc updateMotionManagerActiveState];
    }
#else
    (void)display;
#endif
}

// MARK: - GLFM public functions

double glfmGetTime() {
    return CACurrentMediaTime();
}

GLFMProc glfmGetProcAddress(const char *functionName) {
    static void *handle = NULL;
    if (!handle) {
        handle = dlopen(NULL, RTLD_LAZY);
    }
    return handle ? (GLFMProc)dlsym(handle, functionName) : NULL;
}

void glfmSwapBuffers(GLFMDisplay *display) {
    if (display && display->platformData) {
        GLFMViewController *vc = (__bridge GLFMViewController *)display->platformData;
        [vc.glfmViewIfLoaded swapBuffers];
    }
}

void glfmSetSupportedInterfaceOrientation(GLFMDisplay *display, GLFMInterfaceOrientation supportedOrientations) {
    if (display) {
        if (display->supportedOrientations != supportedOrientations) {
            display->supportedOrientations = supportedOrientations;
#if TARGET_OS_IOS
            GLFMViewController *vc = (__bridge GLFMViewController *)display->platformData;
            if (@available(iOS 16, *)) {
                [vc setNeedsUpdateOfSupportedInterfaceOrientations];
            } else if (vc.isViewLoaded && vc.view.window) {
                // HACK: Notify that the value of supportedInterfaceOrientations has changed
                [vc.glfmView requestRefresh];
                UIViewController *dummyVC = GLFM_AUTORELEASE([[UIViewController alloc] init]);
                dummyVC.view = GLFM_AUTORELEASE([[UIView alloc] init]);
                [vc presentViewController:dummyVC animated:NO completion:^{
                    [vc dismissViewControllerAnimated:NO completion:NULL];
                }];
            }
#endif
        }
    }
}

GLFMInterfaceOrientation glfmGetInterfaceOrientation(const GLFMDisplay *display) {
    (void)display;
#if TARGET_OS_IOS
    UIInterfaceOrientation orientation = [[UIApplication sharedApplication] statusBarOrientation];
    switch (orientation) {
        case UIInterfaceOrientationPortrait:
            return GLFMInterfaceOrientationPortrait;
        case UIInterfaceOrientationPortraitUpsideDown:
            return GLFMInterfaceOrientationPortraitUpsideDown;
        case UIInterfaceOrientationLandscapeLeft:
            return GLFMInterfaceOrientationLandscapeLeft;
        case UIInterfaceOrientationLandscapeRight:
            return GLFMInterfaceOrientationLandscapeRight;
        case UIInterfaceOrientationUnknown: default:
            return GLFMInterfaceOrientationUnknown;
    }
#else
    return GLFMInterfaceOrientationUnknown;
#endif
}

void glfmGetDisplaySize(const GLFMDisplay *display, int *width, int *height) {
    if (display && display->platformData) {
        GLFMViewController *vc = (__bridge GLFMViewController *)display->platformData;
        if (vc.isViewLoaded) {
            *width = vc.glfmView.drawableWidth;
            *height = vc.glfmView.drawableHeight;
        } else {
            double displayWidth, displayHeight, displayScale;
            glfm__getDefaultDisplaySize(display, &displayWidth, &displayHeight, &displayScale);
            glfm__getDrawableSize(displayWidth, displayHeight, displayScale, width, height);
        }
    } else {
        *width = 0;
        *height = 0;
    }
}

double glfmGetDisplayScale(const GLFMDisplay *display) {
#if TARGET_OS_OSX
    NSWindow *window = nil;
    if (display && display->platformData) {
        GLFMViewController *vc = (__bridge GLFMViewController *)display->platformData;
        window = vc.glfmViewIfLoaded.window;
    }
    return window ? window.backingScaleFactor : [NSScreen mainScreen].backingScaleFactor;
#else
    (void)display;
    return (double)[UIScreen mainScreen].nativeScale;
#endif
}

void glfmGetDisplayChromeInsets(const GLFMDisplay *display, double *top, double *right,
                                double *bottom, double *left) {
    if (display && display->platformData) {
        GLFMViewController *vc = (__bridge GLFMViewController *)display->platformData;
        if (!vc.isViewLoaded) {
            *top = 0.0;
            *right = 0.0;
            *bottom = 0.0;
            *left = 0.0;
        } else if (@available(iOS 11, tvOS 11, macOS 11, *)) {
#if TARGET_OS_IOS || TARGET_OS_TV
            UIEdgeInsets insets = vc.view.safeAreaInsets;
            *top = (double)(insets.top * vc.view.contentScaleFactor);
            *right = (double)(insets.right * vc.view.contentScaleFactor);
            *bottom = (double)(insets.bottom * vc.view.contentScaleFactor);
            *left = (double)(insets.left * vc.view.contentScaleFactor);
#else
            // NOTE: This has not been tested.
            // Run glfm_test_pattern fullscreen on a 2021-2022 MacBook Pro/Air with a notch.
            NSEdgeInsets insets = vc.view.safeAreaInsets;
            *top = (double)(insets.top * vc.view.layer.contentsScale);
            *right = (double)(insets.right * vc.view.layer.contentsScale);
            *bottom = (double)(insets.bottom * vc.view.layer.contentsScale);
            *left = (double)(insets.left * vc.view.layer.contentsScale);
#endif
        } else {
#if TARGET_OS_IOS
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            if (![vc prefersStatusBarHidden]) {
                *top = (double)([UIApplication sharedApplication].statusBarFrame.size.height *
                        vc.view.contentScaleFactor);
            } else {
                *top = 0.0;
            }
#pragma clang diagnostic pop
#else
            *top = 0.0;
#endif
            *right = 0.0;
            *bottom = 0.0;
            *left = 0.0;
        }
    } else {
        *top = 0.0;
        *right = 0.0;
        *bottom = 0.0;
        *left = 0.0;
    }
}

GLFMRenderingAPI glfmGetRenderingAPI(const GLFMDisplay *display) {
    if (display && display->platformData) {
        GLFMViewController *vc = (__bridge GLFMViewController *)display->platformData;
        if (vc.isViewLoaded) {
            return vc.glfmView.renderingAPI;
        } else {
            return GLFMRenderingAPIOpenGLES2;
        }
    } else {
        return GLFMRenderingAPIOpenGLES2;
    }
}

bool glfmHasTouch(const GLFMDisplay *display) {
    (void)display;
#if TARGET_OS_IOS || TARGET_OS_TV
    return true;
#else
    return false;
#endif
}

void glfmSetMouseCursor(GLFMDisplay *display, GLFMMouseCursor mouseCursor) {
    (void)display;
    (void)mouseCursor;
#if TARGET_OS_OSX
    // TODO: macOS mouse cursor
#else
    // Do nothing
#endif
}

void glfmSetMultitouchEnabled(GLFMDisplay *display, bool multitouchEnabled) {
#if TARGET_OS_IOS
    if (display) {
        GLFMViewController *vc = (__bridge GLFMViewController *)display->platformData;
        vc.multipleTouchEnabled = (BOOL)multitouchEnabled;
        vc.glfmViewIfLoaded.multipleTouchEnabled = (BOOL)multitouchEnabled;
    }
#else
    (void)display;
    (void)multitouchEnabled;
#endif
}

bool glfmGetMultitouchEnabled(const GLFMDisplay *display) {
#if TARGET_OS_IOS || TARGET_OS_TV
    if (display) {
        GLFMViewController *vc = (__bridge GLFMViewController *)display->platformData;
        return vc.multipleTouchEnabled;
    } else {
        return false;
    }
#else
    (void)display;
    return false;
#endif
}

void glfmSetKeyboardVisible(GLFMDisplay *display, bool visible) {
#if TARGET_OS_IOS || TARGET_OS_TV
    if (display) {
        GLFMViewController *vc = (__bridge GLFMViewController *)display->platformData;
        vc.keyboardRequested = visible;
        if (visible) {
            [vc becomeFirstResponder];
        } else {
            [vc resignFirstResponder];
        }
    }
#else
    (void)display;
    (void)visible;
#endif
}

bool glfmIsKeyboardVisible(const GLFMDisplay *display) {
#if TARGET_OS_IOS || TARGET_OS_TV
    if (display) {
        GLFMViewController *vc = (__bridge GLFMViewController *)display->platformData;
        return vc.keyboardRequested;
    } else {
        return false;
    }
#else
    return false;
#endif
}

bool glfmIsSensorAvailable(const GLFMDisplay *display, GLFMSensor sensor) {
#if TARGET_OS_IOS
    if (display) {
        GLFMViewController *vc = (__bridge GLFMViewController *)display->platformData;
        switch (sensor) {
            case GLFMSensorAccelerometer:
                return vc.motionManager.deviceMotionAvailable && vc.motionManager.accelerometerAvailable;
            case GLFMSensorMagnetometer:
                return vc.motionManager.deviceMotionAvailable && vc.motionManager.magnetometerAvailable;
            case GLFMSensorGyroscope:
                return vc.motionManager.deviceMotionAvailable && vc.motionManager.gyroAvailable;
            case GLFMSensorRotationMatrix:
                return (vc.motionManager.deviceMotionAvailable &&
                        ([CMMotionManager availableAttitudeReferenceFrames] & CMAttitudeReferenceFrameXMagneticNorthZVertical));
        }
    }
    return false;
#else
    (void)display;
    (void)sensor;
    return false;
#endif
}

bool glfmIsHapticFeedbackSupported(const GLFMDisplay *display) {
    (void)display;
#if TARGET_OS_IOS
    if (@available(iOS 13, *)) {
        return [CHHapticEngine capabilitiesForHardware].supportsHaptics;
    } else {
        return false;
    }
#else
    return false;
#endif
}

void glfmPerformHapticFeedback(GLFMDisplay *display, GLFMHapticFeedbackStyle style) {
    (void)display;
#if TARGET_OS_IOS
    if (@available(iOS 10, *)) {
        UIImpactFeedbackStyle uiStyle;
        switch (style) {
            case GLFMHapticFeedbackLight: default:
                uiStyle = UIImpactFeedbackStyleLight;
                break;
            case GLFMHapticFeedbackMedium:
                uiStyle = UIImpactFeedbackStyleMedium;
                break;
            case GLFMHapticFeedbackHeavy:
                uiStyle = UIImpactFeedbackStyleHeavy;
                break;
        }
        UIImpactFeedbackGenerator *generator = [[UIImpactFeedbackGenerator alloc] initWithStyle:uiStyle];
        [generator impactOccurred];
        GLFM_RELEASE(generator);
    }
#else
    (void)style;
#endif
}

// MARK: - Apple-specific functions

bool glfmIsMetalSupported(const GLFMDisplay *display) {
#if GLFM_INCLUDE_METAL
    if (display) {
        GLFMViewController *vc = (__bridge GLFMViewController *)display->platformData;
        return (vc.metalDevice != nil);
    }
#endif
    return false;
}

void *glfmGetMetalView(const GLFMDisplay *display) {
#if GLFM_INCLUDE_METAL
    if (display) {
        GLFMViewController *vc = (__bridge GLFMViewController *)display->platformData;
        UIView<GLFMView> *view = vc.glfmViewIfLoaded;
        if ([view isKindOfClass:[MTKView class]]) {
            return (__bridge void *)view;
        }
    }
#endif
    return NULL;
}

void *glfmGetViewController(const GLFMDisplay *display) {
    if (display) {
        GLFMViewController *vc = (__bridge GLFMViewController *)display->platformData;
        return (__bridge void *)vc;
    } else {
        return NULL;
    }
}

#endif // __APPLE__
