#include "ultralight_controller.h"
#include <Ultralight/platform/Platform.h>
#include <Ultralight/platform/FontLoader.h>
#include <Ultralight/platform/FileSystem.h>
#include <Ultralight/platform/Logger.h>
#include <Ultralight/platform/Surface.h>
#include <Ultralight/String.h>
#include <Ultralight/JavaScript.h>
#include <vector>
#include <fstream>
#include <iostream>
#include <Windows.h>
#include <shellapi.h>

#include "db_handler.h"

class ConsoleLogger : public ultralight::Logger {
public:
    virtual void LogMessage(ultralight::LogLevel log_level, const ultralight::String& message) override {
        std::wcout << L"[Logger]: " << message.utf16().data() << std::endl;
    }
};


/**
 * Custom FileSystem implementation for Ultralight.
 * This class handles all file load requests from the web view.
 */
class AppFileSystem : public ultralight::FileSystem {
public:
    AppFileSystem(const std::string& base_dir) : base_dir_(base_dir) {}
    virtual ~AppFileSystem() {}

    // Checks if a file exists.
    virtual bool FileExists(const ultralight::String& path) override {
        std::string full_path = base_dir_ + "/" + path.utf8().data();
        std::ifstream file(full_path);
        return file.good();
    }

    // Gets the mime-type of a file by checking its extension.
    virtual ultralight::String GetFileMimeType(const ultralight::String& path) override {
        // FIX: Replaced non-existent EndsWith with standard string logic
        std::string path_str = path.utf8().data();
        size_t last_dot = path_str.find_last_of(".");
        if (last_dot != std::string::npos) {
            std::string ext = path_str.substr(last_dot);
            if (ext == ".html") return "text/html";
            if (ext == ".css") return "text/css";
            if (ext == ".js") return "application/javascript";
            if (ext == ".png") return "image/png";
            if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
            if (ext == ".gif") return "image/gif";
        }
        return "application/octet-stream";
    }

    // FIX: Implemented the missing pure virtual function GetFileCharset.
    // Per the documentation, "utf-8" is a safe default to return.
    virtual ultralight::String GetFileCharset(const ultralight::String& file_path) override {
        return "utf-8";
    }

    // Opens a file and returns its contents.
    virtual ultralight::RefPtr<ultralight::Buffer> OpenFile(const ultralight::String& path) override {
        std::string full_path = base_dir_ + "/" + path.utf8().data();
        std::ifstream file(full_path, std::ios::binary | std::ios::ate);

        if (!file.is_open()) {
            return nullptr;
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<char> buffer(static_cast<size_t>(size));
        if (file.read(buffer.data(), size)) {
            return ultralight::Buffer::CreateFromCopy(buffer.data(), buffer.size());
        }
        return nullptr;
    }

private:
    std::string base_dir_;
};

std::string GetExecutableDirectory() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string exe_path(path);
    return exe_path.substr(0, exe_path.find_last_of("\\/"));
}

// this is where we actually bind the C++ functions to the javascript context
void UltralightController::BindCallbacks() {
    if (!view_) return;
    ultralight::RefPtr<ultralight::JSContext> context = view_->LockJSContext();
    ultralight::SetJSContext(*context);
    ultralight::JSObject globalObj = ultralight::JSGlobalObject();

    for (auto const& [name, func] : callbacks_) {
        globalObj[name.c_str()] = func;
    }
    for (auto const& [name, func] : callbacks_with_ret_) {
        globalObj[name.c_str()] = func;
    }
}
// The constructor now uses our corrected AppFileSystem
UltralightController::UltralightController(ID3D11Device* device, ID3D11DeviceContext* context)
    : device_(device), context_(context) {
    auto exe_path = GetExecutableDirectory();

    ultralight::Platform& platform = ultralight::Platform::instance();
    ultralight::Config config;
    config.force_repaint = true;

    platform.set_config(config);
    platform.set_logger(new ConsoleLogger());

    platform.set_file_system(new AppFileSystem(exe_path));

    platform.set_font_loader(ultralight::GetPlatformFontLoader());
    renderer_ = ultralight::Renderer::Create();
}
UltralightController::~UltralightController() {
    view_ = nullptr;
    renderer_ = nullptr;
}

