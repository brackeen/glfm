/*
 GLFM
 https://github.com/brackeen/glfm
 Copyright (c) 2014-2020 David Brackeen
 
 This software is provided 'as-is', without any express or implied warranty.
 In no event will the authors be held liable for any damages arising from the
 use of this software. Permission is granted to anyone to use this software
 for any purpose, including commercial applications, and to alter it and
 redistribute it freely, subject to the following restrictions:
 
 1. The origin of this software must not be misrepresented; you must not
    claim that you wrote the original software. If you use this software in a
    product, an acknowledgment in the product documentation would be appreciated
    but is not required.
 2. Altered source versions must be plainly marked as such, and must not be
    misrepresented as being the original software.
 3. This notice may not be removed or altered from any source distribution.
 */

#include "glfm.h"

#if !defined(GLFM_INCLUDE_METAL)
#define GLFM_INCLUDE_METAL 1
#endif

#if defined(GLFM_PLATFORM_IOS) || defined(GLFM_PLATFORM_TVOS)

#import <UIKit/UIKit.h>
#if GLFM_INCLUDE_METAL
#import <MetalKit/MetalKit.h>
#endif

#include <dlfcn.h>
#include "glfm_platform.h"

#define MAX_SIMULTANEOUS_TOUCHES 10

#ifndef NDEBUG
#define CHECK_GL_ERROR() ((void)0)
#else
#define CHECK_GL_ERROR() do { GLenum error = glGetError(); if (error != GL_NO_ERROR) \
NSLog(@"OpenGL error 0x%04x at glfm_platform_ios.m:%i", error, __LINE__); } while(0)
#endif

#if __has_feature(objc_arc)
#define GLFM_AUTORELEASE(value) value
#define GLFM_RELEASE(value) ((void)0)
#else
#define GLFM_AUTORELEASE(value) [value autorelease]
#define GLFM_RELEASE(value) [value release]
#endif

@interface GLFMAppDelegate : NSObject <UIApplicationDelegate>

@property(nonatomic, strong) UIWindow *window;
@property(nonatomic, assign) BOOL active;

@end

#pragma mark - GLFMView

static void _glfmPreferredDrawableSize(CGRect bounds, CGFloat contentScaleFactor,
                                       int *width, int *height);

@protocol GLFMView

@property(nonatomic, readonly) GLFMRenderingAPI renderingAPI;
@property(nonatomic, readonly) int drawableWidth;
@property(nonatomic, readonly) int drawableHeight;
@property(nonatomic, assign) BOOL animating;

- (void)draw;

@end

#if GLFM_INCLUDE_METAL

#pragma mark - GLFMMetalView

@interface GLFMMetalView : MTKView <GLFMView, MTKViewDelegate>

@property(nonatomic, assign) GLFMDisplay *glfmDisplay;
@property(nonatomic, assign) int drawableWidth;
@property(nonatomic, assign) int drawableHeight;
@property(nonatomic, assign) BOOL surfaceCreatedNotified;

@end

@implementation GLFMMetalView

@dynamic renderingAPI, animating;

