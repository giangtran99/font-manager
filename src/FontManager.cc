#include <stdlib.h>
#include <node_api.h>
#include <uv.h>
#include "FontDescriptor.h"

// Platform-specific function declarations
ResultSet* getAvailableFontsImpl();
ResultSet* findFontsImpl(FontDescriptor*);
FontDescriptor* findFontImpl(FontDescriptor*);
FontDescriptor* substituteFontImpl(char*, char*);

// converts a ResultSet to a JavaScript array
napi_value collectResults(napi_env env, ResultSet* results) {
    napi_value resultArray;
    napi_create_array_with_length(env, results->size(), &resultArray);

    int i = 0;
    for (ResultSet::iterator it = results->begin(); it != results->end(); ++it) {
        napi_value jsObject = (*it)->toJSObject(env);
        napi_set_element(env, resultArray, i++, jsObject);
    }

    delete results;
    return resultArray;
}

// Helper: Converts a FontDescriptor to a JavaScript object
napi_value wrapResult(napi_env env, FontDescriptor* result) {
    if (result == nullptr) {
        napi_value nullValue;
        napi_get_null(env, &nullValue);
        return nullValue;
    }

    napi_value jsObject = result->toJSObject(env);
    delete result;
    return jsObject;
}

// Async request structure for passing data
struct AsyncRequest {
    uv_work_t work;
  FontDescriptor *desc;     // used by findFont and findFonts
  char *postscriptName;     // used by substituteFont
  char *substitutionString; // ditto
  FontDescriptor *result;   // for functions with a single result
  ResultSet *results;       // for functions with multiple results
  napi_ref callback;        // the actual JS callback to call when we are done
  napi_env env = nullptr;    

    AsyncRequest(napi_ref callback_ref) {
      work.data = (void *)this;
      callback = callback_ref;
      desc = nullptr;
      postscriptName = nullptr;
      substitutionString = nullptr;
      result = nullptr;
      results = nullptr;
    }
    
  ~AsyncRequest() {
    delete callback;

    if (desc)
      delete desc;

    if (postscriptName)
      delete postscriptName;

    if (substitutionString)
      delete substitutionString;

    // result/results deleted by wrapResult/collectResults respectively
  }
};

// calls the JavaScript callback for a request
void asyncCallback(uv_work_t *work) {
  AsyncRequest *req = (AsyncRequest *) work->data;
  napi_value resource_name;
  napi_create_string_utf8(req->env, "asyncCallback", NAPI_AUTO_LENGTH, &resource_name);

  napi_async_context async_context;
  napi_async_init(req->env, nullptr, resource_name, &async_context);

  napi_value argv[1];
  if (req->results) {
    argv[0] = collectResults(req->env, req->results);
  } else if (req->result) {
    argv[0] = wrapResult(req->env, req->result);
  } else {
    napi_get_null(req->env, &argv[0]);
  }

  napi_value global;
  napi_get_global(req->env, &global);

  napi_value callback;
  napi_get_reference_value(req->env, req->callback, &callback);
  napi_make_callback(req->env, async_context, global, callback, 1, argv, nullptr);
  napi_async_destroy(req->env, async_context);

  delete req;
}

void getAvailableFontsAsync(uv_work_t *work) {
  AsyncRequest *req = (AsyncRequest *) work->data;
  req->results = getAvailableFontsImpl();
}


template<bool async>
napi_value getAvailableFonts(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
  if (async) {
    napi_valuetype type;
    napi_typeof(env, argv[0], &type);
    if (type != napi_function) {
      napi_throw_type_error(env, nullptr, "Expected a callback");
      return nullptr;
    }
    napi_ref callback_ref;
    napi_create_reference(env, argv[0], 1, &callback_ref);
    AsyncRequest* req = new AsyncRequest(callback_ref);
    req->env = env;
    uv_queue_work(uv_default_loop(), &req->work, getAvailableFontsAsync, (uv_after_work_cb) asyncCallback);

    return nullptr;
  } else {
    return collectResults(env, getAvailableFontsImpl());
  }
}
void findFontsAsync(uv_work_t *work) {
  AsyncRequest *req = (AsyncRequest *) work->data;
  req->results = findFontsImpl(req->desc);
}

