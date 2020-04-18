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

  bool hasProps = false;

  auto hasOption = [&](
    Local<v8::Object> o,
    const std::string name) -> bool {
    v8::Maybe<bool> maybe_result = o->Has(Nan::GetCurrentContext(), Nan::New(name).ToLocalChecked());

    if(maybe_result.IsNothing()) {
      return false;
    } else {
      return maybe_result.ToChecked();
    }
  };

  auto getOptionString = [&](
      Local<v8::Object> o,
      const std::string name) -> TagLib::String {

    auto r = o->Get(Nan::GetCurrentContext(), Nan::New(name).ToLocalChecked());

    std::string st = *v8::String::Utf8Value(v8::Isolate::GetCurrent(), r.ToLocalChecked());
    return StringToTagLibString(st);
  };

  auto getOptionInt = [&](
      Local<v8::Object> o,
      const std::string name) -> int {

    auto v = o->Get(Nan::GetCurrentContext(), Nan::New(name).ToLocalChecked());

    Nan::Maybe<int> maybe = Nan::To<int32_t>(v.ToLocalChecked());

    return maybe.ToChecked();
  };

  if (hasOption(options, "albumartist")) {
    hasProps = true;
    TagLib::String value = getOptionString(options, "albumartist");
    map.erase(TagLib::String("ALBUMARTIST"));
    map.insert(TagLib::String("ALBUMARTIST"), value);
  }

  if (hasOption(options, "discnumber")) {
    hasProps = true;
    TagLib::String value = getOptionString(options, "discnumber");
    map.erase(TagLib::String("DISCNUMBER"));
    map.insert(TagLib::String("DISCNUMBER"), value);
  }

  if (hasOption(options, "tracknumber")) {
    hasProps = true;
    TagLib::String value = getOptionString(options, "tracknumber");
    map.erase(TagLib::String("TRACKNUMBER"));
    map.insert(TagLib::String("TRACKNUMBER"), value);
  }

  if (hasOption(options, "composer")) {
    hasProps = true;
    TagLib::String value = getOptionString(options, "composer");
    map.erase(TagLib::String("COMPOSER"));
    map.insert(TagLib::String("COMPOSER"), value);
  }

  if (hasOption(options, "id")) {
    hasProps = true;
    TagLib::String value = getOptionString(options, "id");
    map.erase(TagLib::String("ID"));
    map.insert(TagLib::String("ID"), value);
  }

  if (hasOption(options, "bpm")) {
    hasProps = true;
    TagLib::String value = getOptionString(options, "bpm");
    map.erase(TagLib::String("BPM"));
    map.insert(TagLib::String("BPM"), value);
  }

  if (hasProps) {
    f.setProperties(map);
  }

  if (hasOption(options, "artist")) {
    tag->setArtist(getOptionString(options, "artist"));
  }

  if (hasOption(options, "title")) {
    tag->setTitle(getOptionString(options, "title"));
  }

  if (hasOption(options, "album")) {
    tag->setAlbum(getOptionString(options, "album"));
  }

  if (hasOption(options, "comment")) {
    tag->setComment(getOptionString(options, "comment"));
  }

  if (hasOption(options, "genre")) {
    tag->setGenre(getOptionString(options, "genre"));
  }

  if (hasOption(options, "year")) {
    tag->setYear(getOptionInt(options, "year"));
  }

  if (hasOption(options, "track")) {
    tag->setTrack(getOptionInt(options, "track"));
  }

  if (hasOption(options, "pictures")) {
    auto pictures = options->Get(Nan::GetCurrentContext(), Nan::New("pictures").ToLocalChecked());
    Local<Array> pics = Local<Array>::Cast(pictures.ToLocalChecked());
    unsigned int plen = pics->Length();

    TagLib::PictureMap picMap;
    bool hasPics = false;

    for (unsigned int i = 0; i < plen; i++) {
      Local<v8::Object> imgObj = Handle<Object>::Cast(pics->Get(Nan::GetCurrentContext(), i).ToLocalChecked());

      if (!hasOption(imgObj, "mimetype")) {
        Nan::ThrowTypeError("mimetype required for each picture");
        return;
      }

      if (!hasOption(imgObj, "picture")) {
        Nan::ThrowTypeError("picture required for each item in pictures array");
        return;
      }

      auto mimetype = getOptionString(imgObj, "mimetype");
      auto picture = Nan::To<v8::Object>(
        imgObj->Get(Nan::GetCurrentContext(), Nan::New("picture").ToLocalChecked()).ToLocalChecked()
      );

      if (!picture.IsEmpty() && node::Buffer::HasInstance(picture.ToLocalChecked())) {

        char* buffer = node::Buffer::Data(picture.ToLocalChecked());
        const size_t blen = node::Buffer::Length(picture.ToLocalChecked());
        TagLib::ByteVector data(buffer, blen);

        TagLib::Picture pic(data,
          TagLib::Picture::FrontCover,
          mimetype,
          "Added with node-taglib2");

        picMap.insert(pic);
        hasPics = true;
      }
    }

    if (hasPics) {
      tag->setPictures(picMap);
    }
  }

  f.save();

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
