#pragma once
#include <Ultralight/Ultralight.h>
#include <AppCore/AppCore.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <Ultralight/ConsoleMessage.h>
#include <filesystem>
#include <optional>

// define a type for our js callbacks to make it cleaner
using JSCallbackFunc = std::function<void(const ultralight::JSObject&, const ultralight::JSArgs&)>;
using JSCallbackFuncWithRet = std::function<ultralight::JSValue(const ultralight::JSObject&, const ultralight::JSArgs&)>;

class UltralightController : public ultralight::ViewListener, public ultralight::LoadListener {
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;

    ultralight::RefPtr<ultralight::Renderer> renderer_;
    ultralight::RefPtr<ultralight::View> view_;

    bool needs_texture_update_ = false;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture_view_ = nullptr;
    int width_ = 0;
    int height_ = 0;

    // maps to hold the functions we bind to javascript
    std::map<std::string, JSCallbackFunc> callbacks_;
    std::map<std::string, JSCallbackFuncWithRet> callbacks_with_ret_;


public:
    UltralightController(ID3D11Device* device, ID3D11DeviceContext* context);
    ~UltralightController();


    ID3D11ShaderResourceView* getTextureView() { return texture_view_.Get(); }


    void LoadHTML(const std::string& html_string);
    void Resize(int width, int height);
    void Update();
    void Render();

    // new way to add callbacks before the view is created
    void AddCallback(const std::string& name, JSCallbackFunc callback);
    void AddCallback(const std::string& name, JSCallbackFuncWithRet callback);

    ID3D11ShaderResourceView* GetTexture() { return texture_view_.Get(); }
    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }
    bool IsLoaded() const { return view_ && !view_->is_loading(); }
    bool IsRendererValid() const { return renderer_.get() != nullptr; }
    ultralight::View* GetView() { return view_.get(); }

    void init();

    // config helpers
    void setCachePath(const std::string& path);
    void setResourcePrefix(const std::string& prefix);
    void setFaceWinding(ultralight::FaceWinding winding);
    void setFontHinting(ultralight::FontHinting hinting);
    void setFontGamma(double gamma);
    void setUserCSS(const std::string& css);
    void forceRepaint(bool force);
    void setAnimTimerDelay(double delay);
    void setScrollTimerDelay(double delay);
    void setRecycleDelay(double delay);
    void setMemCacheSize(uint32_t size);
    void setPageCacheSize(uint32_t size);
    void setRAMOverride(uint32_t size);
    void setMinLargeHeap(uint32_t size);
    void setMinSmallHeap(uint32_t size);
    void setRenderThreads(uint32_t num_threads);
    void setMaxUpdateTime(double time);
    void setBitmapAlign(uint32_t alignment);
    void setEffectQuality(ultralight::EffectQuality quality);

    // js script evaluation
    std::optional<std::string> evalScript(const std::string& script);
    bool checkScriptSyntax(const std::string& script);
    void forceGC();

    // js value creation
    JSValueRef createJSString(const std::string& str);
    JSValueRef createJSNumber(double num);
    JSValueRef createJSBoolean(bool val);
    JSValueRef createJSNull();
    JSValueRef createJSUndefined();

    // js value conversion and type checking
    std::string jsValueToCppString(JSValueRef val);
    double jsValueToNumber(JSValueRef val, std::string& err);
    bool jsValueToBoolean(JSValueRef val);
    JSType getJSValueType(JSValueRef val);
    bool isValueUndefined(JSValueRef val);
    bool isValueNull(JSValueRef val);
    bool isValueBoolean(JSValueRef val);
    bool isValueNumber(JSValueRef val);
    bool isValueString(JSValueRef val);
    bool isValueObject(JSValueRef val);

    // js object property helpers
    bool hasProperty(JSObjectRef obj, const std::string& propName);
    JSValueRef getProperty(JSObjectRef obj, const std::string& propName, std::string& err);
    void setProperty(JSObjectRef obj, const std::string& propName, JSValueRef val, std::string& err);
    bool deleteProperty(JSObjectRef obj, const std::string& propName, std::string& err);
    std::vector<std::string> getPropertyNames(JSObjectRef obj);

    // js function calling
    JSValueRef callFunction(JSObjectRef func, JSObjectRef thisObj, const std::vector<JSValueRef>& args, std::string& err);

    // -- Typed Array and Array Buffer Helpers --
    JSObjectRef createTypedArray(JSTypedArrayType arrayType, size_t length, std::string& err);
    JSObjectRef createTypedArrayWithBytes(JSTypedArrayType arrayType, void* bytes, size_t byteLength, std::string& err);
    JSObjectRef createArrayBufferWithBytes(void* bytes, size_t byteLength, std::string& err);
    void* getTypedArrayBytes(JSObjectRef object, std::string& err);
    size_t getTypedArrayLength(JSObjectRef object, std::string& err);
    size_t getTypedArrayByteLength(JSObjectRef object, std::string& err);
    JSObjectRef getTypedArrayBuffer(JSObjectRef object, std::string& err);
    JSContextRef getJSContext();


private:
    ultralight::Config config_;

protected:
    void BindCallbacks();
    void UpdateTexture();

    // -- ViewListener --
    void OnChangeTitle(ultralight::View* caller, const ultralight::String& title) override;
    void OnChangeURL(ultralight::View* caller, const ultralight::String& url) override;
    void OnChangeTooltip(ultralight::View* caller, const ultralight::String& tooltip) override;
    void OnChangeCursor(ultralight::View* caller, ultralight::Cursor cursor) override;
    void OnAddConsoleMessage(ultralight::View* caller, const ultralight::ConsoleMessage& message) override;

    // -- LoadListener --
    void OnBeginLoading(ultralight::View* caller, uint64_t frame_id, bool is_main_frame, const ultralight::String& url) override;
    void OnFinishLoading(ultralight::View* caller, uint64_t frame_id, bool is_main_frame, const ultralight::String& url) override;
    void OnFailLoading(ultralight::View* caller, uint64_t frame_id, bool is_main_frame, const ultralight::String& url, const ultralight::String& description, const ultralight::String& error_domain, int error_code) override;
    void OnDOMReady(ultralight::View* caller, uint64_t frame_id, bool is_main_frame, const ultralight::String& url) override;

    
    std::string jsValueToErrorString(JSValueRef err);
};