template<bool async>
napi_value findFonts(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value argv[2];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  if (argc < 1) {
    napi_throw_type_error(env, nullptr, "Expected a font descriptor");
    return nullptr;
  }

  FontDescriptor* descriptor = new FontDescriptor(env, argv[0]);

  if (async) {
    napi_valuetype type;
    napi_typeof(env, argv[1], &type);
    if (argc < 2 || type != napi_function) {
      napi_throw_type_error(env, nullptr, "Expected a callback");
      delete descriptor;
      return nullptr;
    }
    napi_ref callback_ref;
    napi_create_reference(env, argv[1], 1, &callback_ref);
    AsyncRequest* req = new AsyncRequest(callback_ref);
    req->desc = descriptor;
    req->env = env;
    uv_queue_work(uv_default_loop(), &req->work, findFontsAsync, (uv_after_work_cb) asyncCallback);
    return nullptr;
  } else {
    napi_value result = collectResults(env, findFontsImpl(descriptor));
    delete descriptor;
    return result;
  }
}
void findFontAsync(uv_work_t *work) {
  AsyncRequest *req = (AsyncRequest *) work->data;
  req->result = findFontImpl(req->desc);
}

template <bool async>
napi_value findFont(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value argv[2];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  napi_valuetype type;
  napi_typeof(env, argv[0], &type);
  if (type != napi_object || type == napi_function) {
    napi_throw_type_error(env, nullptr, "Expected a font descriptor");
    return nullptr;
  }
  FontDescriptor* descriptor = new FontDescriptor(env, argv[0]);

  if (async) {
    napi_typeof(env, argv[1], &type);
    if (type != napi_function) {
      napi_throw_type_error(env, nullptr, "Expected a callback");
      return nullptr;
    }
    napi_ref callback_ref;
    napi_create_reference(env, argv[1], 1, &callback_ref);
    AsyncRequest* req = new AsyncRequest(callback_ref);
    req->env = env;
    req->desc = descriptor;
    uv_queue_work(uv_default_loop(), &req->work, findFontAsync, (uv_after_work_cb) asyncCallback);
    return nullptr;
  } else {
    napi_value result = wrapResult(env, findFontImpl(descriptor));
    delete descriptor;
    return result;
  }
}

void substituteFontAsync(uv_work_t *work) {
  AsyncRequest *req = (AsyncRequest *) work->data;
  req->result = substituteFontImpl(req->postscriptName, req->substitutionString);
}

template<bool async>
napi_value substituteFont(napi_env env, napi_callback_info info) {
  size_t argc = 3;
  napi_value argv[3];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  // Validate the first argument (postscriptName)
  napi_valuetype type;
  size_t postscript_len;
  napi_typeof(env, argv[0], &type);
  if (argc < 1 || napi_string) {
    napi_throw_type_error(env, nullptr, "Expected postscript name");
    return nullptr;
  }
  napi_get_value_string_utf8(env, argv[0], nullptr, 0, &postscript_len);

  size_t substitution_len;
  napi_typeof(env, argv[1], &type);
  if (argc < 2 || type != napi_string) {
    napi_throw_type_error(env, nullptr, "Expected substitution string");
    return nullptr;
  }
  napi_get_value_string_utf8(env, argv[1], nullptr, 0, &substitution_len);

  if (async) {
    
    napi_typeof(env, argv[2], &type);
    if (argc < 3 || type != napi_function) {
      napi_throw_type_error(env, nullptr, "Expected a callback");
      return nullptr;
    }
    // Allocate memory and copy the strings
    char* postscriptName = new char[postscript_len + 1];
    napi_get_value_string_utf8(env, argv[0], postscriptName, postscript_len + 1, nullptr);

    char* substitutionString = new char[substitution_len + 1];
    napi_get_value_string_utf8(env, argv[1], substitutionString, substitution_len + 1, nullptr);

    // Create an async request
    napi_ref callback_ref;
    napi_create_reference(env, argv[2], 1, &callback_ref);
    AsyncRequest* req = new AsyncRequest(callback_ref);
    req->postscriptName = postscriptName;
    req->substitutionString = substitutionString;
    req->env = env;
    uv_queue_work(uv_default_loop(),  &req->work, substituteFontAsync, (uv_after_work_cb) asyncCallback);

    // Return undefined for async case
    napi_value undefined;
    napi_get_undefined(env, &undefined);
    return nullptr;
  } else {
    // Synchronous execution
    char* postscriptName = new char[postscript_len + 1];
    napi_get_value_string_utf8(env, argv[0], postscriptName, postscript_len + 1, nullptr);

    char* substitutionString = new char[substitution_len + 1];
    napi_get_value_string_utf8(env, argv[1], substitutionString, substitution_len + 1, nullptr);

    delete[] postscriptName;
    delete[] substitutionString;

    return wrapResult(env, substituteFontImpl(postscriptName, substitutionString));
  }
}

