#ifndef FONT_DESCRIPTOR_H
#define FONT_DESCRIPTOR_H

#include <node.h>
#include <node_api.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

enum FontWeight {
  FontWeightUndefined   = 0,
  FontWeightThin        = 100,
  FontWeightUltraLight  = 200,
  FontWeightLight       = 300,
  FontWeightNormal      = 400,
  FontWeightMedium      = 500,
  FontWeightSemiBold    = 600,
  FontWeightBold        = 700,
  FontWeightUltraBold   = 800,
  FontWeightHeavy       = 900
};

enum FontWidth {
  FontWidthUndefined      = 0,
  FontWidthUltraCondensed = 1,
  FontWidthExtraCondensed = 2,
  FontWidthCondensed      = 3,
  FontWidthSemiCondensed  = 4,
  FontWidthNormal         = 5,
  FontWidthSemiExpanded   = 6,
  FontWidthExpanded       = 7,
  FontWidthExtraExpanded  = 8,
  FontWidthUltraExpanded  = 9
};

struct FontDescriptor {
public:
  const char *path;
  const char *postscriptName;
  const char *family;
  const char *style;
  FontWeight weight;
  FontWidth width;
  bool italic;
  bool monospace;

  FontDescriptor(napi_env env, napi_value obj) {
    path = nullptr;
    postscriptName = getString(env, obj, "postscriptName");
    family = getString(env, obj, "family");
    style = getString(env, obj, "style");
    weight = static_cast<FontWeight>(getNumber(env, obj, "weight"));
    width = static_cast<FontWidth>(getNumber(env, obj, "width"));
    italic = getBool(env, obj, "italic");
    monospace = getBool(env, obj, "monospace");
  }

  FontDescriptor() {
    path = NULL;
    postscriptName = NULL;
    family = NULL;
    style = NULL;
    weight = FontWeightUndefined;
    width = FontWidthUndefined;
    italic = false;
    monospace = false;
  }

  FontDescriptor(const char *path, const char *postscriptName, const char *family, const char *style,
                 FontWeight weight, FontWidth width, bool italic, bool monospace) {
    this->path = copyString(path);
    this->postscriptName = copyString(postscriptName);
    this->family = copyString(family);
    this->style = copyString(style);
    this->weight = weight;
    this->width = width;
    this->italic = italic;
    this->monospace = monospace;
  }

  FontDescriptor(FontDescriptor *desc) {
    path = copyString(desc->path);
    postscriptName = copyString(desc->postscriptName);
    family = copyString(desc->family);
    style = copyString(desc->style);
    weight = desc->weight;
    width = desc->width;
    italic = desc->italic;
    monospace = desc->monospace;
  }

  ~FontDescriptor() {
    if (path)
      delete path;

    if (postscriptName)
      delete postscriptName;

    if (family)
      delete family;

    if (style)
      delete style;

    postscriptName = NULL;
    family = NULL;
    style = NULL;
  }

  napi_value toJSObject(napi_env env) {
    napi_value res;
    napi_create_object(env, &res);

    if (path) {
      napi_value pathValue;
      napi_create_string_utf8(env, path, NAPI_AUTO_LENGTH, &pathValue);
      napi_set_named_property(env, res, "path", pathValue);
    }

    if (postscriptName) {
      napi_value psNameValue;
      napi_create_string_utf8(env, postscriptName, NAPI_AUTO_LENGTH, &psNameValue);
      napi_set_named_property(env, res, "postscriptName", psNameValue);
    }

    if (family) {
      napi_value familyValue;
      napi_create_string_utf8(env, family, NAPI_AUTO_LENGTH, &familyValue);
      napi_set_named_property(env, res, "family", familyValue);
    }

    if (style) {
      napi_value styleValue;
      napi_create_string_utf8(env, style, NAPI_AUTO_LENGTH, &styleValue);
      napi_set_named_property(env, res, "style", styleValue);
    }

    napi_value weightValue;
    napi_create_int32(env, weight, &weightValue);
    napi_set_named_property(env, res, "weight", weightValue);

    napi_value widthValue;
    napi_create_int32(env, width, &widthValue);
    napi_set_named_property(env, res, "width", widthValue);

    napi_value italicValue;
    napi_get_boolean(env, italic, &italicValue);
    napi_set_named_property(env, res, "italic", italicValue);

    napi_value monospaceValue;
    napi_get_boolean(env, monospace, &monospaceValue);
    napi_set_named_property(env, res, "monospace", monospaceValue);

    return res;
  }

private:
  char *copyString(const char *input) {
    if (!input) return nullptr;
    char *str = new char[strlen(input) + 1];
    strcpy(str, input);
    return str;
  }

  char *getString(napi_env env, napi_value obj, const char *name) {
    napi_value value;
    napi_status status = napi_get_named_property(env, obj, name, &value);

    if (status == napi_ok) {
      size_t strLength;
      napi_get_value_string_utf8(env, value, nullptr, 0, &strLength);
      char *str = new char[strLength + 1];
      napi_get_value_string_utf8(env, value, str, strLength + 1, nullptr);
      return str;
    }

    return nullptr;
  }

  int getNumber(napi_env env, napi_value obj, const char *name) {
    napi_value value;
    napi_status status = napi_get_named_property(env, obj, name, &value);

    if (status == napi_ok) {
      int32_t result;
      napi_get_value_int32(env, value, &result);
      return result;
    }

    return 0;
  }

  bool getBool(napi_env env, napi_value obj, const char *name) {
    napi_value value;
    napi_status status = napi_get_named_property(env, obj, name, &value);

    if (status == napi_ok) {
      bool result;
      napi_get_value_bool(env, value, &result);
      return result;
    }

    return false;
  }
};

class ResultSet : public std::vector<FontDescriptor *> {
public:
  ~ResultSet() {
    for (ResultSet::iterator it = this->begin(); it != this->end(); it++) {
      delete *it;
    }
  }
};

#endif
