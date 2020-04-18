#include <errno.h>
#include <string.h>

#include <v8.h>
#include <nan.h>
#include <node.h>
#include <node_buffer.h>

#include <fstream>
#include <cctype>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>

#define NDEBUG
#define TAGLIB_STATIC
#include <taglib/tag.h>
#include <taglib/tlist.h>
#include <taglib/fileref.h>
#include <taglib/tfile.h>
#include <taglib/flacfile.h>
#include <taglib/mp4tag.h>
#include <taglib/mp4atom.h>
#include <taglib/mp4file.h>
#include <taglib/tpicturemap.h>
#include <taglib/tpropertymap.h>
#include <taglib/tbytevector.h>
#include <taglib/tbytevectorlist.h>

using namespace std;
using namespace v8;
using namespace node;

Local<Value> TagLibStringToString(TagLib::String s) {

  if (s.isEmpty()) return Nan::Null();

  TagLib::ByteVector str = s.data(TagLib::String::UTF16);

  return Nan::New<v8::String>(
    (uint16_t *) str.mid(2, str.size()-2).data(),
    s.size()
  ).ToLocalChecked();
}

TagLib::String StringToTagLibString(std::string s) {
  return TagLib::String(s, TagLib::String::UTF8);
}

bool isFile(const char *s) {
  struct stat st;
#ifdef _WIN32
  return ::stat(s, &st) == 0 && (st.st_mode & (S_IFREG));
#else
  return ::stat(s, &st) == 0 && (st.st_mode & (S_IFREG | S_IFLNK));
#endif
}

NAN_METHOD(writeTagsSync) {
  Nan::HandleScope scope;
  Local<v8::Object> options;

  if (info.Length() < 2) {
    Nan::ThrowTypeError("Expected 2 arguments");
    return;
  }

  if (!info[0]->IsString()) {
    Nan::ThrowTypeError("Expected a path to audio file");
    return;
  }

  if (!info[1]->IsObject()) return;

  options = v8::Local<v8::Object>::Cast(info[1]);

  Nan::MaybeLocal<v8::String> audio_file_maybe = Nan::To<v8::String>(info[0]);
  if(audio_file_maybe.IsEmpty()) {
    Nan::ThrowTypeError("Audio file not found");
    return;
  }

  std::string audio_file = *v8::String::Utf8Value(v8::Isolate::GetCurrent(), audio_file_maybe.ToLocalChecked());

  if (!isFile(audio_file.c_str())) {
    Nan::ThrowTypeError("Audio file not found");
    return;
  }

  TagLib::FileRef f(audio_file.c_str());
  TagLib::Tag *tag = f.tag();
  TagLib::PropertyMap map = f.properties();

  if (!tag) {
    Nan::ThrowTypeError("Could not parse file");
    return;
  }

  // TODO type check
  Local<Array> property_names = options->GetOwnPropertyNames(Nan::GetCurrentContext()).ToLocalChecked();
  for (int i = 0; i < property_names->Length(); ++i) {
    auto key = property_names->Get(Nan::New(i));

    TagLib::StringList list = TagLib::StringList();
    auto input_array = options->Get(Nan::GetCurrentContext(), key).ToLocalChecked().As<v8::Array>();
    for (int j = 0; j < input_array->Length(); ++j) {
      auto value = *Nan::Utf8String(input_array->Get(Nan::GetCurrentContext(), j).ToLocalChecked());
      TagLib::String st = TagLib::String(value);
      list.append(st);
    }

    auto list_key = *Nan::Utf8String(key);
    map.erase(TagLib::String(list_key));
    map.insert(TagLib::String(list_key), list);
  }

  if (map.size() > 0) {
    f.setProperties(map);
    f.save();
  }

  info.GetReturnValue().Set(Nan::True());
}

NAN_METHOD(readTagsSync) {
  Nan::HandleScope scope;

  Nan::MaybeLocal<v8::String> audio_file_maybe = Nan::To<v8::String>(info[0]);
  if(audio_file_maybe.IsEmpty()) {
    Nan::ThrowTypeError("Audio file not found");
    return;
  }

  std::string audio_file = *v8::String::Utf8Value(v8::Isolate::GetCurrent(), audio_file_maybe.ToLocalChecked());

  if (!isFile(audio_file.c_str())) {
    Nan::ThrowTypeError("Audio file not found");
    return;
  }

  string ext;
  const size_t pos = audio_file.find_last_of(".");

  if (pos != -1) {
    ext = audio_file.substr(pos + 1);

    for (std::string::size_type i = 0; i < ext.length(); ++i)
      ext[i] = std::toupper(ext[i]);
  }

  TagLib::FileRef f(audio_file.c_str());
  TagLib::Tag *tag = f.tag();
  TagLib::PropertyMap map = f.properties();

  if (!tag || f.isNull()) {
    Nan::ThrowTypeError("Could not parse file");
    return;
  }

  v8::Local<v8::Object> obj = Nan::New<v8::Object>();
  for (TagLib::PropertyMap::ConstIterator i = map.begin(); i != map.end(); ++i) {
    v8::Local<v8::Array> array = Nan::New<v8::Array>(i->second.size());
    int index = 0;
    for (TagLib::StringList::ConstIterator j = i->second.begin(); j != i->second.end(); ++j) {
      array->Set(index++, TagLibStringToString(*j));
    }

    obj->Set(
      Nan::GetCurrentContext(),
      Nan::New(i->first.toCString(true)).ToLocalChecked(),
      array
    );
  }

  info.GetReturnValue().Set(obj);
}

void Init(v8::Local<v8::Object> exports, v8::Local<v8::Value> module, void *) {
  auto test = exports->Set(
    Nan::GetCurrentContext(),
    Nan::New("writeTagsSync").ToLocalChecked(),
    Nan::New<v8::FunctionTemplate>(writeTagsSync)->GetFunction(Nan::GetCurrentContext()).ToLocalChecked()
  );

  test = exports->Set(
    Nan::GetCurrentContext(),
    Nan::New("readTagsSync").ToLocalChecked(),
    Nan::New<v8::FunctionTemplate>(readTagsSync)->GetFunction(Nan::GetCurrentContext()).ToLocalChecked()
  );
}

NODE_MODULE(taglib2, Init)
