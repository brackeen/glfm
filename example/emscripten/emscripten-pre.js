// Prevent the browser from decoding image and audio resources
// From https://hacks.mozilla.org/2014/11/porting-to-emscripten/
var Module;
if (!Module) Module = (typeof Module !== 'undefined' ? Module : null) || {};
Module.noImageDecoding = true;
Module.noAudioDecoding = true;