void UltralightController::AddCallback(const std::string& name, JSCallbackFunc callback) {
    callbacks_[name] = callback;
}

void UltralightController::AddCallback(const std::string& name, JSCallbackFuncWithRet callback) {
    callbacks_with_ret_[name] = callback;
}

void UltralightController::LoadHTML(const std::string& html_string) {
    if (!renderer_) return;
    ultralight::ViewConfig view_config;
    view_config.is_transparent = true;
    view_ = renderer_->CreateView(1000, 700, view_config, nullptr);
    view_->set_view_listener(this);
    view_->set_load_listener(this);
    view_->LoadHTML(html_string.c_str());
}

void UltralightController::Resize(int width, int height) {
    if (view_ && (width_ != width || height_ != height)) {
        width_ = width;
        height_ = height;
        view_->Resize(width, height);
        texture_view_ = nullptr;
    }
}

void UltralightController::Update() {
    if (renderer_) {
        renderer_->Update();
    }
}

void UltralightController::Render() {
    if (renderer_) {
        renderer_->Render();
    }
    UpdateTexture();
}

void UltralightController::UpdateTexture() {
    if (!view_) return;
    ultralight::Surface* surface = view_->surface();
    if (!surface) return;
    ultralight::IntRect dirty_bounds = surface->dirty_bounds();
    if (dirty_bounds.IsEmpty()) return;
    ultralight::BitmapSurface* bitmap_surface = static_cast<ultralight::BitmapSurface*>(surface);
    void* pixels = bitmap_surface->LockPixels();
    Microsoft::WRL::ComPtr<ID3D11Texture2D> pTexture = nullptr;
    if (!texture_view_) {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = bitmap_surface->width();
        desc.Height = bitmap_surface->height();
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        HRESULT hr = device_->CreateTexture2D(&desc, nullptr, &pTexture);
        if (FAILED(hr)) {
            bitmap_surface->UnlockPixels();
            return;
        }
        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Format = desc.Format;
        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MipLevels = 1;
        hr = device_->CreateShaderResourceView(pTexture.Get(), &srv_desc, &texture_view_);
        if (FAILED(hr)) {
            bitmap_surface->UnlockPixels();
            return;
        }
    }
    else {
        ID3D11Resource* resource = nullptr;
        texture_view_->GetResource(&resource);
        resource->QueryInterface<ID3D11Texture2D>(&pTexture);
        resource->Release();
    }
    D3D11_MAPPED_SUBRESOURCE mapped;
    context_->Map(pTexture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    const uint8_t* src_pixels = static_cast<const uint8_t*>(pixels);
    uint8_t* dest_pixels = static_cast<uint8_t*>(mapped.pData);
    for (uint32_t y = 0; y < bitmap_surface->height(); ++y) {
        memcpy(dest_pixels, src_pixels, bitmap_surface->width() * 4);
        src_pixels += bitmap_surface->row_bytes();
        dest_pixels += mapped.RowPitch;
    }
    context_->Unmap(pTexture.Get(), 0);
    bitmap_surface->UnlockPixels();
    surface->ClearDirtyBounds();
}

void UltralightController::OnDOMReady(ultralight::View* caller, uint64_t frame_id, bool is_main_frame, const ultralight::String& url) {
    if (is_main_frame) {
        BindCallbacks();

    }
}
void UltralightController::OnChangeTitle(ultralight::View* caller, const ultralight::String& title) {}
void UltralightController::OnChangeURL(ultralight::View* caller, const ultralight::String& url) {}
void UltralightController::OnChangeTooltip(ultralight::View* caller, const ultralight::String& tooltip) {}
void UltralightController::OnBeginLoading(ultralight::View* caller, uint64_t frame_id, bool is_main_frame, const ultralight::String& url) {}
void UltralightController::OnChangeCursor(ultralight::View* caller, ultralight::Cursor cursor) {}
void UltralightController::OnAddConsoleMessage(ultralight::View* caller, const ultralight::ConsoleMessage& message) {
    std::wcout << L"[Console]: " << message.message().utf16().data() << std::endl;
}
void UltralightController::OnFinishLoading(ultralight::View* caller, uint64_t frame_id, bool is_main_frame, const ultralight::String& url) {
    if (is_main_frame) {
        needs_texture_update_ = true;
    }
}
void UltralightController::OnFailLoading(ultralight::View* caller, uint64_t frame_id, bool is_main_frame, const ultralight::String& url, const ultralight::String& description, const ultralight::String& error_domain, int error_code) {
    std::wcout << L"[Load Error]: " << description.utf16().data() << std::endl;
}



// add this new init function
void UltralightController::init() {
    auto exe_path = GetExecutableDirectory();
    ultralight::Platform& platform = ultralight::Platform::instance();
    platform.set_config(config_);
    platform.set_logger(new ConsoleLogger());
    platform.set_file_system(ultralight::GetPlatformFileSystem(exe_path.c_str()));
    platform.set_font_loader(ultralight::GetPlatformFontLoader());
    renderer_ = ultralight::Renderer::Create();
}

// add these new helper function implementations
void UltralightController::setCachePath(const std::string& path) {
    config_.cache_path = path.c_str();
}

void UltralightController::setResourcePrefix(const std::string& prefix) {
    config_.resource_path_prefix = prefix.c_str();
}

void UltralightController::setFaceWinding(ultralight::FaceWinding winding) {
    config_.face_winding = winding;
}

void UltralightController::setFontHinting(ultralight::FontHinting hinting) {
    config_.font_hinting = hinting;
}

void UltralightController::setFontGamma(double gamma) {
    config_.font_gamma = gamma;
}

void UltralightController::setUserCSS(const std::string& css) {
    config_.user_stylesheet = css.c_str();
}

void UltralightController::forceRepaint(bool force) {
    config_.force_repaint = force;
}

void UltralightController::setAnimTimerDelay(double delay) {
    config_.animation_timer_delay = delay;
}

void UltralightController::setScrollTimerDelay(double delay) {
    config_.scroll_timer_delay = delay;
}

void UltralightController::setRecycleDelay(double delay) {
    config_.recycle_delay = delay;
}

void UltralightController::setMemCacheSize(uint32_t size) {
    config_.memory_cache_size = size;
}

void UltralightController::setPageCacheSize(uint32_t size) {
    config_.page_cache_size = size;
}

void UltralightController::setRAMOverride(uint32_t size) {
    config_.override_ram_size = size;
}

void UltralightController::setMinLargeHeap(uint32_t size) {
    config_.min_large_heap_size = size;
}

void UltralightController::setMinSmallHeap(uint32_t size) {
    config_.min_small_heap_size = size;
}

void UltralightController::setRenderThreads(uint32_t num_threads) {
    config_.num_renderer_threads = num_threads;
}

void UltralightController::setMaxUpdateTime(double time) {
    config_.max_update_time = time;
}

void UltralightController::setBitmapAlign(uint32_t alignment) {
    config_.bitmap_alignment = alignment;
}

void UltralightController::setEffectQuality(ultralight::EffectQuality quality) {
    config_.effect_quality = quality;
}

// new helper to safely get the js context
JSContextRef UltralightController::getJSContext() {
    if (!view_) return nullptr;
    auto context = view_->LockJSContext();
    if (!context) return nullptr;
    return context->ctx();
}

// new helper to convert a js error value to a c++ string
std::string UltralightController::jsValueToErrorString(JSValueRef err) {
    if (!err) return "";
    JSContextRef ctx = getJSContext();
    if (!ctx) return "Could not get JS context for error reporting.";

    JSStringRef err_str_ref = JSValueToStringCopy(ctx, err, nullptr);
    size_t len = JSStringGetMaximumUTF8CStringSize(err_str_ref);
    std::vector<char> buffer(len);
    JSStringGetUTF8CString(err_str_ref, buffer.data(), len);
    JSStringRelease(err_str_ref);
    return std::string(buffer.data());
}

// fixed evalScript
std::optional<std::string> UltralightController::evalScript(const std::string& script) {
    if (!view_) return std::nullopt;
    ultralight::RefPtr<ultralight::JSContext> context = view_->LockJSContext();
    if (!context) return std::nullopt;

    ultralight::SetJSContext(*context);
    ultralight::JSValue result = view_->EvaluateScript(script.c_str());

    if (result.IsString()) {
        return ultralight::String(result.ToString()).utf8().data();
    }

    return ""; // represents undefined or non-string result
}

// fixed checkScriptSyntax (renamed from checkScript)
bool UltralightController::checkScriptSyntax(const std::string& script) {
    JSContextRef ctx = getJSContext();
    if (!ctx) return false;

    JSStringRef script_ref = JSStringCreateWithUTF8CString(script.c_str());
    JSValueRef exception = nullptr;
    bool result = JSCheckScriptSyntax(ctx, script_ref, nullptr, 1, &exception);
    JSStringRelease(script_ref);

    if (exception) {
        std::wcout << L"[JSSyntaxError]: " << jsValueToErrorString(exception).c_str() << std::endl;
    }

    return result;
}

// fixed runGC (renamed to forceGC)
void UltralightController::forceGC() {
    JSContextRef ctx = getJSContext();
    if (ctx) {
        JSGarbageCollect(ctx);
    }
}

// -- new js value creation helpers --
JSValueRef UltralightController::createJSString(const std::string& str) {
    JSContextRef ctx = getJSContext();
    if (!ctx) return nullptr;
    JSStringRef str_ref = JSStringCreateWithUTF8CString(str.c_str());
    JSValueRef val = JSValueMakeString(ctx, str_ref);
    JSStringRelease(str_ref);
    return val;
}

JSValueRef UltralightController::createJSNumber(double num) {
    JSContextRef ctx = getJSContext();
    if (!ctx) return nullptr;
    return JSValueMakeNumber(ctx, num);
}

JSValueRef UltralightController::createJSBoolean(bool val) {
    JSContextRef ctx = getJSContext();
    if (!ctx) return nullptr;
    return JSValueMakeBoolean(ctx, val);
}

JSValueRef UltralightController::createJSNull() {
    JSContextRef ctx = getJSContext();
    if (!ctx) return nullptr;
    return JSValueMakeNull(ctx);
}

JSValueRef UltralightController::createJSUndefined() {
    JSContextRef ctx = getJSContext();
    if (!ctx) return nullptr;
    return JSValueMakeUndefined(ctx);
}


// -- new js value conversion and type checking helpers --
std::string UltralightController::jsValueToCppString(JSValueRef val) {
    JSContextRef ctx = getJSContext();
    if (!ctx || !val || !JSValueIsString(ctx, val)) return "";

    JSValueRef exception = nullptr;
    JSStringRef str_ref = JSValueToStringCopy(ctx, val, &exception);
    if (exception) {
        return "[Error converting to string]";
    }

    size_t max_len = JSStringGetMaximumUTF8CStringSize(str_ref);
    std::vector<char> buffer(max_len);
    JSStringGetUTF8CString(str_ref, buffer.data(), max_len);
    JSStringRelease(str_ref);

    return std::string(buffer.data());
}

double UltralightController::jsValueToNumber(JSValueRef val, std::string& err) {
    JSContextRef ctx = getJSContext();
    if (!ctx || !val) {
        err = "context or value is null";
        return 0.0;
    }
    JSValueRef exception = nullptr;
    double result = JSValueToNumber(ctx, val, &exception);
    if (exception) {
        err = jsValueToErrorString(exception);
    }
    return result;
}

bool UltralightController::jsValueToBoolean(JSValueRef val) {
    JSContextRef ctx = getJSContext();
    if (!ctx || !val) return false;
    return JSValueToBoolean(ctx, val);
}

JSType UltralightController::getJSValueType(JSValueRef val) {
    JSContextRef ctx = getJSContext();
    if (!ctx || !val) return kJSTypeUndefined;
    return JSValueGetType(ctx, val);
}

bool UltralightController::isValueUndefined(JSValueRef val) {
    JSContextRef ctx = getJSContext();
    if (!ctx || !val) return true;
    return JSValueIsUndefined(ctx, val);
}

bool UltralightController::isValueNull(JSValueRef val) {
    JSContextRef ctx = getJSContext();
    if (!ctx || !val) return false;
    return JSValueIsNull(ctx, val);
}

bool UltralightController::isValueBoolean(JSValueRef val) {
    JSContextRef ctx = getJSContext();
    if (!ctx || !val) return false;
    return JSValueIsBoolean(ctx, val);
}

bool UltralightController::isValueNumber(JSValueRef val) {
    JSContextRef ctx = getJSContext();
    if (!ctx || !val) return false;
    return JSValueIsNumber(ctx, val);
}

bool UltralightController::isValueString(JSValueRef val) {
    JSContextRef ctx = getJSContext();
    if (!ctx || !val) return false;
    return JSValueIsString(ctx, val);
}

bool UltralightController::isValueObject(JSValueRef val) {
    JSContextRef ctx = getJSContext();
    if (!ctx || !val) return false;
    return JSValueIsObject(ctx, val);
}


// -- new js object property helpers --
bool UltralightController::hasProperty(JSObjectRef obj, const std::string& propName) {
    JSContextRef ctx = getJSContext();
    if (!ctx || !obj) return false;

    JSStringRef prop_ref = JSStringCreateWithUTF8CString(propName.c_str());
    bool result = JSObjectHasProperty(ctx, obj, prop_ref);
    JSStringRelease(prop_ref);
    return result;
}

JSValueRef UltralightController::getProperty(JSObjectRef obj, const std::string& propName, std::string& err) {
    JSContextRef ctx = getJSContext();
    if (!ctx || !obj) {
        err = "context or object is null";
        return nullptr;
    }

    JSStringRef prop_ref = JSStringCreateWithUTF8CString(propName.c_str());
    JSValueRef exception = nullptr;
    JSValueRef result = JSObjectGetProperty(ctx, obj, prop_ref, &exception);
    JSStringRelease(prop_ref);

    if (exception) {
        err = jsValueToErrorString(exception);
    }
    return result;
}

void UltralightController::setProperty(JSObjectRef obj, const std::string& propName, JSValueRef val, std::string& err) {
    JSContextRef ctx = getJSContext();
    if (!ctx || !obj) {
        err = "context or object is null";
        return;
    }

    JSStringRef prop_ref = JSStringCreateWithUTF8CString(propName.c_str());
    JSValueRef exception = nullptr;
    JSObjectSetProperty(ctx, obj, prop_ref, val, kJSPropertyAttributeNone, &exception);
    JSStringRelease(prop_ref);

    if (exception) {
        err = jsValueToErrorString(exception);
    }
}

bool UltralightController::deleteProperty(JSObjectRef obj, const std::string& propName, std::string& err) {
    JSContextRef ctx = getJSContext();
    if (!ctx || !obj) {
        err = "context or object is null";
        return false;
    }

    JSStringRef prop_ref = JSStringCreateWithUTF8CString(propName.c_str());
    JSValueRef exception = nullptr;
    bool result = JSObjectDeleteProperty(ctx, obj, prop_ref, &exception);
    JSStringRelease(prop_ref);

    if (exception) {
        err = jsValueToErrorString(exception);
    }
    return result;
}

std::vector<std::string> UltralightController::getPropertyNames(JSObjectRef obj) {
    std::vector<std::string> names;
    JSContextRef ctx = getJSContext();
    if (!ctx || !obj) return names;

    JSPropertyNameArrayRef names_ref = JSObjectCopyPropertyNames(ctx, obj);
    size_t count = JSPropertyNameArrayGetCount(names_ref);

    for (size_t i = 0; i < count; ++i) {
        JSStringRef name_ref = JSPropertyNameArrayGetNameAtIndex(names_ref, i);
        size_t max_len = JSStringGetMaximumUTF8CStringSize(name_ref);
        std::vector<char> buffer(max_len);
        JSStringGetUTF8CString(name_ref, buffer.data(), max_len);
        names.push_back(std::string(buffer.data()));
    }

    JSPropertyNameArrayRelease(names_ref);
    return names;
}

// -- new js function calling helper --
JSValueRef UltralightController::callFunction(JSObjectRef func, JSObjectRef thisObj, const std::vector<JSValueRef>& args, std::string& err) {
    JSContextRef ctx = getJSContext();
    if (!ctx || !func) {
        err = "context or function is null";
        return nullptr;
    }

    if (!JSObjectIsFunction(ctx, func)) {
        err = "provided object is not a function";
        return nullptr;
    }

    JSValueRef exception = nullptr;
    JSValueRef result = JSObjectCallAsFunction(ctx, func, thisObj, args.size(), args.data(), &exception);

    if (exception) {
        err = jsValueToErrorString(exception);
    }

    return result;
}

// -- Typed Array and Array Buffer Helpers --
JSObjectRef UltralightController::createTypedArray(JSTypedArrayType arrayType, size_t length, std::string& err) {
    JSContextRef ctx = getJSContext();
    if (!ctx) {
        err = "context is null";
        return nullptr;
    }
    JSValueRef exception = nullptr;
    JSObjectRef result = JSObjectMakeTypedArray(ctx, arrayType, length, &exception);
    if (exception) {
        err = jsValueToErrorString(exception);
    }
    return result;
}

JSObjectRef UltralightController::createTypedArrayWithBytes(JSTypedArrayType arrayType, void* bytes, size_t byteLength, std::string& err) {
    JSContextRef ctx = getJSContext();
    if (!ctx) {
        err = "context is null";
        return nullptr;
    }
    JSValueRef exception = nullptr;
    // passing null for deallocator, user manages memory
    JSObjectRef result = JSObjectMakeTypedArrayWithBytesNoCopy(ctx, arrayType, bytes, byteLength, nullptr, nullptr, &exception);
    if (exception) {
        err = jsValueToErrorString(exception);
    }
    return result;
}

JSObjectRef UltralightController::createArrayBufferWithBytes(void* bytes, size_t byteLength, std::string& err) {
    JSContextRef ctx = getJSContext();
    if (!ctx) {
        err = "context is null";
        return nullptr;
    }
    JSValueRef exception = nullptr;
    // passing null for deallocator, user manages memory
    JSObjectRef result = JSObjectMakeArrayBufferWithBytesNoCopy(ctx, bytes, byteLength, nullptr, nullptr, &exception);
    if (exception) {
        err = jsValueToErrorString(exception);
    }
    return result;
}

void* UltralightController::getTypedArrayBytes(JSObjectRef object, std::string& err) {
    JSContextRef ctx = getJSContext();
    if (!ctx || !object) {
        err = "context or object is null";
        return nullptr;
    }
    JSValueRef exception = nullptr;
    void* result = JSObjectGetTypedArrayBytesPtr(ctx, object, &exception);
    if (exception) {
        err = jsValueToErrorString(exception);
    }
    return result;
}

size_t UltralightController::getTypedArrayLength(JSObjectRef object, std::string& err) {
    JSContextRef ctx = getJSContext();
    if (!ctx || !object) {
        err = "context or object is null";
        return 0;
    }
    JSValueRef exception = nullptr;
    size_t result = JSObjectGetTypedArrayLength(ctx, object, &exception);
    if (exception) {
        err = jsValueToErrorString(exception);
    }
    return result;
}

size_t UltralightController::getTypedArrayByteLength(JSObjectRef object, std::string& err) {
    JSContextRef ctx = getJSContext();
    if (!ctx || !object) {
        err = "context or object is null";
        return 0;
    }
    JSValueRef exception = nullptr;
    size_t result = JSObjectGetTypedArrayByteLength(ctx, object, &exception);
    if (exception) {
        err = jsValueToErrorString(exception);
    }
    return result;
}

JSObjectRef UltralightController::getTypedArrayBuffer(JSObjectRef object, std::string& err) {
    JSContextRef ctx = getJSContext();
    if (!ctx || !object) {
        err = "context or object is null";
        return nullptr;
    }
    JSValueRef exception = nullptr;
    JSObjectRef result = JSObjectGetTypedArrayBuffer(ctx, object, &exception);
    if (exception) {
        err = jsValueToErrorString(exception);
    }
    return result;
}