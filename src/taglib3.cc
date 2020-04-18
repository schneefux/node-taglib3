#include <v8.h>
#include <nan.h>
#include <node.h>
#include <node_buffer.h>

#define NDEBUG
#define TAGLIB_STATIC
#include <taglib/fileref.h>
#include <taglib/tfile.h>
#include <taglib/tpropertymap.h>

using namespace v8;
using namespace node;

// TagLib string -> V8 string
v8::Local<v8::String> TagLibStringToString(TagLib::String s) {
  return Nan::New<v8::String>(s.toCString(true)).ToLocalChecked();
}

// V8 string -> TagLib string
TagLib::String StringToTagLibString(v8::Local<v8::String> s) {
  return TagLib::String(*Nan::Utf8String(s), TagLib::String::UTF8);
}

NAN_METHOD(writeTagsSync) {
  Nan::HandleScope scope;
  v8::Local<v8::Context> context = Nan::GetCurrentContext();
  Local<v8::Object> options;

  if (info.Length() < 2) {
    Nan::ThrowTypeError("Expected 2 arguments");
    return;
  }

  if (!info[0]->IsString()) {
    Nan::ThrowTypeError("Expected a string");
    return;
  }

  if (!info[1]->IsObject()) {
    Nan::ThrowTypeError("Expected an object");
    return;
  }

  options = v8::Local<v8::Object>::Cast(info[1]);

  Nan::MaybeLocal<v8::String> audio_path_maybe = Nan::To<v8::String>(info[0]);
  if (audio_path_maybe.IsEmpty()) {
    Nan::ThrowTypeError("Audio file not found");
    return;
  }

  auto audio_path = StringToTagLibString(audio_path_maybe.ToLocalChecked()).toCString();
  TagLib::FileRef f(audio_path, false);

  if (f.isNull()) {
    Nan::ThrowTypeError("Could not parse file");
    return;
  }

  TagLib::PropertyMap map = f.file()->properties();

  // TODO type check
  Local<Array> property_names = options->GetOwnPropertyNames(context).ToLocalChecked();
  for (int i = 0; i < property_names->Length(); ++i) {
    auto key = property_names->Get(context, i).ToLocalChecked().As<v8::String>();

    TagLib::StringList list = TagLib::StringList();

    auto values = options->Get(context, key).ToLocalChecked().As<v8::Array>();
    for (int j = 0; j < values->Length(); ++j) {
      auto value = values->Get(context, j).ToLocalChecked().As<v8::String>();
      list.append(StringToTagLibString(value));
    }

    map.erase(StringToTagLibString(key)); // will merge both lists if not erased
    map.insert(StringToTagLibString(key), list);
  }

  if (map.size() > 0) {
    f.file()->setProperties(map);
    f.file()->save();
  }

  info.GetReturnValue().Set(Nan::True());
}

NAN_METHOD(readTagsSync) {
  Nan::HandleScope scope;
  v8::Local<v8::Context> context = Nan::GetCurrentContext();

  Nan::MaybeLocal<v8::String> audio_path_maybe = Nan::To<v8::String>(info[0]);
  if (audio_path_maybe.IsEmpty()) {
    Nan::ThrowTypeError("Audio file not found");
    return;
  }

  auto audio_path = StringToTagLibString(audio_path_maybe.ToLocalChecked()).toCString();
  TagLib::FileRef f(audio_path, false);

  if (f.isNull()) {
    Nan::ThrowTypeError("Could not parse file");
    return;
  }

  TagLib::PropertyMap map = f.file()->properties();
  v8::Local<v8::Object> obj = Nan::New<v8::Object>();

  for (TagLib::PropertyMap::ConstIterator i = map.begin(); i != map.end(); ++i) {
    v8::Local<v8::Array> array = Nan::New<v8::Array>(i->second.size());
    int index = 0;
    for (TagLib::StringList::ConstIterator j = i->second.begin(); j != i->second.end(); ++j) {
      array->Set(context, index++, TagLibStringToString(*j));
    }

    obj->Set(context, TagLibStringToString(i->first), array);
  }

  info.GetReturnValue().Set(obj);
}

void Init(v8::Local<v8::Object> exports, v8::Local<v8::Value> module, void *) {
  v8::Local<v8::Context> context = Nan::GetCurrentContext();
  exports->Set(context,
    Nan::New("writeTagsSync").ToLocalChecked(),
    Nan::New<v8::FunctionTemplate>(writeTagsSync)->GetFunction(context).ToLocalChecked()
  );

  exports->Set(context,
    Nan::New("readTagsSync").ToLocalChecked(),
    Nan::New<v8::FunctionTemplate>(readTagsSync)->GetFunction(context).ToLocalChecked()
  );
}

NODE_MODULE(taglib3, Init)