- (instancetype)initWithFrame:(CGRect)frame contentScaleFactor:(CGFloat)contentScaleFactor
                       device:(id<MTLDevice>)device glfmDisplay:(GLFMDisplay *)glfmDisplay {
    if ((self = [super initWithFrame:frame device:device])) {
        self.contentScaleFactor = contentScaleFactor;
        self.delegate = self;
        self.glfmDisplay = glfmDisplay;
        self.drawableWidth = (int)self.drawableSize.width;
        self.drawableHeight = (int)self.drawableSize.height;

        switch (glfmDisplay->colorFormat) {
            case GLFMColorFormatRGB565:
                self.colorPixelFormat = MTLPixelFormatB5G6R5Unorm;
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
            if (@available(iOS 13, tvOS 13, *)) {
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

- (BOOL)animating {
    return !self.paused;
}

- (void)setAnimating:(BOOL)animating {
    self.paused = !animating;
}

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
    
}

- (void)drawInMTKView:(MTKView *)view {
    int newDrawableWidth = (int)self.drawableSize.width;
    int newDrawableHeight = (int)self.drawableSize.height;
    if (!self.surfaceCreatedNotified) {
        self.surfaceCreatedNotified = YES;

        self.drawableWidth = newDrawableWidth;
        self.drawableHeight = newDrawableHeight;
        if (_glfmDisplay->surfaceCreatedFunc) {
            _glfmDisplay->surfaceCreatedFunc(_glfmDisplay, self.drawableWidth, self.drawableHeight);
        }
    } else if (newDrawableWidth != self.drawableWidth || newDrawableHeight != self.drawableHeight) {
        self.drawableWidth = newDrawableWidth;
        self.drawableHeight = newDrawableHeight;
        if (_glfmDisplay->surfaceResizedFunc) {
            _glfmDisplay->surfaceResizedFunc(_glfmDisplay, self.drawableWidth, self.drawableHeight);
        }
    }
    
    if (_glfmDisplay->mainLoopFunc) {
        _glfmDisplay->mainLoopFunc(_glfmDisplay, CACurrentMediaTime());
    }
}

- (void)layoutSubviews {
    // First render as soon as safeAreaInsets are set
    if (!self.surfaceCreatedNotified) {
        [self draw];
    }
}

@end

#endif

#pragma mark - GLFMOpenGLView

@interface GLFMOpenGLView : UIView <GLFMView> {
    GLint _drawableWidth;
    GLint _drawableHeight;
    GLuint _defaultFramebuffer;
    GLuint _colorRenderbuffer;
    GLuint _attachmentRenderbuffer;
    GLuint _msaaFramebuffer;
    GLuint _msaaRenderbuffer;
}

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

@end

@implementation GLFMOpenGLView

@dynamic drawableWidth, drawableHeight, animating;

+ (Class)layerClass {
    return [CAEAGLLayer class];
}

- (instancetype)initWithFrame:(CGRect)frame contentScaleFactor:(CGFloat)contentScaleFactor
                  glfmDisplay:(GLFMDisplay *)glfmDisplay {
    if ((self = [super initWithFrame:frame])) {
        
        self.contentScaleFactor = contentScaleFactor;
        self.glfmDisplay = glfmDisplay;
        
        if (glfmDisplay->preferredAPI >= GLFMRenderingAPIOpenGLES3) {
            self.context = GLFM_AUTORELEASE([[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3]);
            self.renderingAPI = GLFMRenderingAPIOpenGLES3;
        }
        if (!self.context) {
            self.context = GLFM_AUTORELEASE([[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2]);
            self.renderingAPI = GLFMRenderingAPIOpenGLES2;
        }
        
        if (!self.context) {
            _glfmReportSurfaceError(glfmDisplay, "Failed to create ES context");
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
#if !__has_feature(objc_arc)
    self.context = nil;
    self.colorFormat = nil;
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
    if (eaglLayer.contentsScale == 2.343750) {
        if (eaglLayer.bounds.size.width == 320.0 && eaglLayer.bounds.size.height == 568.0) {
            eaglLayer.bounds = CGRectMake(eaglLayer.bounds.origin.x, eaglLayer.bounds.origin.y,
                                          eaglLayer.bounds.size.width,
                                          1334 / eaglLayer.contentsScale);
        } else if (eaglLayer.bounds.size.width == 568.0 && eaglLayer.bounds.size.height == 320.0) {
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

    if (_multisampling) {
        glGenFramebuffers(1, &_msaaFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, _msaaFramebuffer);

        glGenRenderbuffers(1, &_msaaRenderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, _msaaRenderbuffer);

        GLenum internalformat = GL_RGBA8_OES;
        if ([kEAGLColorFormatRGB565 isEqualToString:_colorFormat]) {
            internalformat = GL_RGB565;
        }

        glRenderbufferStorageMultisampleAPPLE(GL_RENDERBUFFER, 4, internalformat,
                                              _drawableWidth, _drawableHeight);

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                  _msaaRenderbuffer);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            NSLog(@"Error: Couldn't create multisample framebuffer: 0x%04x", status);
        }
    }

    if (_depthBits > 0 || _stencilBits > 0) {
        glGenRenderbuffers(1, &_attachmentRenderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, _attachmentRenderbuffer);

        GLenum internalformat;
        if (_depthBits > 0 && _stencilBits > 0) {
            internalformat = GL_DEPTH24_STENCIL8_OES;
        } else if (_depthBits >= 24) {
            internalformat = GL_DEPTH_COMPONENT24_OES;
        } else if (_depthBits > 0) {
            internalformat = GL_DEPTH_COMPONENT16;
        } else {
            internalformat = GL_STENCIL_INDEX8;
        }

        if (_multisampling) {
            glRenderbufferStorageMultisampleAPPLE(GL_RENDERBUFFER, 4, internalformat,
                                                  _drawableWidth, _drawableHeight);
        } else {
            glRenderbufferStorage(GL_RENDERBUFFER, internalformat, _drawableWidth, _drawableHeight);
        }

        if (_depthBits > 0) {
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                      _attachmentRenderbuffer);
        }
        if (_stencilBits > 0) {
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
    if (_multisampling) {
        glBindFramebuffer(GL_FRAMEBUFFER, _msaaFramebuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, _msaaRenderbuffer);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, _defaultFramebuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, _colorRenderbuffer);
    }
    CHECK_GL_ERROR();
}

- (void)finishRender {
    if (_multisampling) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER_APPLE, _msaaFramebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER_APPLE, _defaultFramebuffer);
        glResolveMultisampleFramebufferAPPLE();
    }

    static bool checked_GL_EXT_discard_framebuffer = false;
    static bool has_GL_EXT_discard_framebuffer = false;
    if (!checked_GL_EXT_discard_framebuffer) {
        checked_GL_EXT_discard_framebuffer = true;
        if (glfmExtensionSupported("GL_EXT_discard_framebuffer")) {
            has_GL_EXT_discard_framebuffer = true;
        }
    }
    if (has_GL_EXT_discard_framebuffer) {
        GLenum target = GL_FRAMEBUFFER;
        GLenum attachments[3];
        GLsizei numAttachments = 0;
        if (_multisampling) {
            target = GL_READ_FRAMEBUFFER_APPLE;
            attachments[numAttachments++] = GL_COLOR_ATTACHMENT0;
        }
        if (_depthBits > 0) {
            attachments[numAttachments++] = GL_DEPTH_ATTACHMENT;
        }
        if (_stencilBits > 0) {
            attachments[numAttachments++] = GL_STENCIL_ATTACHMENT;
        }
        if (numAttachments > 0) {
            if (_multisampling) {
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

        if (_glfmDisplay->surfaceCreatedFunc) {
            _glfmDisplay->surfaceCreatedFunc(_glfmDisplay, self.drawableWidth, self.drawableHeight);
        }
    }
    
    if (self.surfaceSizeChanged) {
        self.surfaceSizeChanged  = NO;
        if (_glfmDisplay->surfaceResizedFunc) {
            _glfmDisplay->surfaceResizedFunc(_glfmDisplay, self.drawableWidth, self.drawableHeight);
        }
    }
    
    [self prepareRender];
    if (_glfmDisplay->mainLoopFunc) {
        CFTimeInterval timestamp = displayLink.timestamp;
        if (timestamp <= 0) {
            // First render from layoutSubviews
            timestamp = CACurrentMediaTime();
        }
        _glfmDisplay->mainLoopFunc(_glfmDisplay, timestamp);
    }
    [self finishRender];
}

- (void)draw {
    if (self.displayLink) {
        [self render:self.displayLink];
    }
}

- (void)layoutSubviews {
    int newDrawableWidth;
    int newDrawableHeight;
    _glfmPreferredDrawableSize(self.bounds, self.contentScaleFactor,
                               &newDrawableWidth, &newDrawableHeight);

    if (self.drawableWidth != newDrawableWidth || self.drawableHeight != newDrawableHeight) {
        [self deleteDrawable];
        [self createDrawable];
        self.surfaceSizeChanged = self.surfaceCreatedNotified;
    }
    
    // First render as soon as safeAreaInsets are set
    if (!self.surfaceCreatedNotified) {
        [self draw];
    }
}

@end

#pragma mark - GLFMViewController

@interface GLFMViewController : UIViewController<UIKeyInput, UITextInputTraits> {
    const void *activeTouches[MAX_SIMULTANEOUS_TOUCHES];
}

#if GLFM_INCLUDE_METAL
@property(nonatomic, strong) id<MTLDevice> metalDevice;
#endif
@property(nonatomic, assign) GLFMDisplay *glfmDisplay;
@property(nonatomic, assign) BOOL multipleTouchEnabled;
@property(nonatomic, assign) BOOL keyboardRequested;
@property(nonatomic, assign) BOOL keyboardVisible;

@end

@implementation GLFMViewController

- (id)init {
    if ((self = [super init])) {
        [self clearTouches];
        _glfmDisplay = calloc(1, sizeof(GLFMDisplay));
        _glfmDisplay->platformData = (__bridge void *)self;
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

- (BOOL)prefersStatusBarHidden {
    return _glfmDisplay->uiChrome != GLFMUserInterfaceChromeNavigationAndStatusBar;
}

- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures {
    return _glfmDisplay->uiChrome == GLFMUserInterfaceChromeFullscreen ? UIRectEdgeBottom : UIRectEdgeNone;
}

- (UIView<GLFMView> *)glfmView {
    return (UIView<GLFMView> *)self.view;
}

- (void)loadView {
    glfmMain(_glfmDisplay);

    GLFMAppDelegate *delegate = UIApplication.sharedApplication.delegate;
    CGRect frame = delegate.window.bounds;
    CGFloat scale = [UIScreen mainScreen].nativeScale;
    UIView<GLFMView> *glfmView = nil;
    
#if GLFM_INCLUDE_METAL
    if (_glfmDisplay->preferredAPI == GLFMRenderingAPIMetal && self.metalDevice) {
        glfmView = GLFM_AUTORELEASE([[GLFMMetalView alloc] initWithFrame:frame
                                                       contentScaleFactor:scale
                                                                   device:self.metalDevice
                                                              glfmDisplay:_glfmDisplay]);
    }
#endif
    if (!glfmView) {
        glfmView = GLFM_AUTORELEASE([[GLFMOpenGLView alloc] initWithFrame:frame
                                                        contentScaleFactor:scale
                                                               glfmDisplay:_glfmDisplay]);
    }
    self.view = glfmView;
    self.view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
}

- (void)viewDidLoad {
    [super viewDidLoad];

    GLFMAppDelegate *delegate = UIApplication.sharedApplication.delegate;
    self.glfmView.animating = delegate.active;
    
#if TARGET_OS_IOS
    self.view.multipleTouchEnabled = self.multipleTouchEnabled;

    [self setNeedsStatusBarAppearanceUpdate];

    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(keyboardFrameChanged:)
                                               name:UIKeyboardWillChangeFrameNotification
                                             object:self.view.window];
#endif
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
    BOOL isTablet = (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad);
    GLFMUserInterfaceOrientation uiOrientations = _glfmDisplay->allowedOrientations;
    if (uiOrientations == GLFMUserInterfaceOrientationAny) {
        if (isTablet) {
            return UIInterfaceOrientationMaskAll;
        } else {
            return UIInterfaceOrientationMaskAllButUpsideDown;
        }
    } else if (uiOrientations == GLFMUserInterfaceOrientationPortrait) {
        if (isTablet) {
            return (UIInterfaceOrientationMask)(UIInterfaceOrientationMaskPortrait |
                    UIInterfaceOrientationMaskPortraitUpsideDown);
        } else {
            return UIInterfaceOrientationMaskPortrait;
        }
    } else {
        return UIInterfaceOrientationMaskLandscape;
    }
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    if (_glfmDisplay->lowMemoryFunc) {
        _glfmDisplay->lowMemoryFunc(_glfmDisplay);
    }
}

- (void)dealloc {
    if (_glfmDisplay->surfaceDestroyedFunc) {
        _glfmDisplay->surfaceDestroyedFunc(_glfmDisplay);
    }
    free(_glfmDisplay);
#if !__has_feature(objc_arc)
#if GLFM_INCLUDE_METAL
    self.metalDevice = nil;
#endif
    [super dealloc];
#endif
}

#pragma mark - UIResponder

- (void)clearTouches {
    for (int i = 0; i < MAX_SIMULTANEOUS_TOUCHES; i++) {
        activeTouches[i] = NULL;
    }
}

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

    if (_glfmDisplay->touchFunc) {
        CGPoint currLocation = [touch locationInView:self.view];
        currLocation.x *= self.view.contentScaleFactor;
        currLocation.y *= self.view.contentScaleFactor;

        _glfmDisplay->touchFunc(_glfmDisplay, index, phase,
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
    if (_glfmDisplay->keyFunc) {
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
        }
        if (key != 0) {
            return _glfmDisplay->keyFunc(_glfmDisplay, key, action, 0);
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

#endif

#pragma mark - UIKeyInput

#if TARGET_OS_IOS

- (void)keyboardFrameChanged:(NSNotification *)notification {
    NSObject *value = notification.userInfo[UIKeyboardFrameEndUserInfoKey];
    if ([value isKindOfClass:[NSValue class]]) {
        NSValue *nsValue = (NSValue *)value;
        CGRect keyboardFrame = [nsValue CGRectValue];

        self.keyboardVisible = CGRectIntersectsRect(self.view.window.frame, keyboardFrame);

        if (_glfmDisplay->keyboardVisibilityChangedFunc) {
            // Convert to view coordinates
            keyboardFrame = [self.view convertRect:keyboardFrame fromView:nil];

            // Convert to pixels
            keyboardFrame.origin.x *= self.view.contentScaleFactor;
            keyboardFrame.origin.y *= self.view.contentScaleFactor;
            keyboardFrame.size.width *= self.view.contentScaleFactor;
            keyboardFrame.size.height *= self.view.contentScaleFactor;

            _glfmDisplay->keyboardVisibilityChangedFunc(_glfmDisplay, self.keyboardVisible,
                                                        keyboardFrame.origin.x,
                                                        keyboardFrame.origin.y,
                                                        keyboardFrame.size.width,
                                                        keyboardFrame.size.height);
        }
    }
}

#endif

// UITextInputTraits - disable suggestion bar
- (UITextAutocorrectionType)autocorrectionType {
    return UITextAutocorrectionTypeNo;
}

- (BOOL)hasText {
    return YES;
}

- (void)insertText:(NSString *)text {
    if (_glfmDisplay->charFunc) {
        _glfmDisplay->charFunc(_glfmDisplay, text.UTF8String, 0);
    }
}

- (void)deleteBackward {
    if (_glfmDisplay->keyFunc) {
        _glfmDisplay->keyFunc(_glfmDisplay, GLFMKeyBackspace, GLFMKeyActionPressed, 0);
        _glfmDisplay->keyFunc(_glfmDisplay, GLFMKeyBackspace, GLFMKeyActionReleased, 0);
    }
}

- (BOOL)canBecomeFirstResponder {
    return self.keyboardRequested;
}

- (NSArray<UIKeyCommand *> *)keyCommands {
    static NSArray<UIKeyCommand *> *keyCommands = NULL;
    if (!keyCommands) {
        keyCommands = @[ [UIKeyCommand keyCommandWithInput:UIKeyInputUpArrow
                                             modifierFlags:(UIKeyModifierFlags)0
                                                    action:@selector(keyPressed:)],
                         [UIKeyCommand keyCommandWithInput:UIKeyInputDownArrow
                                             modifierFlags:(UIKeyModifierFlags)0
                                                    action:@selector(keyPressed:)],
                         [UIKeyCommand keyCommandWithInput:UIKeyInputLeftArrow
                                             modifierFlags:(UIKeyModifierFlags)0
                                                    action:@selector(keyPressed:)],
                         [UIKeyCommand keyCommandWithInput:UIKeyInputRightArrow
                                             modifierFlags:(UIKeyModifierFlags)0
                                                    action:@selector(keyPressed:)],
                         [UIKeyCommand keyCommandWithInput:UIKeyInputEscape
                                             modifierFlags:(UIKeyModifierFlags)0
                                                    action:@selector(keyPressed:)] ];
#if !__has_feature(objc_arc)
        [keyCommands retain];
#endif
    }

    return keyCommands;
}

- (void)keyPressed:(UIKeyCommand *)keyCommand {
    if (_glfmDisplay->keyFunc) {
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
        }

        if (keyCode != 0) {
            _glfmDisplay->keyFunc(_glfmDisplay, keyCode, GLFMKeyActionPressed, 0);
            _glfmDisplay->keyFunc(_glfmDisplay, keyCode, GLFMKeyActionReleased, 0);
        }
    }
}

@end

#pragma mark - Application Delegate

@implementation GLFMAppDelegate

- (BOOL)application:(UIApplication *)application
    didFinishLaunchingWithOptions:(NSDictionary<UIApplicationLaunchOptionsKey, id> *)launchOptions {
    _active = YES;
    self.window = GLFM_AUTORELEASE([[UIWindow alloc] init]);
    if (self.window.bounds.size.width <= 0.0 || self.window.bounds.size.height <= 0.0) {
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
                [vc.glfmView draw];
            }
            vc.glfmView.animating = active;
        }
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
#if !__has_feature(objc_arc)
    self.window = nil;
    [super dealloc];
#endif
}

@end

#pragma mark - Main

int main(int argc, char *argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([GLFMAppDelegate class]));
    }
}

#pragma mark - GLFM implementation

GLFMProc glfmGetProcAddress(const char *functionName) {
    static void *handle = NULL;
    if (!handle) {
        handle = dlopen(NULL, RTLD_LAZY);
    }
    return handle ? (GLFMProc)dlsym(handle, functionName) : NULL;
}

void glfmSetUserInterfaceOrientation(GLFMDisplay *display,
                                     GLFMUserInterfaceOrientation allowedOrientations) {
    if (display) {
        if (display->allowedOrientations != allowedOrientations) {
            display->allowedOrientations = allowedOrientations;

            // HACK: Notify that the value of supportedInterfaceOrientations has changed
            GLFMViewController *vc = (__bridge GLFMViewController *)display->platformData;
            if (vc.isViewLoaded && vc.view.window) {
                UIViewController *dummyVC = GLFM_AUTORELEASE([[UIViewController alloc] init]);
                dummyVC.view = GLFM_AUTORELEASE([[UIView alloc] init]);
                [vc presentViewController:dummyVC animated:NO completion:^{
                    [vc dismissViewControllerAnimated:NO completion:NULL];
                }];
            }
        }
    }
}

static void _glfmPreferredDrawableSize(CGRect bounds, CGFloat contentScaleFactor, int *width, int *height) {
    int newDrawableWidth = (int)(bounds.size.width * contentScaleFactor);
    int newDrawableHeight = (int)(bounds.size.height * contentScaleFactor);

    // On the iPhone 6 when "Display Zoom" is set, the size will be incorrect.
    if (contentScaleFactor == 2.343750) {
        if (newDrawableWidth == 750 && newDrawableHeight == 1331) {
            newDrawableHeight = 1334;
        } else if (newDrawableWidth == 1331 && newDrawableHeight == 750) {
            newDrawableWidth = 1334;
        }
    }
    *width = newDrawableWidth;
    *height = newDrawableHeight;
}

void glfmGetDisplaySize(GLFMDisplay *display, int *width, int *height) {
    if (display && display->platformData) {
        GLFMViewController *vc = (__bridge GLFMViewController *)display->platformData;
        if (vc.isViewLoaded) {
            *width = vc.glfmView.drawableWidth;
            *height = vc.glfmView.drawableHeight;
        } else {
            _glfmPreferredDrawableSize(UIScreen.mainScreen.bounds, UIScreen.mainScreen.nativeScale,
                                       width, height);
        }
    } else {
        *width = 0;
        *height = 0;
    }
}

double glfmGetDisplayScale(GLFMDisplay *display) {
    return [UIScreen mainScreen].nativeScale;
}

void glfmGetDisplayChromeInsets(GLFMDisplay *display, double *top, double *right, double *bottom,
                                double *left) {
    if (display && display->platformData) {
        GLFMViewController *vc = (__bridge GLFMViewController *)display->platformData;
        if (!vc.isViewLoaded) {
            *top = 0.0;
            *right = 0.0;
            *bottom = 0.0;
            *left = 0.0;
        } else if (@available(iOS 11, tvOS 11, *)) {
            UIEdgeInsets insets = vc.view.safeAreaInsets;
            *top = insets.top * vc.view.contentScaleFactor;
            *right = insets.right * vc.view.contentScaleFactor;
            *bottom = insets.bottom * vc.view.contentScaleFactor;
            *left = insets.left * vc.view.contentScaleFactor;
        } else {
#if TARGET_OS_IOS
            if (![vc prefersStatusBarHidden]) {
                *top = ([UIApplication sharedApplication].statusBarFrame.size.height *
                        vc.view.contentScaleFactor);
            } else {
                *top = 0.0;
            }
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

void _glfmDisplayChromeUpdated(GLFMDisplay *display) {
    if (display && display->platformData) {
#if TARGET_OS_IOS
        GLFMViewController *vc = (__bridge GLFMViewController *)display->platformData;
        [vc setNeedsStatusBarAppearanceUpdate];
        if (@available(iOS 11, *)) {
            [vc setNeedsUpdateOfScreenEdgesDeferringSystemGestures];
        }
#endif
    }
}

GLFMRenderingAPI glfmGetRenderingAPI(GLFMDisplay *display) {
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

bool glfmHasTouch(GLFMDisplay *display) {
    return true;
}

void glfmSetMouseCursor(GLFMDisplay *display, GLFMMouseCursor mouseCursor) {
    // Do nothing
}

void glfmSetMultitouchEnabled(GLFMDisplay *display, bool multitouchEnabled) {
    if (display) {
#if TARGET_OS_IOS
        GLFMViewController *vc = (__bridge GLFMViewController *)display->platformData;
        vc.multipleTouchEnabled = (BOOL)multitouchEnabled;
        if (vc.isViewLoaded) {
            vc.view.multipleTouchEnabled = (BOOL)multitouchEnabled;
        }
#endif
    }
}

bool glfmGetMultitouchEnabled(GLFMDisplay *display) {
    if (display) {
        GLFMViewController *vc = (__bridge GLFMViewController *)display->platformData;
        return vc.multipleTouchEnabled;
    } else {
        return false;
    }
}

void glfmSetKeyboardVisible(GLFMDisplay *display, bool visible) {
    if (display) {
        GLFMViewController *vc = (__bridge GLFMViewController *)display->platformData;
        vc.keyboardRequested = visible;
        if (visible) {
            [vc becomeFirstResponder];
        } else {
            [vc resignFirstResponder];
        }
    }
}

bool glfmIsKeyboardVisible(GLFMDisplay *display) {
    if (display) {
        GLFMViewController *vc = (__bridge GLFMViewController *)display->platformData;
        return vc.keyboardRequested;
    } else {
        return false;
    }
}

// MARK: Platform-specific functions

bool glfmIsMetalSupported(GLFMDisplay *display) {
#if GLFM_INCLUDE_METAL
    if (display) {
        GLFMViewController *vc = (__bridge GLFMViewController *)display->platformData;
        return (vc.metalDevice != nil);
    }
#endif
    return false;
}

void *glfmGetMetalView(GLFMDisplay *display) {
#if GLFM_INCLUDE_METAL
    if (display) {
        GLFMViewController *vc = (__bridge GLFMViewController *)display->platformData;
        if (vc.isViewLoaded) {
            UIView *view = vc.view;
            if ([view isKindOfClass:[MTKView class]]) {
                return (__bridge void *)view;
            }
        }
    }
#endif
    return NULL;
}

#endif