// Remaining functions (similar pattern) ...

// Module Initialization
napi_value Init(napi_env env, napi_value exports) {
  napi_status status;

  // Add "getAvailableFonts" function
  napi_value getAvailableFontsFunction;
  status = napi_create_function(env, nullptr, 0, getAvailableFonts<true>, nullptr, &getAvailableFontsFunction);
  if (status != napi_ok) return nullptr;
  status = napi_set_named_property(env, exports, "getAvailableFonts", getAvailableFontsFunction);
  if (status != napi_ok) return nullptr;

  // Add "getAvailableFontsSync" function
  napi_value getAvailableFontsSyncFunction;
  status = napi_create_function(env, nullptr, 0, getAvailableFonts<false>, nullptr, &getAvailableFontsSyncFunction);
  if (status != napi_ok) return nullptr;
  status = napi_set_named_property(env, exports, "getAvailableFontsSync", getAvailableFontsSyncFunction);
  if (status != napi_ok) return nullptr;

  // Add "findFonts" function
  napi_value findFontsFunction;
  status = napi_create_function(env, nullptr, 0, findFonts<true>, nullptr, &findFontsFunction);
  if (status != napi_ok) return nullptr;
  status = napi_set_named_property(env, exports, "findFonts", findFontsFunction);
  if (status != napi_ok) return nullptr;

  // Add "findFontsSync" function
  napi_value findFontsSyncFunction;
  status = napi_create_function(env, nullptr, 0, findFonts<false>, nullptr, &findFontsSyncFunction);
  if (status != napi_ok) return nullptr;
  status = napi_set_named_property(env, exports, "findFontsSync", findFontsSyncFunction);
  if (status != napi_ok) return nullptr;

  // Add "findFont" function
  napi_value findFontFunction;
  status = napi_create_function(env, nullptr, 0, findFont<true>, nullptr, &findFontFunction);
  if (status != napi_ok) return nullptr;
  status = napi_set_named_property(env, exports, "findFont", findFontFunction);
  if (status != napi_ok) return nullptr;

  // Add "findFontSync" function
  napi_value findFontSyncFunction;
  status = napi_create_function(env, nullptr, 0, findFont<false>, nullptr, &findFontSyncFunction);
  if (status != napi_ok) return nullptr;
  status = napi_set_named_property(env, exports, "findFontSync", findFontSyncFunction);
  if (status != napi_ok) return nullptr;

  // Add "substituteFont" function
  napi_value substituteFontFunction;
  status = napi_create_function(env, nullptr, 0, substituteFont<true>, nullptr, &substituteFontFunction);
  if (status != napi_ok) return nullptr;
  status = napi_set_named_property(env, exports, "substituteFont", substituteFontFunction);
  if (status != napi_ok) return nullptr;

  // Add "substituteFontSync" function
  napi_value substituteFontSyncFunction;
  status = napi_create_function(env, nullptr, 0, substituteFont<false>, nullptr, &substituteFontSyncFunction);
  if (status != napi_ok) return nullptr;
  status = napi_set_named_property(env, exports, "substituteFontSync", substituteFontSyncFunction);
  if (status != napi_ok) return nullptr;

  return exports;
}


NAPI_MODULE(fontmanager, Init)
